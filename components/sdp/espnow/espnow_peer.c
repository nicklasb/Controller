

#include <sdkconfig.h>
#ifdef CONFIG_SDP_LOAD_ESP_NOW

#include "espnow_peer.h"

#include <esp_now.h>
#include <esp_log.h>
#include <string.h>
#include <espnow/espnow.h>

char * log_prefix;

esp_now_peer_info_t* espnow_add_peer(sdp_mac_address mac_adress)
{

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(log_prefix, "Malloc peer information fail");
        return ESP_FAIL;
    }

    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, mac_adress, ESP_NOW_ETH_ALEN);
    int rc = esp_now_add_peer(peer);
    if (rc != 0) {
        if (rc == ESP_ERR_ESPNOW_NOT_INIT) {
            ESP_LOGE(log_prefix,"Error adding ESP-NOW-peer: ESPNOW is not initialized");
        } else
        if (rc == ESP_ERR_ESPNOW_ARG) {
            ESP_LOGE(log_prefix,"Error adding ESP-NOW-peer: Invalid argument (bad espnow_peer object?)");
        } else
        if (rc == ESP_ERR_ESPNOW_FULL) {
            ESP_LOGE(log_prefix,"Error adding ESP-NOW-peer: The peer list is full");
        } else
        if (rc == ESP_ERR_ESPNOW_NO_MEM) {
            ESP_LOGE(log_prefix,"Error adding ESP-NOW-peer: Out of memory");
        } else
        if (rc == ESP_ERR_ESPNOW_EXIST) {
            ESP_LOGE(log_prefix,"Error adding ESP-NOW-peer: Peer has existed");
        } 

    }
    return peer;
}

void espnow_peer_init(char * _log_prefix) {
    log_prefix = _log_prefix;
}

#endif