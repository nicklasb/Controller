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
#include "sdp_work_queue.h"

#include <sys/queue.h>

#include <esp_log.h>
#include <string.h>

// The queue context
queue_context gsm_queue_context;

char *gsm_worker_log_prefix;

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
    if (queue_item != NULL)
    {
        free(queue_item->parts);
        free(queue_item->raw_data);
        free(queue_item);
    }
    cleanup_queue_task(&gsm_queue_context);
}

void gsm_set_queue_blocked(bool blocked) {
    set_queue_blocked(&gsm_queue_context,blocked);
}

void gsm_shutdown_worker() {
    ESP_LOGI(gsm_worker_log_prefix, "Telling gsm worker to shut down.");
    gsm_queue_context.shutdown = true;
}

esp_err_t gsm_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix)
{
    gsm_worker_log_prefix = _log_prefix;
    // Initialize the work queue
    STAILQ_INIT(&gsm_work_q);

    gsm_queue_context.first_queue_item_cb = &gsm_first_queueitem; 
    gsm_queue_context.remove_first_queueitem_cb = &gsm_remove_first_queue_item; 
    gsm_queue_context.insert_tail_cb = &gsm_insert_tail;
    gsm_queue_context.on_work_cb = work_cb; 
    gsm_queue_context.on_priority_cb = &priority_cb;
    gsm_queue_context.max_task_count = 1;
    // This queue cannot start processing items until GSM is initialized
    gsm_queue_context.blocked = true;

    init_work_queue(&gsm_queue_context, _log_prefix, "GSM Queue");

    return ESP_OK;

}
