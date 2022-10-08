#include "sdp_peer.h"

#include <os/os_mempool.h>
#include <esp_log.h>

#include "sdp.h"

#ifdef CONFIG_SDP_LOAD_BLE
    #include "ble/ble_spp.h"
#endif    


/* */
static void *sdp_peer_mem;
static struct os_mempool sdp_peer_pool;

/* Used for creating new peer handles*/
__uint16_t _peer_handle_incrementor_ = -1;

struct sdp_peer*
sdp_peer_find_name(const sdp_peer_name name)
{
    struct sdp_peer *peer;

    SLIST_FOREACH(peer, &sdp_peers, next)
    {
        if (strcmp(peer->name, name) != 0)
        {
            return peer;
        }
    }

    return NULL;
}

struct sdp_peer*
sdp_peer_find_handle(__int16_t peer_handle)
{
    struct sdp_peer *peer;

    SLIST_FOREACH(peer, &sdp_peers, next)
    {
        if (peer->peer_handle == peer_handle)
        {
            return peer;
        }
    }

    return NULL;
}

int sdp_peer_delete(__uint16_t peer_handle)
{
    struct sdp_peer *peer;
    int rc;

    peer = sdp_peer_find_handle(peer_handle);
    if (peer == NULL)
    {
        return SDP_ERR_PEER_NOT_FOUND;
    }

    #ifdef CONFIG_SDP_LOAD_BLE
        // If connected,remove BLE peer
        if (peer->ble_conn_handle != 0) {
            ble_peer_delete(peer->ble_conn_handle);
        }
    #endif 

    SLIST_REMOVE(&sdp_peers, peer, sdp_peer, next);

    rc = os_memblock_put(&sdp_peer_pool, peer);
    if (rc != 0)
    {
        return SDP_ERR_OS_ERROR;
    }
 

    return 0;
}

int sdp_peer_add(const sdp_peer_name name)
{
    struct sdp_peer *peer;

    /* TODO: Make sure the peer name is unique*/
    peer = sdp_peer_find_name(name);
    if (peer != NULL)
    {
        return SDP_ERR_PEER_EXISTS;
    }

    peer = os_memblock_get(&sdp_peer_pool);
    if (peer == NULL)
    {
        /* Out of memory. */
        return SDP_ERR_OUT_OF_MEMORY;
    }
    memset(peer, 0, sizeof *peer);
    peer->peer_handle = _peer_handle_incrementor_++;
    strcpy(peer->name, name);

    SLIST_INSERT_HEAD(&sdp_peers, peer, next);

    return peer->peer_handle;
}

static void
sdp_peer_free_mem(void)
{
    free(sdp_peer_mem);
    sdp_peer_mem = NULL;

}

int sdp_peer_init(int max_peers)
{
    int rc;



    /* Free memory first in case this function gets called more than once. */
    sdp_peer_free_mem();

    sdp_peer_mem = malloc(
    OS_MEMPOOL_BYTES(max_peers, sizeof(struct sdp_peer)));
    if (sdp_peer_mem == NULL)
    {
        rc = SDP_ERR_OUT_OF_MEMORY;
        goto err;
    }



    rc = os_mempool_init(&sdp_peer_pool, max_peers,
                         sizeof(struct sdp_peer), sdp_peer_mem,
                         "peer_pool");
    if (rc != 0)
    {
        rc = SDP_ERR_OS_ERROR;
        goto err;
    }

    return 0;

err:
    sdp_peer_free_mem();
    return rc;
}
