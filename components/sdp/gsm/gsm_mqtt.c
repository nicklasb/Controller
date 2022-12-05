/**
 * @file gsm_mqtt.c
 * @author your name (you@domain.com)
 * @brief Usage of the ESP-IDF MQTT client. 
 * TODO: This should probably be separate, able to also use wifi or any other channel.
 * @version 0.1
 * @date 2022-11-20
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "gsm_mqtt.h"

#include "mqtt_client.h"

#include <sleep/sleep.h>
#include <orchestration/orchestration.h>
#include "esp_log.h"
#include "gsm.h"
#include "gsm_worker.h"



#define BROKER_URL "mqtt://mqtt.eclipseprojects.io"

#define TOPIC "/topic/lurifax"

char *log_prefix;
esp_mqtt_client_handle_t mqtt_client = NULL;

RTC_DATA_ATTR int mqtt_count;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(log_prefix, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(log_prefix, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/lurifax_test", 0);
        // All is initiated, we can now start handling the queue
        gsm_set_queue_blocked(false);
        //ESP_LOGI(log_prefix, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(log_prefix, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(log_prefix, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        ask_for_time(5000000); 
        //msg_id = esp_mqtt_client_publish(client, "/topic/esp-pppos", "esp32-pppos", 0, 0, 0);
        //ESP_LOGI(log_prefix, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(log_prefix, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(log_prefix, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        mqtt_count++;    
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(log_prefix, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        xEventGroupSetBits(gsm_event_group, GOT_DATA_BIT);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(log_prefix, "MQTT_EVENT_ERROR");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(log_prefix, "MQTT_EVENT_BEFORE_CONNECT");
        ask_for_time(6000000); 
        break;   
    default:
        ESP_LOGI(log_prefix, "MQTT other event id: %d", event->event_id);
        break;
    }
}

void gsm_mqtt_cleanup() {
    if (mqtt_client) {

        ESP_LOGI(log_prefix, "* MQTT shutting down.");
        ESP_LOGI(log_prefix, " - Success in %i of %i of sleep cycles.", mqtt_count, get_sleep_count());
        ESP_LOGI(log_prefix, " - Unsubscribing the client from the %s topic.", TOPIC);
        esp_mqtt_client_unsubscribe(mqtt_client, TOPIC);
        ESP_LOGI(log_prefix, " - Destroying the client.");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    } else {
        ESP_LOGI(log_prefix, "* MQTT already shut down and cleaned up or not started, doing nothing.");
    }


}

int publish(char * topic, char * payload, int payload_len) {
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, payload_len, 0, 1);
    ESP_LOGI(log_prefix, "Data published.");
    return msg_id;
}



void gsm_mqtt_init(char * _log_prefix) {
    
    log_prefix = _log_prefix;

    if (is_first_boot()) {
        mqtt_count = 0;
    }

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
    ESP_LOGI(log_prefix, "* Start MQTT client:");
    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    ESP_LOGI(log_prefix, " + Register callbacks");
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL); 
    gsm_abort_if_shutting_down();
    ask_for_time(5000000); 
    ESP_LOGI(log_prefix, " + Start the client");
    esp_mqtt_client_start(mqtt_client);
    gsm_abort_if_shutting_down();
    ESP_LOGI(log_prefix, " + Subscribe to the the client from the %s topic.", TOPIC);
    esp_mqtt_client_subscribe(mqtt_client, TOPIC, 1);
    ESP_LOGI(log_prefix, "* MQTT Running.");
    //TODO:Move the following to a task
    #if 0
    ESP_LOGI(log_prefix, "Waiting for MQTT data");
    EventBits_t uxBits = xEventGroupWaitBits(gsm_event_group, GOT_DATA_BIT | SHUTTING_DOWN_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if ((uxBits & SHUTTING_DOWN_BIT) != 0)
    {
        ESP_LOGE(log_prefix, "Getting that we are shutting down, pausing indefinitely.");
        vTaskDelay(portMAX_DELAY);
    }
    
    ESP_LOGI(log_prefix, "Has MQTT data");


    mqtt_count++;    
    #endif
}
