
#include "esp_log.h"
#include <string.h>

#include "sdp.h"
#include "ble_service.h"
#include "ble_task.h"

/**
 * @brief Handle priority messages
 *
 * @param queue_item
 */
void on_priority(struct work_queue_item queue_item)
{

    ESP_LOGI(task_tag, "In ble data callback on the controller!");
    if (strcmp((char *)(queue_item.data), (char *)"status") == 0)
    {
        ESP_LOGI(task_tag, "Got asked for status!");
    }
}

/**
 * @brief Takes a closer look on the incoming request queue item, does it need urgent attention?
 *
 * @param queue_item
 */
void on_request(struct work_queue_item queue_item)
{

    ESP_LOGI(task_tag, "In ble data callback on the controller!");
    if (strcmp((char *)(queue_item.data), (char *)"status") == 0)
    {
        ESP_LOGI(task_tag, "Got asked for status!");
    }
}

/**
 * @brief Takes a closer look on the incoming request queue item, does it need urgent attention?
 *
 * @param queue_item
 */
void on_data(struct work_queue_item queue_item)
{

    if (queue_item.work_type == PRIORITY)
    {
    }
    ESP_LOGI(task_tag, "In ble data callback on the controller!");
    if (strcmp((char *)(queue_item.data), (char *)"status") == 0)
    {
        ESP_LOGI(task_tag, "Got asked for status!");
    }
}
/*
This is the running task of the client (the central), it currently connects to the server (actually peripheral) and sends data.
TODO: Instead this will be rewritten competely:
It will instead contact all peripherals and ask for data, in the following situations:
1. After periodically being woken from a timer signal
2. Because a GPIO went high (which is likely to be because of an alarm)
3. Because of a instruction via SMS (or other way in).
*/

void ble_client_my_task(void *pvParameters)
{

    /* Use a special semaphore for BLE.
   TODO: Consider if there instead should be one for Core 0 instead
    (that might not be possible if not a param here; should a controller init exist?)
   */


    on_request_cb = on_request;
    on_data_cb = on_data;
    on_priority_cb = on_priority;

    char myarray[7] = "status\0";
    int ret;
    int i = 0;
    ESP_LOGI(task_tag, "My Task controller: BLE client UART task started\n");
    for (;;)
    {
        /*
        ESP_LOGI(task_tag, "My Task controller: Loop peers: %i", i);
        struct peer *curr_peer;
        const struct peer_chr *chr;
        SLIST_FOREACH(curr_peer, &peers, next) {


            chr = peer_chr_find_uuid(curr_peer,
                                    BLE_UUID16_DECLARE(GATT_SPP_SVC_UUID),
                                    BLE_UUID16_DECLARE(GATT_SPP_CHR_UUID));


            ESP_LOGI(task_tag, "Peer: %i,  Address: %02X%02X%02X%02X%02X%02X", curr_peer->conn_handle,
            curr_peer->desc.peer_ota_addr.val[0],curr_peer->desc.peer_ota_addr.val[1],
            curr_peer->desc.peer_ota_addr.val[2],curr_peer->desc.peer_ota_addr.val[3],
            curr_peer->desc.peer_ota_addr.val[4],curr_peer->desc.peer_ota_addr.val[5]);


        }
        i++;
        */
        vTaskDelay(2000);
    }
    vTaskDelete(NULL);
}