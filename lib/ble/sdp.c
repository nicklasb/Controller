#include "sdp.h"

#include "ble_global.h"
#include "esp_log.h"
#include "sdp_worker.h"

/* The current conversation id */
uint16_t conversation_id = 0;




int sdp_init(work_callback work_cb, work_callback priority_cb, const char *_log_prefix, bool is_controller)
{
    log_prefix = _log_prefix;
    if (work_cb == NULL || priority_cb == NULL)
    {
        ESP_LOGE(log_prefix, "Error: Both work_cb and priority_cb are mandatory parameters, cannot be NULL!");
        return SDP_ERR_INIT_FAIL;
    }

    on_work_cb = work_cb;
    on_priority_cb = priority_cb;

    xQueue_Semaphore = xSemaphoreCreateMutex();
    init_worker(log_prefix);
    ESP_LOGI(log_prefix, "SDP initiated!");
    return 0;
}

int safe_add_work_queue(struct work_queue_item *new_item)
{
    if (pdTRUE == xSemaphoreTake(xQueue_Semaphore, portMAX_DELAY))
    {
        /* As the worker takes the queue from the head, and we want a LIFO, add the item to the tail */
        STAILQ_INSERT_TAIL(&work_q, new_item, items);
        xSemaphoreGive(xQueue_Semaphore);
    }
    else
    {
        ESP_LOGE(log_prefix, "Couldn't get semaphore to add to work queue!");
        return 1;
    }
    return 0;
}

int safe_add_conversation(uint16_t conn_handle, media_type media_type)
{
    /* Create a conversation list item to keep track */

    struct conversation_list_item *new_item = malloc(sizeof(struct conversation_list_item));
    new_item->conn_handle = conn_handle;
    new_item->media_type = media_type;
    /* Some things needs to be thread-safe */
    if (pdTRUE == xSemaphoreTake(xQueue_Semaphore, portMAX_DELAY))
    {
        new_item->conversation_id = conversation_id++;

        SLIST_INSERT_HEAD(&conversation_l, new_item, items);
        xSemaphoreGive(xQueue_Semaphore);
        return new_item->conversation_id;
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: Couldn't get semaphore to add to conversation queue!");
        return 0;
    }

}

int respond(struct work_queue_item queue_item, enum work_type work_type, const void *data, int data_length)
{

    switch (queue_item.media_type)
    {
    case BLE:
        return ble_send_message(queue_item.conn_handle, queue_item.conversation_id,
                         work_type, data, data_length, log_prefix);
        break;
    default:
        break;
    }
    return 0;
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
int start_conversation(enum media_type media_type, int conn_handle,
                       enum work_type work_type, const void *data, int data_length)
{   // Create and add a new conversation item and add to queue
    int new_conversation_id = safe_add_conversation(conn_handle, media_type);
    if (new_conversation_id != 0) // 
    {
        switch (media_type)
        {
        case BLE:
            return -ble_send_message(conn_handle, new_conversation_id, work_type, data, data_length, log_prefix);
            break;

        default:
            break;
        }
    } else {
        // -1 means that the conversation wasn't created.
        return -SDP_ERR_CONV_QUEUE;
    }

    return 0;
}
