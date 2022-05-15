#include "esp_log.h"
#include <string.h>

#include "sdp.h"
#include "sdp_task.h"

#include "esp_timer.h"

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
/**
 * @brief The work task get a work item from the queue and reacts to it.
 *
 * @param queue_item The work item
 */
void do_on_work(struct work_queue_item *queue_item)
{

    switch (queue_item->work_type)
    {
    case DATA:

        /* Always end the conversations if this is expeced to be the last response */
        end_conversation(queue_item->conversation_id);

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
void prediodic_sensor_query(void *arg)
{
    /* Note that the worker task is run on Core 1 (APP) as upposed to all the other callbacks. */
    ESP_LOGI(log_prefix, "In prediodic_sensor_query task on the controller.");

    char data[28] = "status\0testdata\0\0test\0data2\0";
    ESP_LOGI(log_prefix, "Test broadcast beginning.");
    start_conversation(BLE, -1, REQUEST, &data, sizeof(data));
    ESP_LOGI(log_prefix, "Test broadcast done.");
    ESP_ERROR_CHECK(esp_timer_start_once(periodic_timer, 5000000));
}
