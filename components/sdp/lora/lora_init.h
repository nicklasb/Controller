#include "sdkconfig.h"
#ifdef CONFIG_SDP_LOAD_LORA

#ifndef _LORA_INIT_H_
#define _LORA_INIT_H_

#include <stdbool.h>


void lora_init(char * _log_prefix);

void lora_shutdown();

#endif
#endif