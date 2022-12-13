/**
 * @file sleep.c
 * @author Nicklas Borjesson (nicklasb@gmail.com)
 * @brief Manages and tracks how a peer goes to sleep and wakes up
 * @date 2022-10-22
 *
 * @copyright Copyright Nicklas Borjesson(c) 2022
 *
 */

#include "sleep.h"
#include "../sdp_def.h"
#include <sdp.h>
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "inttypes.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

/* Store the moment we last went to sleep in persistent storage */
RTC_DATA_ATTR uint64_t last_sleep_time;
RTC_DATA_ATTR int sleep_count;
RTC_DATA_ATTR uint64_t wake_time;

bool b_first_boot;

char *sleep_log_prefix;

void goto_sleep_for_microseconds(uint64_t microsecs)
{


    ESP_LOGI(sleep_log_prefix, "---------------------------------------- S L E E P ----------------------------------------");
    ESP_LOGI(sleep_log_prefix, "At %lli and going to sleep for %llu microseconds", esp_timer_get_time(), microsecs);


    sleep_count++;
    /* Now we know how long we were awake this time */
    wake_time+= esp_timer_get_time();
    /* Set the sleep time just before going to sleep. */
    last_sleep_time = get_time_since_start();
    if (esp_sleep_enable_timer_wakeup(microsecs) == ESP_OK)
    {

        esp_deep_sleep_start();
    }
    else
    {
        ESP_LOGE(sleep_log_prefix, "Error going to sleep for %"PRIu64" microseconds!", microsecs);
    }
}
/**
 * @brief Tells if we woke from sleeping or not
 * 
 * @return true If we were sleeping
 * @return false If it was from first boot.
 */
bool is_first_boot() {
    return b_first_boot;
}

/**
 * @brief Initialization of sleep management
 *
 * @param _log_prefix
 * @return true Returns true if waking up from sleep
 * @return false Returns false if normal boot
 */
bool sleep_init(char *_log_prefix)
{
    // TODO: Do I need a wake stub like: https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/deep-sleep-stub.rst
    sleep_log_prefix = _log_prefix;
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    switch (wakeup_cause)
    {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        ESP_LOGI(sleep_log_prefix, "-------------------------------------------------------------------------------------------");
        ESP_LOGI(sleep_log_prefix, "----------------------------------------- B O O T -----------------------------------------");
        ESP_LOGI(sleep_log_prefix, "-------------------------------------------------------------------------------------------");
        ESP_LOGI(sleep_log_prefix, "Normal boot, not returning from sleep mode.");
        // No sleep time has happened if we don't return from sleep mode.
        last_sleep_time = 0;
        sleep_count = 0;
        wake_time = 0;
        sdp_reset_rtc();
        b_first_boot = false;
        return false;
        break;

    default:
        ESP_LOGI(sleep_log_prefix, "----------------------------------------- W A K E -----------------------------------------");
        ESP_LOGI(sleep_log_prefix, "Returning from sleep mode.");
        b_first_boot = false;
        return true;
        break;
    }
}

/**
 * @brief Get the number of sleeps
 * 
 * @return int 
 */
int get_sleep_count() {
    return sleep_count;
}

/**
 * @brief Get the last sleep time
 *
 * @return int
 */
uint64_t get_last_sleep_time()
{
    return last_sleep_time;
}
/**
 * @brief
 *
 * @return int
 */
uint64_t get_time_since_start()
{
    if (last_sleep_time > 0)
    {
        /* The time we fell asleep + the time we waited + the time since waking up = Total time*/
        return last_sleep_time + SDP_SLEEP_TIME_uS + esp_timer_get_time();
    }
    else
    {
        /* Can't include the cycle delay if we haven't cycled.. */
        return esp_timer_get_time();
    }
}

uint64_t get_total_time_awake() {
    return wake_time + esp_timer_get_time();
}
