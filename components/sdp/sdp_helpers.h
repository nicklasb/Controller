    
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
#include "stdint.h"
    
void cleanup_queue_task(struct work_queue_item *queue_item);
int add_to_message(uint8_t **message, const char *format, ...);


