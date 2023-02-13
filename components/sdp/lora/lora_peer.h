#define _LORA_PEER_H_
#ifdef _LORA_PEER_H_

#include "sdp_peer.h"

void lora_stat_reset(sdp_peer *peer);
void lora_peer_init_peer(sdp_peer *peer);

float lora_score_peer(sdp_peer *peer, int data_length);

void lora_peer_init(char * _log_prefix);

#endif