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


    /**********************
     *      TYPEDEFS
     **********************/
    /* Creates a semaphore to handle concurrent call to BLE stuff */
    
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
