#ifdef __cplusplus
extern "C"
{
#endif

    /*********************
     * PRERQUISITES
     * *******************/

#if CONFIG_LV_TOUCH_CONTROLLER == TOUCH_CONTROLLER_NONE
#error "This application needs a touch controller!"
#endif

    /*********************
     * INCLUDES
     * *******************/

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl_helpers.h"
#include "freertos/semphr.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "Controller"
#define LV_TICK_PERIOD_MS 1

    /**********************
     *      TYPEDEFS
     **********************/
    /* Creates a semaphore to handle concurrent call to lvgl stuff
     * If you wish to call *any* lvgl function from other threads/tasks
     * you should lock on the very same semaphore! */
    SemaphoreHandle_t xGuiSemaphore;

    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void ui_init();

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif