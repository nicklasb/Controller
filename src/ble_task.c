
#include "ble_task.h"
#include "esp_log.h"
#include "host/ble_gatt.h"
#include "peer.c"
#include "host/ble_uuid.h"
#include "ble_service.h"
#include "esp_crc.h" 

/**
 * @brief Handles incoming data
 * 
 * @param conn_handle The connection handle of the peer
 * @param attr_handle The handle of the attribute being sent
 * @param ctxt Access context information
 */
void on_ble_data(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt) {
        ESP_LOGI(task_tag,"In ble data callback on the controller!");
        if (strcmp((char *) (ctxt->om->om_data), (char *)"status") == 0) {
            ESP_LOGI(task_tag,"Got asked for status!");
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
    xBLESemaphore = xSemaphoreCreateMutex();
    on_ble_data_cb = on_ble_data;
       
    char myarray[7] = "status\0";
    int ret;
    int i =0;
    ESP_LOGI(task_tag, "My Task controller: BLE client UART task started\n");
    for (;;)
    {
        
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
        
            if (pdTRUE == xSemaphoreTake(xBLESemaphore, portMAX_DELAY) && (curr_peer!=NULL))
            {
                ret = ble_gattc_write_flat(curr_peer->conn_handle, ble_spp_svc_gatt_read_val_handle, &myarray, sizeof(myarray), NULL, NULL);
                if (ret == 0)
                {
                    ESP_LOGI(task_tag, "My Task controller: Success writing characteristic! Number: %i, CRC32: %u", i, esp_crc32_be(0, &myarray, sizeof(myarray)));
                }
                else
                {
                    ESP_LOGI(task_tag, "My Task controller: Error in writing characteristic! Number: %i", i);
                }
                xSemaphoreGive(xBLESemaphore);
            }
            else
            {
                ESP_LOGI(task_tag, "My Task controller: Couldn't get semaphore Number: %i", i);
            }
        }
        i++;
        vTaskDelay(2000);

    }
    vTaskDelete(NULL);
}