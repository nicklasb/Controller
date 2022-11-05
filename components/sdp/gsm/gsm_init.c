#include "gsm_init.h"

#include <string.h>
#include <sdkconfig.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_modem_api.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "sleep/sleep.h"

#include "driver/gpio.h"

#include "esp_log.h"

#define BROKER_URL "mqtt://mqtt.eclipseprojects.io"

static const char *TAG = "Controller";
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int GOT_DATA_BIT = BIT2;
static const int USB_DISCONNECTED_BIT = BIT3; // Used only with USB DTE but we define it unconditionally, to avoid too many #ifdefs in the code
static const int SHUTTING_DOWN_BIT = BIT4;
TaskHandle_t *gsm_modem_task;

char *log_prefix;

esp_modem_dce_t *dce = NULL;
esp_netif_t *esp_netif = NULL;
char *operator_name;

RTC_DATA_ATTR int mqtt_count;

#if defined(CONFIG_EXAMPLE_SERIAL_CONFIG_USB)
#include "esp_modem_usb_c_api.h"
#include "esp_modem_usb_config.h"
#include "freertos/task.h"
static void usb_terminal_error_handler(esp_modem_terminal_error_t err)
{
    if (err == ESP_MODEM_TERMINAL_DEVICE_GONE)
    {
        ESP_LOGI(TAG, "USB modem disconnected");
        assert(event_group);
        xEventGroupSetBits(event_group, USB_DISCONNECTED_BIT);
    }
}
#define CHECK_USB_DISCONNECTION(event_group)                                              \
    if ((xEventGroupGetBits(event_group) & USB_DISCONNECTED_BIT) == USB_DISCONNECTED_BIT) \
    {                                                                                     \
        esp_modem_destroy(dce);                                                           \
        continue;                                                                         \
    }
#else
#define CHECK_USB_DISCONNECTION(event_group)
#endif

void cleanup()
{
    ESP_LOGI(TAG, "1. GSM shut down. MQTT in %i of %i of sleep cycles.", mqtt_count, get_sleep_count());

    // Making sure gsm_modem_task is null if gsm_before_sleep_cb
    // happens to be called simultaneously to avoid a race condition
    TaskHandle_t *tmpTask = gsm_modem_task;
    gsm_modem_task = NULL;
    // Power off the modem.
    ESP_LOGI(TAG, "2. Informing GSM task it is shutting down.");
    xEventGroupSetBits(event_group, SHUTTING_DOWN_BIT);
    ESP_LOGI(TAG, "3. Deleting event group");
    vEventGroupDelete(event_group);
    event_group = NULL;

    ESP_LOGI(TAG, "4. Set command mode");
    esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND) failed with %d", err);
    }
 
    ESP_LOGI(TAG, "5. Deregister from network.");
    err = esp_modem_set_operator(dce,  3, 1, operator_name);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_operator(dce,  3, 1, %s) failed with %d",operator_name, err);
    }   

    char *res = malloc(40);


    ESP_LOGI(TAG, "6. Hang up.");
    err = esp_modem_at(dce, "ATH", res, 1000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ATH failed with %d", err);
    } else {
        ESP_LOGI(TAG, "  ATH result: %s", res);    
    }
    
    ESP_LOGI(TAG, "7. Modem Power down");
    err = esp_modem_power_down(dce);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_power_down(dce) failed with %d", err);
    }

    // Freeing memory
    ESP_LOGI(TAG, "8. Freeing memory..");
    esp_modem_destroy(dce);
    dce = NULL;
    esp_netif_destroy(esp_netif);
    esp_netif = NULL;
    
    // Short delay before cutting power.
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ESP_LOGI(log_prefix, "9. Cutting modem power.");
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4, 0);

    ESP_LOGI(log_prefix, "11. Deleting GSM task.");
    vTaskDelete(tmpTask);
}

bool gsm_before_sleep_cb()
{
    if (gsm_modem_task != NULL)
    {
        ESP_LOGI(log_prefix, "----- Before sleep: Turning off GSM modem -----");
        cleanup();

        // ESP_LOGI(log_prefix, "6.");
        // gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
        // gpio_set_level(GPIO_NUM_4, 0);
    }

    return true;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/esp-pppos", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/esp-pppos", "esp32-pppos", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        xEventGroupSetBits(event_group, GOT_DATA_BIT);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
        break;
    }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = event_data;
        ESP_LOGW(TAG, "User interrupted event from netif:%p", netif);
     } else {
        ESP_LOGW(TAG, "Got Uncategorized ppp event! %i", event_id);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGW(TAG, "Modem Disconnect from PPP Server");
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(TAG, "GOT IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    } else {
        ESP_LOGW(TAG, "GOT Uncategorized IP event! %i", event_id);
    }
}

void gsm_start()
{
    operator_name = malloc(40);
    /* Init and register system/core components */
    ESP_ERROR_CHECK(esp_netif_init());
    //    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    /* Configure the PPP netif */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    event_group = xEventGroupCreate();

    ESP_LOGI(log_prefix, "Turning off, waiting!");
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_4, 0);
    vTaskDelay(100/portTICK_PERIOD_MS);



    /* Configure the DTE */
#if defined(CONFIG_EXAMPLE_SERIAL_CONFIG_UART)
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    dte_config.uart_config.baud_rate = 9600;
    dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = -1; // CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = -1; // CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;

    dte_config.uart_config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;

#if CONFIG_EXAMPLE_MODEM_DEVICE_BG96 == 1
    ESP_LOGI(TAG, "Initializing esp_modem for the BG96 module...");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_BG96, &dte_config, &dce_config, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM800 == 1
    ESP_LOGI(TAG, "Initializing esp_modem for the SIM800 module...");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600 == 1
    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7600 module...");
    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_SIM7000 == 1

    // test();
    ESP_LOGI(TAG, "Initializing esp_modem for the SIM7000 module...");
    dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7000, &dte_config, &dce_config, esp_netif);

    ESP_LOGI(log_prefix, "Wait 1 second,power on modem pin 4, s");
    vTaskDelay(1000/portTICK_PERIOD_MS);
    
    gpio_set_level(GPIO_NUM_4, 1);
    vTaskDelay(4000/portTICK_PERIOD_MS);  

#else
    ESP_LOGI(TAG, "Initializing esp_modem for a generic module...");
    esp_modem_dce_t *dce = esp_modem_new(&dte_config, &dce_config, esp_netif);
#endif

#endif

    esp_err_t err = ESP_FAIL;
    while (err != ESP_OK)
    {
        ESP_LOGI(TAG, "Syncing with the modem...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        err = esp_modem_sync(dce);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_modem_sync failed with error:  %i", err);
        }
    }
    

    ESP_LOGI(TAG, "Getting information.");
    char res[100];   
    err = esp_modem_at(dce, "SIMCOMATI", res, 10000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_at SIMCOMATI failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(TAG, "SIMCOMATI returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   
    
    ESP_LOGI(TAG, "Preferred mode selection");

    err = esp_modem_at(dce, "CNMP?", res, 7000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_at CNMP? failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(TAG, "CNMP? returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   

    ESP_LOGI(TAG, "Preferred selection between CAT-M and NB-IoT");

    err = esp_modem_at(dce, "CMNB?", res, 7000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_at CMNB? failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(TAG, "CMNB? returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   
    
    /*
    ESP_LOGE(TAG, "Checking registration.");
    char res[100];
    err = esp_modem_at(dce, "CREG?", &res, 20000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_at CREG failed with error:  %i", err);
    }
    else
    {
        ESP_LOGE(TAG, "CREG? returned:  %s", res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    */
    /*ESP_LOGE(TAG, "Modem synced. resetting");
    char res[100];
    err = esp_modem_at(dce, "ATZ", &res, 1000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_reset failed with error:  %i", err);
    }
    else
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }*/

    assert(dce);
    xEventGroupClearBits(event_group, CONNECT_BIT | GOT_DATA_BIT | USB_DISCONNECTED_BIT | SHUTTING_DOWN_BIT);

    /* Run the modem demo app */
#if CONFIG_EXAMPLE_NEED_SIM_PIN == 1
    // check if PIN needed
    bool pin_ok = false;
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok == false)
    {
        if (esp_modem_set_pin(dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK)
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

    ESP_LOGI(TAG, "Checking for signal quality..");
    err = esp_modem_get_signal_quality(dce, &rssi, &ber);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_get_signal_quality failed with error:  %i", err);
        goto cleanup;
    }
    if (rssi == 99)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retries++;
        if (retries > 6)
        {
            ESP_LOGE(TAG, "esp_modem_get_signal_quality returned 99 for rssi after 3 retries.");
            ESP_LOGE(TAG, "It seems we don't have a proper connection, quitting (TODO: troubleshoot).");
        }
        else
        {
            ESP_LOGI(TAG, "esp_modem_get_signal_quality returned 99 for rssi, trying again");
            goto signal_quality;
        }
    }
    ESP_LOGI(TAG, "Signal quality: rssi=%d, ber=%d", rssi, ber);
    /*

    ESP_LOGI(TAG, "Waiting a little");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    int state;

    if (esp_modem_get_radio_state(dce, &state) == ESP_OK)
    {
        ESP_LOGI(TAG, "Radio state: %i", state);
    }
    else
    {
        ESP_LOGE(TAG, "Could not get radio state: %i", state);
    }

    
    err = esp_modem_set_network_attachment_state(dce, 1);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_network_attachment_state(dce) failed with %d", err);
        goto cleanup;
    }
    */
    int act = 0;

    err = esp_modem_get_operator_name(dce, operator_name, &act);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_get_operator_name(dce) failed with %d", err);

    } else {
        ESP_LOGI(TAG, "Operator name : %s, act: %i", operator_name, act);
    }


    // Setting data mode.
    err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_data_mode(dce) failed with %d", err);
        goto cleanup;
    }
    /* Wait for IP address */
    ESP_LOGI(TAG, "Waiting for IP address");
    EventBits_t uxBits;
    do
    {

        uxBits = xEventGroupWaitBits(event_group, CONNECT_BIT | USB_DISCONNECTED_BIT | SHUTTING_DOWN_BIT, pdFALSE, pdFALSE, 4000 / portTICK_PERIOD_MS); // portMAX_DELAY);
        if ((uxBits & (CONNECT_BIT | USB_DISCONNECTED_BIT)) == (CONNECT_BIT | USB_DISCONNECTED_BIT))
        {
            ESP_LOGE(log_prefix, "Both IP connected and USB disconnected? Taking a chance on that and moving on...");
            break;
        }
        else if ((uxBits & SHUTTING_DOWN_BIT) != 0)
        {
            ESP_LOGE(log_prefix, "Getting that we are shutting down, pausing indefinitely.");
            vTaskDelay(portMAX_DELAY);
        }
        else if ((uxBits & USB_DISCONNECTED_BIT) != 0)
        {
            ESP_LOGE(log_prefix, "USB disconnected, but no IP connection? Wierd error. Quitting.");
            goto cleanup;
            // xEventGroupWaitBits() returned because just BIT_0 was set.
        }
        else if ((uxBits & CONNECT_BIT) != 0)
        {
            ESP_LOGI(log_prefix, "Got an IP connection, great!");
            break;
        }
        else
        {
            ESP_LOGI(log_prefix, "Timed out. Continuing waiting for IP.");
        }
    } while (1);

    CHECK_USB_DISCONNECTION(event_group);
    ESP_LOGI(TAG, "Got an IP address");
    

/* Config MQTT */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = BROKER_URL,
    };
#else
    esp_mqtt_client_config_t mqtt_config = {
        .uri = BROKER_URL,

    };
#endif
    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "Waiting for MQTT data");
    uxBits = xEventGroupWaitBits(event_group, GOT_DATA_BIT | USB_DISCONNECTED_BIT | SHUTTING_DOWN_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if ((uxBits & SHUTTING_DOWN_BIT) != 0)
    {
        ESP_LOGE(log_prefix, "Getting that we are shutting down, pausing indefinitely.");
        vTaskDelay(portMAX_DELAY);
    }
    CHECK_USB_DISCONNECTION(event_group);
    ESP_LOGI(TAG, "Has MQTT data");
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_count++;

    //esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
    //vTaskDelay(4000/portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Sending SMS after MQTT");
    err = esp_modem_send_sms(dce, "0733600343", "After MQTT");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_send_sms(); failed with error:  %i", err);
    } else {

        vTaskDelay(4000/portTICK_PERIOD_MS);

    }/*    ESP_LOGI(TAG, "esp_mqtt_client_destroyed, setting command mode.");
    err = esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_COMMAND) failed with %d", err);
        goto cleanup;
    }
    char imsi[32];
    ESP_LOGI(TAG, "Getting the IMSI.");

    err = esp_modem_get_imsi(dce, imsi);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_modem_get_imsi failed with %d", err);
        goto cleanup;
    }
    ESP_LOGI(TAG, "IMSI=%s", imsi);
    */
cleanup:
    cleanup();
}

void gsm_init(char *_log_prefix)
{
    if (is_first_boot()) {
        mqtt_count = 0;
    }
    log_prefix = _log_prefix;
    int rc = xTaskCreatePinnedToCore(gsm_start, "GSM main task", /*8192*/ 16384, NULL, 8, &gsm_modem_task, 0);
    if (rc != pdPASS)
    {
        ESP_LOGE(log_prefix, "Failed creating GSM task, returned: %i (see projdefs.h)", rc);
    }
    ESP_LOGI(log_prefix, "GSM main task registered.");
}
