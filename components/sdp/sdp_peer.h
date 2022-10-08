#ifndef _SDP_PEER_H_
#define _SDP_PEER_H_

#include <sys/queue.h>
#include <sdkconfig.h>

typedef char sdp_peer_name[CONFIG_SDP_PEER_NAME_LEN];

typedef struct sdp_peer
{
    SLIST_ENTRY(sdp_peer) next;

    __uint16_t peer_handle;
    sdp_peer_name name;

    #ifdef CONFIG_SDP_LOAD_BLE
    int ble_conn_handle;
    #endif    

} sdp_peer;

SLIST_HEAD(, sdp_peer) sdp_peers;

int sdp_peer_delete(__uint16_t peer_handle);
int sdp_peer_add(sdp_peer_name name);
int sdp_peer_init(char *_log_prefix, int max_peers);

struct sdp_peer *sdp_peer_find_name(const sdp_peer_name name);
struct sdp_peer *sdp_peer_find_handle(__int16_t peer_handle);

#endif