/**
 * @file ble_init.c
 * @brief This is the BLE initialization routines
 *
 */

#include "esp_log.h"

#include "host/ble_hs.h"
#include "ble_spp.h" // This needs to be after ble_hs.h

#include "nvs.h"
#include "nvs_flash.h"

#include "esp_nimble_hci.h"
#include "services/gap/ble_svc_gap.h"

#include "ble_client.h"
#include "ble_service.h"
#include "ble_server.h"

#include "ble_global.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "ble_init.h"

/**
 * @brief Initialize the  BLE server
 *
 * @param log_prefix The prefix for logging and naming
 * @param pvTaskFunction A function containing the task to run
 */
void ble_init(const char *log_prefix, bool is_controller)
{

    /**
     * TODO: We have the is_controller param.
     * Figure out what should be the difference between a controller and a peripheral.
     * They should probably both be both server and client.
     * But likely, they should have mostly different API:s.
     * Do they have any common functions? Check-alive? Status?
     * Maybe that is where to start?
     * Remember to make it test driven from now on, should not be hard given the protocol.
     * Create constants for commands. See
     * Figure out if there should be a menuconfig for this?
     * Add network name/ID/UUID (list of clients?) whatever to ensure that only those belonging to the network is connected to.
     */

    char task_tag[50] = "\0";
    strcpy(task_tag, log_prefix);
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

    /* Create mutexes for blocking during BLE operations */
    xBLE_Comm_Semaphore = xSemaphoreCreateMutex();

    /* TODO: Add setting for stack size (it might need to become bigger) */

    /* Initialize the NimBLE host configuration. */

    /* Configure the host callbacks */
    ble_hs_cfg.reset_cb = ble_on_reset;
    if (is_controller)
    {
        ble_hs_cfg.sync_cb = ble_spp_client_on_sync;
    }
    else
    {
        ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
    }

    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    // Not secure connections
    ble_hs_cfg.sm_sc = 0;
    /* Initialize data structures to track connected peers.
    There is a local pool in spp.h */
    ESP_LOGI(task_tag, "Init peer with %i ", MYNEWT_VAL(BLE_MAX_CONNECTIONS));
    ret = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(ret == 0);

    /* Generate and set the GAP device name. */
    char gapname[50] = "\0";
    strcpy(gapname, log_prefix);
    ret = ble_svc_gap_device_name_set(strncat(strlwr(gapname), "-cli", 100));
    assert(ret == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    /* Generate and set the log for the client */
    strupr(strcpy(client_tag, log_prefix));
    strcat(client_tag, "_CENTRAL_CLIENT\0");
    /* Start the thread for the host stack, pass the client task which nimble_port_run */
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(log_prefix, "BLE initialized.");
}
