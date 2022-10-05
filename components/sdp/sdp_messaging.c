#include "sdp_messaging.h"
#include <esp_log.h>
#include <esp32/rom/crc.h>
#include <os/queue.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_global.h"
#endif

int callcount = 0;

void parse_message(struct work_queue_item *queue_item)
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

int handle_incoming(uint16_t conn_handle, uint16_t attr_handle, char* data, int data_len, e_media_type media_type, void *arg)
{
    ESP_LOGI(log_prefix, "Payload length: %i, call count %i, CRC32: %u", data_len, callcount++,
             crc32_be(0, &data, data_len));

    struct work_queue_item *new_item;

    if (data_len > SDP_PREAMBLE_LENGTH)
    {
        // TODO: Change malloc to something more optimized?
        new_item = malloc(sizeof(struct work_queue_item));
        new_item->version = (uint8_t)data[0];
        new_item->conversation_id = (uint16_t)data[1];
        new_item->work_type = (uint8_t)data[3];
        new_item->raw_data_length = data_len - SDP_PREAMBLE_LENGTH;
        new_item->raw_data = malloc(new_item->raw_data_length);
        memcpy(new_item->raw_data, &(data[SDP_PREAMBLE_LENGTH]), new_item->raw_data_length);

        
        new_item->media_type = media_type;
        new_item->conn_handle = conn_handle;
        parse_message(new_item);

        ESP_LOGI(log_prefix, "Message info : Version: %u, Conv.id: %u, Work type: %u, Media type: %u,Data len: %u, Message parts: %i.",
                 new_item->version, new_item->conversation_id, new_item->work_type,
                 new_item->media_type, new_item->raw_data_length, new_item->partcount);
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: The request must be more than %i bytes for SDP v %i compliance.",
                 SPD_PROTOCOL_VERSION, SDP_PREAMBLE_LENGTH);
        return SDP_ERR_MESSAGE_TOO_SHORT;
    }

    // Handle the different request types
    // TODO:Interestingly, on_filter_data_cb seems to initialize to NULL by itself. Or does it?

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
                    ESP_LOGE(log_prefix, "BLE service: on_filter_request_cb returned a nonzero value, request not added to queue!");
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
                    ESP_LOGE(log_prefix, "BLE service: on_filter_data_cb returned a nonzero value, request not added to queue!");
                    return SDP_ERR_MESSAGE_FILTERED;
                }
            }
            else
            {
                safe_add_work_queue(new_item);
            }
            break;

        case PRIORITY:
            /* If it is a problem report,
            immidiately respond with CRC32 to tell the
            reporter that the information has reached the controller. */
            
            send_message(new_item->conn_handle,
                            new_item->conversation_id, DATA, &(new_item->crc32), 2);

            /* Do NOT add the work item to the queue, it will be immidiately adressed in the callback */

            if (on_priority_cb != NULL)
            {
                ESP_LOGW(log_prefix, "BLE Calling on_priority_callback!");

                on_priority_cb(new_item);
            }
            else
            {
                ESP_LOGE(log_prefix, "ERROR: BLE on_priority callback is not assigned, assigning to normal handling!");
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
                          enum e_work_type work_type, const void *data, int data_length)
{
    struct peer *curr_peer;
    int ret = 0, total = 0, errors = 0;
    SLIST_FOREACH(curr_peer, &peers, next)
    {
        ret = send_message(curr_peer->conn_handle, conversation_id, work_type, data, data_length);
        if (ret != 0)
        {
            ESP_LOGE(log_prefix, "Error: ble_broadcast_message: Failure sending message! Peer: %u Code: %i", curr_peer->conn_handle, ret);
            errors++;
        } else {
            ESP_LOGI(log_prefix, "Sent a message to Peer: %u Code: %i", curr_peer->conn_handle, ret);
            
        }

        total++;
    }

    if (total == 0) {
        ESP_LOGW(log_prefix, "Broadcast had no peers to send to!");
    }

    if (errors == total)
    {
        return SDP_ERR_SEND_FAIL;
    }
    else if (errors > 0)
    {
        return SDP_ERR_SEND_SOME_FAIL;
    }
    else
    {
        return SDP_OK;
    }
}

/**
 * @brief Like send_message, but only sends to one specified peer.
 * Note the unsigned type of the connection handle, a positive value is needed.
 */
int send_message(uint16_t conn_handle, uint16_t conversation_id,
                     enum e_work_type work_type, const void *data, int data_length)
{
    #ifdef CONFIG_SDP_LOAD_BLE
    // Send message using BLE
    return ble_send_message(conn_handle, conversation_id, work_type, data, data_length);
    #endif

}