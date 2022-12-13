/**
 * @file sdp_worker.c
 * @author Nicklas Borjesson
 * @brief The work_queue maintains and monitors a work queue using a worker task and uses callbacks to notify the user code
 * This is a kind of generalization, consumers themselves initialize the queue and provide some callbacks.
 * See sdp_worker.c for an example.
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "sdp_work_queue.h"

#include <esp_log.h>
#include <string.h>

/* The log prefix for all logging */
char *spd_work_queue_log_prefix;

esp_err_t safe_add_work_queue(queue_context *q_context, work_queue_item_t *new_item)
{
    if (q_context->shutdown) {
        ESP_LOGE(spd_work_queue_log_prefix, "The queue is shut down.");
        return SDP_ERR_SEMAPHORE;
    } else
    if (pdTRUE == xSemaphoreTake(q_context->__x_queue_semaphore, portMAX_DELAY))
    {
        /* As the worker takes the queue from the head, and we want a LIFO, add the item to the tail */
        q_context->insert_tail_cb(new_item);
        xSemaphoreGive(q_context->__x_queue_semaphore);
    }
    else
    {
        ESP_LOGE(spd_work_queue_log_prefix, "Couldn't get semaphore to add to work queue!");
        return SDP_ERR_SEMAPHORE;
    }
    return ESP_OK;
}

work_queue_item_t *safe_get_head_work_item(queue_context *q_context)
{
    if (pdTRUE == xSemaphoreTake(q_context->__x_queue_semaphore, portMAX_DELAY))
    {
        /* Pull the first item from the work queue */
        work_queue_item_t *curr_work = q_context->first_queue_item_cb();

        /* Immidiate deletion from the head of the queue */
        if (curr_work != NULL)
        {
            q_context->remove_first_queueitem_cb();
        }
        xSemaphoreGive(q_context->__x_queue_semaphore);
        return curr_work;
    }
    else
    {
        ESP_LOGE(spd_work_queue_log_prefix, "Error: Couldn't get semaphore to access work queue!");
        return NULL;
    }
}

void alter_task_count(queue_context *q_context, int change)
{
    if (pdTRUE == xSemaphoreTake(q_context->__x_task_state_semaphore, portMAX_DELAY))
    {
        q_context->task_count = q_context->task_count + change;
        xSemaphoreGive(q_context->__x_task_state_semaphore);
    }
    else
    {
        ESP_LOGE(spd_work_queue_log_prefix, "Error: Couldn't get semaphore to alter task count (change: %i, taskhandle: %p)!", change, xTaskGetCurrentTaskHandle());
    }
}

void set_queue_blocked(queue_context *q_context, bool blocked)
{
    if (pdTRUE == xSemaphoreTake(q_context->__x_task_state_semaphore, portMAX_DELAY))
    {
        q_context->blocked = blocked;
        xSemaphoreGive(q_context->__x_task_state_semaphore);
    }
    else
    {
        ESP_LOGE(spd_work_queue_log_prefix, "Error: Couldn't get semaphore to block/unblock it (blocked: %i, taskhandle: %p)!", (int)blocked, xTaskGetCurrentTaskHandle());
    }
}

static void sdp_worker(queue_context *q_context)
{
    ESP_LOGI(spd_work_queue_log_prefix, "Worker task %s now running, context: ", q_context->worker_task_name);
    ESP_LOGI(spd_work_queue_log_prefix, "on_work_cb: %p", q_context->on_work_cb);
    ESP_LOGI(spd_work_queue_log_prefix, "on_priority_cb: %p", q_context->on_priority_cb);   
    ESP_LOGI(spd_work_queue_log_prefix, "max_task_count: %i", q_context->max_task_count);
    ESP_LOGI(spd_work_queue_log_prefix, "first_queue_item_cb: %p", q_context->first_queue_item_cb);
    ESP_LOGI(spd_work_queue_log_prefix, "insert_tail_cb: %p", q_context->insert_tail_cb);
    ESP_LOGI(spd_work_queue_log_prefix, "---------------------------");

    work_queue_item_t *curr_work;

    for (;;)
    {
        // Are we shutting down?
        if (q_context->shutdown) {
            break;
        }
        if (
            // First check so that the queue isn't blocked
            (!q_context->blocked) &&
            // And that we don't have a maximum number of tasks
            (q_context->max_task_count == 0 ||
            // Or if we do, that we don't exceed it
            (q_context->max_task_count > 0 && (q_context->task_count < q_context->max_task_count))
            )
        )
        {
            
            curr_work = safe_get_head_work_item(q_context);
            if (curr_work != NULL)
            {
                char taskname[50] = "\0";
                sprintf(taskname, "%s_worker_%d_%d", spd_work_queue_log_prefix, curr_work->conversation_id, q_context->task_count);
                if (q_context->on_work_cb != NULL)
                {
                    ESP_LOGI(spd_work_queue_log_prefix, "Running callback on_work. Partcount : %i, Worker address %p", curr_work->partcount, q_context->on_work_cb);
                    /* To avoid congestion on Core 0, we act on non-immidiate requests on Core 1 (APP) */
                    TaskHandle_t th;
                    alter_task_count(q_context, 1);
                    int rc = xTaskCreatePinnedToCore((TaskFunction_t)q_context->on_work_cb, taskname, 8192, curr_work, 8, &th, 1);
                    if (rc != pdPASS)
                    {
                        ESP_LOGE(spd_work_queue_log_prefix, "Failed creating work task, returned: %i (see projdefs.h)", rc);
                        alter_task_count(q_context, -1);
                    }
                    ESP_LOGI(spd_work_queue_log_prefix, "Created task: %s, taskhandle: %i, taskcount: %i", taskname, (int)th, q_context->task_count);
                }
            }
        }
        // TODO: Use event loop to wait instead?
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(spd_work_queue_log_prefix, "Worker task %s shut down, deleting task.", q_context->worker_task_name);
    free(q_context->__x_queue_semaphore);
        
    q_context->__x_queue_semaphore = NULL;
    q_context->__x_task_state_semaphore = NULL;
    
    vTaskDelete(NULL);
}

esp_err_t init_work_queue(queue_context *q_context, char *_log_prefix, const char *queue_name)
{
    spd_work_queue_log_prefix = _log_prefix;


    ESP_LOGI(spd_work_queue_log_prefix, "Initiating the %s work queue.", queue_name);

    /* Create a semaphores to ensure thread safety (queue and tasks) */
    q_context->__x_queue_semaphore = xSemaphoreCreateMutex();
    q_context->__x_task_state_semaphore = xSemaphoreCreateMutex();

    // Reset task count (unsafely as this must be the only initiator)
    q_context->task_count = 0;

    /* Create the xTask name. */
    strcpy(q_context->worker_task_name, "\0");
    strcpy(q_context->worker_task_name, queue_name);
    strcat(q_context->worker_task_name, " worker task");

    /** Register the worker task.
     *
     * We are running it on Core 0, or PRO as it is called
     * traditionally (cores are basically the same now)
     * Feels more reasonable to focus on comms on 0 and
     * applications on 1, traditionally called APP
     */

    ESP_LOGI(spd_work_queue_log_prefix, "Register the worker task. Name: %s", q_context->worker_task_name);
    int rc = xTaskCreatePinnedToCore((TaskFunction_t)sdp_worker, q_context->worker_task_name, 8192, (void *)(q_context), 8, q_context->worker_task_handle, 0);
    if (rc != pdPASS)
    {
        ESP_LOGE(spd_work_queue_log_prefix, "Failed creating worker task, returned: %i (see projdefs.h)", rc);
        return SDP_ERR_INIT_FAIL;
    }
    ESP_LOGI(spd_work_queue_log_prefix, "Worker task registered.");

    return ESP_OK;
}

/**
 * @brief Cleanup after a queue item is handled, typically not called directly
 * @param q_context The actur
 * @param queue_item A queue item to free.
 */
void cleanup_queue_task(queue_context *q_context, work_queue_item_t *queue_item)
{
    ESP_LOGI(spd_work_queue_log_prefix, "Cleaning up tasks. queue_item %p ", queue_item);
    
    if (queue_item != NULL) {
        free(queue_item->parts);
        free(queue_item->raw_data);
        free(queue_item);
    }
    alter_task_count(q_context, -1);
    vTaskDelete(NULL);
}