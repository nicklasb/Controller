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

#include "sdp_def.h"
#include "driver/gpio.h"

void sdp_blink_led(gpio_num_t gpio_num, uint16_t time_on, uint16_t time_off, uint16_t times);

int add_to_message(uint8_t **message, const char *format, ...);

void *sdp_add_preamble(e_work_type work_type, uint16_t conversation_id, const void *data, int data_length);

void log_peer_info(char * _log_prefix, sdp_peer *peer);

uint32_t calc_relation_id(sdp_mac_address *mac_1, sdp_mac_address *mac_2);

float sdp_read_battery();

void sdp_helpers_init(char * _log_prefix);

#endif
