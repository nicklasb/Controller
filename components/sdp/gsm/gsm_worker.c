/**
 * @file gsm_worker.c
 * @author Nicklas Borjesson 
 * @brief The GSM worker maintain and monitor the GSM work queue and uses callbacks to notify the user code
 * This code uses sdp_work_queue to automatically handle queues, semaphores and tasks
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "gsm_worker.h"

#include <os/queue.h>

#include <esp_log.h>
#include <string.h>


/* Expands to a declaration for the work queue */
STAILQ_HEAD(gsm_work_q, work_queue_item) gsm_work_q;

struct work_queue_item_t *gsm_first_queueitem() {
    return STAILQ_FIRST(&gsm_work_q); 
}

void gsm_remove_first_queue_item(){
    STAILQ_REMOVE_HEAD(&gsm_work_q, items); 
}
void gsm_insert_tail(work_queue_item_t *new_item) {
    STAILQ_INSERT_TAIL(&gsm_work_q, new_item, items);
}

esp_err_t gsm_safe_add_work_queue(work_queue_item_t *new_item) {
    return safe_add_work_queue(&gsm_queue_context, new_item);
}
void gsm_cleanup_queue_task(work_queue_item_t *queue_item) {
    cleanup_queue_task(&gsm_queue_context, queue_item);
}

esp_err_t gsm_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix)
{
    // Initialize the work queue
    STAILQ_INIT(&gsm_work_q);

    gsm_queue_context.first_queue_item_cb = &gsm_first_queueitem; 
    gsm_queue_context.remove_first_queueitem_cb = &gsm_remove_first_queue_item; 
    gsm_queue_context.insert_tail_cb = &gsm_insert_tail;
    gsm_queue_context.on_work_cb = &work_cb; 
    gsm_queue_context.on_priority_cb = &priority_cb;

    init_work_queue(&gsm_queue_context, _log_prefix, "GSM Queue");

    return ESP_OK;

}
