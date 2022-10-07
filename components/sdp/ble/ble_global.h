

/*********************
 *      INCLUDES
 *********************/

#include "sdp.h"
#include "ble_spp.h"

/*********************
 *      DEFINES
 *********************/
SemaphoreHandle_t BLE_Semaphore;

void ble_on_disc_complete(const ble_peer *peer, int status, void *arg);
void ble_on_reset(int reason);
int ble_negotiate_mtu(uint16_t conn_handle);
void ble_host_task(void *param);

int ble_broadcast_message(uint16_t conversation_id, 
    e_work_type work_type, const void *data, int data_length);

int ble_send_message(uint16_t conn_handle, uint16_t conversation_id, 
    e_work_type work_type, const void *data, int data_length);


