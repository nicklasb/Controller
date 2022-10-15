#include "sdp_peer.h"

#include <string.h>
#include <esp_log.h>


#include "sdp_def.h"
#include "sdp_helpers.h"
#include "sdp_messaging.h"

char * log_prefix;

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
    
    int me_length = add_to_message(&me_msg,"ME|%i|%i|%s|%hhu", 
        pv, pvm, log_prefix, supported_media_types);

    if (me_length > 0) {
        retval = sdp_reply(*queue_item, HANDSHAKE, me_msg, me_length);
    } else {
        retval = SDP_ERR_SEND_FAIL;
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


    // In this version, the part count should be 5
    if (queue_item->partcount != 5) {
        if (queue_item->partcount > 1) {
            ESP_LOGE(log_prefix, "ME-message from %s didn't have the correct number of parts. Claims to use protocol version %s", 
                queue_item->peer->name, queue_item->parts[1]);
        } else {
            ESP_LOGE(log_prefix, "ME-message from %s didn't have enough parts to parse. Partcount: %i", 
                queue_item->peer->name, queue_item->partcount);
        }

        return -SDP_ERR_INVALID_PARAM;
    }

    /* Set the protocol versions*/
    queue_item->peer->protocol_version = (uint8_t)atoi(queue_item->parts[1]);
    queue_item->peer->min_protocol_version = (uint8_t)atoi(queue_item->parts[2]);
    /* Set the name of the peer */
    strcpy(queue_item->peer->name, queue_item->parts[3]);
    /* Set supported media types*/
    queue_item->peer->supported_media_types = (uint8_t)atoi(queue_item->parts[4]);
    if (strcmp(queue_item->peer->name, "") == 0) {
        queue_item->peer->state = PEER_UNKNOWN;
    } else {
        queue_item->peer->state = PEER_KNOWN_INSECURE;
    }
    // TODO: Check protocol version for highest matching protocol version.
    
    return 0;


}

void sdp_peer_init(char *_log_prefix) {
    log_prefix = _log_prefix;
}