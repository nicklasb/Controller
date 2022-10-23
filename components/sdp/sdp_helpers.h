    
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

#ifndef _SDP_HELPERS_H_
#define _SDP_HELPERS_H_

#include "stdint.h"
#include "sdp_def.h"



int add_to_message(uint8_t **message, const char *format, ...);

void *sdp_add_preamble(e_work_type work_type, uint16_t conversation_id, const void *data, int data_length);

void sdp_helpers_init(char * _log_prefix);


#endif
