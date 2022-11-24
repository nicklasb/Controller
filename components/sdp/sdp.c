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


#include <esp_log.h>

#include "sdkconfig.h"
#include "string.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <esp_event.h>

#include "monitor/monitor.h"
#include "sleep/sleep.h"
#include "orchestration/orchestration.h"

#include "sdp_worker.h"
#include "sdp_messaging.h"

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_init.h"
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
#include "espnow/espnow_init.h"
#endif

#include "gsm/gsm.h"

char *log_prefix;

void sdp_shutdown() {
    ESP_LOGI(log_prefix, "Shutting down SDP.");
    sdp_shutdown_worker();

    #ifdef CONFIG_SDP_LOAD_BLE
    ble_shutdown(); 
    #endif

    #ifdef CONFIG_SDP_LOAD_ESP_NOW
    espnow_shutdown();
    #endif

    sdp_shutdown_monitor();
}

int sdp_init(work_callback *work_cb, work_callback *priority_cb, char *_log_prefix, bool is_conductor)
{
    log_prefix = _log_prefix;
     // Begin with initializing sleep functionality, it will also de
    if (sleep_init(log_prefix)) {
        ESP_LOGI(log_prefix, "Needs to consider that we returned from sleep.");
    }       
    orchestration_init(log_prefix);

 
    sdp_host.protocol_version = SDP_PROTOCOL_VERSION;
    sdp_host.min_protocol_version = SDP_PROTOCOL_VERSION_MIN;
    strcpy(sdp_host.sdp_host_name, CONFIG_SDP_PEER_NAME);

    // Begin with initialising the monitor to capture initial memory state.
    sdp_init_monitor(log_prefix);



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
 
    sdp_init_worker(work_cb, priority_cb, _log_prefix);
    sdp_init_messaging(_log_prefix);

    // Create the default event loop (almost all technologies use it    )
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    /* Init media types */
    #ifdef CONFIG_SDP_LOAD_BLE
        ESP_LOGI(_log_prefix, "Initiating BLE..");
        ble_init(_log_prefix, is_conductor);
    #endif
    #ifdef CONFIG_SDP_LOAD_ESP_NOW
        ESP_LOGI(_log_prefix, "Initiating ESP-NOW..");
        espnow_init(_log_prefix);
    #endif
    #ifdef CONFIG_SDP_LOAD_GSM
        ESP_LOGI(_log_prefix, "Initiating GSM modem..");
        gsm_init(_log_prefix);
    #endif
     
    ESP_LOGI(_log_prefix, "SDP initiated!");
    return 0;
}

#endif