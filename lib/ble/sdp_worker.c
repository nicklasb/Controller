
#include "sdp.h"
#include "esp_log.h"
#include "sdp_worker.h"

static void sdp_worker(void)
{

    struct work_queue_item *curr_work;
    int worker_task_count = 0;
    ESP_LOGI(log_prefix, "Worker task running.");
    for (;;)
    {
        if (pdTRUE == xSemaphoreTake(xQueue_Semaphore, portMAX_DELAY))
        {
            /* Pull the first item from the work queue */
            curr_work = STAILQ_FIRST(&work_q);
            /* Immidiate deletion from the head of the queue */
            if (curr_work != NULL)
            {
                STAILQ_REMOVE_HEAD(&work_q, items);
                xSemaphoreGive(xQueue_Semaphore);
                char taskname[50] = "\0";             

                sprintf(taskname, "%s_worker_%d_%d", log_prefix, curr_work->conversation_id, worker_task_count);
                if (on_work_cb != NULL)
                {
                    /* To avoid congestion on Core 0, we act on non-immidiate requests on Core 1 (APP) */
                    xTaskCreatePinnedToCore((TaskFunction_t)on_work_cb, taskname, 8192, curr_work, 8, NULL, 1);
                }
            }
            else
            {
                xSemaphoreGive(xQueue_Semaphore);
            }
        }
        else
        {
            ESP_LOGE(log_prefix, "Error: Couldn't get semaphore to access work queue!");
        }

        vTaskDelay(2000);
    }
    vTaskDelete(NULL);
}

int init_worker(const char *log_prefix)
{

    char taskname[50] = "\0";
    strcpy(taskname, log_prefix);
    strcat(taskname, " Worker task");
    /** Register the client task.
     * 
     * We are running it on Core 0, or PRO as it is called
     * traditionally (cores are basically the same now)
     * Feels more reasonable to focus on comms on 0 and 
     * applications on 1, traditionally called APP 
     */
     
    ESP_LOGI(log_prefix, "Register the worker task. Name: %s", taskname);
    xTaskCreatePinnedToCore((TaskFunction_t)sdp_worker, taskname, 8192, NULL, 8, NULL, 0);
    ESP_LOGI(log_prefix, "Worker task registered.");

    return 0;
}