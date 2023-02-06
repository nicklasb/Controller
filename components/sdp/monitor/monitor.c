/**
 * @file monitor.c
 * @author Nicklas Borjesson (nicklasb@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2022-11-20
 * 
 * @copyright Copyright Nicklas Borjesson(c) 2022
 * 
 */
#include "monitor.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_timer.h>
#include <esp_log.h>

#include "monitor_relations.h"
#include "monitor_memory.h"
#include "monitor_queue.h"

/* How often should we look */
#define CONFIG_SDP_MONITOR_DELAY 10000000

/* Monitor timer & task */

bool shutdown = false;

esp_timer_handle_t monitor_timer;


/* The log prefix for all logging */
char *monitor_log_prefix;

void run_monitors_initial() {
        monitor_memory();
        
}


void run_monitors() {
    ESP_LOGI(monitor_log_prefix, "Call monitor_memory");
    monitor_memory();
    ESP_LOGI(monitor_log_prefix, "Call monitor_relations");
    monitor_relations();
    ESP_LOGI(monitor_log_prefix, "Call monitor_queue");
    monitor_queue();
}

/**
 * @brief The monitor tasks periodically takes sampes of the current state
 * It uses that history data to perform some simple statistical calculations,
 * helping out with finding memory leaks.
 * 
 * 
 * @param arg 
 */

void monitor_task(void *arg)
{
    if (!shutdown) {
        // Call different monitors
        run_monitors();

        // TODO: Add an SLIST of external monitors
        // TODO: Add problem and warning callbacks to make it possible to raise the alarm if .
        
        ESP_ERROR_CHECK(esp_timer_start_once(monitor_timer, CONFIG_SDP_MONITOR_DELAY));
    } else {
        vTaskDelete(NULL);
    }

}

void sdp_shutdown_monitor() {
     ESP_LOGI(monitor_log_prefix, "Telling monitor to shut down.");
     shutdown = true;
}

void sdp_init_monitor(char *_log_prefix)
{   
    monitor_log_prefix = _log_prefix;
    
    /* Init the monitors if needed*/
    memory_monitor_init(monitor_log_prefix);

    ESP_LOGI(monitor_log_prefix, "Launching monitor, activate every %.2f seconds.", (float)CONFIG_SDP_MONITOR_DELAY/500000);
    // First run once immidiately to capture initial (to include the monitors themselves if monitored)
    run_monitors_initial();

    const esp_timer_create_args_t monitor_timer_args = {
        .callback = &monitor_task,
        .name = "monitor"
        };

    ESP_ERROR_CHECK(esp_timer_create(&monitor_timer_args, &monitor_timer));

    // TODO: Probably monitors will need to have different frequencies, and have separate tasks or timers. 
    

    monitor_task(NULL);
}

/**

 * TODO: Add a security monitor - every 10 seconds 
 *          * Attack detection?
 *          * Lots of login attempts (lock down to specific accounts or tell to slow down OOB)
 * TODO: Add a conversation monitor - every 2 minutes
 *          * Are requests getting replies? And how quickly? 
 *          * Unanswered conversations needs to be pruned after a timeout
 *          * Stats needs to be collected
 * TODO: Add a mesh manager
 *          * Periodically look for new connections and explore their connections
*/