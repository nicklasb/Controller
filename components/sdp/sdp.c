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

#include "sdkconfig.h"

#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_init.h"
#endif


#include "sdp_worker.h"
#include "sdp_monitor.h"
#include "sdp_messaging.h"

#include <esp_log.h>

int sdp_init(work_callback work_cb, work_callback priority_cb, char *_log_prefix, bool is_controller)
{

    // Begin with initialising the monitor to capture initial memory state.
    init_monitor(_log_prefix);

    if (work_cb == NULL || priority_cb == NULL)
    {
        ESP_LOGE(_log_prefix, "Error: Both work_cb and priority_cb are mandatory parameters, cannot be NULL!");
        return SDP_ERR_INIT_FAIL;
    }
 
    init_worker(work_cb, priority_cb, _log_prefix);
    init_messaging(_log_prefix);

    ESP_LOGI(_log_prefix, "SDP initiated!");

    /* Init media types */
    #ifdef CONFIG_SDP_LOAD_BLE
        ble_init(_log_prefix, is_controller);
    #endif
    return 0;
}

#endif