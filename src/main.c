/* Boat monitoring central
 *
 * Based on the lvgl port ESP32 Example
 * 
 * Prerequisited:
 * Multicolor display with touch (tested with resistive)
 *
 * LICENSE:
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */


/**********************
 *  INCLUDES
 **********************/


#include "sdp_task.h"
#include "ui_task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"



/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    ESP_LOGE("___INIT___",  "8BIT: %i, EXEC: %i", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_EXEC));
    init_sdp_task();

    ui_init("UI\0");


/*
    int i = 0;
    ESP_LOGI(log_prefix, "Waiting to broadcast");
    vTaskDelay(1000);   
    while (1) {

        char data[17] = "status\0testdata";
        ESP_LOGI(log_prefix, "Test broadcast %i beginning.", i);
        start_conversation(BLE, -1, REQUEST,  &data, sizeof(data));
        ESP_LOGI(log_prefix, "Test broadcast %i done.", i);
        vTaskDelay(50);   
        i++;
    }
*/
}