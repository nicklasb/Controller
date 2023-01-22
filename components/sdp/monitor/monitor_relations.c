#include "monitor_relations.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_log.h>

#include "sdp_peer.h"

#include "sdp_mesh.h"

#include "sdp_def.h"
#include "sdp_messaging.h"
#include "sdp_helpers.h"

#define UNACCEPTABLE_SCORE -90
#define I2C_HEARTBEAT_MS 20000
#define ESPNOW_HEARTBEAT_MS 20000

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
                
            sdp_send_message_media_type(peer, qos_message, SDP_PREAMBLE_LENGTH + 2, SDP_MT_ESPNOW);
        }
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_LoRa
    if (peer->supported_media_types & SDP_MT_LoRa) {
        // Not Implemented
    }
    #endif
    #ifdef CONFIG_SDP_LOAD_I2C
    if (peer->supported_media_types & SDP_MT_I2C) {
        if((peer->i2c_stats.last_score < UNACCEPTABLE_SCORE) || 
        (peer->i2c_stats.last_score_time < (curr_time - (I2C_HEARTBEAT_MS/portTICK_PERIOD_MS)))) {
                
            sdp_send_message_media_type(peer, qos_message, SDP_PREAMBLE_LENGTH + 2, SDP_MT_I2C);
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

    // Loop all peers
        // Loop all supported medias
            // Calculate points? Or is it enough to stow away the last score?
            // And if its really low, an the last success was a while ago(?), send a health check message
                // Should this be a more advanced 000000001111111100001111001101101010101
                //  (probably this is only the point for extreme LoRa)
                    // Is a check there a loop or:ing the two values in 
                    // Order would be most likely, exact, then going 

                    // Is this the direction we always check is? I mean are we always looking at the shorter range? (nope)
                    // We are just testing everything:
                    // Low score often.
                    // High score less often. High score vs speed, for example?
                    // Or is there more nuance? 


}