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
    ESP_LOGI(log_prefix, "---------------------------------------- S L E E P ----------------------------------------");
    ESP_LOGI(log_prefix, "Going to sleep for %llu microseconds", microsecs);
    if (esp_sleep_enable_timer_wakeup(microsecs) == ESP_OK) {
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
        return false;
        break;
    
    default:
        ESP_LOGI(log_prefix, "----------------------------------------- W A K E -----------------------------------------");
        ESP_LOGI(log_prefix, "Returning from sleep mode.");
        return true;
        break;
    }


}

