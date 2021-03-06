

/*********************
 *      INCLUDES
 *********************/



/*********************
 *      DEFINES
 *********************/
SemaphoreHandle_t BLE_Semaphore;

void ble_on_disc_complete(const struct peer *peer, int status, void *arg);
void ble_on_reset(int reason);
int ble_negotiate_mtu(uint16_t conn_handle);
void ble_host_task(void *param);

int ble_broadcast_message(uint16_t conversation_id, 
    enum work_type work_type, const void *data, int data_length);

int ble_send_message(uint16_t conn_handle, uint16_t conversation_id, 
    enum work_type work_type, const void *data, int data_length);


