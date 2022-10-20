/**
 * @file sdp.c
 * @author Nicklas Borjesson
 * @brief This is the sensor data protocol server
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _SDP_H_
#define _SDP_H_

#include "sdp.h"
#include "sdp_def.h"

#include "sdkconfig.h"
#include "string.h"

#include <nvs.h>
#include <nvs_flash.h>

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_init.h"
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
#include "espnow/espnow_init.h"
#endif

#include "sdp_worker.h"
#include "sdp_monitor.h"
#include "sdp_messaging.h"

#include <esp_log.h>

int sdp_init(work_callback work_cb, work_callback priority_cb, char *_log_prefix, bool is_controller)
{
    sdp_host.protocol_version = SDP_PROTOCOL_VERSION;
    sdp_host.min_protocol_version = SDP_PROTOCOL_VERSION_MIN;
    strcpy(sdp_host.sdp_host_name, CONFIG_SDP_PEER_NAME);

    // Begin with initialising the monitor to capture initial memory state.
    init_monitor(_log_prefix);

    if (work_cb == NULL || priority_cb == NULL)
    {
        ESP_LOGE(_log_prefix, "Error: Both work_cb and priority_cb are mandatory parameters, cannot be NULL!");
        return SDP_ERR_INIT_FAIL;
    }

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
 
    init_worker(work_cb, priority_cb, _log_prefix);
    init_messaging(_log_prefix);

   

    /* Init media types */
    #ifdef CONFIG_SDP_LOAD_BLE
        ESP_LOGI(_log_prefix, "Initiating BLE..");
        ble_init(_log_prefix, is_controller);
    #endif
    #ifdef CONFIG_SDP_LOAD_ESP_NOW
        ESP_LOGI(_log_prefix, "Initiating ESP-NOW..");
        espnow_init(_log_prefix, is_controller);
    #endif
     
    ESP_LOGI(_log_prefix, "SDP initiated!");
    return 0;
}

#endif