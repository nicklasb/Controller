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
char *log_prefix;

esp_err_t safe_add_work_queue(queue_context *q_context, work_queue_item_t *new_item)
{
    if (pdTRUE == xSemaphoreTake(q_context->_x_queue_semaphore, portMAX_DELAY))
    {
        /* As the worker takes the queue from the head, and we want a LIFO, add the item to the tail */
        q_context->insert_tail_cb(new_item);
        xSemaphoreGive(q_context->_x_queue_semaphore);
    }
    else
    {
        ESP_LOGE(log_prefix, "Couldn't get semaphore to add to work queue!");
        return SDP_ERR_SEMAPHORE;
    }
    return ESP_OK;
}

work_queue_item_t *safe_get_head_work_item(queue_context *q_context)
{
    if (pdTRUE == xSemaphoreTake(q_context->_x_queue_semaphore, portMAX_DELAY))
    {
        /* Pull the first item from the work queue */

        work_queue_item_t *curr_work = q_context->first_queue_item_cb();
        /* Immidiate deletion from the head of the queue */
        if (curr_work != NULL)
        {
            q_context->remove_first_queueitem_cb();
        }
        xSemaphoreGive(q_context->_x_queue_semaphore);
        return curr_work;
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: Couldn't get semaphore to access work queue!");
        return NULL;
    }
}

static void sdp_worker(queue_context *q_context)
{

    int worker_task_count = 0;
    work_queue_item_t *curr_work;
    ESP_LOGI(log_prefix, "Worker task running.");
    for (;;)
    {

        curr_work = safe_get_head_work_item(q_context);
        if (curr_work != NULL)
        {
            char taskname[50] = "\0";
            sprintf(taskname, "%s_worker_%d_%d", log_prefix, curr_work->conversation_id, worker_task_count);
            if (q_context->on_work_cb != NULL)
            {
                ESP_LOGD(log_prefix, "Running callback on_work.%i, %i", curr_work->partcount, (int)q_context->on_work_cb);
                /* To avoid congestion on Core 0, we act on non-immidiate requests on Core 1 (APP) */
                TaskHandle_t th;
                int rc = xTaskCreatePinnedToCore((TaskFunction_t)q_context->on_work_cb, taskname, 8192, curr_work, 8, &th, 1);
                if (rc != pdPASS)
                {
                    ESP_LOGE(log_prefix, "Failed creating work task, returned: %i (see projdefs.h)", rc);
                }
                ESP_LOGD(log_prefix, "Created task %s, taskhandle %i", taskname, (int)th);
            }
        }
        // TODO: Use event loop to wait instead?
        vTaskDelay(5);
    }
    vTaskDelete(NULL);
}

esp_err_t init_work_queue(queue_context *q_context, char *_log_prefix, const char *queue_name)
{

    /* Create a queue semaphore to ensure thread safety */
    q_context->_x_queue_semaphore = xSemaphoreCreateMutex();

    /* Create the xTask name. */
    char x_task_name[50] = "\0";
    strcpy(x_task_name, queue_name);
    strcat(x_task_name, " worker task");

    /** Register the worker task.
     *
     * We are running it on Core 0, or PRO as it is called
     * traditionally (cores are basically the same now)
     * Feels more reasonable to focus on comms on 0 and
     * applications on 1, traditionally called APP
     */

    ESP_LOGI(log_prefix, "Register the worker task. Name: %s", x_task_name);
    int rc = xTaskCreatePinnedToCore((TaskFunction_t)sdp_worker, x_task_name, 8192, (void *)(q_context), 8, NULL, 0);
    if (rc != pdPASS)
    {
        ESP_LOGE(log_prefix, "Failed creating worker task, returned: %i (see projdefs.h)", rc);
        return SDP_ERR_INIT_FAIL;
    }
    ESP_LOGI(log_prefix, "Worker task registered.");

    return ESP_OK;
}

void cleanup_queue_task(queue_context *q_context, work_queue_item_t *queue_item)
{
    free(queue_item->parts);
    free(queue_item->raw_data);
    free(queue_item);

    vTaskDelete(NULL);
}