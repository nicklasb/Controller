/**
 * @file lora_worker.c
 * @author Nicklas Borjesson 
 * @brief The Lora worker maintain and monitor the Lora work queue and uses callbacks to notify the user code
 * This code uses sdp_work_queue to automatically handle queues, semaphores and tasks
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "lora_worker.h"
#include "sdp_work_queue.h"

#include <sys/queue.h>

#include <esp_log.h>
#include <string.h>

// The queue context
queue_context lora_queue_context;

char *lora_worker_log_prefix;

/* Expands to a declaration for the work queue */
STAILQ_HEAD(lora_work_q, work_queue_item) lora_work_q;

struct work_queue_item_t *lora_first_queueitem() {
    return STAILQ_FIRST(&lora_work_q); 
}

void lora_remove_first_queue_item(){
    STAILQ_REMOVE_HEAD(&lora_work_q, items); 
}
void lora_insert_tail(work_queue_item_t *new_item) {
    STAILQ_INSERT_TAIL(&lora_work_q, new_item, items);
}

esp_err_t lora_safe_add_work_queue(work_queue_item_t *new_item) {   
    return safe_add_work_queue(&lora_queue_context, new_item);
}
void lora_cleanup_queue_task(work_queue_item_t *queue_item) {
    cleanup_queue_task(&lora_queue_context, queue_item);
}

void lora_set_queue_blocked(bool blocked) {
    set_queue_blocked(&lora_queue_context,blocked);
}

void lora_shutdown_worker() {
    ESP_LOGI(lora_worker_log_prefix, "Telling lora worker to shut down.");
    lora_queue_context.shutdown = true;
}

esp_err_t lora_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix)
{
    lora_worker_log_prefix = _log_prefix;
    // Initialize the work queue
    STAILQ_INIT(&lora_work_q);

    lora_queue_context.first_queue_item_cb = &lora_first_queueitem; 
    lora_queue_context.remove_first_queueitem_cb = &lora_remove_first_queue_item; 
    lora_queue_context.insert_tail_cb = &lora_insert_tail;
    lora_queue_context.on_work_cb = work_cb; 
    lora_queue_context.on_priority_cb = &priority_cb;
    lora_queue_context.max_task_count = 1;
    // This queue cannot start processing items until lora is initialized
    lora_queue_context.blocked = true;

    init_work_queue(&lora_queue_context, _log_prefix, "LoRa Queue");

    return ESP_OK;

}
