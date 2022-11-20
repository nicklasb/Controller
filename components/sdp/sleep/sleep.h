#ifndef _SLEEP_H_
#define _SLEEP_H_

#include "stdint.h"
#include "stdbool.h"



void goto_sleep_for_microseconds(uint64_t microsecs);

uint64_t get_last_sleep_time();
uint64_t get_time_since_start();
uint64_t get_total_time_awake();
bool is_first_boot();
int get_sleep_count();

bool sleep_init(char * _log_prefix); 

#endif