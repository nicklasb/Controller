/**
 * @file sleep.c
 * @author Nicklas Borjesson (nicklasb@gmail.com)
 * @brief This is about sleep management
 * @date 2022-10-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "sleep.h"
#include "sdp_def.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

RTC_DATA_ATTR int last_sleep_time;

char * log_prefix;




void goto_sleep_for_microseconds(uint64_t microsecs) {
    if (on_before_sleep_cb) {
        ESP_LOGI(log_prefix, "Calling before sleep callback");
        if (!on_before_sleep_cb()) {
            ESP_LOGW(log_prefix, "Stopped from going to sleep by callback!");
            return;
        }        
    }
    ESP_LOGI(log_prefix, "---------------------------------------- S L E E P ----------------------------------------");
    ESP_LOGI(log_prefix, "At %lli Going to sleep for %llu microseconds", esp_timer_get_time(), microsecs);
    
        
    if (esp_sleep_enable_timer_wakeup(microsecs) == ESP_OK) {
        last_sleep_time+= esp_timer_get_time();
        esp_deep_sleep_start();
    }
}

/**
 * @brief Initialization of sleep management
 * 
 * @param _log_prefix 
 * @return true Returns true if waking up from sleep
 * @return false Returns false if normal boot
 */
bool sleep_init(char * _log_prefix) {
    // TODO: Do I need a wake stub like: https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/deep-sleep-stub.rst
    log_prefix = _log_prefix;
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    switch (wakeup_cause)
    {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        ESP_LOGI(log_prefix, "-------------------------------------------------------------------------------------------");
        ESP_LOGI(log_prefix, "----------------------------------------- B O O T -----------------------------------------");
        ESP_LOGI(log_prefix, "-------------------------------------------------------------------------------------------");
        ESP_LOGI(log_prefix, "Normal boot, not returning from sleep mode.");
        // No sleep time has happened if we don't return from sleep mode.
        last_sleep_time = 0;
        return false;
        break;
    
    default:
        ESP_LOGI(log_prefix, "----------------------------------------- W A K E -----------------------------------------");
        ESP_LOGI(log_prefix, "Returning from sleep mode.");
        last_sleep_time+= SDP_CYCLE_DELAY_uS;
        return true;
        break;
    }
}
/**
 * @brief Get the last sleep time
 * 
 * @return int 
 */
int get_last_sleep_time() {
    return last_sleep_time;
}
/**
 * @brief Get the time since first boot (not wakeup)
 * 
 * @return int 
 */
int get_time_since_start() {
    if (last_sleep_time > 0) {
        /* Can't include the cycle delay if we haven't cycled.. */
        return last_sleep_time + SDP_CYCLE_DELAY_uS + esp_timer_get_time();
    } else {
        return esp_timer_get_time();
    }
    
}


