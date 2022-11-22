
#include <string.h>

#include "sdp.h"
#include "sdp_task.h"
#include "sdp_messaging.h"
#include "sdp_mesh.h"

#include "sleep/sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_timer.h"
#include "ui_builder.h"
#include "local_settings.h"

#include "orchestration/orchestration.h"

#include "sdp/sdp_worker.h"
#include "sdp/gsm/gsm_worker.h"

#include "sdp/gsm/gsm.h"

#include "sdp/sdp_worker.h"

#include "esp_log.h"

esp_timer_handle_t periodic_timer;


#define SDP_SLEEP_TIME 5000000

/**
 * @brief Takes a closer look on the incoming request queue item, does it need urgent attention?
 *
 * @param work_item
 */
int do_on_filter_request(work_queue_item_t *work_item)
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
int do_on_filter_data(work_queue_item_t *work_item)
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
void do_on_priority(work_queue_item_t *work_item)
{

    ESP_LOGI(log_prefix, "In SDP data callback on the controller!");
    if (strcmp((char *)(work_item->raw_data), (char *)"status") == 0)
    {
        ESP_LOGI(log_prefix, "Got asked for status!");
    }
}
/**
 * @brief Handle incoming data messages
 * 
 * @param queue_item 
 * @return true The queue item has been passed on, do not free
 * @return false 
 */
bool do_on_data(work_queue_item_t *queue_item)
{
    ESP_LOGI(log_prefix, "In do_on_data on the controller, conversation: %i", queue_item->conversation_id);

    struct conversation_list_item *conversation;
    conversation = find_conversation(queue_item->peer, queue_item->conversation_id);
    if (conversation) {
        ESP_LOGI(log_prefix, "In do_on_data on the controller, for reason: %s", conversation->reason);
    } else {
        ESP_LOGI(log_prefix, "No local conversation found");
    }

    if (strcmp(conversation->reason, "status") == 0)
    {
        ESP_LOGI(log_prefix, "Status is returned");
        lv_label_set_text_fmt(vberth, queue_item->parts[1]);

        /* Always end the conversations if this is expeced to be the last response */
        end_conversation(queue_item->conversation_id);
        
    } else
    if (strcmp(conversation->reason, "sensors") == 0)
    {
        if (strcmp(queue_item->parts[0], "-2")  == 0) {
            lv_label_set_text(vberth, "Sensor not detected yet");
        } else if (strcmp(queue_item->parts[0], "-1")  == 0) {
            lv_label_set_text(vberth, "Checksum failed");
        } else {
            lv_label_set_text_fmt(vberth, "Temperature %s °C", queue_item->parts[0]);
        }   
        end_conversation(queue_item->conversation_id);
        
        
    } else
    if (strcmp(conversation->reason, "external") == 0)
    {      
        ESP_LOGI(log_prefix, "Had an external message");
        // TODO: Send on using MQTT
        gsm_safe_add_work_queue(queue_item);
        end_conversation(queue_item->conversation_id);
        return true;
        
        
    } else
    if (strcmp(conversation->reason, "env_central") == 0)
    {      
        // TODO: Send on using MQTT
        end_conversation(queue_item->conversation_id);
    }

    return false;
}

/**
 * @brief The work task get a work item from the queue and reacts to it.
 *
 * @param queue_item The work item
 */
void do_on_work(work_queue_item_t *queue_item)
{

    ESP_LOGI(log_prefix, "In do_on_work task on the controller, got a message:\n");
    for (int i = 0; i < queue_item->partcount; i++)
    {
        ESP_LOGI(log_prefix, "Message part %i: \"%s\"", i, queue_item->parts[i]);
    }

    switch (queue_item->work_type)
    {
    case DATA:
        if (do_on_data(queue_item)) {
            // Reset this pointer so I won't be freed in the cleanup.
            queue_item = NULL;
        }
        break;

    default:
        break;
    }
    /* Note that the worker task is run on Core 1 (APP) as upposed to all the other callbacks. */
    if (queue_item == NULL) {
        ESP_LOGI(log_prefix, "End of do_on_work task on the controller, a message was passed on");
    } else {
        ESP_LOGI(log_prefix, "End of do_on_work task on the controller, got a message:\n%s", (char *)queue_item->raw_data);
    }
    
    /* Always call the cleanup crew when done */
    sdp_cleanup_queue_task(queue_item);
}

/**
 * @brief This is periodically waking up the controller, sends a request for sensor data
 *
 */
void periodic_sensor_query(void *arg)
{
    /* Note that the worker task is run on Core 1 (APP) as upposed to all the other callbacks. */
    ESP_LOGI(log_prefix, "In prediodic_sensor_query task on the controller.");
    gsm_abort_if_shutting_down();
    char data[8] = "sensors\0";
    ESP_LOGI(log_prefix, "Test broadcast beginning.");
    
    sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address(local_hosts[1].base_mac_address);
    start_conversation(peer, REQUEST, "sensors", &data, sizeof(data));

    
    ESP_LOGI(log_prefix, "Test broadcast done.");
    gsm_abort_if_shutting_down();

    ESP_ERROR_CHECK(esp_timer_start_once(periodic_timer, 10000000));
}


bool before_sleep_cb() {
    ESP_LOGI(log_prefix, "Before sleep:");
    sdp_set_queue_blocked(true);
    gsm_set_queue_blocked(true);

    int res = gsm_before_sleep_cb();

    return res;
}

void init_sdp_task() {

    on_before_sleep_cb = &before_sleep_cb;
    on_filter_request_cb = &do_on_filter_request;
    on_filter_data_cb = &do_on_filter_data;  
    sdp_init(&do_on_work, &do_on_priority, "Controller\0", true);

    sdp_add_init_new_peer(local_hosts[1].hostname, local_hosts[1].base_mac_address, SDP_MT_ESPNOW);


/* Controller:  e0:e2:e6:bd:8e:58*/
    /* Peripheral:  34:86:5d:39:b1:18 */

    const esp_timer_create_args_t periodic_timer_args = {
            .callback =  &periodic_sensor_query,
            .name = "periodic_query"
    };


    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(periodic_timer, 000000));

    // Let the orchestrator take over. 
    take_control();
}
