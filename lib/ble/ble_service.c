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




void parse_message(struct work_queue_item *queue_item) {



    /* Check that the data ends with a NULL value to avoid problems (there should always be one there) */
    
    if (queue_item->raw_data[queue_item->raw_data_length -1] != 0) {
        ESP_LOGW(log_prefix, "WARNING: The data doesn't end with a NULL value, setting it forcefully!");
        queue_item->raw_data[queue_item->raw_data_length -1] = 0;
    }
    // Count the parts TODO: Suppose this loop doesn't take long but it would be interesting not having to do it.
    int nullcount = 0;    
    for (int i = 0; i< queue_item->raw_data_length; i++) {
        if (queue_item->raw_data[i] == 0) {
            nullcount++;
        } 
    } 
    queue_item->parts = heap_caps_malloc(nullcount * sizeof(int32_t), MALLOC_CAP_32BIT);
    // The first byte is always the beginning of a part
    queue_item->parts[0] = queue_item->raw_data;      
    int partcount = 0;  
    for (int j = 0; j< queue_item->raw_data_length; j++) {
        if ((queue_item->raw_data[j] == 0) & (j < queue_item->raw_data_length -1)) {
            queue_item->parts[partcount] = &(queue_item->raw_data[j +1]);
            partcount++;
        } 
    }    
    queue_item->partcount = partcount;


}

static int handle_incoming(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(log_prefix, "Payload length: %i, call count %i, CRC32: %u", ctxt->om->om_len, callcount++,
             esp_crc32_be(0, ctxt->om->om_data, ctxt->om->om_len));

    struct work_queue_item *new_item;
    
    if (ctxt->om->om_len > SDP_PREAMBLE_LENGTH)
    {
        // TODO: Change malloc to something more optimized?
        new_item = malloc(sizeof(struct work_queue_item));
        new_item->version = (uint8_t)ctxt->om->om_data[0];
        new_item->conversation_id = (uint16_t)ctxt->om->om_data[1];
        new_item->work_type = (uint8_t)ctxt->om->om_data[3];
        new_item->raw_data_length = ctxt->om->om_len-SDP_PREAMBLE_LENGTH;
        new_item->raw_data = malloc(new_item->raw_data_length);
        memcpy(new_item->raw_data,&(ctxt->om->om_data[SDP_PREAMBLE_LENGTH]), new_item->raw_data_length); 

        new_item->media_type = BLE;
        new_item->conn_handle = conn_handle;
        parse_message(new_item);

        ESP_LOGI(log_prefix, "Message info : Version: %u, Conversation id: %u, Work type: %u, Media type: %u,Data length: %u.", 
        new_item->version, new_item->conversation_id, new_item->work_type, new_item->media_type, new_item->raw_data_length);



    }
    else
    {
        ESP_LOGE(log_prefix, "Error: The request must be more than %i bytes for SDP compliance.", SDP_PREAMBLE_LENGTH);
        return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
    }
    

    // Handle the different request types
    // TODO:Interestingly, on_filter_data_cb seems to initialize to NULL by itself. Or does it?
  
    switch (new_item->work_type)
    {

    case REQUEST:
        if (on_filter_request_cb != NULL)
        {

            if (on_filter_request_cb(new_item) == 0) {
                // Add the request to the work queue
                safe_add_work_queue(new_item);
            } else {
                ESP_LOGE(log_prefix, "BLE service: on_filter_request_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
            
        }
        else
        {
            safe_add_work_queue(new_item);
        }
        break;  
    case DATA:
        if (on_filter_data_cb != NULL) {

            if (on_filter_data_cb(new_item) == 0) {
                // Add the request to the work queue
                safe_add_work_queue(new_item);
            } else {
                ESP_LOGE(log_prefix, "BLE service: on_filter_data_cb returned a nonzero value, request not added to queue!");
                return SDP_ERR_MESSAGE_FILTERED;
            }
            
        }
        else
        {
            safe_add_work_queue(new_item);
        }
        break;

    case PRIORITY:
        /* If it is a problem report,
        immidiately respond with CRC32 to tell the
        reporter that the information has reached the controller. */
        ble_send_message(new_item->conn_handle,
                         new_item->conversation_id, DATA, &(new_item->crc32), 2);

        /* Do NOT add the work item to the queue, it will be immidiately adressed in the callback */

        if (on_priority_cb != NULL)
        {
            ESP_LOGW(log_prefix, "BLE Calling on_priority_callback!");

            on_priority_cb(new_item);
        }
        else
        {
            ESP_LOGE(log_prefix, "ERROR: BLE on_priority callback is not assigned, assigning to normal handling!");
            safe_add_work_queue(new_item);
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
        ESP_LOGI(log_prefix, "Callback for read");
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        handle_incoming(conn_handle, attr_handle, ctxt, arg);

        break;

    default:
        ESP_LOGI(log_prefix, "\nDefault Callback");
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


