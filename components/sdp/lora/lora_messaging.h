#ifndef _LORA_MESSAGING_H_
#define _LORA_MESSAGING_H_

#include "../sdp_def.h"

int lora_send_message(sdp_mac_address *dest_mac_address, void *data, int data_length);

void lora_messaging_init(char * _log_prefix);

#endif