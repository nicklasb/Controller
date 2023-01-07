#include "i2c_messaging.h"


#include <esp_log.h>
#include <esp_timer.h>
#include <sdp_mesh.h>
#include <sdp_peer.h>
#include <sdp_messaging.h>

#include <string.h>

/* The log prefix for all logging */
char *i2c_messaging_log_prefix;

int i2c_unknown_counter = 0;

void i2c_receive() {

}

int i2c_send_message(sdp_peer *peer, char *data, int data_length) {

#if 0
	// Maximum Payload size of SX1276/77/78/79 is 255
    if (data_length + SDP_MAC_ADDR_LEN > 256) {
		ESP_LOGE(i2c_messaging_log_prefix, ">> Message too long (max 250 bytes): %i", data_length);
        return SDP_ERR_MESSAGE_TOO_LONG;
    }
    uint8_t *tmp_data;
    uint16_t message_len;
	if (peer->state != PEER_UNKNOWN) {
        uint8_t relation_size = sizeof(peer->relation_id);
        // We have an established relation, use the relation id
        ESP_LOGI(i2c_messaging_log_prefix, ">> Relation id > 0: %i, size: %hhu.", peer->relation_id, relation_size);
        message_len = relation_size + data_length;
        tmp_data = malloc(message_len);
        // Add the destination address
        memcpy(tmp_data, &(peer->relation_id), relation_size);
        // Add the data
        memcpy(tmp_data + relation_size , data, data_length);

    } else {
        ESP_LOGI(i2c_messaging_log_prefix, ">> Relation id = 0, connecting using mac adresses");
        // We have no established relation, sending both mac adresses
        message_len = (SDP_MAC_ADDR_LEN * 2) + data_length;
        tmp_data = malloc(message_len);
        // Add the destination address
        memcpy(tmp_data, &(peer->base_mac_address), SDP_MAC_ADDR_LEN);
        // Add source address
        memcpy(tmp_data + SDP_MAC_ADDR_LEN, sdp_host.base_mac_address, SDP_MAC_ADDR_LEN);
        // Add the data
        memcpy(tmp_data + (SDP_MAC_ADDR_LEN * 2) , data, data_length);
    }

	
// TODO: Make some way of handling longer messages

	uint64_t starttime;
	int tx_count = 0;

	tx_count++;
	starttime = esp_timer_get_time();
	ESP_LOGI(i2c_messaging_log_prefix, ">> Sending message: \"%.*s\", data is %i, total %i bytes...", data_length-4, data+4, data_length, data_length + (SDP_MAC_ADDR_LEN *2));
	ESP_LOGI(i2c_messaging_log_prefix, ">> Data (including all) preamble): ");
    ESP_LOG_BUFFER_HEX(i2c_messaging_log_prefix, (uint8_t *)tmp_data, message_len);  
    
    i2c_send_packet((uint8_t *)tmp_data, message_len);
	
	ESP_LOGI(i2c_messaging_log_prefix, ">> %d byte packet sent...speed %f byte/s", message_len, 
	(float)(message_len/((float)(esp_timer_get_time()-starttime))*1000000));

	
    #endif
    return ESP_OK;
}

void i2c_do_on_work_cb(i2c_queue_item_t *work_item) {
    ESP_LOGI(i2c_messaging_log_prefix, ">> In i2c work callback.");

    // TODO: Should we add a check for received here, or is it close enough after the poll?
    i2c_send_message(work_item->peer, work_item->data, work_item->data_length);
    i2c_receive();
}

void i2c_do_on_poll_cb(queue_context *q_context) {
    // TODO: Should the delay here should be related to the speed of the connection? 
    // Wait a moment to let any response in.
    vTaskDelay(100 / portTICK_PERIOD_MS);
	#if 0
    if (i2c_received()) {
        uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 256 bytes   
        int message_length = 0;
        int receive_len = 0;
        bool more_data = true;
        while (more_data) {
            receive_len = i2c_receive_packet(&buf + message_length, sizeof(buf));
            message_length += receive_len;
            // TODO: Here, the delay here should certainly be related to the speed of the connection
            // TODO: Test without retries, this entire while might be pointless as its always full packets?
            vTaskDelay(100/portTICK_PERIOD_MS);
            more_data = receive_len > 0;
        }
        
        ESP_LOGI(i2c_messaging_log_prefix, "<< In i2c POLL callback;i2c_received %i bytes.", message_length);
        ESP_LOGI(i2c_messaging_log_prefix, "<< Received data (including all) preamble): ");
        ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, &buf,message_length, ESP_LOG_INFO);  
        ESP_LOGI(i2c_messaging_log_prefix, "<< sdp_host.base_mac_address: ");
        ESP_LOG_BUFFER_HEX(i2c_messaging_log_prefix, &sdp_host.base_mac_address,SDP_MAC_ADDR_LEN);
        
        // TODO: Do some kind of better non-hardcoded length check. Perhaps it just has to be longer then the mac address?
        if (message_length > 9) {
            
            sdp_mac_address *src_mac_addr = NULL;
            uint8_t data_start = 0;
            // TODO: Add a near match on long ranges (all bits but two needs to match, emergency messaging?) 
            if (memcmp(&sdp_host.base_mac_address, &buf, SDP_MAC_ADDR_LEN) == 0)
            {
                // So, the first 
                src_mac_addr = buf + SDP_MAC_ADDR_LEN;
                data_start = (SDP_MAC_ADDR_LEN *2);
            } else {
                uint32_t relation_id = malloc(4);
                memcpy(&relation_id, buf, 4);
                ESP_LOGI(i2c_messaging_log_prefix, "Relation id in first bytes %u", relation_id);
                src_mac_addr = relation_id_to_mac_address(relation_id);
                if (src_mac_addr == NULL) {
                    ESP_LOGI(i2c_messaging_log_prefix, "<< %d byte packet to someone else received:[%.*s], RSSI %i", 
                        message_length, message_length, (char *)&buf , i2c_packet_rssi());
                    goto finish;
                }
                data_start = sizeof(uint32_t);
            }

            ESP_LOGI(i2c_messaging_log_prefix, "<< %d byte packet received:[%.*s], RSSI %i", 
            message_length, message_length, (char *)&buf , i2c_packet_rssi());

            sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address(src_mac_addr);
            if (!peer) {
                char *new_name;
                asprintf(&new_name, "UNKNOWN_%i", i2c_unknown_counter++);
                
                peer = sdp_add_init_new_peer(new_name, src_mac_addr, SDP_MT_i2c);
                
            }
            handle_incoming(peer, buf + data_start, message_length - data_start, SDP_MT_i2c);    
  
        }
        
    }

finish:
    i2c_receive(); 
     #endif   
}

void i2c_messaging_init(char * _log_prefix){
    i2c_messaging_log_prefix = _log_prefix;
}
