#include "host/ble_hs.h"
#include "ble_spp.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_crc.h"

#include "ble_service.h"
#include "sdp.h"

#include "ble_global.h"

/**
 * @brief The general client host task
 * @details This is the actual task the host runs in.
 * TODO: Does this have to be sticked to core 0? Set in the config?
 */
void ble_host_task(void *param)
{
    ESP_LOGI(log_prefix, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

/**
 * Called when service discovery of the specified peer has completed.
 */

void ble_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0)
    {
        /* Service discovery failed.  Terminate the connection. */
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
                           "conn_handle=%d\n",
                    status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    MODLOG_DFLT(INFO, "Service discovery complete; status=%d "
                      "conn_handle=%d\n",
                status, peer->conn_handle);
}

int ble_negotiate_mtu(uint16_t conn_handle)
{

    MODLOG_DFLT(INFO, "Negotiate MTU.\n");

    int rc = ble_gattc_exchange_mtu(conn_handle, NULL, NULL);
    if (rc != 0)
    {

        MODLOG_DFLT(ERROR, "Error from exchange_mtu; rc=%d\n", rc);

        return rc;
    }
    MODLOG_DFLT(INFO, "MTU exchange request sent.\n");

    vTaskDelay(10);
    return 0;
}
/**
 * @brief Send a message to one or more peers
 *
 * @param conn_handle A negative value will cause all peers to be messaged
 * @param conversation_id Used to keep track of conversations
 * @param work_type The kind of message
 * @param data A pointer to the data to be sent
 * @param data_length The length of the data in bytes
 * @return int A negative return value will mean a failure of the operation
 * TODO: Handle partial failure, for example if one peripheral doesn't answer.
 */
int ble_broadcast_message(uint16_t conversation_id,
                          enum work_type work_type, const void *data, int data_length)
{
    struct peer *curr_peer;
    int ret = 0, total = 0, errors = 0;
    SLIST_FOREACH(curr_peer, &peers, next)
    {
        ret = ble_send_message(curr_peer->conn_handle, conversation_id, work_type, data, data_length);
        if (ret != 0)
        {
            ESP_LOGE(log_prefix, "Error: ble_broadcast_message: Failure sending message! Peer: %u Code: %i", curr_peer->conn_handle, ret);
            errors++;
        } else {
            ESP_LOGI(log_prefix, "Sent a message to Peer: %u Code: %i", curr_peer->conn_handle, ret);
            
        }

        total++;
    }
    if (errors == total)
    {
        return SDP_ERR_SEND_FAIL;
    }
    else if (errors > 0)
    {
        return SDP_ERR_SEND_SOME_FAIL;
    }
    else
    {
        return SDP_OK;
    }
}

/**
 * @brief Like ble_send_message, but only sends to one specified peer.
 * Note the unsigned type of the connection handle, a positive value is needed.
 */
int ble_send_message(uint16_t conn_handle, uint16_t conversation_id,
                     enum work_type work_type, const void *data, int data_length)
{

    
    if (pdTRUE == xSemaphoreTake(xBLE_Comm_Semaphore, portMAX_DELAY))
    {
        int ret;
        ret = ble_gattc_write_flat(conn_handle, ble_spp_svc_gatt_read_val_handle, data, data_length, NULL, NULL);
        if (ret == 0)
        {
            ESP_LOGI(log_prefix, "ble_send_message: Success sending data! CRC32: %u", (int)esp_crc32_be(0, data, data_length));
        }
        else
        {
            ESP_LOGE(log_prefix, "Error: ble_send_message  - Failure when writing data! Peer: %i Code: %i", conn_handle, ret);
            return -ret;
        }
        xSemaphoreGive(xBLE_Comm_Semaphore);
    }
    else
    {
        ESP_LOGE(log_prefix, "Error: ble_send_message  - Couldn't get semaphore!");
        return SDP_ERR_CONV_QUEUE;
    }
    return 0;
}