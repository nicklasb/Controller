#ifndef _SDP_PEER_H_
#define _SDP_PEER_H_

#include "sdkconfig.h"

#include <stdint.h>

#include "sdp_def.h"

/* The host details */
extern sdp_peer sdp_host;

sdp_mac_address *relation_id_to_mac_address(uint32_t relation_id);
bool add_relation(sdp_mac_address mac_address, uint32_t relation_id
    #ifdef CONFIG_SDP_LOAD_I2C
    , uint8_t i2c_address
    #endif
); 

// TODO: Add add_external_relation - to add a relation between to external peers

float add_to_failure_rate_history(struct sdp_peer_media_stats *stats, float rate    );

int sdp_peer_send_hi_message(sdp_peer *peer, bool is_reply);
int sdp_peer_inform(work_queue_item_t *queue_item);

float sdp_helper_calc_suitability(int bitrate, int min_offset, int base_offset, float multiplier);
e_media_type select_media(struct sdp_peer *peer, int data_length);
void sdp_peer_init_peer(sdp_peer * peer);
void sdp_peer_init(char *_log_prefix);

#endif