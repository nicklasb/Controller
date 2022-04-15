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

static const char *tag = "BLE_CENTRAL_TASK";

void ble_client_my_task(void *pvParameters)
{
    char myarray[13] = "anyfukingdat\0";
    int rc;
    ESP_LOGI(tag, "My Task: BLE client UART task started\n");
    for (;;)
    {
        vTaskDelay(2000);
        if (pdTRUE == xSemaphoreTake(xBLESemaphore, portMAX_DELAY))
        {
            rc = ble_gattc_write_flat(connection_handle, attribute_handle, &myarray, 13, NULL, NULL);
            if (rc == 0)
            {
                ESP_LOGI(tag, "My Task: Write in uart task success!");
            }
            else
            {
                ESP_LOGI(tag, "My Task: Error in writing characteristic");
            }
            xSemaphoreGive(xBLESemaphore);
        }
        else
        {
            ESP_LOGI(tag, "My Task: Couldn't get semaphore");
        }
    }
    vTaskDelete(NULL);
}

void ble_init(void)
{
    int rc;
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());

    nimble_port_init();

    // Server as well
    rc = new_gatt_svr_init();
    assert(rc == 0);

    /* Register custom service */
    rc = gatt_svr_register();
    assert(rc == 0);

    xBLESemaphore = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(ble_client_my_task, "myTask", 8192, NULL, 8, NULL, 0);

    /* Configure the host. */
    ble_hs_cfg.reset_cb = ble_spp_client_on_reset;
    ble_hs_cfg.sync_cb = ble_spp_client_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-ble-spp-client");
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(ble_spp_client_host_task);
}
