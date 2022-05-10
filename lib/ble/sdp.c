#include "sdp.h"

#include "ble_global.h"
#include "esp_log.h"

uint16_t conversation_id = 0;

int safe_add_work_queue(struct work_queue_item *new_item, const char *log_tag) {
    if (pdTRUE == xSemaphoreTake(xQueue_Semaphore, portMAX_DELAY))
    { 
        STAILQ_INSERT_HEAD(&work_q, new_item, items);
        xSemaphoreGive(xQueue_Semaphore);
    }
    else
    {
        ESP_LOGI(log_tag, "Couldn't get semaphore to add to work queue!");
        return 1;
    }   
    return 0;
}

int safe_add_conversation_queue(struct conversation_list_item *new_item, const char *log_tag) {

    if (pdTRUE == xSemaphoreTake(xQueue_Semaphore, portMAX_DELAY))
    { 
        SLIST_INSERT_HEAD(&conversation_l, new_item, items);
        xSemaphoreGive(xQueue_Semaphore);
    }
    else
    {
        ESP_LOGI(log_tag, "Couldn't get semaphore to add to conversation queue!");
        return 1;
    }   
    return 0;
}

int respond(struct work_queue_item queue_item, enum work_type work_type, const void *data, int data_length, const char *log_tag) {

    switch (queue_item.media_type)
    {
    case BLE:
        ble_send_message(queue_item.conn_handle, queue_item.conversation_id, 
            work_type, data, data_length, log_tag);
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
 * @return int 
 */
int start_conversation(enum media_type media_type, int conn_handle, 
    enum work_type work_type, const void *data, int data_length, const char *log_tag) {

    /* Create a conversation list item to keep track */
    struct conversation_list_item *new_item = malloc(sizeof(struct conversation_list_item));
    new_item->conversation_id = conversation_id++;
    new_item->conn_handle = conn_handle;
    new_item->media_type = media_type;


    int ret = safe_add_conversation_queue(new_item, log_tag);
    if (ret != 0) {
        switch (media_type)
        {
        case BLE:
            
            ble_send_message(conn_handle, conversation_id, 
                work_type, data, data_length, log_tag);
            break;
        
        default:
            break;
        }
        

    }

    
    return 0;
}

