/**
 * @file sdp.c
 * @author Nicklas Borjesson
 * @brief This is the sensor data protocol server
 *
 * @copyright Copyright (c) 2022
 *
 */


#include "sdp.h"

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_init.h"
#endif

#include "sdp_monitor.h" /* TODO: Should this be separate so it can be optional? */
#include "sdp_worker.h"
#include "sdp_messaging.h"

#include <esp_log.h>

/* The current conversation id */
uint16_t conversation_id = 0;

/* Must be used when changing the work queue  */
SemaphoreHandle_t xQueue_Semaphore;

int sdp_init(work_callback work_cb, work_callback priority_cb, const char *_log_prefix, bool is_controller)
{
    log_prefix = _log_prefix;
    // Begin with initialising the monitor to capture initial memory state.
    init_monitor(log_prefix);

    if (work_cb == NULL || priority_cb == NULL)
    {
        ESP_LOGE(log_prefix, "Error: Both work_cb and priority_cb are mandatory parameters, cannot be NULL!");
        return SDP_ERR_INIT_FAIL;
    }

    on_work_cb = work_cb;
    on_priority_cb = priority_cb;

    /* Initialize queues and lists, can't be done in header files as that would reinit the list on include */
    STAILQ_INIT(&work_q);
    SLIST_INIT(&conversation_l);

    /* Create a semaphore for queue-handling */
    /* TODO: Consider putting this behind local functions instead */
    xQueue_Semaphore = xSemaphoreCreateMutex();
    init_worker(log_prefix);
    ESP_LOGI(log_prefix, "SDP initiated!");

    /* Init media types */
    #ifdef CONFIG_SDP_LOAD_BLE
    ble_init(log_prefix, is_controller);
    #endif
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

struct work_queue_item *safe_get_head_work_item(void)
{

    struct work_queue_item *curr_work = NULL;
    if (pdTRUE == xSemaphoreTake(xQueue_Semaphore, portMAX_DELAY))
    {
        /* Pull the first item from the work queue */
        curr_work = STAILQ_FIRST(&work_q);
        /* Immidiate deletion from the head of the queue */
        if (curr_work != NULL)
        {
            STAILQ_REMOVE_HEAD(&work_q, items);
        }
        xSemaphoreGive(xQueue_Semaphore);
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: Couldn't get semaphore to access work queue!");
    }
    return curr_work;
}

int safe_add_conversation(uint16_t conn_handle, e_media_type media_type, const char *reason)
{
    /* Create a conversation list item to keep track */

    struct conversation_list_item *new_item = malloc(sizeof(struct conversation_list_item));
    new_item->conn_handle = conn_handle;
    new_item->media_type = media_type;
    new_item->reason =malloc(strlen(reason));
    strcpy(new_item->reason,reason);
    
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
        return -SDP_ERR_SEMAPHORE;
    }
}

void *sdp_add_preamble(enum e_work_type work_type, uint16_t conversation_id, const void *data, int data_length)
{
    char *new_data = malloc(data_length + SDP_PREAMBLE_LENGTH);
    new_data[0] = SPD_PROTOCOL_VERSION;
    new_data[1] = (uint8_t)(&conversation_id)[0];
    new_data[2] = (uint8_t)(&conversation_id)[1];
    new_data[3] = (uint8_t)work_type;
    memcpy(&(new_data[SDP_PREAMBLE_LENGTH]), data, (size_t)data_length);
    return new_data;
}
/**
 * @brief Replies to the sender in the queue item
 *  
 * 
 * @param queue_item 
 * @param work_type 
 * @param data 
 * @param data_length 
 * @return int 
 */
int sdp_reply(struct work_queue_item queue_item, enum e_work_type work_type, const void *data, int data_length)
{
    int retval = SDP_OK;
    ESP_LOGI(log_prefix, "In sdp reply- media type: %u.", (uint8_t)queue_item.media_type);
    // Add preamble in a new data
    void *new_data = sdp_add_preamble(work_type, (uint16_t)(queue_item.conversation_id), data, data_length);
    switch (queue_item.media_type)
    {
    case BLE:
        ESP_LOGI(log_prefix, "In sdp reply.");
        retval = send_message(queue_item.conn_handle, queue_item.conversation_id, work_type,
                                  new_data, data_length + SDP_PREAMBLE_LENGTH);
        break;
    default:
        ESP_LOGE(log_prefix, "An unimplemented media type was used: %i", queue_item.media_type);
        retval = SDP_ERR_INVALID_PARAM;
        break;
    }
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
int start_conversation(enum e_media_type media_type, int conn_handle, enum e_work_type work_type, 
                       const char *reason, const void *data, int data_length)
{
    int retval = SDP_OK;
    // Create and add a new conversation item and add to queue
    int new_conversation_id = safe_add_conversation(conn_handle, media_type, reason);
    if (new_conversation_id >= 0) //
    {
        // Add preamble in a new data

        void *new_data = sdp_add_preamble(work_type, (uint16_t)(new_conversation_id), data, data_length);

        switch (media_type)
        {
            case BLE:
                if (conn_handle < 0)
                {
                    retval = -broadcast_message(new_conversation_id, work_type,
                                                    new_data, data_length + SDP_PREAMBLE_LENGTH);
                }
                else
                {
                    retval = -send_message(conn_handle, new_conversation_id, work_type,
                                            new_data, data_length + SDP_PREAMBLE_LENGTH);
                }
                break;

            default:
                ESP_LOGE(log_prefix, "An unimplemented media type was used: %i", media_type);
                retval = SDP_ERR_INVALID_PARAM;
                break;
        }
        free(new_data);
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
