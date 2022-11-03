
#ifndef _GSM_INIT_H_
#define _GSM_INIT_H_

#include "stdbool.h"

void gsm_init(char * _log_prefix);

bool gsm_before_sleep_cb();

#endif