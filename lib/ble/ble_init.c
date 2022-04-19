/**
 * @file ble_init.c
 * @brief This is the BLE initialization routines
 * 
 */

#include "ble_init.h"
#include "ble_client.h"
#include "ble_service.h"

#include "esp_log.h"

#include "host/ble_hs.h"
#include "ble_spp_client.h" // This needs to be after ble_hs.h

#include "nvs.h"  
#include "nvs_flash.h"

#include "gatt_svr.c"
#include "esp_nimble_hci.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"


/**
 * @brief Initialize the  BLE server
 * 
 * @param log_prefix The prefix for logging and naming
 * @param pvTaskFunction A function containing the task to run
 */
void ble_init(const char *log_prefix, TaskFunction_t pvTaskFunction)
{

    char task_tag[35] = "\0";
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

    char taskname[35] = "\0";
    strcpy(taskname, log_prefix);
    strcat(taskname, " BLE main task");
    
    /* Register the client task.
    We are running it on Core 0, or PRO as it is called traditionally (cores are basically the same now) 
    Feels more reasonable to focus on comms on 0 and applications on 1, traditionally called APP */   
    ESP_LOGI(task_tag, "Register the client task. Name: %s", taskname);
    xTaskCreatePinnedToCore(pvTaskFunction, taskname, 8192, NULL, 8, NULL, 0);
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
