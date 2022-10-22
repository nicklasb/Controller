/**
 * @file sleep.c
 * @author your name (you@domain.com)
 * @brief This is about sleep management
 * @version 0.1
 * @date 2022-10-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "sleep.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_log.h"

char * log_prefix;

void goto_sleep_for_microseconds(uint64_t microsecs) {
    ESP_LOGI(log_prefix, "GOING INTO SLEEP MODE FOR %llu microseconds", microsecs);
    if (esp_sleep_enable_timer_wakeup(microsecs) == ESP_OK) {
        esp_deep_sleep_start();
    }

}

void sleep_init(char * _log_prefix) {
    log_prefix = _log_prefix;

}

