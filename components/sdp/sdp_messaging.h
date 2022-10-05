
#ifndef _SDP_SERVICE_H_
#define _SDP_SERVICE_H_

#include "sdp.h"

void parse_message(struct work_queue_item *queue_item);

int handle_incoming(uint16_t conn_handle, uint16_t attr_handle, char* data, int data_len, e_media_type media_type, void *arg);

int broadcast_message(uint16_t conversation_id,
                          enum e_work_type work_type, const void *data, int data_length);

int send_message(uint16_t conn_handle, uint16_t conversation_id,
                     enum e_work_type work_type, const void *data, int data_length);

#endif