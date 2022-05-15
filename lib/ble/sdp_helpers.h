    
    /**
 * @file sdp_helpers.h
 * @author Nicklas Borjesson
 * @brief This is the sensor data protocol server helper functions
 * 
 * They provide easy ways to assemble messages, clean up and so on
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifdef __cplusplus
extern "C"
{
#endif
    
void cleanup_queue_task(struct work_queue_item *queue_item);
void add_to_message(uint8_t **message, int *curr_length, char *value);

#ifdef __cplusplus
} /* extern "C" */
#endif
