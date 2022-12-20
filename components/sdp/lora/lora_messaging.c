#include "lora_messaging.h"

#include "lora.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sdp_mesh.h>
#include <sdp_messaging.h>

#include <string.h>

/* The log prefix for all logging */
char *lora_messaging_log_prefix;

int lora_unknown_counter = 0;

int lora_send_message(sdp_mac_address *dest_mac_address, char *data, int data_length) {

	// Maximum Payload size of SX1276/77/78/79 is 255
    if (data_length + SDP_MAC_ADDR_LEN > 256) {
		ESP_LOGE(lora_messaging_log_prefix, "Message too long (max 250 bytes): %i", data_length);
        return SDP_ERR_MESSAGE_TOO_LONG;
    }
	
    // TODO: 12 bytes is quite a lot of data
	uint8_t *tmp_data = malloc((SDP_MAC_ADDR_LEN * 2) + data_length);
    // Add the destination address
	memcpy(tmp_data, dest_mac_address, SDP_MAC_ADDR_LEN);
    // Add source address
	memcpy(tmp_data + SDP_MAC_ADDR_LEN, sdp_host.base_mac_address, SDP_MAC_ADDR_LEN);
    // Add the data
    memcpy(tmp_data + (SDP_MAC_ADDR_LEN * 2) , data, data_length);
	
// TODO: Make some way of handling longer messages

	uint64_t starttime;
	int tx_count = 0;

	tx_count++;
	starttime = esp_timer_get_time();
	ESP_LOGI(lora_messaging_log_prefix, "Sending message: \"%.*s\", data is %i, total %i bytes...", data_length-4, data+4, data_length, data_length + (SDP_MAC_ADDR_LEN *2));
	ESP_LOGI(lora_messaging_log_prefix, "Data (including all) preamble): ");
    ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, (uint8_t *)tmp_data, (SDP_MAC_ADDR_LEN * 2) + data_length);  
    
    lora_send_packet((uint8_t *)tmp_data, (SDP_MAC_ADDR_LEN * 2) + data_length);
	
	ESP_LOGI(lora_messaging_log_prefix, "%d byte packet sent...speed %f byte/s", data_length + SDP_MAC_ADDR_LEN *2, 
	(float)(data_length + SDP_MAC_ADDR_LEN)/((float)(esp_timer_get_time()-starttime))*1000000);

	return ESP_OK;

}

void lora_do_on_work_cb(lora_queue_item_t *work_item) {
    ESP_LOGI(lora_messaging_log_prefix, "In LoRa work callback.");

    // TODO: Should we add a check for received here, or is it close enough after the poll?
    lora_send_message(work_item->peer->base_mac_address, work_item->data, work_item->data_length);
    lora_receive();
}

void lora_do_on_poll_cb(queue_context *q_context) {

	uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 255    
    if (lora_received()) {
        int receive_len = lora_receive_packet(&buf, sizeof(buf));
        ESP_LOGI(lora_messaging_log_prefix, "In LoRa POLL callback;lora_received %i bytes.", receive_len);
        ESP_LOGI(lora_messaging_log_prefix, "Received data (including all) preamble): ");
        ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, &buf,receive_len);  
        ESP_LOGI(lora_messaging_log_prefix, "sdp_host.base_mac_address: ");
        ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, &sdp_host.base_mac_address,SDP_MAC_ADDR_LEN);

        if (receive_len > 4 + (SDP_MAC_ADDR_LEN * 2)) {
            // Is it to us? TODO: Add a near match on long ranges (all bits but two needs to match)
            if (memcmp(&sdp_host.base_mac_address, &buf, SDP_MAC_ADDR_LEN) == 0) {
                ESP_LOGI(lora_messaging_log_prefix, "%d byte packet received:[%.*s], RSSI %i", 
                receive_len, receive_len, (char *)&buf , lora_packet_rssi());

                sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address(buf + SDP_MAC_ADDR_LEN);
                if (!peer) {
                    char *new_name;
                    asprintf(&new_name, "UNKNOWN_%i", lora_unknown_counter++);
                    peer = sdp_add_init_new_peer(new_name, buf + SDP_MAC_ADDR_LEN, SDP_MT_LoRa);
//                    ESP_LOGI(lora_messaging_log_prefix, "Did not find peer: ");
//                    ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, buf + 6, SDP_MAC_ADDR_LEN);
                }
                handle_incoming(peer, &buf + (SDP_MAC_ADDR_LEN * 2), receive_len - (SDP_MAC_ADDR_LEN * 2), SDP_MT_LoRa);    
                
                
            } else {
                ESP_LOGI(lora_messaging_log_prefix, "%d byte packet to someone else received:[%.*s], RSSI %i", 
                receive_len, receive_len, (char *)&buf , lora_packet_rssi());
            }
            
        }
        lora_receive(); 
    }

}

void lora_messaging_init(char * _log_prefix){
    lora_messaging_log_prefix = _log_prefix;
}
