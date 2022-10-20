#include "sdp_mesh.h"

#include <os/os_mempool.h>
#include <esp_log.h>

#include "sdp.h"
#include "sdp_peer.h"
#include "sdkconfig.h"

#ifdef CONFIG_SDP_LOAD_BLE
    #include "ble/ble_spp.h"
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
    #include "espnow/espnow_peer.h"
#endif



/* */
static void *sdp_peer_mem;
static struct os_mempool sdp_peer_pool;

/* Used for creating new peer handles*/
uint16_t _peer_handle_incrementor_ = 0;

/* The log prefix for all logging */
char *log_prefix;

struct sdp_peer *
sdp_mesh_find_peer_by_name(const sdp_peer_name name)
{
    struct sdp_peer *peer;

    SLIST_FOREACH(peer, &sdp_peers, next)
    {
        ESP_LOGI(log_prefix, "Comparing %s with %s.", peer->name, name);
        if (strcmp(peer->name, name) == 0)
        {
            return peer;
        }
    }

    return NULL;
}

struct sdp_peer *
sdp_mesh_find_peer_by_handle(__int16_t peer_handle)
{
    struct sdp_peer *peer;

    SLIST_FOREACH(peer, &sdp_peers, next)
    {
        ESP_LOGI(log_prefix, "sdp_peer_find_handle %i, %i ", peer->peer_handle, peer_handle);
        if (peer->peer_handle == peer_handle)
        {
            return peer;
        }
    }

    return NULL;
}

struct sdp_peer *
sdp_mesh_find_peer_by_base_mac_address(sdp_mac_address mac_address)
{
    struct sdp_peer *peer;

    SLIST_FOREACH(peer, &sdp_peers, next)
    {
        ESP_LOGI(log_prefix, "sdp_mesh_find_peer_by_base_mac_address (peer->base..., mac_address):");
        ESP_LOG_BUFFER_HEX(log_prefix, peer->base_mac_address, SDP_MAC_ADDR_LEN);
        ESP_LOG_BUFFER_HEX(log_prefix, mac_address, SDP_MAC_ADDR_LEN);
        if (memcmp(peer->base_mac_address, mac_address, SDP_MAC_ADDR_LEN) == 0) {
            return peer;
        }

    }

    return NULL;
}


int sdp_mesh_delete_peer(uint16_t peer_handle)
{
    struct sdp_peer *peer;
    int rc;

    peer = sdp_mesh_find_peer_by_handle(peer_handle);
    if (peer == NULL)
    {
        return SDP_ERR_PEER_NOT_FOUND;
    }

#ifdef CONFIG_SDP_LOAD_BLE
    // If connected,remove BLE peer
    if (peer->ble_conn_handle != 0)
    {
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

int sdp_mesh_peer_add(sdp_peer_name name)
{
    struct sdp_peer *peer;

    ESP_LOGI(log_prefix, "sdp_peer_add() - adding SDP peer, name: %s", name);


    /* TODO: Make sure the peer name is unique*/
    peer = sdp_mesh_find_peer_by_name(name);
    if (peer != NULL)
    {
        return -SDP_ERR_PEER_EXISTS;
    }

    peer = os_memblock_get(&sdp_peer_pool);
    if (peer == NULL)
    {
        ESP_LOGE(log_prefix, "sdp_peer_add() - Out of memory!");
        /* Out of memory. */
        return SDP_ERR_OUT_OF_MEMORY;
    }
    memset(peer, 0, sizeof *peer);
    peer->peer_handle = _peer_handle_incrementor_++;
    strcpy(peer->name, name);

    SLIST_INSERT_HEAD(&sdp_peers, peer, next);

    ESP_LOGI(log_prefix, "sdp_peer_add() - Peer added - asking for more information: %s", peer->name);


    return peer->peer_handle;
}

static void
sdp_peer_free_mem(void)
{
    free(sdp_peer_mem);
    sdp_peer_mem = NULL;
}


/**
 * @brief Convenience function to add new peers, usually at startup
 * 
 * @param peer_name 
 * @param mac_address 
 * @return sdp_peer* 
 */
sdp_peer* sdp_add_init_new_peer(sdp_peer_name peer_name, const sdp_mac_address mac_address, e_media_type media_type) {

    int peer_handle = sdp_mesh_peer_add(peer_name);
    sdp_peer* peer = sdp_mesh_find_peer_by_handle(peer_handle);
    if (peer != NULL) {
        memcpy(peer->base_mac_address, mac_address,SDP_MAC_ADDR_LEN);

#ifdef CONFIG_SDP_LOAD_BLE
        if (media_type & SDP_MT_BLE) {
        //(mac_address);       
        }
        //TODO: We might 
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
        if (media_type & SDP_MT_ESPNOW) {
            ESP_LOGE(log_prefix, "Adding espnow peer at:"); 
            ESP_LOG_BUFFER_HEX(log_prefix, peer->base_mac_address, SDP_MAC_ADDR_LEN);
            int rc = espnow_add_peer(peer->base_mac_address);  
            
        }
#endif
        // We know too little about the peer; ask for more information.
        if (sdp_peer_send_who_message(peer) == SDP_MT_NONE)
        {
            ESP_LOGE(log_prefix, "sdp_peer_add() - Failed to ask for more information: %s", peer_name);
        }
    } else {
        ESP_LOGE(log_prefix, "Failed to add the %s", peer_name);
    }



    return peer;
}


int sdp_mesh_init(char *_log_prefix, int max_peers)
{
    int rc;
    log_prefix = _log_prefix;

    /* Free memory first in case this function gets called more than once. */
    sdp_peer_free_mem();

    sdp_peer_mem = malloc(
        OS_MEMPOOL_BYTES(max_peers, sizeof(struct sdp_peer)));
    if (sdp_peer_mem == NULL)
    {
        rc = SDP_ERR_OUT_OF_MEMORY;
        ESP_LOGE(log_prefix, "sdp_peer_init() - Out of memory!");
        goto err;
    }

    rc = os_mempool_init(&sdp_peer_pool, max_peers,
                         sizeof(struct sdp_peer), sdp_peer_mem,
                         "sdp_peer_pool");
    if (rc != 0)
    {
        rc = SDP_ERR_OS_ERROR;
        goto err;
    }

    sdp_peer_init(log_prefix);

    return 0;

err:
    sdp_peer_free_mem();
    return rc;
}
