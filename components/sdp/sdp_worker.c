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

#include <sys/queue.h>

#include <esp_log.h>
#include <string.h>
#include "sdp_work_queue.h"

#define SDP_QUEUE_PROBLEM_COUNT 3


// The queue context
queue_context sdp_queue_context;

char *sdp_worker_log_prefix = NULL;

/* Expands to a declaration for the work queue */
STAILQ_HEAD(sdp_work_q, work_queue_item)
sdp_work_q;

struct work_queue_item_t *sdp_first_queueitem()
{
    return STAILQ_FIRST(&sdp_work_q);
}

void sdp_remove_first_queue_item()
{
    STAILQ_REMOVE_HEAD(&sdp_work_q, items);
}
void sdp_insert_tail(work_queue_item_t *new_item)
{
    STAILQ_INSERT_TAIL(&sdp_work_q, new_item, items);
}

esp_err_t sdp_safe_add_work_queue(work_queue_item_t *new_item)
{
    return safe_add_work_queue(&sdp_queue_context, new_item);
}

void sdp_cleanup_queue_task(work_queue_item_t *queue_item)
{
    if (queue_item != NULL)
    {
        free(queue_item->parts);
        free(queue_item->raw_data);
        free(queue_item);
    }
    cleanup_queue_task(&sdp_queue_context);
}

void sdp_set_queue_blocked(bool blocked)
{
    set_queue_blocked(&sdp_queue_context, blocked);
}


void sdp_worker_on_monitor() {
    // TODO: Looking at the log prefix isn't the best method to see if the queue is running.
    if (sdp_worker_log_prefix) {

        struct work_queue_item *curr_item;
        struct work_queue_item *tmp_curr_item;
        uint16_t itemcount = 0;
        ESP_LOGI(sdp_worker_log_prefix, "in sdp_worker_on_monitor");
        STAILQ_FOREACH_SAFE(curr_item, &sdp_work_q, items, tmp_curr_item) {
            itemcount++;
        }
        
        if (itemcount > SDP_QUEUE_PROBLEM_COUNT) {
            ESP_LOGW(sdp_worker_log_prefix, "SDP Worker queue has %i items which indicates there is a problem!", itemcount);
        } else {
            ESP_LOGI(sdp_worker_log_prefix, "SDP Worker queue has %i items.", itemcount);
        }
    }

}


void sdp_shutdown_worker()
{
    ESP_LOGI(sdp_worker_log_prefix, "Telling main sdp worker to shut down.");
    sdp_queue_context.shutdown = true;
}

esp_err_t sdp_init_worker(work_callback *work_cb, char *_log_prefix)
{
    sdp_worker_log_prefix = _log_prefix;
    // Initialize the work queue
    STAILQ_INIT(&sdp_work_q);

    sdp_queue_context.first_queue_item_cb = &sdp_first_queueitem;
    sdp_queue_context.remove_first_queueitem_cb = &sdp_remove_first_queue_item;
    sdp_queue_context.insert_tail_cb = &sdp_insert_tail;
    sdp_queue_context.on_work_cb = work_cb,
    sdp_queue_context.max_task_count = 0;
    sdp_queue_context.multitasking = true;

    init_work_queue(&sdp_queue_context, _log_prefix, "SDP Queue");

    return ESP_OK;
}
