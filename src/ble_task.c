#include "nvs.h"
#include "ble_task.h"
#include "ble_client.h"

#include "host/ble_hs.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nvs_flash.h"
#include "ble_spp_client.h"
#include "gatt_svr.c"
#include "esp_nimble_hci.h"
#include "ble_service.h"
#include "nimble/nimble_port_freertos.h"



/* 
This is the running task of the client (the central), if connect to the server (actually peripheral) and asks for data.
Either after periodically being woken from a timer signal,
or because a GPIO went high, which is likely to be because of an alarm. 
*/

void ble_client_my_task(void *pvParameters)
{
    char myarray[13] = "datatosend\0";
    int ret;
    ESP_LOGI(task_tag, "My Task: BLE client UART task started\n");
    for (;;)
    {
        vTaskDelay(2000);
        if (pdTRUE == xSemaphoreTake(xBLESemaphore, portMAX_DELAY))
        {
            ret = ble_gattc_write_flat(connection_handle, attribute_handle, &myarray, 13, NULL, NULL);
            if (ret == 0)
            {
                ESP_LOGI(task_tag, "My Task: Write in uart task success!");
            }
            else
            {
                ESP_LOGI(task_tag, "My Task: Error in writing characteristic");
            }
            xSemaphoreGive(xBLESemaphore);
        }
        else
        {
            ESP_LOGI(task_tag, "My Task: Couldn't get semaphore");
        }
    }
    vTaskDelete(NULL);
}

/*
Initialize BLE
*/
void ble_init(const char *log_prefix)
{
    /* TODO: add a TaskFunction_t pvTaskCode-parameter, a taskname, client name (perhaps a suffix?) here,
     *and move this into the BLE lib-folder.
    This doesn't need to be in-scope of adjusted of or by the implementor
    */ 

    strcpy(task_tag,log_prefix);
    strupr(task_tag);
    strcat(task_tag, "_CENTRAL_TASK\0");

    
    ESP_LOGI(task_tag, "Initialising BLE..");

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    /* Initialize the host stack */
    nimble_port_init();
    
    // TODO: Check out if ESP_ERROR_CHECK could't be used.
    // Server as well
    ret = new_gatt_svr_init();
    assert(ret == 0);

    /* Register custom service */
    ret = gatt_svr_register();
    assert(ret == 0);

    /* Use a special semaphore for BLE. 
    TODO: Consider if there instead should be one for Core 0 instead 
     (that might not be possible if not a param here; should a controller init exist?)
    */
    xBLESemaphore = xSemaphoreCreateMutex();


    char taskname[35] = "\0";
    strcpy(taskname, log_prefix);
    strcat(taskname, " BLE main task");
    
    /* Register the client task.
    We are running it on Core 0, or PRO as it is called traditionally (cores are basically the same now) 
    Feels more reasonable to focus on comms on 0 and applications on 1, traditionally called APP */   
    ESP_LOGI(task_tag, "Register the client task. Name: %s", taskname);
    xTaskCreatePinnedToCore(ble_client_my_task, taskname, 8192, NULL, 8, NULL, 0);
    /* TODO: Add setting for stack size (it might need to become bigger) */

    /* Configure the host callbacks */
    ble_hs_cfg.reset_cb = ble_spp_client_on_reset;
    ble_hs_cfg.sync_cb = ble_spp_client_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize data structures to track connected peers. 
    There is a local pool in peer.c */
    ret = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(ret == 0);

    /* Generate and set the GAP device name. */
    char gapname[35] = "\0";
    strcpy(gapname, log_prefix);
    ret = ble_svc_gap_device_name_set(strncat(strlwr(gapname), "-ble-client", 100));
    assert(ret == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    /* Generate and set the log for the client */
    strupr(strcpy(client_tag,log_prefix));
    strcat(client_tag, "_CENTRAL_CLIENT\0");
    /* Start the thread for the host stack, pass the client task which nimble_port_run */
    nimble_port_freertos_init(ble_spp_client_host_task);
}
