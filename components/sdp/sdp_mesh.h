#ifndef _SDP_MESH_H_
#define _SDP_MESH_H_

#include <stdint.h>
#include "sdp_peer.h"


SLIST_HEAD(, sdp_peer)
sdp_peers;

int sdp_peer_delete(uint16_t peer_handle);
int sdp_peer_add(sdp_peer_name name);
int sdp_peer_init(char *_log_prefix, int max_peers);



struct sdp_peer *sdp_peer_find_name(const sdp_peer_name name);
struct sdp_peer *sdp_peer_find_handle(__int16_t peer_handle);

#endif