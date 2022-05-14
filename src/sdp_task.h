#ifdef __cplusplus
extern "C"
{
#endif

   /*********************
    *      DEFINES
    *********************/
    esp_timer_handle_t periodic_timer;
   /*********************
    *      INCLUDES
    *********************/

   /**********************
    *      TYPEDEFS
    **********************/

   /**********************
    * GLOBAL PROTOTYPES
    **********************/

   /* Tasks for handling work items */
   void do_on_priority(struct work_queue_item *queue_item);
   void do_on_work(struct work_queue_item *queue_item);

   /* Filters */
   int do_on_filter_request(struct work_queue_item *queue_item);
   int do_on_filter_data(struct work_queue_item *queue_item);

   /* */
   void prediodic_sensor_query(void* arg);

   /**********************
    *      MACROS
    **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif
