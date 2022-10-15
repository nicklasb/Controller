#ifndef _SDP_PEER_H_
#define _SDP_PEER_H_

#include <stdint.h>

#include "sdp_def.h"

int sdp_peer_send_me_message(work_queue_item_t *queue_item);
int sdp_peer_send_who_message(sdp_peer *peer);
int sdp_peer_inform(work_queue_item_t *queue_item);
void sdp_peer_init(char *_log_prefix);

#endif