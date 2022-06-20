
#include <string.h>

#include "sdp.h"
#include "sdp_task.h"

#include "sdp_helpers.h"

#include "esp_timer.h"
#include "ui_builder.h"

#include "esp_log.h"

esp_timer_handle_t periodic_timer;

/**
 * @brief Takes a closer look on the incoming request queue item, does it need urgent attention?
 *
 * @param work_item
 */
int do_on_filter_request(struct work_queue_item *work_item)
{

    ESP_LOGI(log_prefix, "In filter request callback on the controller!");

    /* Allow the data to be imported */
    return 0;
}

/**
 * @brief Takes a closer look on the incoming data queue item, does it need urgent attention?
 *
 * @param work_item
 */
int do_on_filter_data(struct work_queue_item *work_item)
{
    ESP_LOGI(log_prefix, "In filter data callback on the controller!");

    /* Allow the data to be imported */
    return 0;
}
/*
This is the running task of the client (the central), it currently connects to the server (actually peripheral) and sends data.
TODO: Instead this will be rewritten competely:
It will instead contact all peripherals and ask for data, in the following situations:
1. After periodically being woken from a timer signal
2. Because a GPIO went high (which is likely to be because of an alarm)
3. Because of a instruction via SMS (or other way in).
*/

/**
 * @brief Handle priority messages
 *
 * @param work_item
 */
void do_on_priority(struct work_queue_item *work_item)
{

    ESP_LOGI(log_prefix, "In ble data callback on the controller!");
    if (strcmp((char *)(work_item->raw_data), (char *)"status") == 0)
    {
        ESP_LOGI(log_prefix, "Got asked for status!");
    }
}

void do_on_data(struct work_queue_item *queue_item)
{
    struct conversation_list_item *conversation;
    conversation = find_conversation(queue_item->conversation_id);
    ESP_LOGI(log_prefix, "In do_on_data on the controller, for reason: %s", conversation->reason);
    

    if (strcmp(conversation->reason, "status") == 0)
    {
        ESP_LOGI(log_prefix, "Status is returned");
        lv_label_set_text_fmt(vberth, queue_item->parts[1]);
        
    }
    if (strcmp(conversation->reason, "sensors") == 0)
    {
        if (strcmp(queue_item->parts[0], "-2")  == 0) {
            lv_label_set_text(vberth, "Sensor not detected yet");
        } else if (strcmp(queue_item->parts[0], "-1")  == 0) {
            lv_label_set_text(vberth, "Checksum failed");
        } else {
            lv_label_set_text_fmt(vberth, "Temperature %s °C", queue_item->parts[0]);
        }   
        
        
    }    

    /* Always end the conversations if this is expeced to be the last response */
    end_conversation(queue_item->conversation_id);
}

/**
 * @brief The work task get a work item from the queue and reacts to it.
 *
 * @param queue_item The work item
 */
void do_on_work(struct work_queue_item *queue_item)
{
    ESP_LOGI(log_prefix, "In do_on_work task on the controller, got a message:\n");
    for (int i = 0; i < queue_item->partcount; i++)
    {
        ESP_LOGI(log_prefix, "Message part %i: \"%s\"", i, queue_item->parts[i]);
    }

    switch (queue_item->work_type)
    {
    case DATA:
        do_on_data(queue_item);
        break;

    default:
        break;
    }
    /* Note that the worker task is run on Core 1 (APP) as upposed to all the other callbacks. */
    ESP_LOGI(log_prefix, "In do_on_work task on the controller, got a message:\n%s", (char *)queue_item->raw_data);
    /* Always call the cleanup crew when done */
    cleanup_queue_task(queue_item);
}

/**
 * @brief This is periodically waking up the controller, sends a request for sensor data
 *
 */
void periodic_sensor_query(void *arg)
{
    /* Note that the worker task is run on Core 1 (APP) as upposed to all the other callbacks. */
    ESP_LOGI(log_prefix, "In prediodic_sensor_query task on the controller.");

    char data[9] = "sensors\0";
    ESP_LOGI(log_prefix, "Test broadcast beginning.");
    start_conversation(BLE, -1, REQUEST, "sensors", &data, sizeof(data));
    ESP_LOGI(log_prefix, "Test broadcast done.");
    ESP_ERROR_CHECK(esp_timer_start_once(periodic_timer, 10000000));
}

void init_sdp_task() {

    on_filter_request_cb = &do_on_filter_request;
    on_filter_data_cb = &do_on_filter_data;  
    sdp_init(do_on_work, do_on_priority, "Controller\0", true);

    const esp_timer_create_args_t periodic_timer_args = {
            .callback =  &periodic_sensor_query,
            .name = "periodic_query"
    };


    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(periodic_timer, 1000000));
}
