#include "sdp_peer.h"


#include <string.h>
#include <esp_log.h>
#include <esp32/rom/crc.h>

#include <esp_attr.h>

#include "sdp_helpers.h"
#include "sdp_messaging.h"
#include "sdp_mesh.h"

#ifdef CONFIG_SDP_LOAD_I2C
#include "i2c/i2c_peer.h"
#endif

#ifdef CONFIG_SDP_LOAD_LORA
#include "lora/lora_peer.h"
#endif

char *peer_log_prefix;


struct relation {
    uint32_t relation_id;
    sdp_mac_address mac_address;
    #ifdef CONFIG_SDP_LOAD_I2C
    uint8_t i2c_address;
    #endif
};

float add_to_failure_rate_history(struct sdp_peer_media_stats *stats, float rate) {
    // Calc average of the current history + rate, start with summarizing
    float sum = 0;
    for (int i=0; i< FAILURE_RATE_HISTORY_LENGTH; i++) {
        sum+= stats->failure_rate_history[i];
        ESP_LOGI(peer_log_prefix, "FRH %i: fr: %f", i, stats->failure_rate_history[i]);
    }   
    sum+=rate;
    // Move the array one step to the left
    memmove((void *)(stats->failure_rate_history), (void *)(stats->failure_rate_history) + sizeof(float),  sizeof(float)* (FAILURE_RATE_HISTORY_LENGTH - 1));
    stats->failure_rate_history[FAILURE_RATE_HISTORY_LENGTH -1] = rate;

    ESP_LOGI(peer_log_prefix, "FRH avg %f", (float)(sum/(FAILURE_RATE_HISTORY_LENGTH + 1)));

    return sum/(FAILURE_RATE_HISTORY_LENGTH + 1);
}


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
     #ifdef CONFIG_SDP_LOAD_I2C
    + SDP_MT_I2C
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
    char fmt_str[27] = "HIR";
    if (!is_reply) {
        strcpy(fmt_str, "HI");
        uint8_t *tmp_crc_data = malloc(12);
        memcpy(tmp_crc_data, peer->base_mac_address, SDP_MAC_ADDR_LEN);
        memcpy(tmp_crc_data+SDP_MAC_ADDR_LEN, sdp_host.base_mac_address, SDP_MAC_ADDR_LEN);
        // TODO: Why big endian? Change to little endian everywhere unless BLE have other ideas

        peer->relation_id = crc32_be(0, tmp_crc_data, SDP_MAC_ADDR_LEN * 2);
        free(tmp_crc_data);
    }

    add_relation(peer->base_mac_address, peer->relation_id
    #ifdef CONFIG_SDP_LOAD_I2C
    , peer->i2c_address
    #endif   
    );
    #ifdef CONFIG_SDP_LOAD_I2C
    uint8_t i2c_address = CONFIG_I2C_ADDR;
    #else
    uint8_t i2c_address = 0;
    #endif   
    
    uint8_t *hi_msg = NULL;
    int hi_length = add_to_message(&hi_msg, strcat(fmt_str, "|%u|%u|%s|%hhu|%u|%hhu|%b6"), 
        pv, pvm, sdp_host.name, supported_media_types, peer->relation_id, i2c_address,sdp_host.base_mac_address);
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
    if (queue_item->peer == NULL) {
        ESP_LOGE(peer_log_prefix, "<< ..but queue_item->peer is NULL, internal error!");
        return ESP_FAIL;
    } 
     if (queue_item->partcount < 6) {
        ESP_LOGE(peer_log_prefix, "<< ..but there is not enough parts!");
        return ESP_FAIL;
    }    
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
    #ifdef CONFIG_SDP_LOAD_I2C
    queue_item->peer->i2c_address = atoi(queue_item->parts[6]);
    #endif
    
    /* If we haven't before or it's updated; set base MAC address*/ 
    if (memcmp(&queue_item->peer->base_mac_address, queue_item->parts[7], SDP_MAC_ADDR_LEN) != 0) {
        memcpy(&queue_item->peer->base_mac_address, queue_item->parts[7], SDP_MAC_ADDR_LEN);    
        // Also init any other supported media type
        // TODO: sdp_mesh and sdp_pee
        init_supported_media_types_mac_address(queue_item->peer);
        ESP_LOGI(peer_log_prefix, "<< Initiated other supported medias");
    }   

    ESP_LOGI(peer_log_prefix, "<< Peer %s now more informed ",queue_item->peer->name);
    log_peer_info(peer_log_prefix, queue_item->peer);
    

    // TODO: Check protocol version for highest matching protocol version.
    
    return ESP_OK;

}


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

bool add_relation(sdp_mac_address mac_address, uint32_t relation_id
    #ifdef CONFIG_SDP_LOAD_I2C
    , uint8_t i2c_address
    #endif
    ) {
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
        #ifdef CONFIG_SDP_LOAD_I2C
        relations[rel_end].i2c_address = i2c_address;
        #endif
        ESP_LOGI(peer_log_prefix, "Relation added at %hhu", rel_end);
        rel_end++;
        
        return true;
    }
}

float sdp_helper_calc_suitability(int bitrate, int min_offset, int base_offset, float multiplier)
{
    float retval = min_offset - ((bitrate - base_offset) * multiplier);
    if (retval < -50) {
        retval = -50;
    }
    return retval;
}
/**
 * @brief
 *
 * @param peer
 * @param data_length
 * @return e_media_type
 */
e_media_type select_media(struct sdp_peer *peer, int data_length)
{

    // Loop media types and find the highest scoring

    // TODO: If this is slow, perhaps we could have a short-lived selection cache?

    e_media_type top_media_type = 0;
    float top_score  = 0;
    float curr_score = 0;
    for (unsigned int curr_media_type = 1; curr_media_type < SDP_MT_ANY; curr_media_type = curr_media_type * 2)
    {
        if (peer->supported_media_types & curr_media_type)
        {
#ifdef CONFIG_SDP_LOAD_BLE

            if (curr_media_type == SDP_MT_BLE)
            {
                curr_score = 1;
            }

#endif
#ifdef CONFIG_SDP_LOAD_I2C
            if (curr_media_type == SDP_MT_I2C)
            {
                curr_score = i2c_score_peer(peer, data_length);
            }
#endif
#ifdef CONFIG_SDP_LOAD_ESP_NOW

            if (curr_media_type == SDP_MT_ESPNOW)
            {
                curr_score = 1;
            }

#endif
#ifdef CONFIG_SDP_LOAD_LORA
            if (curr_media_type == SDP_MT_LoRa)
            {
                curr_score = lora_score_peer(peer, data_length);
            }
#endif

            if (curr_score > top_score)
            {
                top_score = curr_score;
                top_media_type = curr_media_type;
            }
        }
        // TODO: Warn if we are forced to use an obviously unsuitable media (or fail if we go below minimum scores)
        // For example if someone wants to send a video stream and the only available connection is LoRa.
    }

    return top_media_type;
}

void sdp_peer_init_peer(sdp_peer * peer){
    #ifdef CONFIG_SDP_LOAD_I2C
    i2c_peer_init_peer(peer);
    #endif
    #ifdef CONFIG_SDP_LOAD_LORA
    lora_peer_init_peer(peer);
    #endif
}


/**
 * @brief 
 * 
 * @param _log_prefix 
 */
void sdp_peer_init(char *_log_prefix) {
    peer_log_prefix = _log_prefix;
    sdp_host.protocol_version = SDP_PROTOCOL_VERSION;
    sdp_host.min_protocol_version = SDP_PROTOCOL_VERSION_MIN;
    strcpy(sdp_host.name, CONFIG_SDP_PEER_NAME);
}