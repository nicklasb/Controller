

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

int ble_negotiate_mtu(uint16_t conn_handle);



#ifdef __cplusplus
}
#endif
