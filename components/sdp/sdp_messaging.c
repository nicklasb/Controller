/**
 * @file sdp_messaging.c
 * @author Nicklas Börjesson (nicklasb@gmail.com)
 * @brief Handles incoming and outgoing messaging
 *
 * @version 0.1
 * @date 2023-01-07
 *
 * @copyright Copyright (c) 2023
 *
 */

/** TODO:
 * We should probably split this into an incoming and an outgoing part.
 * The incoming part could then be unaware/uncaring of what media is the source, and the sending could contain all the media specific #ifdefs
 */
#include "sdp_messaging.h"
#include <esp_log.h>
#include <esp32/rom/crc.h>

#include "string.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "sdp_mesh.h"
#include "sdp_helpers.h"
#include "orchestration/orchestration.h"
#include "sdp_worker.h"

#include "sdkconfig.h"

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_global.h"
#endif
#ifdef CONFIG_SDP_LOAD_ESP_NOW
#include "espnow/espnow_messaging.h"
#endif
#ifdef CONFIG_SDP_LOAD_LORA
#include "lora/lora_worker.h"
#endif

#ifdef CONFIG_SDP_LOAD_I2C
#include "i2c/i2c_worker.h"
#include "i2c/i2c_messaging.h"
#endif

int callcount = 0;

filter_callback *on_filter_request_cb = NULL;
filter_callback *on_filter_reply_cb = NULL;
filter_callback *on_filter_data_cb = NULL;

work_callback *on_priority_cb;

SLIST_HEAD(conversation_list, conversation_list_item)
conversation_l;

/* The last created conversation id */
uint16_t last_conversation_id = 0;

/* The log prefix for all logging */
char *messaging_log_prefix;

/* Semaphore for thread safety  */
SemaphoreHandle_t x_conversation_list_semaphore;

/* Forward declarations*/
int sdp_send_message(struct sdp_peer *peer, void *data, int data_length);

/**
 * @brief Create a complete message structure by adding a preamble with crc32 of the data, work type and conversation 
 * NOTE: This makes it possible to free() the original data memory. This could be a good idea if its a large amount.
 * TODO: This should probably be called create message or something like that.
 * @param work_type The type of work
 * @param conversation_id The conversation id
 * @param data The data to be sent
 * @param data_length The length of the data
 * @return void*
 */

void *sdp_add_preamble(e_work_type work_type, uint16_t conversation_id, const void *data, int data_length)
{
    char *preambled_data = malloc(data_length + SDP_PREAMBLE_LENGTH);

    preambled_data[SDP_CRC_LENGTH] = (uint8_t)work_type;
    preambled_data[SDP_CRC_LENGTH + 1] = (uint8_t)(&conversation_id)[0];
    preambled_data[SDP_CRC_LENGTH + 2] = (uint8_t)(&conversation_id)[1];
    memcpy(preambled_data + SDP_PREAMBLE_LENGTH, data, (size_t)data_length);
    // Calc crc on the entire message
    unsigned int crc32 = crc32_be(0, (uint8_t *)preambled_data + SDP_CRC_LENGTH, 
        data_length + SDP_PREAMBLE_LENGTH - SDP_CRC_LENGTH);
    // Put it first in the message
    memcpy(preambled_data, &crc32, SDP_CRC_LENGTH);

    return preambled_data;
}

void parse_message(work_queue_item_t *queue_item)
{
    /* Check that the data ends with a NULL value to avoid having to
    check later (there should always be one there) */
    if (queue_item->raw_data[queue_item->raw_data_length - 1] != 0)
    {
        ESP_LOGW(messaging_log_prefix, "WARNING: The data doesn't end with a NULL value, setting it forcefully!");
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

/**
 * @brief Add a new conversation
 *
 * @param peer The peer we are conversing with
 * @param reason The reason for the conversation
 * @param conversation_id if < 0, it is a new conversation we initiated locally.
 * @return int
 */
int safe_add_conversation(sdp_peer *peer, const char *reason, int conversation_id)
{
    /* Create a conversation list item to keep track */

    struct conversation_list_item *new_item = malloc(sizeof(struct conversation_list_item));
    new_item->peer = peer;
    new_item->local = (conversation_id < 0);
    new_item->reason = malloc(strlen(reason));
    strcpy(new_item->reason, reason);

    /* Some things needs to be thread-safe */
    if (pdTRUE == xSemaphoreTake(x_conversation_list_semaphore, portMAX_DELAY))
    {
        if (new_item->local)
        {
            // This is a conversation we initiated
            new_item->conversation_id = last_conversation_id++;
        }
        else
        {
            // Initiated by the other peer
            new_item->conversation_id = conversation_id;
        }

        SLIST_INSERT_HEAD(&conversation_l, new_item, items);
        xSemaphoreGive(x_conversation_list_semaphore);
        return new_item->conversation_id;
    }
    else
    {
        ESP_LOGE(messaging_log_prefix, "Error: Couldn't get semaphore to add to conversation queue!");
        return -SDP_ERR_SEMAPHORE;
    }
}

int handle_incoming(sdp_peer *peer, const uint8_t *data, int data_len, e_media_type media_type)
{
    ESP_LOGI(messaging_log_prefix, "<< Payload length: %i, call count %i, CRC32: %"PRIu32".", data_len, callcount++,
             crc32_be(0, data, data_len));

    ESP_LOG_BUFFER_HEXDUMP(messaging_log_prefix, data, data_len, ESP_LOG_INFO);

    work_queue_item_t *new_item;

    if (data_len > SDP_PREAMBLE_LENGTH)
    {
        // TODO: Change malloc to something more optimized?
        new_item = malloc(sizeof(work_queue_item_t));
        new_item->crc32 = (uint32_t)data[0];
        new_item->work_type = (uint8_t)data[4];
        new_item->conversation_id = (uint16_t)data[5];
        new_item->raw_data_length = data_len - SDP_PREAMBLE_LENGTH;
        new_item->raw_data = malloc(new_item->raw_data_length);
        memcpy(new_item->raw_data, &(data[SDP_PREAMBLE_LENGTH]), new_item->raw_data_length);

        new_item->media_type = media_type;
        new_item->peer = peer;
        parse_message(new_item);

        // Save the conversation
        safe_add_conversation(peer, "external", new_item->conversation_id);

        ESP_LOGI(messaging_log_prefix, "<< Message info : Work type: %u, Conv.id: %u, Media type: %u,Data len: %u, Message parts: %i.",
                 new_item->work_type, new_item->conversation_id,
                 new_item->media_type, new_item->raw_data_length, new_item->partcount);
    }
    else
    {
        ESP_LOGE(messaging_log_prefix, "<< Error: The request must be more than %i bytes for SDP v %i compliance.",
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
                sdp_safe_add_work_queue(new_item);
            }
            else
            {
                ESP_LOGE(messaging_log_prefix, "<< SDP messaging: on_filter_request_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
        }
        else
        {
            sdp_safe_add_work_queue(new_item);
        }
        break;
    case REPLY:
        if (on_filter_reply_cb != NULL)
        {

            if (on_filter_reply_cb(new_item) == 0)
            {
                // Add the request to the work queue
                sdp_safe_add_work_queue(new_item);
            }
            else
            {
                ESP_LOGE(messaging_log_prefix, "<< SDP messaging: on_filter_request_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
        }
        else
        {
            sdp_safe_add_work_queue(new_item);
        }
        break;
    case DATA:
        if (on_filter_data_cb != NULL)
        {

            if (on_filter_data_cb(new_item) == 0)
            {
                // Add the request to the work queue
                sdp_safe_add_work_queue(new_item);
            }
            else
            {
                ESP_LOGE(messaging_log_prefix, "<< SDP messaging: on_filter_data_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
        }
        else
        {
            sdp_safe_add_work_queue(new_item);
        }
        break;

    case HANDSHAKE:

        // TODO: Are there security considerations here?
        // Should this not happen if a peer isn't already on some list
        // A peer says HI, and tells us a little about itself.
        if ((strncmp(new_item->parts[0], "HI", 2) == 0))
        {
            // We have already added it as a peer, lets get any information
            sdp_peer_inform(new_item);
            
            // If its wasn't a reply, we reply with our information
            if (strcmp(new_item->parts[0], "HIR") != 0)
            {
                if (sdp_peer_send_hi_message(peer, true) == SDP_MT_NONE)
                {
                    ESP_LOGE(messaging_log_prefix, "handle_incoming() - Failed to send HIR-message %s", new_item->peer->name);
                }
            }
        }

        break;
    case ORCHESTRATION:
        if (strcmp(new_item->parts[0], "WHEN") == 0)
        {
            // Reply
            sdp_orchestration_send_next_message(new_item);
        }
        else if (strcmp(new_item->parts[0], "NEXT") == 0)
        {
            sdp_orchestration_parse_next_message(new_item);
        }
        break;

    case PRIORITY:
        /* This is likely to be some kind of problem report or alarm,
        immidiately respond with CRC32 to tell the
        reporter that the information has reached the controller. */
        // TODO: Consider if the response perhaps whould be nonblocking (perhaps this is not needed if all media have queues)
        sdp_send_message(new_item->peer, &(new_item->crc32), 2);

        /* Do NOT add the work item to the queue, it will be immidiately adressed in the callback */

        if (on_priority_cb != NULL)
        {
            ESP_LOGW(messaging_log_prefix, "<< SDP Calling on_priority_callback!");

            on_priority_cb(new_item);
        }
        else
        {
            ESP_LOGE(messaging_log_prefix, "<< ERROR: SDP on_priority callback is not assigned, assigning to normal handling!");
            sdp_safe_add_work_queue(new_item);
        }
        break;
    case QOS:
        // Don't do much here, usually
        break;

    default:
        ESP_LOGE(messaging_log_prefix, "<< ERROR: Invalid work type (%i)!", new_item->work_type);
        return 1;
    }
    return 0;
}

// TODO: The following is inactivated for two reasons:
// 1. I don't properly understand how *declare* the sdp_peers list in a way that can be share using the extern keyword
// 1. FIX: This has now been understood, see mesh.h for the new solution
// 2. This might not be how to broadcast as it is not thread safe if a peer disconnects and the memory is freed, we will crash
#if 0
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
    int ret;
    SLIST_FOREACH(curr_peer, &sdp_peers, next)
    {
        ret = sdp_send_message(curr_peer, data, data_length);
        if (ret < 0)
        {
            ESP_LOGE(messaging_log_prefix, "Error: broadcast_message: Failure sending message! Peer: %s Code: %i", curr_peer->name, ret);
            errors++;
        }
        else
        {
            ESP_LOGI(messaging_log_prefix, "Sent a message to peer: %s Code: %i", curr_peer->name, ret);
        }

        total++;
    }

    if (total == 0)
    {
        ESP_LOGW(messaging_log_prefix, "Broadcast had no peers to send to!");
        return -SDP_WARN_NO_PEERS;
    }

    if (errors == total)
    {   
        ESP_LOGW(messaging_log_prefix, "Broadcast failed all %i send attempts!", total);
        return -SDP_ERR_SEND_FAIL;
    }
    else if (errors > 0)
    {
        ESP_LOGW(messaging_log_prefix, "Broadcast failed %i send attempts!", errors);
        return -SDP_ERR_SEND_SOME_FAIL;
    }
    else
    {
        ESP_LOGI(messaging_log_prefix, "Broadcast sent to %i peers.", total);
        return SDP_OK;
    }
}

#endif



/**
 * @brief Sends to a specified peer

 */
int sdp_send_message_media_type(struct sdp_peer *peer, void *data, int data_length, e_media_type media_type, bool just_checking)
{
    // Should this function also be put in a separate file? Perhaps along with other #ifs? To simplify and clean up this one?
    int rc = 0;
    int result = ESP_FAIL;
    

    sdp_media_types host_supported_media_types = get_host_supported_media_types();

    ESP_LOGI(messaging_log_prefix, ">> sdp_send_message_media_type called, media type: %hhu ", media_type);

    if (!(host_supported_media_types & media_type)) {
        ESP_LOGE(messaging_log_prefix, ">> sdp_send_message_media_type called, media type %hhu not supported, available are %hhu", media_type, host_supported_media_types);
        return -SDP_ERR_NOT_SUPPORTED;
    }

#ifdef CONFIG_SDP_LOAD_BLE

    // TODO; The BLE-connection handle stuff feels like it needs to be reworked. 
    // If it has a connection handle should be sorted when becoming a peer, right?
    // (peer->ble_conn_handle >= 0) && (peer->supported_media_types &
    // Send message using BLE
    if (media_type == SDP_MT_BLE)
    {
        rc = ble_send_message(peer->ble_conn_handle, data, data_length);
        if (rc == 0)
        {
            result = SDP_MT_BLE;
        }
        else
        {
            report_ble_connection_error(peer->ble_conn_handle, rc);
            result = -SDP_MT_BLE

        }
    }
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
    if (media_type == SDP_MT_ESPNOW)
    {
        if (!just_checking) {
            ESP_LOGI(messaging_log_prefix, ">> ESP-NOW sending to: ");
            ESP_LOG_BUFFER_HEX(messaging_log_prefix, peer->base_mac_address, SDP_MAC_ADDR_LEN);
            ESP_LOGI(messaging_log_prefix, ">> Data (including 4 bytes preamble): ");
            ESP_LOG_BUFFER_HEXDUMP(messaging_log_prefix, data, data_length, ESP_LOG_INFO);
   
        }
        rc = espnow_send_message(peer->base_mac_address, data, data_length, just_checking);
        if (rc == 0)
        {
            result = SDP_MT_ESPNOW;
        }
        else
        {
            if (!just_checking) {  
                ESP_LOGE(messaging_log_prefix, ">> Sending using ESP-NOW failed.");
            }
            result = -SDP_MT_ESPNOW;
            //  TODO: Add start general QoS monitoring, stop using some technologies if they are failing
        }
    }



#endif
#ifdef CONFIG_SDP_LOAD_LORA

    if (media_type == SDP_MT_LoRa)
    {
        ESP_LOGI(messaging_log_prefix, ">> LoRa sending to: ");
        ESP_LOG_BUFFER_HEX(messaging_log_prefix, peer->base_mac_address, SDP_MAC_ADDR_LEN);
        ESP_LOGI(messaging_log_prefix, ">> Data (including 4 bytes preamble): ");
        ESP_LOG_BUFFER_HEXDUMP(messaging_log_prefix, data, data_length, ESP_LOG_INFO);

        rc = lora_safe_add_work_queue(peer, data, data_length, just_checking);
        if (rc == 0)
        {
            result = SDP_MT_LoRa;
        }
        else
        {
            ESP_LOGE(messaging_log_prefix, ">> Sending using Lora failed.");
            result = -SDP_MT_LoRa;
            //  TODO: Add start general QoS monitoring, stop using medias if they are failing
        }
    }
#endif
#ifdef CONFIG_SDP_LOAD_I2C

    if (media_type == SDP_MT_I2C)
    {
        ESP_LOGI(messaging_log_prefix, ">> I2C sending to I2C host: %hhu ", peer->i2c_address);
        ESP_LOGI(messaging_log_prefix, ">> Data (including 4 bytes preamble): ");
        ESP_LOG_BUFFER_HEXDUMP(messaging_log_prefix, data, data_length, ESP_LOG_INFO);

        rc = i2c_safe_add_work_queue(peer, data, data_length, just_checking);
        if (rc == 0)
        {
            result = SDP_MT_I2C;
        }
        else
        {
            ESP_LOGE(messaging_log_prefix, ">> Sending using Lora failed.");
            result = -SDP_MT_I2C;
            //  TODO: Add start general QoS monitoring, stop using a medium if it is failing
        }
    }
#endif
    return result;
}

/**
 * @brief Sends to a specified peer

 */
int sdp_send_message(struct sdp_peer *peer, void *data, int data_length)
{

    int rc = ESP_FAIL;
    int retries = 0;
    e_media_type preferred = SDP_MT_NONE;
    ESP_LOGI(messaging_log_prefix, ">> peer->supported_media_types: %hhx ", peer->supported_media_types);

    do
    {
        // For each try, we need to reevalue what media we are selecting.
        e_media_type selected_media_type = select_media(peer, data_length);
        rc = sdp_send_message_media_type(peer, data, data_length, selected_media_type, false);
        if (rc < 0)
        {
            if (retries < 4) {
                ESP_LOGI(messaging_log_prefix, ">> Send failed; retrying %i more times ", 3 - retries );
            } else {
                ESP_LOGE(messaging_log_prefix, ">> Send failed, will not retry any more.");
            }
        }
        retries++;
        
    } while ((rc < 0) && (retries < 4)); // We need to have a general retry limit here? Or is it even at this level we retry?
        

    return rc;
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

    ESP_LOGD(messaging_log_prefix, ">> In sdp reply.");
    retval = sdp_send_message(queue_item.peer, new_data, data_length + SDP_PREAMBLE_LENGTH);
    free(new_data);
    return retval;
}
/**
 * @brief Start a new conversation
 *
 * @param peer The peer
 * @param work_type The type of work
 * @param data The message data
 * @param data_length Length of the data
 * @return int Returns the conversation id if successful.
 * NOTE: Returns negative error values on failure.
 */
int start_conversation(sdp_peer *peer, enum e_work_type work_type,
                       const char *reason, const void *data, int data_length)
{
    int retval = -SDP_ERR_SEND_FAIL;
    // Create and add a new conversation item and add to queue
    int new_conversation_id = safe_add_conversation(peer, reason, -1);
    if (new_conversation_id >= 0) //
    {
        // Add preamble including all SDP specifics in a new data
        void *new_data = sdp_add_preamble(work_type, new_conversation_id, data, data_length);
        if (peer == NULL)
        {
            retval = SDP_ERR_NOT_SUPPORTED;
            /* See definition for information
            retval = broadcast_message(new_conversation_id, work_type,
                                        new_data, data_length + SDP_PREAMBLE_LENGTH);
            */
        }
        else
        {
            retval = sdp_send_message(peer, new_data, data_length + SDP_PREAMBLE_LENGTH);
        }
        free(new_data);

        if (retval < 0)
        {
            if (retval == -SDP_WARN_NO_PEERS)
            {
                ESP_LOGW(messaging_log_prefix, "Removing conversation, no peers");
            }
            else
            {
                ESP_LOGE(messaging_log_prefix, "Error %i in communication, removing conversation.", retval);
            }
            /* The communication failed, remove the conversation*/
            end_conversation(new_conversation_id);
            retval = -SDP_ERR_SEND_FAIL;
        }
    }
    else
    {
        // < 0 means that the conversation wasn't created.
        ESP_LOGE(messaging_log_prefix, "Error: start_conversation - Failed to create a conversation.");
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

struct conversation_list_item *find_conversation(sdp_peer *peer, uint16_t conversation_id)
{
    struct conversation_list_item *curr_conversation;
    SLIST_FOREACH(curr_conversation, &conversation_l, items)
    {
        if ((curr_conversation->conversation_id == conversation_id) &&
            (curr_conversation->peer->peer_handle == peer->peer_handle))
        {
            return curr_conversation;
        }
    }
    return NULL;
}

void sdp_init_messaging(char *_log_prefix, work_callback *priority_cb)
{

    messaging_log_prefix = _log_prefix;
    on_priority_cb = priority_cb;

    sdp_mesh_init(messaging_log_prefix);

    /* Create a queue semaphore to ensure thread safety */
    x_conversation_list_semaphore = xSemaphoreCreateMutex();

    /* Conversation list initialisation */
    SLIST_INIT(&conversation_l);
}
