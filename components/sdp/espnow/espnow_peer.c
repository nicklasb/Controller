

#include <sdkconfig.h>
#ifdef CONFIG_SDP_LOAD_ESP_NOW

#include "espnow_peer.h"

#include <esp_now.h>
#include <esp_log.h>
#include <string.h>
#include <espnow/espnow.h>
#include "sdp_peer.h"
#include "esp_timer.h"


char * espnow_peer_log_prefix;

uint32_t espnow_unknown_failures = 0;
uint32_t espnow_crc_failures = 0;

void espnow_stat_reset(sdp_peer *peer)
{
    espnow_unknown_failures = 0;
    espnow_crc_failures = 0;
    peer->espnow_stats.send_successes = 0;
    peer->espnow_stats.send_failures = 0;
    peer->espnow_stats.receive_successes = 0;
    peer->espnow_stats.receive_failures = 0;
    peer->espnow_stats.score_count = 0;
}

void espnow_peer_init_peer(sdp_peer *peer)
{
    memset(peer->espnow_stats.failure_rate_history, 0, sizeof(float) * FAILURE_RATE_HISTORY_LENGTH);
    
    espnow_stat_reset(peer);
}

esp_now_peer_info_t* espnow_add_peer(sdp_mac_address mac_adress)
{

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL)
    {
        ESP_LOGE(espnow_peer_log_prefix, "Malloc peer information fail");
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
            ESP_LOGE(espnow_peer_log_prefix,"Error adding ESP-NOW-peer: ESPNOW is not initialized");
        } else
        if (rc == ESP_ERR_ESPNOW_ARG) {
            ESP_LOGE(espnow_peer_log_prefix,"Error adding ESP-NOW-peer: Invalid argument (bad espnow_peer object?)");
        } else
        if (rc == ESP_ERR_ESPNOW_FULL) {
            ESP_LOGE(espnow_peer_log_prefix,"Error adding ESP-NOW-peer: The peer list is full");
        } else
        if (rc == ESP_ERR_ESPNOW_NO_MEM) {
            ESP_LOGE(espnow_peer_log_prefix,"Error adding ESP-NOW-peer: Out of memory");
        } else
        if (rc == ESP_ERR_ESPNOW_EXIST) {
            ESP_LOGE(espnow_peer_log_prefix,"Error adding ESP-NOW-peer: Peer has existed");
        } 

    }
    return peer;
}

/**
 * @brief Returns a connection score for the peer
 *
 * @param peer The peer to analyze
 * @param data_length The length of data to send
 * @return float The score = -100 don't use, +100 use
 */
float espnow_score_peer(sdp_peer *peer, int data_length)
{

    // ESP-NOW is not very fast, but not super-slow.
    // If there are many peers, per
    // Is there a level of usage that is "too much"
    // TODO: Add different demands, like "wired"? "secure", "fast", "roundtrip", or similar?


    // TODO: Obviously, the length score should go down if we are forced to slow down, with a low actual speed.
    float length_score = sdp_helper_calc_suitability(data_length, 55, 1000, 0.0005);
 
    ESP_LOGD(espnow_peer_log_prefix, "peer: %s ss: %i, rs: %i, sf: %i, rf: %i ", peer->name,
             peer->espnow_stats.send_successes, peer->espnow_stats.receive_successes,
             peer->espnow_stats.send_failures, peer->espnow_stats.receive_failures);
    // Success score

    float failure_rate = 0;

    if (peer->espnow_stats.send_successes + peer->espnow_stats.receive_successes > 0)
    {
        // Neutral if failures are lower than 0.01 of tries.
        failure_rate = ((float)peer->espnow_stats.send_failures + (float)peer->espnow_stats.receive_failures) /
                       ((float)peer->espnow_stats.send_successes + (float)peer->espnow_stats.receive_successes);
    }
    else if (peer->espnow_stats.send_failures + peer->espnow_stats.receive_failures > 0)
    {
        // If there are only failures, that causes a 1 as a failure fraction
        failure_rate = 1;
    }
    if (failure_rate > 1)
    {
        failure_rate = 1;
    }

    // A failure fraction of 0.1 - 0. No failures - 25. Anything over 0.5 returns -100.
    float success_score = 10 - (add_to_failure_rate_history(&(peer->espnow_stats), failure_rate) * 100);
    if (success_score < -100)
    {
        success_score = -100;
    }

    float total_score = length_score + success_score;
    if (total_score < -100)
    {
        total_score = -100;
    }
    ESP_LOGI(espnow_peer_log_prefix, "ESP-NOW - Scoring - peer: %s LSCR  : %f FR: %f, SSCR : %f - TSCR  = %f",
        peer->name, length_score, failure_rate, success_score, total_score);

    peer->espnow_stats.last_score = (total_score + peer->espnow_stats.last_score) / 2;
    peer->espnow_stats.last_score_time = esp_timer_get_time();

    espnow_stat_reset(peer);

    return peer->espnow_stats.last_score;
}


void espnow_peer_init(char * _log_prefix) {
    espnow_peer_log_prefix = _log_prefix;
}

#endif