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

#include <esp_log.h>

#include "sdkconfig.h"
#include "string.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <esp_event.h>

#include "monitor/monitor.h"
#include "sleep/sleep.h"
#include "orchestration/orchestration.h"
#include "sdp_peer.h"
#include "sdp_worker.h"
#include "sdp_messaging.h"

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_init.h"
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
#include "espnow/espnow.h"
#endif

#ifdef CONFIG_SDP_LOAD_UMTS
#include "gsm/gsm.h"
#endif

#ifdef CONFIG_SDP_LOAD_LORA
#include "lora/lora_init.h"
#endif

char *sdp_log_prefix;

enum sdp_states sdp_state;

enum sdp_states get_sdp_state()
{
    return sdp_state;
}


void sdp_reset_rtc() {
#ifdef CONFIG_SDP_LOAD_UMTS
    gsm_reset_rtc();
#endif
}

void sdp_shutdown()
{
    ESP_LOGI(sdp_log_prefix, "Shutting down SDP.");
    sdp_state = SHUTTING_DOWN;

    sdp_shutdown_worker();

#ifdef CONFIG_SDP_LOAD_BLE
    ble_shutdown();
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
    espnow_shutdown();
#endif

#ifdef CONFIG_SDP_LOAD_UMTS
    gsm_shutdown();
#endif

    sdp_shutdown_monitor();
}

void delete_task_if_shutting_down()
{
    if (sdp_state == SHUTTING_DOWN)
    {
        vTaskDelete(NULL);
    }
}

int sdp_init(work_callback *work_cb, work_callback *priority_cb, before_sleep *before_sleep_cb, char *_log_prefix, bool is_conductor)
{
    sdp_log_prefix = _log_prefix;
    // Begin with initialising the monitor to capture initial memory state.
    sdp_init_monitor(sdp_log_prefix);
    
    // Then initialize sleep functionality  
    if (sleep_init(sdp_log_prefix))
    {
        ESP_LOGI(sdp_log_prefix, "Needs to consider that we returned from sleep.");
    }

    if (work_cb == NULL || priority_cb == NULL)
    {
        ESP_LOGE(_log_prefix, "Error: Both work_cb and priority_cb are mandatory parameters, \nsdp workers and queues will not work properly!");
        return SDP_ERR_INIT_FAIL;
    }

    sdp_peer_init(sdp_log_prefix);

    sdp_orchestration_init(sdp_log_prefix, before_sleep_cb);

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
#ifdef CONFIG_SDP_LOAD_UMTS
    ESP_LOGI(_log_prefix, "Initiating GSM modem..");
    gsm_init(_log_prefix);
#endif
#ifdef CONFIG_SDP_LOAD_LORA
    ESP_LOGI(_log_prefix, "Initiating LoRa..");
    lora_init(_log_prefix);
#endif

    ESP_LOGI(_log_prefix, "SDP initiated!");
    return 0;
}

#endif