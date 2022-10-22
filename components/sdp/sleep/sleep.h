#ifndef _SLEEP_H_
#define _SLEEP_H_

#include "stdint.h"

void goto_sleep_for_microseconds(uint64_t microsecs);

void sleep_init(char * _log_prefix); 

#endif