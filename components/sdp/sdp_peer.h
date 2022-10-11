#ifndef _SDP_PEER_H_
#define _SDP_PEER_H_

#include <sys/queue.h>
#include "sdkconfig.h"
#include "stdint.h"

#include "sdp_def.h"


typedef char sdp_peer_name[CONFIG_SDP_PEER_NAME_LEN];

typedef struct sdp_peer
{
    SLIST_ENTRY(sdp_peer) next;

    /* The unique handle of the peer*/
    uint16_t peer_handle;
     /* The name of the peer*/
    sdp_peer_name name;   
    /* Eight bits of the media types*/
    uint8_t supported_media_types;
    /* Last time heard from the peer*/
    uint64_t last_time_in;
    /* Last time we tried to contact the  peer*/
    uint64_t last_time_out;    
    /* The peer state, if unknown, it cannot be used in many situations*/
    e_peer_state state;

    /* Protocol version*/
    uint8_t protocol_version;
    /* Minimum supported protocol version*/
    uint8_t min_protocol_version;
    

#ifdef CONFIG_SDP_LOAD_BLE
    /* The connection handle of the BLE connection*/
    int ble_conn_handle;
#endif

} sdp_peer;

SLIST_HEAD(, sdp_peer)
sdp_peers;

int sdp_peer_delete(uint16_t peer_handle);
int sdp_peer_add(sdp_peer_name name);
int sdp_peer_init(char *_log_prefix, int max_peers);



struct sdp_peer *sdp_peer_find_name(const sdp_peer_name name);
struct sdp_peer *sdp_peer_find_handle(__int16_t peer_handle);

#endif