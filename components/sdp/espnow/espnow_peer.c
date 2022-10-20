#include "espnow_peer.h"

#include <esp_now.h>
#include <esp_log.h>
#include <string.h>
#include "espnow_init.h"

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
    ESP_ERROR_CHECK(esp_now_add_peer(peer));

    return peer;
}

void espnow_peer_init(char * _log_prefix) {
    log_prefix = _log_prefix;
}
