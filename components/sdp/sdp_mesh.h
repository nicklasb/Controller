#ifndef _SDP_MESH_H_
#define _SDP_MESH_H_

#include <stdint.h>
#include "sdp_peer.h"

/**
 * @brief Construct the peer list
 * Declare here for convenience, because some loop sdp_peers.
 * TODO: Perhaps this should be hidden. 
 */


int sdp_mesh_delete_peer(uint16_t peer_handle);
int sdp_mesh_peer_add(sdp_peer_name name);
int sdp_mesh_init(char *_log_prefix, int max_peers);

struct sdp_peer *sdp_mesh_find_peer_by_name(const sdp_peer_name name);
struct sdp_peer *sdp_mesh_find_peer_by_handle(__int16_t peer_handle);


sdp_peer *sdp_add_init_new_peer(sdp_peer_name peer_name, const sdp_mac_address mac_address, e_media_type media_type);
struct sdp_peer *sdp_mesh_find_peer_by_base_mac_address(sdp_mac_address mac_address);

#endif