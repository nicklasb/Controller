/**
 * @file monitor_relations.c
 * @author Nicklas Börjesson (nicklasb@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-02-04
 * 
 * @copyright Copyright (c) 2023
 * 
 
 * TODO:
 *          * Have we duplicate, unknown or other peers
 */

#include "monitor_relations.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_log.h>
#include <esp_timer.h>

#include "sdp_peer.h"

#include "sdp_mesh.h"

#include "sdp_def.h"
#include "sdp_messaging.h"
#include "sdp_helpers.h"

#define UNACCEPTABLE_SCORE -90
#define I2C_HEARTBEAT_MS 20000
#define ESPNOW_HEARTBEAT_MS 20000
#define LORA_HEARTBEAT_MS 20000



void check_peer(sdp_peer *peer, void *qos_message){

    uint64_t curr_time = esp_timer_get_time();
    #ifdef CONFIG_SDP_LOAD_TTL 
    if (peer->supported_media_types & SDP_MT_TTL) {
        // Not Implemented
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_BLE
    if (peer->supported_media_types & SDP_MT_BLE) {
        // Not Implemented
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_ESP_NOW
    if (peer->supported_media_types & SDP_MT_ESPNOW) {
        if((peer->espnow_stats.last_score < UNACCEPTABLE_SCORE) || 
        (peer->espnow_stats.last_score_time < (curr_time - (ESPNOW_HEARTBEAT_MS/portTICK_PERIOD_MS)))) {
                
            sdp_send_message_media_type(peer, qos_message, SDP_PREAMBLE_LENGTH + 2, SDP_MT_ESPNOW, false);
        }
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_LoRa
    if (peer->supported_media_types & SDP_MT_LoRa) {
        if((peer->i2c_stats.last_score < UNACCEPTABLE_SCORE) || 
        (peer->i2c_stats.last_score_time < (curr_time - (LORA_HEARTBEAT_MS/portTICK_PERIOD_MS)))) {
                
            sdp_send_message_media_type(peer, qos_message, SDP_PREAMBLE_LENGTH + 2, SDP_MT_LoRa, false);
        }
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_I2C
    if (peer->supported_media_types & SDP_MT_I2C) {
        if((peer->i2c_stats.last_score < UNACCEPTABLE_SCORE) || 
        (peer->i2c_stats.last_score_time < (curr_time - (I2C_HEARTBEAT_MS/portTICK_PERIOD_MS)))) {
                
            sdp_send_message_media_type(peer, qos_message, SDP_PREAMBLE_LENGTH + 2, SDP_MT_I2C, false);
        }
       
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_CANBUS
    if (peer->supported_media_types & SDP_MT_CANBUS) {
        // Not Implemented
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_UMTS
    if (peer->supported_media_types & SDP_MT_UMTS) {
        // Not Implemented
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_ESP_NOW
    if (peer->supported_media_types > 0) {
        // Remove last ","

    }
    #endif



}

void monitor_relations(){
    uint16_t empty_payload = 0x00f0;
    uint8_t *qos_message = sdp_add_preamble(QOS, 0, &empty_payload, 2);
    struct sdp_peer *peer;
    ESP_LOGI("MONITOR", "in monitor_relations()");

    SLIST_FOREACH(peer, get_peer_list(), next)
    {
        ESP_LOGI("MONITOR", " Checking peer: %s", peer->name);
        check_peer(peer, qos_message);
           
    }

}
