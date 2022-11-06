/**
 * @file sdp_worker.c
 * @author Nicklas Borjesson 
 * @brief The worker maintain and monitor the work queue and uses callbacks to notify the user code
 * This code uses sdp_work_queue to automatically handle queues, semaphores and tasks
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "sdp_worker.h"

#include <os/queue.h>

#include <esp_log.h>
#include <string.h>


/* Expands to a declaration for the work queue */
STAILQ_HEAD(gsm_work_q, work_queue_item) gsm_work_q;

struct work_queue_item_t *sdp_first_queueitem() {
    return STAILQ_FIRST(&gsm_work_q); 
}

void sdp_remove_first_queue_item(){
    STAILQ_REMOVE_HEAD(&gsm_work_q, items); 
}
void sdp_insert_tail(work_queue_item_t *new_item) {
    STAILQ_INSERT_TAIL(&gsm_work_q, new_item, items);
}

esp_err_t sdp_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix)
{
    // Initialize the work queue
    STAILQ_INIT(&gsm_work_q);

    q_context.first_queue_item_cb = &sdp_first_queueitem; 
    q_context.remove_first_queueitem_cb = &sdp_remove_first_queue_item; 
    q_context.insert_tail_cb = &sdp_insert_tail;
    q_context.on_work_cb = &work_cb, 
    q_context.on_priority_cb = &priority_cb;

    init_work_queue(&q_context, _log_prefix, "SDP Queue");

    return ESP_OK;

}
