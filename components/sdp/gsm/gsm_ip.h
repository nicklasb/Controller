#ifndef _GSM_IP_H_
#define _GSM_IP_H_

#include "stdbool.h"
#include "esp_netif.h"

extern esp_netif_t *gsm_ip_esp_netif;

void gsm_ip_init(char * _log_prefix);
int gsm_ip_enable_data_mode();
void gsm_ip_cleanup();

#endif