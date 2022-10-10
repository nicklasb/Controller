/**
 * @file sdp_worker.c
 * @author Nicklas Borjesson 
 * @brief The worker maintain and monitor the work queue and uses callbacks to notify the user code
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "sdp_worker.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_log.h>
#include <string.h>

/* The log prefix for all logging */
char *log_prefix;

/* Semaphore for thread safety Must be used when changing the work queue  */
SemaphoreHandle_t x_worker_queue_semaphore;

int safe_add_work_queue(work_queue_item_t *new_item)
{
    if (pdTRUE == xSemaphoreTake(x_worker_queue_semaphore, portMAX_DELAY))
    {
        /* As the worker takes the queue from the head, and we want a LIFO, add the item to the tail */
        STAILQ_INSERT_TAIL(&work_q, new_item, items);
        xSemaphoreGive(x_worker_queue_semaphore);
    }
    else
    {
        ESP_LOGE(log_prefix, "Couldn't get semaphore to add to work queue!");
        return 1;
    }
    return 0;
}

work_queue_item_t *safe_get_head_work_item(void)
{
    
    if (pdTRUE == xSemaphoreTake(x_worker_queue_semaphore, portMAX_DELAY))
    {
        /* Pull the first item from the work queue */
        work_queue_item_t *curr_work = STAILQ_FIRST(&work_q);
        /* Immidiate deletion from the head of the queue */
        if (curr_work != NULL)
        {
            STAILQ_REMOVE_HEAD(&work_q, items);
        }
        xSemaphoreGive(x_worker_queue_semaphore);
        return curr_work;
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: Couldn't get semaphore to access work queue!");
        return NULL;
    }
    
}


static void sdp_worker(void)
{

    int worker_task_count = 0;
    work_queue_item_t *curr_work;
    ESP_LOGI(log_prefix, "Worker task running.");
    for (;;)
    {

        curr_work = safe_get_head_work_item();
        if (curr_work != NULL)
        {
            char taskname[50] = "\0";
            sprintf(taskname, "%s_worker_%d_%d", log_prefix, curr_work->conversation_id, worker_task_count);
            if (on_work_cb != NULL)
            {   
                ESP_LOGI(log_prefix, "Running callback on_work.%i, %i", curr_work->partcount, (int)on_work_cb);
                /* To avoid congestion on Core 0, we act on non-immidiate requests on Core 1 (APP) */
                TaskHandle_t th;
                int rc = xTaskCreatePinnedToCore((TaskFunction_t)on_work_cb, taskname, 8192, curr_work, 8, &th, 1);
                if (rc != pdPASS) {
                    ESP_LOGE(log_prefix, "Failed creating work task, returned: %i (see projdefs.h)", rc);
                } 
                ESP_LOGI(log_prefix, "Created task %s, taskhandle %i",taskname, (int)th);
                   
            }
        }
        vTaskDelay(5);
    }
    vTaskDelete(NULL);
}

int init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix)
{
    // Initialize the work queue
    STAILQ_INIT(&work_q);

    on_work_cb = work_cb;
    on_priority_cb = priority_cb;

    /* Create a queue semaphore to ensure thread safety */
    x_worker_queue_semaphore = xSemaphoreCreateMutex();

    /* Create the xTask name. */
    char x_task_name[50] = "\0";
    strcpy(x_task_name, log_prefix);
    strcat(x_task_name, " Worker task");

    /** Register the client task.
     *
     * We are running it on Core 0, or PRO as it is called
     * traditionally (cores are basically the same now)
     * Feels more reasonable to focus on comms on 0 and
     * applications on 1, traditionally called APP
     */

    ESP_LOGI(log_prefix, "Register the worker task. Name: %s", x_task_name);
    int rc = xTaskCreatePinnedToCore((TaskFunction_t)sdp_worker, x_task_name, 8192, NULL, 8, NULL, 0);
    if (rc != pdPASS) {
        ESP_LOGE(log_prefix, "Failed creating worker task, returned: %i (see projdefs.h)", rc);
        return SDP_ERR_INIT_FAIL;
    } 
    ESP_LOGI(log_prefix, "Worker task registered.");

    return 0;
}



void cleanup_queue_task(work_queue_item_t *queue_item)
{
    free(queue_item->parts);
    free(queue_item->raw_data);

    free(queue_item);
    vTaskDelete(NULL);
}