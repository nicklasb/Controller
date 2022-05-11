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
}