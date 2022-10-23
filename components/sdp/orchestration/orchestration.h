#ifndef _TIMING_H_
#define  _TIMING_H_

#include "sdp_def.h"

void update_next_availability_window();


void take_control();

void orchestration_init(char * _log_prefix); 

void sleep_until_peer_available(sdp_peer *peer, int margin_us);

//  When/Next messaging

int sdp_orchestration_send_when_message(sdp_peer *peer);
int sdp_orchestration_send_next_message(work_queue_item_t *queue_item);
void sdp_orchestration_parse_next_message(work_queue_item_t *queue_item);

#endif