#ifndef _SDP_MESH_H_
#define _SDP_MESH_H_

#include <stdint.h>
#include "sdp_peer.h"

/**
 * @brief Construct the peer list
 * Declare here for convenience, because some loop sdp_peers.
 * TODO: Perhaps this should be hidden. 
 */
SLIST_HEAD(, sdp_peer)
sdp_peers;

int sdp_mesh_delete_peer(uint16_t peer_handle);
int sdp_mesh_peer_add(sdp_peer_name name);
int sdp_mesh_init(char *_log_prefix, int max_peers);

struct sdp_peer *sdp_mesh_find_peer_by_name(const sdp_peer_name name);
struct sdp_peer *sdp_mesh_find_peer_by_handle(__int16_t peer_handle);

#endif