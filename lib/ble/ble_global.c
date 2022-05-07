#include "host/ble_hs.h"
#include "ble_spp.h"
#include "ble_global.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

/**
 * @brief The general client host task
 * @details This is the actual task the host runs in.
 * TODO: Does this have to be sticked to core 0? Set in the config?
 */
void ble_host_task(void *param)
{
    ESP_LOGI("BLE", "BLE Host Task Started");
    // TODO: Set the log tag here name (pass it in params, I think)
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

