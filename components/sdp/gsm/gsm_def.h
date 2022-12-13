#ifndef _GSM_DEF_H_
#define _GSM_DEF_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_modem_api.h>

extern esp_modem_dce_t *gsm_dce;
extern EventGroupHandle_t gsm_event_group;

static const int GSM_CONNECT_BIT = BIT0;
static const int GSM_GOT_DATA_BIT = BIT1;
static const int GSM_SHUTTING_DOWN_BIT = BIT2;

#endif
