/**
 * @file i2c_worker.c
 * @author Nicklas Borjesson 
 * @brief The i2c worker maintain and monitor the i2c work queue and uses callbacks to notify the user code
 * This code uses sdp_work_queue to automatically handle queues, semaphores and tasks
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "i2c_worker.h"

#include <sys/queue.h>

#include <esp_log.h>
#include <string.h>

// The queue context
struct queue_context i2c_queue_context;

char *i2c_worker_log_prefix;

/* Expands to a declaration for the work queue */
STAILQ_HEAD(i2c_work_q, i2c_queue_item) 
i2c_work_q;

struct i2c_queue_item_t *i2c_first_queueitem() 
{
    return STAILQ_FIRST(&i2c_work_q); 
}

void i2c_remove_first_queue_item(){
    STAILQ_REMOVE_HEAD(&i2c_work_q, items);
}
void i2c_insert_tail(i2c_queue_item_t *new_item) { 
    STAILQ_INSERT_TAIL(&i2c_work_q, new_item, items);
}

esp_err_t i2c_safe_add_work_queue(sdp_peer *peer, char *data, int data_length) {  
    i2c_queue_item_t *new_item = malloc(sizeof(i2c_queue_item_t)); 
    new_item->peer = peer;
    new_item->data = malloc(data_length);
    memcpy(new_item->data,data, data_length);
    new_item->data_length = data_length;

    return safe_add_work_queue(&i2c_queue_context, new_item);
}
void i2c_cleanup_queue_task(i2c_queue_item_t *queue_item) {
    if (queue_item != NULL)
    {    
        free(queue_item->peer);
        free(queue_item->data);
        free(queue_item);
    }
    cleanup_queue_task(&i2c_queue_context);
}

void i2c_set_queue_blocked(bool blocked) {
    set_queue_blocked(&i2c_queue_context,blocked);
}

void i2c_shutdown_worker() {
    ESP_LOGI(i2c_worker_log_prefix, "Telling i2c worker to shut down.");
    i2c_queue_context.shutdown = true;
}

esp_err_t i2c_init_worker(work_callback work_cb, work_callback priority_cb, poll_callback poll_cb, char *_log_prefix)
{
    i2c_worker_log_prefix = _log_prefix;
    // Initialize the work queue
    STAILQ_INIT(&i2c_work_q);

    i2c_queue_context.first_queue_item_cb = &i2c_first_queueitem; 
    i2c_queue_context.remove_first_queueitem_cb = &i2c_remove_first_queue_item; 
    i2c_queue_context.insert_tail_cb = &i2c_insert_tail;
    i2c_queue_context.on_work_cb = work_cb; 
    i2c_queue_context.on_priority_cb = priority_cb;
    i2c_queue_context.on_poll_cb = poll_cb;
    i2c_queue_context.max_task_count = 1;
    // This queue cannot start processing items until i2c is initialized
    i2c_queue_context.blocked = true;

    return init_work_queue(&i2c_queue_context, _log_prefix, "i2c Queue");      
}
