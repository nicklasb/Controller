#include "sdp_peer.h"

#include <string.h>
#include <esp_log.h>
#include <esp32/rom/crc.h>

#include "sdp_helpers.h"
#include "sdp_messaging.h"

char *peer_log_prefix;


struct relation {
    uint32_t relation_id;
    sdp_mac_address mac_address;
};

/**
 * @brief  This is an an array with all relations 
 * It has two uses:
 * 1. Faster way to lookup the mac address than looping peers (discern where a communication is relevant)
 * 2. Stores relevant peers relations ids and their mac adresses in RTC, 
 *    making it possible to reconnect after deep sleep
 * 3. We doesn't have to use the peer list when sending and receiving data
 * 
 */
RTC_DATA_ATTR struct relation relations[SDP_MAX_PEERS];
RTC_DATA_ATTR uint8_t rel_end = 0;

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
    char fmt_str[22] = "HIR";
    if (!is_reply) {
        strcpy(fmt_str, "HI");
        uint8_t *tmp_crc_data = malloc(12);
        memcpy(tmp_crc_data, peer->base_mac_address, SDP_MAC_ADDR_LEN);
        memcpy(tmp_crc_data+SDP_MAC_ADDR_LEN, sdp_host.base_mac_address, SDP_MAC_ADDR_LEN);
        peer->relation_id = crc32_be(0, tmp_crc_data, SDP_MAC_ADDR_LEN * 2);
        free(tmp_crc_data);
    }
    add_relation(peer->base_mac_address, peer->relation_id);
    
    uint8_t *hi_msg = NULL;
    int hi_length = add_to_message(&hi_msg, strcat(fmt_str, "|%u|%u|%s|%hhu|%u|%b6"), 
        pv, pvm, sdp_host.name, supported_media_types, peer->relation_id, sdp_host.base_mac_address);
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
    queue_item->peer->relation_id = atoi(queue_item->parts[5]);

    /* Set base MAC address*/ 
    memcpy(&queue_item->peer->base_mac_address, queue_item->parts[6], SDP_MAC_ADDR_LEN);
    
    ESP_LOGI(peer_log_prefix, "<< Peer %s now more informed ",queue_item->peer->name);
    log_peer_info(peer_log_prefix, queue_item->peer);
    

    // TODO: Check protocol version for highest matching protocol version.
    
    return 0;

}

/** 
 * Find the relation id through the mac address.
 * Usua
*/

/**
 * @brief Find the relation id through the mac address.
 * (without having to loop all peers) 
 * @param relation_id The relation id to investigate
 * @return sdp_mac_address* 
 */
sdp_mac_address *relation_id_to_mac_address(uint32_t relation_id) {
    int rel_idx = 0;
    while (rel_idx <= rel_end) {
        if (relations[rel_idx].relation_id == relation_id) {
            return &(relations[rel_idx].mac_address);
        }
        rel_idx++;
    }
    ESP_LOGI(peer_log_prefix, "Relation %u not found.", relation_id);
    return NULL;
}

bool add_relation(sdp_mac_address mac_address, uint32_t relation_id) {
    /* Is it already there? */
    int rel_idx = 0;
    while (rel_idx <= rel_end) {
        if (memcmp(relations[rel_idx].mac_address, mac_address, SDP_MAC_ADDR_LEN) == 0) {
            return false;
        }
        rel_idx++;
    }
    if (rel_end >= SDP_MAX_PEERS) {
        ESP_LOGE(peer_log_prefix, "!!! Relations are added that cannot be accommodated, this is likely an attack! !!!");
        // TODO: This needs to be reported to monitoring
        // TODO: Monitoring should detect slow (but pointless) and fast adding of peers (similar RSSI:s indicate the same source, too). 
        return false;
    } else {
        
        relations[rel_end].relation_id = relation_id;
        memcpy(relations[rel_end].mac_address, mac_address, SDP_MAC_ADDR_LEN);
        ESP_LOGI(peer_log_prefix, "Relation added at %hhu", rel_end);
        rel_end++;
        
        return true;
    }
}

void sdp_peer_init(char *_log_prefix) {
    peer_log_prefix = _log_prefix;
    sdp_host.protocol_version = SDP_PROTOCOL_VERSION;
    sdp_host.min_protocol_version = SDP_PROTOCOL_VERSION_MIN;
    strcpy(sdp_host.name, CONFIG_SDP_PEER_NAME);
}