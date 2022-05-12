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

    char client_tag[50];
    /**********************
     * GLOBAL PROTOTYPES
     **********************/
    void ble_spp_client_on_reset(int reason);
    void ble_spp_client_on_sync(void);
    void ble_spp_client_host_task(void *param);
    /**********************
     *      MACROS
     **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
