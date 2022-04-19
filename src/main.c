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
#include "ble_task.h"
/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {
    ble_init("Controller\0", ble_client_my_task);
    ui_init("Controller\0");
}