#include "sdp_mesh.h"

#include <string.h>
#include <esp_log.h>

#include "sdp_peer.h"
#include "sdkconfig.h"



#ifdef CONFIG_SDP_LOAD_BLE
#include "ble/ble_spp.h"
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
#include "espnow/espnow_peer.h"
#endif

/* Used for creating new peer handles*/
uint16_t _peer_handle_incrementor_ = 0;
struct sdp_peers_t sdp_peers;

/* The log prefix for all logging */
char *mesh_log_prefix;

struct sdp_peer *
sdp_mesh_find_peer_by_name(const sdp_peer_name name)
{
    struct sdp_peer *peer;

    SLIST_FOREACH(peer, &sdp_peers, next)
    {
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
        ESP_LOGI(mesh_log_prefix, "sdp_peer_find_handle %i, %i ", peer->peer_handle, peer_handle);
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
        
        if (memcmp(peer->base_mac_address, mac_address, SDP_MAC_ADDR_LEN) == 0)
        {   
        ESP_LOGI(mesh_log_prefix, "sdp_mesh_find_peer_by_base_mac_address found:");
            ESP_LOG_BUFFER_HEX(mesh_log_prefix, mac_address, SDP_MAC_ADDR_LEN);           
            return peer;
        }
    }
    ESP_LOGI(mesh_log_prefix, "sdp_mesh_find_peer_by_base_mac_address NOT found:");
    ESP_LOG_BUFFER_HEX(mesh_log_prefix, mac_address, SDP_MAC_ADDR_LEN);
    return NULL;
}

#ifdef CONFIG_SDP_LOAD_I2C
struct sdp_peer *
sdp_mesh_find_peer_by_i2c_address(uint8_t i2c_address)
{
    struct sdp_peer *peer;

    ESP_LOGI(mesh_log_prefix, "sdp_mesh_find_peer_by_i2c_address: %hhu", i2c_address);

    SLIST_FOREACH(peer, &sdp_peers, next)
    {

        if (peer->i2c_address == i2c_address)
        {
            return peer;
        }
    }

    return NULL;
}
#endif
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

    free(peer);

    return 0;
}

int sdp_mesh_peer_add(sdp_peer_name name)
{
    // TODO: Why not return the peer? Why use handles at all? Some
    struct sdp_peer *peer;

    ESP_LOGI(mesh_log_prefix, "sdp_mesh_peer_add() - adding SDP peer, name: %s", name);

    /* TODO: Make sure the peer name is unique*/
    peer = sdp_mesh_find_peer_by_name(name);
    if (peer != NULL)
    {
        return -SDP_ERR_PEER_EXISTS;
    }

    peer = malloc(sizeof(sdp_peer));
    if (peer == NULL)
    {
        ESP_LOGE(mesh_log_prefix, "sdp_mesh_peer_add() - Out of memory!");
        /* Out of memory. */
        return SDP_ERR_OUT_OF_MEMORY;
    }
    memset(peer, 0, sizeof *peer);
    peer->peer_handle = _peer_handle_incrementor_++;
    peer->next_availability = 0;
    peer->relation_id = 0;
    peer->state = PEER_UNKNOWN;
    strcpy(peer->name, name);

    sdp_peer_init_peer(peer);

    SLIST_INSERT_HEAD(&sdp_peers, peer, next);

    ESP_LOGI(mesh_log_prefix, "sdp_mesh_peer_add() - Peer added: %s", peer->name);

    return peer->peer_handle;
}

void init_supported_media_types(sdp_peer *peer) {


#ifdef CONFIG_SDP_LOAD_BLE
    if (peer->supported_media_types & SDP_MT_BLE)
    {
        //(mac_address);
        // TODO: Usually BLE sort of finds each other, however we might wan't to do something here
    }
    
#endif

#ifdef CONFIG_SDP_LOAD_ESP_NOW
    if ((peer->supported_media_types & SDP_MT_ESPNOW) && (!peer->espnow_peer_added))
    {
        ESP_LOGI(mesh_log_prefix, "Adding espnow peer at:");
        ESP_LOG_BUFFER_HEX(mesh_log_prefix, peer->base_mac_address, SDP_MAC_ADDR_LEN);
        int rc = espnow_add_peer(peer->base_mac_address);
        peer->espnow_peer_added = true;
    }
#endif

#ifdef CONFIG_SDP_LOAD_LORA
    if (peer->supported_media_types & SDP_MT_LoRa)
    {
    }
#endif

}


/**
 * @brief Add and initialize a new peer (does not contact it, see add_peer for that)
 *
 * @param peer_name The peer name
 * @param mac_address The mac adress
 * @return sdp_peer* An initialized peer
 */
sdp_peer *sdp_add_init_new_peer(sdp_peer_name peer_name, const sdp_mac_address mac_address, e_media_type media_type)
{

    int peer_handle = sdp_mesh_peer_add(peer_name);
    sdp_peer *peer = sdp_mesh_find_peer_by_handle(peer_handle);
    if (peer != NULL)
    {
        memcpy(peer->base_mac_address, mac_address, SDP_MAC_ADDR_LEN);
        peer->supported_media_types = media_type;
        init_supported_media_types(peer);

    }
    else
    {
        ESP_LOGE(mesh_log_prefix, "Failed to add the %s", peer_name);
    }

    return peer;
}

/**
 * @brief Adds a new peer, locates it via its mac address, and contacts it and exchanges information
 *
 * @param peer_name The name of the peer, if we want to call it something
 * @param mac_address The mac_address of the peer
 * @param media_type The way we want to contact the peer
 * @return Returns a pointer to the peer
 */
sdp_peer *add_peer_by_mac_address(sdp_peer_name peer_name, const sdp_mac_address mac_address, e_media_type media_type)
{
    sdp_peer *peer = sdp_add_init_new_peer(peer_name, mac_address, media_type);
    sdp_peer_send_hi_message(peer, false);
    return peer;
}

#ifdef CONFIG_SDP_LOAD_I2C


/**
 * @brief Add and initialize a new peer (does not contact it, see add_peer for that)
 *
 * @param peer_name The peer name
 * @param i2c_address The i2c adress
 * @return sdp_peer* An initialized peer
 */
sdp_peer *sdp_add_init_new_peer_i2c(sdp_peer_name peer_name, const uint8_t i2c_address)
{

    int peer_handle = sdp_mesh_peer_add(peer_name);
    sdp_peer *peer = sdp_mesh_find_peer_by_handle(peer_handle);
    if (peer != NULL)
    {
        peer->i2c_address = i2c_address;
        peer->supported_media_types = SDP_MT_I2C;
    }
    else
    {
        ESP_LOGE(mesh_log_prefix, "Failed to add the %s", peer_name);
        return NULL;
    }

    return peer;
}
/**
 * @brief Adds a new peer, contacts it using I2C and its I2C address it and exchanges information
 * @note To find I2C peers, one has to either loop all 256 addresses or *know* the address, therefor one cannot go by macaddres.
 * Howerver, it could be done, and here is a question on how the network should work in general. As it is also routing..
 * @param peer_name The name of the peer, if we want to call it something
 * @param i2c_address The I2C address of the peer
 * @return Returns a pointer to the peer
 */
sdp_peer *add_peer_by_i2c_address(sdp_peer_name peer_name, uint8_t i2c_address)
{
    sdp_peer *peer = sdp_add_init_new_peer_i2c(peer_name, i2c_address);
    sdp_peer_send_hi_message(peer, false);
    return peer;
}

#endif

struct sdp_peers_t * get_peer_list() {
    return &sdp_peers;
}

int sdp_mesh_init(char *_log_prefix)
{

    mesh_log_prefix = _log_prefix;

    /* Free memory first in case this function gets called more than once. */

    sdp_peer_init(mesh_log_prefix);

    return 0;
}
