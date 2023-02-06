#ifndef _LORA_MESSAGING_H_
#define _LORA_MESSAGING_H_

#include "../sdp_def.h"
#include "lora_worker.h"

int lora_send_message(sdp_peer *peer, char *data, int data_length, bool just_checking);
void lora_do_on_work_cb(lora_queue_item_t *work_item);
void lora_do_on_poll_cb(queue_context *q_context);

void lora_messaging_init(char * _log_prefix);

#endif