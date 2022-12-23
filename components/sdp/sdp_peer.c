#include "sdp_peer.h"

#include <string.h>
#include <esp_log.h>

#include "sdp_helpers.h"
#include "sdp_messaging.h"

char *peer_log_prefix;

sdp_peer sdp_host = {};

/**
 * @brief Compile a HI or HIR message telling a peer about our abilities
 * 
 * @param peer The peer 
 * @param is_reply This is a reply, send a HIR-message
 * 
 */
int sdp_peer_send_hi_message(sdp_peer *peer, bool is_reply) {

    ESP_LOGI(peer_log_prefix, ">> Send a HI or HIR-message with information.");

    int retval;
    // TODO: Use CONFIG_SDP_NAME
    // char *sdf_name = CONFIG_SDP_NAME;

    /* Gather the configured protocol support*/
    uint8_t supported_media_types = 0
    #ifdef CONFIG_SDP_LOAD_BLE
    + SDP_MT_BLE
    #endif
    #ifdef CONFIG_SDP_LOAD_ESP_NOW
    + SDP_MT_ESPNOW
    #endif
    #ifdef CONFIG_SDP_LOAD_LORA
    + SDP_MT_LoRa
    #endif
    ;

    int pv = SDP_PROTOCOL_VERSION;
    int pvm = SDP_PROTOCOL_VERSION_MIN;
    
    /**
     * @brief Construct a HI-message
     * Parameters: 
     * pv : Protocol version
     * pvm: Minimal supported version
     * host name: The SDP host name 
     * supported  media types: A byte describing the what communication technologies the peer supports.
     * adresses: A list of addresses in the order of the bits in the media types byte.
     */
    char fmt_str[22] = "HI";
    if (is_reply) {
        strcpy(fmt_str, "HIR");
    }

    uint8_t *hi_msg = NULL;
    int hi_length = add_to_message(&hi_msg, strcat(fmt_str, "|%i|%i|%s|%hhu|%b6"), 
        pv, pvm, sdp_host.name, supported_media_types, sdp_host.base_mac_address);
    if (hi_length > 0) {
        void *new_data = sdp_add_preamble(HANDSHAKE, 0, hi_msg, hi_length);
        retval = sdp_send_message(peer, new_data, hi_length+ SDP_PREAMBLE_LENGTH);
    } else {
        // Returning the negative of the return value as that denotes an error.
        retval = -hi_length;
    }
    free(hi_msg);
    return retval;
}

int sdp_peer_inform(work_queue_item_t *queue_item) {

    ESP_LOGI(peer_log_prefix, "<< Got a HI or HIR-message with information.");

    /* Set the protocol versions*/
    queue_item->peer->protocol_version = (uint8_t)atoi(queue_item->parts[1]);
    queue_item->peer->min_protocol_version = (uint8_t)atoi(queue_item->parts[2]);
    /* Set the name of the peer */
    strcpy(queue_item->peer->name, queue_item->parts[3]);

    if (strcmp(queue_item->peer->name, "") == 0) {
        queue_item->peer->state = PEER_UNKNOWN;
    } else {
        queue_item->peer->state = PEER_KNOWN_INSECURE;
    }   

    /* Set supported media types*/
    queue_item->peer->supported_media_types = (uint8_t)atoi(queue_item->parts[4]);


    /* Set base MAC address*/ 
    memcpy(&queue_item->peer->base_mac_address, queue_item->parts[5], SDP_MAC_ADDR_LEN);
    
    ESP_LOGI(peer_log_prefix, "<< Peer %s now more informed ",queue_item->peer->name);
    log_peer_info(peer_log_prefix, queue_item->peer);
    

    // TODO: Check protocol version for highest matching protocol version.
    
    return 0;

}

void sdp_peer_init(char *_log_prefix) {
    peer_log_prefix = _log_prefix;
    sdp_host.protocol_version = SDP_PROTOCOL_VERSION;
    sdp_host.min_protocol_version = SDP_PROTOCOL_VERSION_MIN;
    strcpy(sdp_host.name, CONFIG_SDP_PEER_NAME);
}