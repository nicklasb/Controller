#ifndef _i2c_MESSAGING_H_
#define _i2c_MESSAGING_H_

#include "../sdp_def.h"
#include "i2c_worker.h"



int i2c_send_message(sdp_peer *peer, char *data, int data_length, bool just_checking);


void i2c_do_on_work_cb(i2c_queue_item_t *work_item);
void i2c_do_on_poll_cb(queue_context *q_context);

void i2c_messaging_init(char * _log_prefix);

#endif