#ifndef _ESPNOW_PEER_H_
#define _ESPNOW_PEER_H_
#include <esp_now.h>
#include "sdp_def.h"
    
esp_now_peer_info_t* espnow_add_peer(sdp_mac_address mac_adress);
void espnow_peer_init(char * _log_prefix);
#endif