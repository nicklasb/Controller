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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

    /**********************
     *      TYPEDEFS
     **********************/
    /* Creates a semaphore to handle concurrent call to lvgl stuff
     * If you wish to call *any* lvgl function from other threads/tasks
     * you should lock on the very same semaphore! */
    SemaphoreHandle_t xBLESemaphore;

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void ble_init(void);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
