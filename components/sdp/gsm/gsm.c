#include <sdkconfig.h>
// TODO: The naming of things in the KConfig still have the names of the examples, this needs to change

#ifdef CONFIG_SDP_LOAD_UMTS

#include "gsm.h"
#include "gsm_def.h"

#include <string.h>

#include "esp_log.h"
#include "../sdp_helpers.h"

#include "gsm_task.h"
#include "gsm_mqtt.h"



#include "gsm_worker.h"

#include <esp_timer.h>

#include "../sleep/sleep.h"


char *gsm_log_prefix;


bool successful_data = false;

RTC_DATA_ATTR uint connection_failures;
RTC_DATA_ATTR uint connection_successes;

EventGroupHandle_t gsm_event_group = NULL;

bool gsm_shutdown()
{
    if (!successful_data) {
        connection_failures++;
    }
    ESP_LOGI(gsm_log_prefix, "----- Turning off all GSM/UMTS stuff -----");


    gsm_cleanup();
    

  /*  ESP_LOGI(gsm_log_prefix, "- Setting pin 12 (LED) to hight.");
    gpio_set_direction(GPIO_NUM_12, GPIO_MODE_OUTPUT);
    gpio_pullup_en(GPIO_NUM_12);
    gpio_set_level(GPIO_NUM_12, 1);
    vTaskDelay(200/portTICK_PERIOD_MS);
     gpio_set_level(GPIO_NUM_12, 0);
    vTaskDelay(200/portTICK_PERIOD_MS);
     gpio_set_level(GPIO_NUM_12, 1);
    vTaskDelay(200/portTICK_PERIOD_MS);
     gpio_set_level(GPIO_NUM_12, 0);
    vTaskDelay(200/portTICK_PERIOD_MS);*/
    return true;
}

void gsm_do_on_work_cb(work_queue_item_t *work_item) {

    // TODO: Consider what actually is the point here, should GSM=MQTT?

    ESP_LOGI(gsm_log_prefix, "In GSM work callback.");

    if ((strcmp(work_item->parts[1], "-1.00") != 0) && (strcmp(work_item->parts[1], "-2.00") != 0)) {
        publish("/topic/lurifax/peripheral_humidity", work_item->parts[1],  strlen(work_item->parts[1]));
        publish("/topic/lurifax/peripheral_temperature", work_item->parts[2],  strlen(work_item->parts[2]));
    }
    publish("/topic/lurifax/peripheral_since_wake", work_item->parts[3],  strlen(work_item->parts[3]));
    publish("/topic/lurifax/peripheral_since_boot", work_item->parts[4],  strlen(work_item->parts[4]));
    publish("/topic/lurifax/peripheral_free_mem", work_item->parts[5],  strlen(work_item->parts[5]));
    publish("/topic/lurifax/peripheral_total_wake_time", work_item->parts[6],  strlen(work_item->parts[6]));
    publish("/topic/lurifax/peripheral_voltage", work_item->parts[7],  strlen(work_item->parts[7]));
    publish("/topic/lurifax/peripheral_state_of_charge", work_item->parts[8],  strlen(work_item->parts[8]));
    publish("/topic/lurifax/peripheral_battery_current", work_item->parts[9],  strlen(work_item->parts[9]));
    publish("/topic/lurifax/peripheral_mid_point_voltage", work_item->parts[10],  strlen(work_item->parts[10]));


    char * curr_time;
    asprintf(&curr_time, "%.2f", (double)esp_timer_get_time()/(double)(1000000));

    char * since_start;
    asprintf(&since_start, "%.2f", (double)get_time_since_start()/(double)(1000000));

    char * total_wake_time;
    asprintf(&total_wake_time, "%.2f", (double)get_total_time_awake()/(double)(1000000));

    char * free_mem;
    asprintf(&free_mem, "%i", heap_caps_get_free_size(MALLOC_CAP_EXEC));

    char * sync_att;
    asprintf(&sync_att, "%i", get_sync_attempts());

    char * c_connection_failues;
    asprintf(&c_connection_failues, "%i", connection_failures);

    publish("/topic/lurifax/controller_since_wake", curr_time,  strlen(curr_time));
    publish("/topic/lurifax/controller_since_boot", since_start,  strlen(since_start));
    publish("/topic/lurifax/controller_total_wake_time", total_wake_time,  strlen(total_wake_time));    
    publish("/topic/lurifax/controller_free_mem", free_mem,  strlen(free_mem));    
    publish("/topic/lurifax/controller_sync_attempts", sync_att,  strlen(sync_att));   
    publish("/topic/lurifax/controller_connection_failures", c_connection_failues,  strlen(c_connection_failues)); 
    
    // Lets deem this a success.
    connection_successes++;
    char * c_connection_successes;
    asprintf(&c_connection_successes, "%i", connection_successes);
    publish("/topic/lurifax/controller_connection_successes", c_connection_successes,  strlen(c_connection_successes)); 
/*TODO: Fix this? 
    char * c_battery_voltage;
    asprintf(&c_battery_voltage, "%.2f", sdp_read_battery());
    publish("/topic/lurifax/controller_battery_voltage", c_battery_voltage,  strlen(c_battery_voltage)); 
*/
    successful_data = true;

    gsm_cleanup_queue_task(work_item);

}   

void gsm_reset_rtc() {
    connection_failures = 0;
    connection_successes = 0;
}

void gsm_init(char *_log_prefix)
{
    gsm_log_prefix = _log_prefix;
    ESP_LOGI(gsm_log_prefix, "Initiating GSM modem..");

    gsm_init_worker(&gsm_do_on_work_cb, gsm_log_prefix);

    /* Create the event group, this is used for all event handling, initiate in main thread */
    gsm_event_group = xEventGroupCreate();
    xEventGroupClearBits(gsm_event_group, GSM_CONNECT_BIT | GSM_GOT_DATA_BIT | GSM_SHUTTING_DOWN_BIT);

    ESP_LOGI(gsm_log_prefix, "* Registering GSM main task...");
    int rc = xTaskCreatePinnedToCore((TaskFunction_t)gsm_start, "GSM main task", /*8192*/ 16384, 
        (void *)gsm_log_prefix, 5, &gsm_modem_setup_task, 0);
    if (rc != pdPASS)
    {
        ESP_LOGE(gsm_log_prefix, "Failed creating GSM task, returned: %i (see projdefs.h)", rc);
    }
    ESP_LOGI(gsm_log_prefix, "* GSM main task registered.");
}

#endif
