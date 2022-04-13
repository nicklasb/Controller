
#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      DEFINES
 *********************/
/* Include lv_conf.h from the search path (it is in the lib/ directory) */
#define LV_LVGL_H_INCLUDE_SIMPLE 1

/*********************
 *      INCLUDES
 *********************/

//#include "lv_conf.h"
#include "lvgl.h"
#include "lv_core/lv_obj.h"

/**********************
 *      TYPEDEFS
 **********************/
lv_obj_t * vberth;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void build_ui(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
