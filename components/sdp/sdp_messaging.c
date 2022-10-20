#include "sdp_messaging.h"
#include <esp_log.h>
#include <esp32/rom/crc.h>

#include "string.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "sdp_mesh.h"
#include "sdp_peer.h"
#include "sdp_def.h"
#include "sdp_helpers.h"

#include "sdkconfig.h"

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_global.h"
#endif
#ifdef CONFIG_SDP_LOAD_ESP_NOW
#include "espnow/espnow_messaging.h"

#endif

#define CONFIG_SDP_MAX_PEERS 20

int callcount = 0;

/* The current conversation id */
uint16_t conversation_id = 0;

/* The log prefix for all logging */
char *log_prefix;

/* Semaphore for thread safety  */
SemaphoreHandle_t x_conversation_list_semaphore;

/* Forward declarations*/
e_media_type send_message(struct sdp_peer *peer, void *data, int data_length);

/**
 * @brief Add a preamble with general info in the beginning of a message
 *
 * @param work_type
 * @param conversation_id
 * @param data
 * @param data_length
 * @return void*
 */

void *sdp_add_preamble(e_work_type work_type, uint16_t conversation_id, const void *data, int data_length)
{
    char *new_data = malloc(data_length + SDP_PREAMBLE_LENGTH);
    new_data[0] = SDP_PROTOCOL_VERSION;
    new_data[1] = (uint8_t)(&conversation_id)[0];
    new_data[2] = (uint8_t)(&conversation_id)[1];
    new_data[3] = (uint8_t)work_type;
    memcpy(&(new_data[SDP_PREAMBLE_LENGTH]), data, (size_t)data_length);
    return new_data;
}

void parse_message(work_queue_item_t *queue_item)
{

    /* Check that the data ends with a NULL value to avoid having to
    check later (there should always be one there) */
    if (queue_item->raw_data[queue_item->raw_data_length - 1] != 0)
    {
        ESP_LOGW(log_prefix, "WARNING: The data doesn't end with a NULL value, setting it forcefully!");
        queue_item->raw_data[queue_item->raw_data_length - 1] = 0;
    }
    // Count the parts to only need to allocate the array once
    // TODO: Suppose this loop doesn't take long but it would be interesting not having to do it.
    // TODO: Since we seem to have to make this pass, can we do something else here?
    int nullcount = 0;
    for (int i = 0; i < queue_item->raw_data_length; i++)
    {
        if (queue_item->raw_data[i] == 0)
        {
            nullcount++;
        }
    }
    queue_item->parts = heap_caps_malloc(nullcount * sizeof(int32_t), MALLOC_CAP_32BIT);
    // The first byte is always the beginning of a part

    queue_item->parts[0] = queue_item->raw_data;
    queue_item->partcount = 1;
    // Loop the data and set pointers, avoiding the last one which ends everything
    for (int j = 0; j < queue_item->raw_data_length - 1; j++)
    {

        if (queue_item->raw_data[j] == 0)
        {
            queue_item->parts[queue_item->partcount] = &(queue_item->raw_data[j + 1]);
            queue_item->partcount++;
        }
    }
}

int handle_incoming(sdp_peer *peer, const uint8_t *data, int data_len, e_media_type media_type)
{
    ESP_LOGI(log_prefix, "Payload length: %i, call count %i, CRC32: %u", data_len, callcount++,
             crc32_be(0, data, data_len));

    ESP_LOG_BUFFER_HEXDUMP(log_prefix, data, data_len, ESP_LOG_INFO);

    work_queue_item_t *new_item;

    if (data_len > SDP_PREAMBLE_LENGTH)
    {
        // TODO: Change malloc to something more optimized?
        new_item = malloc(sizeof(work_queue_item_t));
        new_item->version = data[0];
        new_item->conversation_id = (uint16_t)data[1];
        new_item->work_type = data[3];
        new_item->raw_data_length = data_len - SDP_PREAMBLE_LENGTH;
        new_item->raw_data = malloc(new_item->raw_data_length);
        memcpy(new_item->raw_data, &(data[SDP_PREAMBLE_LENGTH]), new_item->raw_data_length);

        new_item->media_type = media_type;
        new_item->peer = peer;
        parse_message(new_item);

        ESP_LOGI(log_prefix, "Message info : Version: %u, Conv.id: %u, Work type: %u, Media type: %u,Data len: %u, Message parts: %i.",
                 new_item->version, new_item->conversation_id, new_item->work_type,
                 new_item->media_type, new_item->raw_data_length, new_item->partcount);
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: The request must be more than %i bytes for SDP v %i compliance.",
                 SDP_PROTOCOL_VERSION, SDP_PREAMBLE_LENGTH);
        return SDP_ERR_MESSAGE_TOO_SHORT;
    }

    // Handle the different request types
    // TODO: Interestingly, on_filter_data_cb seems to initialize to NULL by itself. Or does it?

    switch (new_item->work_type)
    {

    case REQUEST:
        if (on_filter_request_cb != NULL)
        {
            if (on_filter_request_cb(new_item) == 0)
            {
                // Add the request to the work queue
                safe_add_work_queue(new_item);
            }
            else
            {
                ESP_LOGE(log_prefix, "SDP service: on_filter_request_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
        }
        else
        {
            safe_add_work_queue(new_item);
        }
        break;
    case REPLY:
        if (on_filter_reply_cb != NULL)
        {

            if (on_filter_reply_cb(new_item) == 0)
            {
                // Add the request to the work queue
                safe_add_work_queue(new_item);
            }
            else
            {
                ESP_LOGE(log_prefix, "SDP service: on_filter_request_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
        }
        else
        {
            safe_add_work_queue(new_item);
        }
        break;
    case DATA:
        if (on_filter_data_cb != NULL)
        {

            if (on_filter_data_cb(new_item) == 0)
            {
                // Add the request to the work queue
                safe_add_work_queue(new_item);
            }
            else
            {
                ESP_LOGE(log_prefix, "SDP service: on_filter_data_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
        }
        else
        {
            safe_add_work_queue(new_item);
        }
        break;

    case HANDSHAKE:
        // A peer asks us about our abilities
        // TODO: Are there security considerations here?
        if (strcmp(new_item->parts[0], "WHO") == 0)
        {
            // Reply
            sdp_peer_send_me_message(new_item);
        }
        else if (strcmp(new_item->parts[0], "ME") == 0)
        {
            sdp_peer_inform(new_item);
        }

        break;

    case PRIORITY:
        /* This is likely to be some kind of problem report or alarm,
        immidiately respond with CRC32 to tell the
        reporter that the information has reached the controller. */

        send_message(new_item->peer, &(new_item->crc32), 2);

        /* Do NOT add the work item to the queue, it will be immidiately adressed in the callback */

        if (on_priority_cb != NULL)
        {
            ESP_LOGW(log_prefix, "SDP Calling on_priority_callback!");

            on_priority_cb(new_item);
        }
        else
        {
            ESP_LOGE(log_prefix, "ERROR: SDP on_priority callback is not assigned, assigning to normal handling!");
            safe_add_work_queue(new_item);
        }
        break;

    default:
        ESP_LOGE(log_prefix, "ERROR: Invalid work type (%i)!", new_item->work_type);
        return 1;
    }
    return 0;
}

/**
 * @brief Send a message to one or more peers
 *
 * @param conn_handle A negative value will cause all peers to be messaged
 * @param conversation_id Used to keep track of conversations
 * @param work_type The kind of message
 * @param data A pointer to the data to be sent
 * @param data_length The length of the data in bytes
 * @return int A negative return value will mean a failure of the operation
 * TODO: Handle partial failure, for example if one peripheral doesn't answer.
 */
int broadcast_message(uint16_t conversation_id,
                      enum e_work_type work_type, void *data, int data_length)
{
    struct sdp_peer *curr_peer;
    int total = 0, errors = 0;
    e_media_type ret;
    SLIST_FOREACH(curr_peer, &sdp_peers, next)
    {
        ret = send_message(curr_peer, data, data_length);
        if (ret == SDP_MT_NONE)
        {
            ESP_LOGE(log_prefix, "Error: broadcast_message: Failure sending message! Peer: %s Code: %i", curr_peer->name, ret);
            errors++;
        }
        else
        {
            ESP_LOGI(log_prefix, "Sent a message to peer: %s Code: %i", curr_peer->name, ret);
        }

        total++;
    }

    if (total == 0)
    {
        ESP_LOGW(log_prefix, "Broadcast had no peers to send to!");
        return -SDP_WARN_NO_PEERS;
    }

    if (errors == total)
    {   
        ESP_LOGW(log_prefix, "Broadcast failed all %i send attempts!", total);
        return -SDP_ERR_SEND_FAIL;
    }
    else if (errors > 0)
    {
        ESP_LOGW(log_prefix, "Broadcast failed %i send attempts!", errors);
        return -SDP_ERR_SEND_SOME_FAIL;
    }
    else
    {
        ESP_LOGI(log_prefix, "Broadcast sent to %i peers.", total);
        return SDP_OK;
    }
}

#ifdef CONFIG_SDP_LOAD_BLE
void report_ble_connection_error(int conn_handle, int code)
{
    struct ble_peer *b_peer = ble_peer_find(conn_handle);

    if (b_peer != NULL)
    {
        struct sdp_peer *s_peer = sdp_mesh_find_peer_by_handle(b_peer->sdp_handle);
        if (s_peer != NULL)
        {
            ESP_LOGE(log_prefix, "Peer %s encountered a BLE error. Code: %i", s_peer->name, code);
        }
        else
        {
            ESP_LOGE(log_prefix, "Unresolved BLE peer (no or invalid SDP peer handle), conn handle %i", conn_handle);
            ESP_LOGE(log_prefix, "encountered a BLE error. Code: %i.", code);
        }
        b_peer->failure_count++;
    }
    ESP_LOGE(log_prefix, "Unregistered peer (!) at conn handle %i encountered a BLE error. Code: %i.", conn_handle, code);
}
#endif

/**
 * @brief Like send_message, but only sends to one specified peer using first available media type.

 */
e_media_type send_message(struct sdp_peer *peer, void *data, int data_length)
{
    int rc = 0;
#ifdef CONFIG_SDP_LOAD_BLE

    // Send message using BLE
    if (peer->ble_conn_handle >= 0)
    {
        rc = ble_send_message(peer->ble_conn_handle, data, data_length);
        if (rc == 0)
        {
            return SDP_MT_BLE;
        }
        else
        {
            report_ble_connection_error(peer->ble_conn_handle, rc);
            // TODO: Add start general QoS monitoring, stop using some technologies if they are failing
        }
    }
#endif
#ifdef CONFIG_SDP_LOAD_ESP_NOW
    ESP_LOGI(log_prefix, "ESP-NOW sending to: ");
    ESP_LOG_BUFFER_HEX(log_prefix, peer->base_mac_address, SDP_MAC_ADDR_LEN);
    rc = espnow_send_message(peer->base_mac_address, data, data_length);

    if (rc == 0)
    {
        return SDP_MT_ESPNOW;
    }
    else
    {
        ESP_LOGE(log_prefix, "Sending using ESPNOW failed.");
        //report_ble_connection_error(peer->ble_conn_handle, rc);
        // TODO: Add start general QoS monitoring, stop using some technologies if they are failing
    }

    //if (peer->base_mac_address) {}

#endif

    return SDP_MT_NONE;
}

int safe_add_conversation(sdp_peer *peer, const char *reason)
{
    /* Create a conversation list item to keep track */

    struct conversation_list_item *new_item = malloc(sizeof(struct conversation_list_item));
    new_item->peer = peer;
    new_item->reason = malloc(strlen(reason));
    strcpy(new_item->reason, reason);

    /* Some things needs to be thread-safe */
    if (pdTRUE == xSemaphoreTake(x_conversation_list_semaphore, portMAX_DELAY))
    {
        new_item->conversation_id = conversation_id++;

        SLIST_INSERT_HEAD(&conversation_l, new_item, items);
        xSemaphoreGive(x_conversation_list_semaphore);
        return new_item->conversation_id;
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: Couldn't get semaphore to add to conversation queue!");
        return -SDP_ERR_SEMAPHORE;
    }
}

/**
 * @brief Replies to the sender in the queue item
 * Automatically adds the correct SDP preamble, data is
 *
 * @param queue_item
 * @param work_type
 * @param data
 * @param data_length
 * @return int
 */
int sdp_reply(work_queue_item_t queue_item, enum e_work_type work_type, const void *data, int data_length)
{
    int retval = SDP_OK;
    // Add preamble with all SDP specifics in a new data
    void *new_data = sdp_add_preamble(work_type, queue_item.conversation_id, data, data_length);

    ESP_LOGD(log_prefix, "In sdp reply.");
    retval = send_message(queue_item.peer, new_data, data_length + SDP_PREAMBLE_LENGTH);
    free(new_data);
    return retval;
}
/**
 * @brief Start a new conversation
 *
 * @param media_type Media type
 * @param conn_handle A handle to the connection (if negative -1 loop all)
 * @param work_type
 * @param data
 * @param data_length
 * @param log_tag
 * @return int Returns the conversation id if successful.
 * NOTE: Returns negative error values on failure.
 */
int start_conversation(sdp_peer *peer, enum e_work_type work_type,
                       const char *reason, const void *data, int data_length)
{
    int retval = -SDP_ERR_SEND_FAIL;
    // Create and add a new conversation item and add to queue
    int new_conversation_id = safe_add_conversation(peer, reason);
    if (new_conversation_id >= 0) //
    {
        // Add preamble including all SDP specifics in a new data
        void *new_data = sdp_add_preamble(work_type, new_conversation_id, data, data_length);
        if (peer == NULL)
        {
            retval = broadcast_message(new_conversation_id, work_type,
                                        new_data, data_length + SDP_PREAMBLE_LENGTH);
        }
        else
        {
            retval = send_message(peer, new_data, data_length + SDP_PREAMBLE_LENGTH);
        }
        free(new_data);

        if (retval < 0)
        {
            if (retval == -SDP_WARN_NO_PEERS)
            {
                ESP_LOGW(log_prefix, "Removing conversation, no peers");
            }
            else
            {
                ESP_LOGE(log_prefix, "Error %i in communication, removing conversation.", retval);
            }
            /* The communication failed, remove the conversation*/
            end_conversation(new_conversation_id);
            retval = -SDP_ERR_SEND_FAIL;
        }
    }
    else
    {
        // < 0 means that the conversation wasn't created.
        ESP_LOGE(log_prefix, "Error: start_conversation - Failed to create a conversation.");
        retval = -SDP_ERR_CONV_QUEUE;
    }

    return retval;
}

int end_conversation(uint16_t conversation_id)
{
    struct conversation_list_item *curr_conversation;
    SLIST_FOREACH(curr_conversation, &conversation_l, items)
    {
        if (curr_conversation->conversation_id == conversation_id)
        {
            SLIST_REMOVE(&conversation_l, curr_conversation, conversation_list_item, items);
            free(curr_conversation->reason);
            free(curr_conversation);
            return SDP_OK;
        }
    }
    return SDP_ERR_CONV_QUEUE;
}

struct conversation_list_item *find_conversation(uint16_t conversation_id)
{
    struct conversation_list_item *curr_conversation;
    SLIST_FOREACH(curr_conversation, &conversation_l, items)
    {
        if (curr_conversation->conversation_id == conversation_id)
        {
            return curr_conversation;
        }
    }
    return NULL;
}

int get_conversation_id(void)
{
    return conversation_id;
}

void init_messaging(char *_log_prefix)
{

    log_prefix = _log_prefix;

    sdp_mesh_init(_log_prefix, CONFIG_SDP_MAX_PEERS);

    /* Create a queue semaphore to ensure thread safety */
    x_conversation_list_semaphore = xSemaphoreCreateMutex();

    /* Conversation list initialisation */
    SLIST_INIT(&conversation_l);
}
