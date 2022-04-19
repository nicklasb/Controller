#ifdef __cplusplus
extern "C"
{
#endif

/*********************
 *      DEFINES
 *********************/

/*********************
 *      INCLUDES
 *********************/

#include "ble_init.h"
#include "ble_client.h"

    /**********************
     *      TYPEDEFS
     **********************/
    /* Creates a semaphore to handle concurrent call to lvgl stuff
     * If you wish to call *any* lvgl function from other threads/tasks
     * you should lock on the very same semaphore! */
    SemaphoreHandle_t xBLESemaphore;
    char task_tag[35];
    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void ble_client_my_task(void *pvParameters);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
