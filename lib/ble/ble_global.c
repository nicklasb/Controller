#include "host/ble_hs.h"
#include "ble_spp.h"
#include "ble_global.h"


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
