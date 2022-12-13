#ifndef _GSM_H_
#define _GSM_H_

#include <sdkconfig.h>
#ifdef CONFIG_SDP_LOAD_UMTS

#include <stdbool.h>

void gsm_init(char * _log_prefix);

bool gsm_shutdown();

void gsm_reset_rtc();


#endif

#endif