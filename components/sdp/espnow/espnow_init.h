
#ifndef _ESPNOW_INIT_H_
#define _ESPNOW_INIT_H_

#include <stdbool.h>
#include "sdkconfig.h"

#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#endif


void espnow_init(char * _log_prefix);

void espnow_shutdown();

#endif