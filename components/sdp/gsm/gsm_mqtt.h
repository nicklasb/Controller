#ifndef _GSM_MQTT_H_
#define _GSM_MQTT_H_

#include "stdbool.h"

int gsm_mqtt_init(char * _log_prefix);
int publish(char * topic, char * payload, int payload_len);
void gsm_mqtt_cleanup();

#endif