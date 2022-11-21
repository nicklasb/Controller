#include "gsm.h"

#include <string.h>
#include <sdkconfig.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "sdp_helpers.h"


#include "gsm_ip.h"
#include "gsm_worker.h"
#include "gsm_mqtt.h"
#include "orchestration/orchestration.h"

#include "sleep/sleep.h"

TaskHandle_t *gsm_modem_setup_task = NULL;

char *log_prefix;

char *operator_name;

int sync_attempts = 0;
bool successful_data = false;

RTC_DATA_ATTR uint connection_failures = 0;
RTC_DATA_ATTR uint connection_successes = 0;

void cut_modem_power() {
    // Short delay before cutting power.

    ESP_LOGI(log_prefix, "* Cutting modem power.");
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4, 0);

    blink_led(GPIO_NUM_12, 100, 100, 10);
}


void cleanup()
{
    /* As we clear gsm_event_group in the end of this function, we'll use it to know if it's been run. */
    if (!gsm_event_group) {
        ESP_LOGW(log_prefix, "GSM cleanup called when no cleanup is necessary. Exiting.");
        return;
    }

    ESP_LOGW(log_prefix, "GSM shutting down.");

    ESP_LOGI(log_prefix, " - Informing everyone that the GSM task it is shutting down.");
    xEventGroupSetBits(gsm_event_group, SHUTTING_DOWN_BIT);

    gsm_ip_cleanup();

    // Making sure gsm_modem_setup_task is null and use a temporary variable instead. 
    // This to avoid a race condition, as gsm_before_sleep_cb may be called simultaneously  

    if (gsm_modem_setup_task) {
        ESP_LOGI(log_prefix, " - Deleting GSM setup task.");
        vTaskDelete(gsm_modem_setup_task);
    }

    if (gsm_dce) {
        /*
        ESP_LOGI(log_prefix, " - Set command mode");
        esp_err_t err = esp_modem_set_mode(gsm_dce, ESP_MODEM_MODE_COMMAND);
        if (err != ESP_OK)
        {
            ESP_LOGE(log_prefix, "esp_modem_set_mode(gsm_dce, ESP_MODEM_MODE_COMMAND) failed with %d", err);
        }
        */
        /*
        ESP_LOGI(log_prefix, "* Sending SMS report after MQTT");
        err = esp_modem_send_sms(gsm_dce, "0733600343", "Going to sleep, all successful.");
        if (err != ESP_OK)
        {
            ESP_LOGE(log_prefix, "esp_modem_send_sms(); failed with error:  %i", err);
        } else {

            vTaskDelay(4000/portTICK_PERIOD_MS);
        }
        */
        /*
        ESP_LOGI(log_prefix, " - De-register from GSM/UMTS network.");
         err = esp_modem_set_operator(gsm_dce,  3, 1, operator_name);
        if (err != ESP_OK)
        {
            ESP_LOGE(log_prefix, "esp_modem_set_operator(gsm_dce,  3, 1, %s) failed with %d",operator_name, err);
        }   
        */
        // Power off the modem AT command.   
        ESP_LOGI(log_prefix, " - Tell modem to power down");
        esp_err_t err = esp_modem_power_down(gsm_dce);
        if (err == ESP_ERR_TIMEOUT)
        {
            ESP_LOGW(log_prefix, "esp_modem_power_down(gsm_dce) timed out");
        } else  
        {
            ESP_LOGE(log_prefix, "esp_modem_power_down(gsm_dce) failed with %d", err);
        }
        
    }
    if (gsm_event_group) {
        ESP_LOGI(log_prefix, " - Delete the event group");
        vEventGroupDelete(gsm_event_group);
        gsm_event_group = NULL;
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    cut_modem_power();


}

void do_on_work_cb(work_queue_item_t *work_item) {
    ESP_LOGI(log_prefix, "In GSM work callback.");

    if ((strcmp(work_item->parts[1], "-1.00") != 0) && (strcmp(work_item->parts[1], "-2.00") != 0)) {
        publish("/topic/lurifax/peripheral_humidity", work_item->parts[1],  strlen(work_item->parts[1]));
        publish("/topic/lurifax/peripheral_temperature", work_item->parts[2],  strlen(work_item->parts[2]));
    }
    publish("/topic/lurifax/peripheral_since_wake", work_item->parts[3],  strlen(work_item->parts[3]));
    publish("/topic/lurifax/peripheral_since_boot", work_item->parts[4],  strlen(work_item->parts[4]));
    publish("/topic/lurifax/peripheral_free_mem", work_item->parts[5],  strlen(work_item->parts[5]));
    publish("/topic/lurifax/peripheral_total_wake_time", work_item->parts[6],  strlen(work_item->parts[6]));
    char * curr_time;
    asprintf(&curr_time, "%.2f", (double)esp_timer_get_time()/(double)(1000000));

    char * since_start;
    asprintf(&since_start, "%.2f", (double)get_time_since_start()/(double)(1000000));

    char * total_wake_time;
    asprintf(&total_wake_time, "%.2f", (double)get_total_time_awake()/(double)(1000000));

    char * free_mem;
    asprintf(&free_mem, "%i", heap_caps_get_free_size(MALLOC_CAP_EXEC));

    char * sync_att;
    asprintf(&sync_att, "%i", sync_attempts);

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

    successful_data = true;

    gsm_cleanup_queue_task(work_item);

}

bool gsm_before_sleep_cb()
{
    if (!successful_data) {
        connection_failures++;
    }


    ESP_LOGI(log_prefix, "----- Before sleep: Turning off all GSM/UMTS stuff -----");
    cleanup();


    // TODO: gsm_modem_setup_task may also be null because it is shutting down, check for that case
    // There might be valid reason to wait with freeing the event group to the end be able to use it here instead.
    // Then, the event group may be the condition, and be used to wait for, if existing.

    return true;
}

void abort_if_shutting_down() {
    if ((gsm_event_group == NULL) || (xEventGroupGetBits(gsm_event_group) & SHUTTING_DOWN_BIT)) {
        ESP_LOGW(log_prefix, "GSM start: Told that we are shutting down; exiting. ");
        gsm_modem_setup_task = NULL;
        vTaskDelete(NULL);
        
    }
}

void gsm_start()
{
    operator_name = malloc(40);

    /* Create the event group, this is used for all event handling */
    gsm_event_group = xEventGroupCreate();
    xEventGroupClearBits(gsm_event_group, CONNECT_BIT | GOT_DATA_BIT | SHUTTING_DOWN_BIT);
    // We need to init the PPP netif as that is a parameter to the modem setup
    gsm_ip_init(log_prefix);

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);


    ESP_LOGI(log_prefix, "Powering on modem.");
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4, 1);
    ESP_LOGI(log_prefix, "Waiting a second.");
    vTaskDelay(1000/portTICK_PERIOD_MS);

    /* Configure the DTE */
#if defined(CONFIG_EXAMPLE_SERIAL_CONFIG_UART)
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    dte_config.uart_config.baud_rate = 115200;
    dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = -1; // CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = -1; // CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;

    dte_config.uart_config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;



#if CONFIG_EXAMPLE_MODEM_DEVICE_BG96 == 1
    ESP_LOGI(log_prefix, "Initializing esp_modem for the BG96 module...");
    gsm_dce = esp_modem_new_dev(ESP_MODEM_DCE_BG96, &dte_config, &dce_config, gsm_ip_esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM800 == 1
    ESP_LOGI(log_prefix, "Initializing esp_modem for the SIM800 module...");
    gsm_dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, gsm_ip_esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600 == 1
    ESP_LOGI(log_prefix, "Initializing esp_modem for the SIM7600 module...");
    gsm_dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, gsm_ip_esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7000 == 1

    ESP_LOGI(log_prefix, "Initializing esp_modem for the SIM7000 module..gsm_ip_esp_netif assigned %p", gsm_ip_esp_netif);
    gsm_dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7000, &dte_config, &dce_config, gsm_ip_esp_netif);
    assert(gsm_dce);

    ESP_LOGI(log_prefix, "Wait 1 second,power on modem pin 4, s");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(GPIO_NUM_4, 1);

    ESP_LOGI(log_prefix, "Waiting 4 seconds before trying to sync with the modem...");
    vTaskDelay(4000/portTICK_PERIOD_MS);  

#else
    ESP_LOGI(log_prefix, "Initializing esp_modem for a generic module...");
    gsm_dce = esp_modem_new(&dte_config, &dce_config, gsm_ip_esp_netif);
#endif

#endif

    esp_err_t err = ESP_FAIL;
    char res[100];   
    while (err != ESP_OK)
    {
        ESP_LOGI(log_prefix, "Querying the modem using SIMCOMATI...");
        
        err = esp_modem_at(gsm_dce, "SIMCOMATI", res, 4000);
        if (err != ESP_OK)
        {
            sync_attempts++;

            if (err == ESP_ERR_TIMEOUT) {
                ESP_LOGW(log_prefix, "SIMCOMATI, attempt %i timed out.", sync_attempts);
            } else {
                ESP_LOGE(log_prefix, "SIMCOMATI, attempt %i failed with error:  %i", sync_attempts, err);
            }
            abort_if_shutting_down();

        }
        else
        {
            ESP_LOGI(log_prefix, "SIMCOMATI returned:  %s", res);
            
        }   
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    abort_if_shutting_down();
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* We are now much more likely to be able to connect, ask for 7.5 more seconds for the next phase */
    ask_for_time(7500000);

    ESP_LOGI(log_prefix, "Preferred mode selection");

    err = esp_modem_at(gsm_dce, "CNMP?", res, 7000);
    if (err != ESP_OK)
    {
        ESP_LOGE(log_prefix, "esp_modem_at CNMP? failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(log_prefix, "CNMP? returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   
    abort_if_shutting_down();
    ask_for_time(7500000);
    ESP_LOGI(log_prefix, "Preferred selection between CAT-M and NB-IoT");

    err = esp_modem_at(gsm_dce, "CMNB?", res, 7000);
    if (err != ESP_OK)
    {
        ESP_LOGE(log_prefix, "esp_modem_at CMNB? failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(log_prefix, "CMNB? returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   
    abort_if_shutting_down();
    ask_for_time(5000000);
    /*
    ESP_LOGE(log_prefix, "Checking registration.");
    char res[100];
    err = esp_modem_at(gsm_dce, "CREG?", &res, 20000);
    if (err != ESP_OK)
    {
        ESP_LOGE(log_prefix, "esp_modem_at CREG failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(log_prefix, "CREG? returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    */
    /*ESP_LOGE(log_prefix, "Modem synced. resetting");
    char res[100];
    err = esp_modem_at(gsm_dce, "ATZ", &res, 1000);
    if (err != ESP_OK)
    {
        ESP_LOGE(log_prefix, "esp_modem_reset failed with error:  %i", err);
    }
    else
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }*/


    

    /* Run the modem demo app */
#if CONFIG_EXAMPLE_NEED_SIM_PIN == 1
    // check if PIN needed
    bool pin_ok = false;
    if (esp_modem_read_pin(gsm_dce, &pin_ok) == ESP_OK && pin_ok == false)
    {
        if (esp_modem_set_pin(gsm_dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            abort();
        }
    }
#endif

    int rssi, ber;
    int retries = 0;
signal_quality:

    ESP_LOGI(log_prefix, "Checking for signal quality..");
    err = esp_modem_get_signal_quality(gsm_dce, &rssi, &ber);

    if (err != ESP_OK)
    {
        ESP_LOGE(log_prefix, "esp_modem_get_signal_quality failed with error:  %i", err);
        cleanup();
    }
    if (rssi == 99)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retries++;
        if (retries > 6)
        {
            ESP_LOGE(log_prefix, "esp_modem_get_signal_quality returned 99 for rssi after 3 retries.");
            ESP_LOGE(log_prefix, "It seems we don't have a proper connection, quitting (TODO: troubleshoot).");
        }
        else
        {
            ESP_LOGI(log_prefix, "esp_modem_get_signal_quality returned 99 for rssi, trying again");
            goto signal_quality;
        }
    }
    ESP_LOGI(log_prefix, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    ask_for_time(5000000);
    int act = 0;

    err = esp_modem_get_operator_name(gsm_dce, operator_name, &act);
    if (err != ESP_OK)
    {
        ESP_LOGE(log_prefix, "esp_modem_get_operator_name(gsm_dce) failed with %d", err);

    } else {
        ESP_LOGI(log_prefix, "Operator name : %s, act: %i", operator_name, act);
    }
    ask_for_time(10000000);
    // Connect to the GSM network
    gsm_ip_enable_data_mode();
    ask_for_time(5000000);
    // Initialize MQTT
    gsm_mqtt_init(log_prefix);  

    // End init task
    vTaskDelete(NULL);

}

void gsm_init(char *_log_prefix)
{
    gsm_init_worker(&do_on_work_cb, NULL, log_prefix);
    log_prefix = _log_prefix;
    int rc = xTaskCreatePinnedToCore(gsm_start, "GSM main task", /*8192*/ 16384, NULL, 8, gsm_modem_setup_task, 0);
    if (rc != pdPASS)
    {
        ESP_LOGE(log_prefix, "Failed creating GSM task, returned: %i (see projdefs.h)", rc);
    }
    ESP_LOGI(log_prefix, "GSM main task registered.");
}
