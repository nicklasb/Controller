#include "sdp_peer.h"
#include "sdp.h"
#include <os/os_mempool.h>

static void *peer_mem;
static struct os_mempool sdp_peer_pool;

struct sdp_peer *
peer_find(__uint16_t peer_handle)
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


int peer_delete(__uint16_t peer_handle)
{
    struct sdp_peer *peer;
    int rc;

    peer = peer_find(peer_handle);
    if (peer == NULL)
    {
        return SDP_ERR_PEER_NOT_FOUND;
    }

    SLIST_REMOVE(&sdp_peers, peer, sdp_peer, next);


    rc = os_memblock_put(&sdp_peer_pool, peer);
    if (rc != 0)
    {
        return SDP_ERR_OS_ERROR;
    }

    return 0;
}

int peer_add(__uint16_t peer_handle, const peer_name name)
{
    struct sdp_peer *peer;

    /* Make sure the connection handle is unique. */
    peer = peer_find(peer_handle);
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
    peer->peer_handle = peer_handle;
    memcpy(&peer->name, &name, sizeof name);

    SLIST_INSERT_HEAD(&sdp_peers, peer, next);

    return 0;
}

static void
peer_free_mem(void)
{
    free(peer_mem);
    peer_mem = NULL;

}

int peer_init(int max_peers)
{
    int rc;

    /* Free memory first in case this function gets called more than once. */
    peer_free_mem();

    peer_mem = malloc(
    OS_MEMPOOL_BYTES(max_peers, sizeof(struct sdp_peer)));
    if (peer_mem == NULL)
    {
        rc = SDP_ERR_OUT_OF_MEMORY;
        goto err;
    }

    rc = os_mempool_init(&sdp_peer_pool, max_peers,
                         sizeof(struct sdp_peer), peer_mem,
                         "peer_pool");
    if (rc != 0)
    {
        rc = SDP_ERR_OS_ERROR;
        goto err;
    }


    return 0;

err:
    peer_free_mem();
    return rc;
}
