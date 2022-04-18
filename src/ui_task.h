#ifdef __cplusplus
extern "C"
{
#endif


    /*********************
     * INCLUDES
     * *******************/

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "lvgl_helpers.h"
#include "freertos/semphr.h"
    
    /*********************
     * PRERQUISITES
     * *******************/

#if CONFIG_LV_TOUCH_CONTROLLER == TOUCH_CONTROLLER_NONE
    #error "This application needs a touch controller!"
#endif



/*********************
 *      DEFINES
 *********************/
char ui_tag[35];

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
    void ui_init(const char *log_prefix);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif