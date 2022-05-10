/*
 * BLE service handling
 * Loosely based on the ESP-IDF-demo
 */

#include "host/ble_hs.h"
#include "ble_spp.h"

#include "esp_crc.h"


#include "sdp.h"
#include "ble_global.h"
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



static int handle_incoming(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(tag, "Payload length: %i, call count %i, CRC32: %u", ctxt->om->om_len, callcount++,
             esp_crc32_be(0, ctxt->om->om_data, ctxt->om->om_len));

    struct work_queue_item *new_item;


    
    if (ctxt->om->om_len > 3)
    {
        // TODO:

        new_item = malloc(sizeof(struct work_queue_item));
        new_item->version = ctxt->om->om_data[0];
        new_item->conversation_id = ctxt->om->om_data[1];
        new_item->work_type = ctxt->om->om_data[2];

        new_item->data_length = ctxt->om->om_len-3;
        new_item->data = malloc(new_item->data_length);
        memcpy(new_item->data,&ctxt->om->om_data[3], new_item->data_length); 

        new_item->conn_handle = conn_handle;

    }
    else
    {
        ESP_LOGI(tag, "ERROR: The request must be more than 3 bytes for SDP compliance.");
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }
    


    // Handle the different request types
    // TODO:Interestingly, on_data_cb seems to initialize to NULL by itself. Or does it?
  
    switch (new_item->work_type)
    {

    case REQUEST:
        /* Add a queue item, make sure we are thread safe */
        safe_add_work_queue(&new_item, tag);
        if (on_request_cb != NULL)
        {
            ESP_LOGI(tag, "BLE service: Calling on_data_callback");

            on_request_cb(*new_item);
        }
        else
        {
            ESP_LOGI(tag, "BLE service ERROR: on_data_callback is not assigned!");
        }
        break;
    case DATA:
        // Put them on the queue
        safe_add_work_queue(&new_item, tag);
        STAILQ_INSERT_HEAD(&work_q, new_item, items);
        if (on_data_cb != NULL)
        {
            ESP_LOGI(tag, "Calling on_ble_data_callback");

            on_data_cb(*new_item);
        }
        else
        {
            ESP_LOGI(tag, "ERROR: on_ble_data_callback is not assigned!");
        }
        break;

    case PRIORITY:
        /* If it is a problem report,
        immidiately respond with CRC32 to tell the
        reporter that the information has reached the controller. */
        ble_send_message(new_item->conn_handle,
                         new_item->conversation_id, DATA, &(new_item->crc32), 2, tag);

        /* Do NOT add the work item to the queue, it will be immidiately adressed in the callback */

        if (on_priority_cb != NULL)
        {
            ESP_LOGI(tag, "BLE Calling on_priority_callback");

            on_priority_cb(*new_item);
        }
        else
        {
            ESP_LOGI(tag, "ERROR: BLE on_priority callback is not assigned!");
        }
        break;

    default:
        break;
    }
    return 0;
}

/**
 * @brief Callback function for custom service
 * 
 */
static int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op)
    {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ESP_LOGI(tag, "Callback for read");
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        handle_incoming(conn_handle, attr_handle, ctxt, arg);

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


