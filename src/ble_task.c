
#include "ble_task.h"
#include "esp_log.h"
#include "host/ble_gatt.h"


/* 
This is the running task of the client (the central), it currently connects to the server (actually peripheral) and sends data.
TODO: Instead this will be rewritten competely:
It will instead contact all peripherals and ask for data, in the following situations:
1. After periodically being woken from a timer signal
2. Because a GPIO went high (which is likely to be because of an alarm) 
3. Because of a instruction via SMS (or other way in). 
*/

void ble_client_my_task(void *pvParameters)
{

     /* Use a special semaphore for BLE. 
    TODO: Consider if there instead should be one for Core 0 instead 
     (that might not be possible if not a param here; should a controller init exist?)
    */
    xBLESemaphore = xSemaphoreCreateMutex();
   
    char myarray[13] = "datatosend\0";
    int ret;
    ESP_LOGI(task_tag, "My Task: BLE client UART task started\n");
    for (;;)
    {
        vTaskDelay(2000);
        if (pdTRUE == xSemaphoreTake(xBLESemaphore, portMAX_DELAY))
        {
            ret = ble_gattc_write_flat(connection_handle, attribute_handle, &myarray, 13, NULL, NULL);
            if (ret == 0)
            {
                ESP_LOGI(task_tag, "My Task: Write in uart task success!");
            }
            else
            {
                ESP_LOGI(task_tag, "My Task: Error in writing characteristic");
            }
            xSemaphoreGive(xBLESemaphore);
        }
        else
        {
            ESP_LOGI(task_tag, "My Task: Couldn't get semaphore");
        }
    }
    vTaskDelete(NULL);
}