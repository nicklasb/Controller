#ifndef _GSM_H_
#define _GSM_H_

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_modem_api.h>

esp_modem_dce_t *gsm_dce;
EventGroupHandle_t gsm_event_group;

static const int CONNECT_BIT = BIT0;
static const int GOT_DATA_BIT = BIT1;
static const int SHUTTING_DOWN_BIT = BIT2;

void gsm_init(char * _log_prefix);

bool gsm_before_sleep_cb();

void gsm_abort_if_shutting_down();

#endif