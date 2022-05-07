

/*********************
 *      INCLUDES
 *********************/


#include "freertos/semphr.h"


#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
SemaphoreHandle_t BLE_Semaphore;

void ble_on_disc_complete(const struct peer *peer, int status, void *arg);
void ble_on_reset(int reason);
int ble_negotiate_mtu(uint16_t conn_handle);
void ble_host_task(void *param);


#ifdef __cplusplus
}
#endif
