#ifndef _SLEEP_H_
#define _SLEEP_H_

#include "stdint.h"
#include "stdbool.h"



void goto_sleep_for_microseconds(uint64_t microsecs);

int get_last_sleep_time();
int get_time_since_start();
int get_total_time_awake();
bool is_first_boot();
int get_sleep_count();

bool sleep_init(char * _log_prefix); 

#endif