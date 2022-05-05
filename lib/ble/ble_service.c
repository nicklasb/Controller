/*
 * BLE service handling
 * Loosely based on the ESP-IDF-demo
 */

#include "host/ble_hs.h"
#include "ble_spp.h"
#include "ble_service.h"

static const char *tag = "BLE_CENTRAL_SERVICE";

QueueHandle_t spp_common_uart_queue = NULL;

/* 16 Bit Alert Notification Service UUID */
#define GATT_SVR_SVC_ALERT_UUID 0x1811



/* 16 Bit Alert Notification Service UUID */
#define BLE_SVC_ANS_UUID16 0x1811

/* 16 Bit Alert Notification Service Characteristic UUIDs */
#define BLE_SVC_ANS_CHR_UUID16_SUP_NEW_ALERT_CAT 0x2a47

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16 0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16 0xABF1

int callcount = 0;

/* Callback function for custom service */
static int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ESP_LOGI(tag, "Callback for read");
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // TODO: So this is where data comes IN, how to combine the to directions?
        // ESP_LOGI(tag,"Data received in write event,conn_handle = %x,attr_handle = %x",conn_handle,attr_handle);
        ESP_LOGI(tag, "Payload length: %i, call count %i", ctxt->om->om_len, callcount++);
        ESP_LOGI(tag,"Some text maybe: %i: %s ", ctxt->om->om_len, ctxt->om->om_data);

        break;

    default:
        ESP_LOGI(tag, "\nDefault Callback");
        break;
    }
    return 0;
}

/* Define new custom service */
static const struct ble_gatt_svc_def new_ble_svc_gatt_defs[] = {
    {
        /*** Service: GATT */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_ANS_UUID16),
        .characteristics =
            (struct ble_gatt_chr_def[]){{
                    /* Support new alert category */
                    .uuid = BLE_UUID16_DECLARE(BLE_SVC_ANS_CHR_UUID16_SUP_NEW_ALERT_CAT),
                    .access_cb = ble_svc_gatt_handler,
                    .val_handle = &ble_svc_gatt_read_val_handle,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
                },
                {
                    0, /* No more characteristics */
                }},
    },
    {
        /*** Service: SPP */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_UUID16),
        .characteristics =
            (struct ble_gatt_chr_def[]){{
                        /* Support SPP service */
                        .uuid = BLE_UUID16_DECLARE(BLE_SVC_SPP_CHR_UUID16),
                        .access_cb = ble_svc_gatt_handler,
                        .val_handle = &ble_spp_svc_gatt_read_val_handle,
                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE,
                    },
                    {
                        0, /* No more characteristics */
                    }},
    },
    {
        0, /* No more services. */
    },
};

int gatt_svr_register(void)
{
    int rc = 0;

    rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);

    if (rc != 0)
    {
        return rc;
    }

    rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
    if (rc != 0)
    {
        return rc;
    }

    return 0;
}
