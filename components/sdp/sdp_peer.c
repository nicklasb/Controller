#include "sdp_peer.h"

#include <string.h>
#include <esp_log.h>

#include "sdp_helpers.h"
#include "sdp_messaging.h"

char *peer_log_prefix;

sdp_host_t sdp_host;

/**
 * @brief Compile a message telling a peer about our abilities
 * 
 * @param peer The peer 
 */
int sdp_peer_send_me_message(work_queue_item_t *queue_item) {

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
    ;
    uint8_t *me_msg = NULL;
    int pv = SDP_PROTOCOL_VERSION;
    int pvm = SDP_PROTOCOL_VERSION_MIN;
    
    /**
     * @brief Construct a me-message
     * Parameters: 
     * pv : Protocol version
     * pvm: Minimal supported version
     * host name: The SDP host name 
     * supported  media types: A byte describing the what communication technologies the peer supports.
     * adresses: A list of addresses in the order of the bits in the media types byte.
     */
    int me_length = add_to_message(&me_msg,"ME|%i|%i|%s|%hhu|%b6", 
        pv, pvm, sdp_host.sdp_host_name, supported_media_types, sdp_host.base_mac_address);

    if (me_length > 0) {
        retval = sdp_reply(*queue_item, HANDSHAKE, me_msg, me_length);
    } else {
        // Returning the negative of the return value as that denotes an error.
        retval = -me_length;
    }
    free(me_msg);
    return retval;
}


/**
 * @brief Send a "WHO"-message that asks the peer to describe themselves
 * 
 * @return int A pointer to the created conversation
 */
int sdp_peer_send_who_message(sdp_peer *peer) {
    char who_msg[4] = "WHO\0";
    return start_conversation(peer, HANDSHAKE, "Handshaking", &who_msg,4);
}

int sdp_peer_inform(work_queue_item_t *queue_item) {

    ESP_LOGI(peer_log_prefix, "Informing peer.");

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
    memcpy(&queue_item->peer->base_mac_address, &(queue_item->raw_data[20]), SDP_MAC_ADDR_LEN);
    
    ESP_LOGI(peer_log_prefix, "Peer %s now more informed ",queue_item->peer->name);
    ESP_LOG_BUFFER_HEX(peer_log_prefix, queue_item->peer->base_mac_address, SDP_MAC_ADDR_LEN);


    // TODO: Check protocol version for highest matching protocol version.
    
    return 0;

}

void sdp_peer_init(char *_log_prefix) {
    peer_log_prefix = _log_prefix;
    sdp_host.protocol_version = SDP_PROTOCOL_VERSION;
    sdp_host.min_protocol_version = SDP_PROTOCOL_VERSION_MIN;
    strcpy(sdp_host.sdp_host_name, CONFIG_SDP_PEER_NAME);
}