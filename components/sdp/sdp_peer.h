#ifndef _SDP_PEER_H_
#define _SDP_PEER_H_

#include <sys/queue.h>
#include <sdkconfig.h>

typedef char peer_name[CONFIG_SDP_PEER_NAME_LEN];

typedef struct sdp_peer
{
    SLIST_ENTRY(sdp_peer) next;

    __uint16_t peer_handle;
    peer_name name;

} sdp_peer;

SLIST_HEAD(, sdp_peer) sdp_peers;

int ble_peer_delete(__uint16_t peer_handle);
int peer_add(__uint16_t conn_handle, const peer_name name);
int peer_init(int max_peers);

struct sdp_peer *peer_find(__uint16_t peer_handle);


#endif