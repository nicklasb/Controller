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

#include "ui_task.h"
#include "sdp.h"
#include "sdp_task.h"
#include "esp_log.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    sdp_init(&do_on_work, &do_on_priority, "Controller\0", true);

    on_filter_request_cb = &do_on_filter_request;
    on_filter_data_cb = &do_on_filter_data;



    ui_init("UI\0");
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

}