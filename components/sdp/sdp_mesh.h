#ifndef _SDP_MESH_H_
#define _SDP_MESH_H_

#include <sdkconfig.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sdp_peer.h"



/**
 * @brief Construct the peer list
 * Declare here for convenience, because some loop sdp_peers.
 * TODO: Perhaps this should be hidden. 
 */

SLIST_HEAD(sdp_peers_t, sdp_peer);

int sdp_mesh_delete_peer(uint16_t peer_handle);
int sdp_mesh_peer_add(sdp_peer_name name);

int sdp_mesh_init(char *_log_prefix);

struct sdp_peers_t * get_peer_list();

struct sdp_peer *sdp_mesh_find_peer_by_name(const sdp_peer_name name);
struct sdp_peer *sdp_mesh_find_peer_by_handle(__int16_t peer_handle);
struct sdp_peer *sdp_mesh_find_peer_by_i2c_address(uint8_t i2c_address);

sdp_peer *add_peer_by_mac_address(sdp_peer_name peer_name, const sdp_mac_address mac_address, e_media_type media_type);
#ifdef CONFIG_SDP_LOAD_I2C
sdp_peer *add_peer_by_i2c_address(sdp_peer_name peer_name, uint8_t i2c_address);
sdp_peer *sdp_add_init_new_peer_i2c(sdp_peer_name peer_name, const uint8_t i2c_address);
#endif
void init_supported_media_types(sdp_peer *peer);
sdp_peer *sdp_add_init_new_peer(sdp_peer_name peer_name, const sdp_mac_address mac_address, e_media_type media_type);
struct sdp_peer *sdp_mesh_find_peer_by_base_mac_address(sdp_mac_address mac_address);

#endif