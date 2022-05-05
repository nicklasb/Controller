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
     * GLOBAL PROTOTYPES
     **********************/
    void ble_init(const char *prefix, TaskFunction_t pvTaskCode, bool is_controller);

    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
