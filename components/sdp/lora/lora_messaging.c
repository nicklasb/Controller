#include "lora_messaging.h"

#ifdef CONFIG_LORA_SX126X
#include "lora_sx126x_lib.h"
#endif
#ifdef CONFIG_LORA_SX127X
#include "lora_sx127x_lib.h"
#endif

#include "esp32/rom/crc.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sdp_mesh.h>
#include <sdp_peer.h>
#include <sdp_helpers.h>
#include <sdp_messaging.h>
#include "lora_peer.h"

#include <string.h>

/* The log prefix for all logging */
char *lora_messaging_log_prefix;

int lora_send_message(sdp_peer *peer, char *data, int data_length) {

	// Maximum Payload size of SX1276/77/78/79 is 255
    if (data_length + SDP_MAC_ADDR_LEN > 256) {
		ESP_LOGE(lora_messaging_log_prefix, ">> Message too long (max 250 bytes): %i", data_length);
        // TODO: Obviously longer messages have to be possible to send.
        // Based on settings, Kbits/sec and thus time-to-send should be possible to calculate and figure out if it is too big of a message.
        return SDP_ERR_MESSAGE_TOO_LONG;
    }
    uint8_t *tmp_data;
    uint16_t message_len;
	if (peer->state != PEER_UNKNOWN) {
        uint8_t relation_size = sizeof(peer->relation_id);
        // We have an established relation, use the relation id
        ESP_LOGI(lora_messaging_log_prefix, ">> Relation id > 0: %i, size: %hhu.", peer->relation_id, relation_size);
        message_len = relation_size + data_length;
        tmp_data = malloc(message_len);
        // Add the destination address
        memcpy(tmp_data, &(peer->relation_id), relation_size);
        // Add the data
        memcpy(tmp_data + relation_size , data, data_length);

    } else {
        ESP_LOGI(lora_messaging_log_prefix, ">> Relation id = 0, connecting using mac adresses");
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
	
	ESP_LOGI(lora_messaging_log_prefix, ">> Sending message: \"%.*s\", data is %i, total %i bytes...", data_length-4, data+4, data_length, data_length + (SDP_MAC_ADDR_LEN *2));
	ESP_LOGI(lora_messaging_log_prefix, ">> Data (including all) preamble): ");
    ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, (uint8_t *)tmp_data, message_len);  
    starttime = esp_timer_get_time();
 	#ifdef CONFIG_LORA_SX126X
    LoRaSend((uint8_t *)tmp_data, message_len, SX126x_TXMODE_SYNC);
	#endif
    #ifdef CONFIG_LORA_SX127X
    lora_send_packet((uint8_t *)tmp_data, message_len);
	#endif

    ESP_LOGI(lora_messaging_log_prefix, ">> %d byte packet sent...speed %f byte/s", message_len, 
	(float)(message_len/((float)(esp_timer_get_time()-starttime))*1000000));

    // TODO: Add a check for the CRC response
    starttime = esp_timer_get_time();
    while (1) {
        
        while (
        #ifdef CONFIG_LORA_SX127X
        !lora_received()
        #endif
        #ifdef CONFIG_LORA_SX126X
        !(GetIrqStatus()  & SX126X_IRQ_RX_DONE)
        #endif
        ) { 
            if (esp_timer_get_time() > starttime + 5000000){
                ESP_LOGE(lora_messaging_log_prefix, "<< Timed out waiting for receipt from %s.", peer->name); 
                return ESP_FAIL;
            }
                
        }

        uint8_t buf[256]; // Maximum Payload size of SX126x/SX127x is 255/256 bytes
        int message_length = 0;
        int receive_len = 0;
        bool more_data = true;
        while (more_data) {
            #ifdef CONFIG_LORA_SX126X
            receive_len = LoRaReceive(&buf + message_length, 255);
            #endif
            #ifdef CONFIG_LORA_SX127X
            receive_len = lora_receive_packet(&buf + message_length, sizeof(buf));
            #endif            
            message_length += receive_len;
            // TODO: Here, the delay here should certainly be related to the speed of the connection
            // TODO: Test without retries, this entire while might be pointless as its always full packets?
            vTaskDelay(1);
            more_data = receive_len > 0;

        }
        if ((memcmp(&buf, &peer->relation_id, 4) == 0) && (message_length >= 6)) {
           if ((buf[4] == 0xff)&& buf[5] == 0x00) {
                ESP_LOGI(lora_messaging_log_prefix, "<< Success messages from %s.", peer->name); 
                peer->lora_stats.receive_successes++;
                return ESP_OK;
           } else if (buf[4] == 0xff && buf[4] == 0x00) {
                ESP_LOGW(lora_messaging_log_prefix, "<< Bad CRC message from %s.", peer->name); 
                peer->lora_stats.receive_failures++;
                return ESP_FAIL;
           } else {
                ESP_LOGE(lora_messaging_log_prefix, "<< Badly formatted CRC message from %s.", peer->name); 
                ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, (uint8_t *)buf, message_length); 
                peer->lora_stats.receive_failures++;
                return ESP_FAIL;
           }
            
        } else  {
                ESP_LOGW(lora_messaging_log_prefix, "<< Got a %i-byte message to someone else (not %i, will keep waiting for a response):", message_length, peer->relation_id); 
                ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, (uint8_t *)buf, message_length); 
           }
    }
	return ESP_FAIL;

}

void lora_do_on_work_cb(lora_queue_item_t *work_item) {
    ESP_LOGI(lora_messaging_log_prefix, ">> In LoRa work callback.");

    /* TODO: 
        0. In I2C, if it fails, resend message using send_message(), as it will re-evaluate media choice - Done
        1. Add a response in the poll 
        2. Add a check in the send message, model after I2C (can we break out CRC handling and stuff?)
        3. If we fail, retry sending the message using send_message(), as it will re-evaluate media choice

     */

    lora_send_message(work_item->peer, work_item->data, work_item->data_length);


    #ifdef CONFIG_LORA_SX127X   
    lora_receive();
    #endif

}

int get_rssi() {
    #ifdef CONFIG_LORA_SX126X   
    return GetRssiInst();
    #endif
    #ifdef CONFIG_LORA_SX127X   
    return lora_packet_rssi();
    #endif  
}

void lora_do_on_poll_cb(queue_context *q_context) {
    // TODO: Should the delay here should be related to the speed of the connection? 
    // Wait a moment to let any response in.
    vTaskDelay(100 / portTICK_PERIOD_MS);

    if (
        #ifdef CONFIG_LORA_SX127X
        lora_received()
        #endif
        #ifdef CONFIG_LORA_SX126X
        GetIrqStatus()  & SX126X_IRQ_RX_DONE 
        #endif
        ) {
        uint8_t buf[256]; // Maximum Payload size of SX126x/SX127x is 255/256 bytes
        int message_length = 0;
        int receive_len = 0;
        bool more_data = true;
        while (more_data) {
            #ifdef CONFIG_LORA_SX126X
            receive_len = LoRaReceive(&buf + message_length, 255);
            #endif
            #ifdef CONFIG_LORA_SX127X
            receive_len = lora_receive_packet(&buf + message_length, sizeof(buf));
            #endif            
            message_length += receive_len;
            // TODO: Here, the delay here should certainly be related to the speed of the connection
            // TODO: Test without retries, this entire while might be pointless as its always full packets?
            vTaskDelay(100/portTICK_PERIOD_MS);
            more_data = receive_len > 0;
        }
        
        ESP_LOGI(lora_messaging_log_prefix, "<< In LoRa POLL callback;lora_received %i bytes.", message_length);
        ESP_LOGI(lora_messaging_log_prefix, "<< Received data (including all) preamble): ");
        ESP_LOG_BUFFER_HEXDUMP(lora_messaging_log_prefix, &buf,message_length, ESP_LOG_INFO);  
        ESP_LOGI(lora_messaging_log_prefix, "<< sdp_host.base_mac_address: ");
        ESP_LOG_BUFFER_HEX(lora_messaging_log_prefix, &sdp_host.base_mac_address,SDP_MAC_ADDR_LEN);

        // TODO: Do some kind of better non-hardcoded length check. Perhaps it just has to be longer then the mac address?
        if (message_length > SDP_PREAMBLE_LENGTH + 1) {
            uint32_t relation_id = 0;
            sdp_mac_address *src_mac_addr = NULL;
            uint8_t data_start = 0;
            // TODO: Add a near match on long ranges (all bits but two needs to match, emergency messaging?) 
            if (memcmp(&sdp_host.base_mac_address, &buf, SDP_MAC_ADDR_LEN) == 0)
            {
                if (message_length > (SDP_MAC_ADDR_LEN * 2))
                {
                    // So, the first 
                    src_mac_addr = buf + SDP_MAC_ADDR_LEN;
                    data_start = (SDP_MAC_ADDR_LEN *2);
                    relation_id = calc_relation_id(&buf, &buf[6]);
                } else {
                    ESP_LOGI(lora_messaging_log_prefix, "<< Matching mac but too short. Too short %d byte,:[%.*s], RSSI %i", 
                        message_length, message_length, (char *)&buf , get_rssi());
                    goto finish;
                }                
            } else {
                memcpy(&relation_id, buf, 4);
                ESP_LOGI(lora_messaging_log_prefix, "Relation id in first bytes %u", relation_id);
                src_mac_addr = relation_id_to_mac_address(relation_id);
                if (src_mac_addr == NULL) {
                    ESP_LOGI(lora_messaging_log_prefix, "<< %d byte packet to someone else received:[%.*s], RSSI %i", 
                        message_length, message_length, (char *)&buf , get_rssi());
                    goto finish;
                }
                data_start = sizeof(uint32_t);
            }

            ESP_LOGI(lora_messaging_log_prefix, "<< %d byte packet received:[%.*s], RSSI %i", 
            message_length, message_length, (char *)&buf , get_rssi());
            sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address(src_mac_addr);
            
            uint8_t *rcv_data = &buf[data_start];
            uint32_t crc32_in = 0;
            memcpy(&crc32_in, rcv_data, 4);
            unsigned int crc_calc = crc32_be(0, rcv_data + 4, message_length - data_start - 4);
            uint8_t response[6];
            memcpy(&response, &relation_id, 4);
            int ret = ESP_OK;
            if (crc32_in != crc_calc)
            {
                ESP_LOGW(lora_messaging_log_prefix, "<< LoRa - CRC Mismatch crc32_in: %u,crc_calc: %u. Create response:", 
                    crc32_in, crc_calc);
                response[4] = 0x00;
                response[5] = 0xff;
                if (peer) {
                    /**
                     * @brief We only log on an existing peer, as there is a number of peers/256 risk that 
                     * also the source address is wrong, as the message failed crc check. 
                     * However, the likelyhood of several conflicts is very low.
                     * Also, new peers are only added if the CRC is correct, and in that case it is unlikely that errors would just 
                     * concern the adress. Also, the HI message contains the mac_address. 
                     */
                    peer->lora_stats.receive_failures++;
                    
                } else {
                    lora_unknown_failures++;

                }
                ret = ESP_FAIL;
            }  else {
                ESP_LOGI(lora_messaging_log_prefix, "<< LoRa - Got %i bytes of data. crc32 : %u, create response.", 6, crc_calc);
                                
                response[4] = 0xff;
                response[5] = 0x00;   
            }

            ESP_LOG_BUFFER_HEXDUMP(lora_messaging_log_prefix, &response, 6, ESP_LOG_INFO);
            int64_t starttime = esp_timer_get_time();
            #ifdef CONFIG_LORA_SX126X
            LoRaSend(&response, 6, SX126x_TXMODE_SYNC);
            #endif
            #ifdef CONFIG_LORA_SX127X
            lora_send_packet(&response, 6);
            #endif
            ESP_LOGI(lora_messaging_log_prefix, ">> %d byte packet sent...speed %f byte/s", 6, 
            (float)(6/((float)(esp_timer_get_time()-starttime))*1000000));
            if (!peer) {
                char *new_name;
                asprintf(&new_name, "UNKNOWN_%i", lora_unknown_counter++);
                
                peer = sdp_add_init_new_peer(new_name, src_mac_addr, SDP_MT_LoRa);
                peer->lora_stats.receive_successes++;
            }
            handle_incoming(peer, buf + data_start, message_length - data_start, SDP_MT_LoRa);    
  
        }
        
    }
finish:
#ifdef CONFIG_LORA_SX127X
    lora_receive(); 
#endif
    return;
}

void lora_messaging_init(char * _log_prefix){
    lora_messaging_log_prefix = _log_prefix;
}
