#include "lora_messaging.h"

#include "lora.h"
#include <esp_log.h>
#include <esp_timer.h>

/* The log prefix for all logging */
char *lora_messaging_log_prefix;

int lora_send_message(sdp_mac_address *dest_mac_address, char *data, int data_length) {

	// Maximum Payload size of SX1276/77/78/79 is 255
    if (data_length > 256) {
		ESP_LOGE(lora_messaging_log_prefix, "Message too long: %i", data_length);
        return SDP_ERR_MESSAGE_TOO_LONG;
    }
// TODO: Make some way of handling longer messages

	uint64_t starttime;
	int tx_count = 0;

	tx_count++;
	starttime = esp_timer_get_time();
	ESP_LOGI(lora_messaging_log_prefix, "Sending message: \"%.*s\", total %d bytes...", data_length-4, data+4, data_length);
	lora_send_packet((uint8_t *)data, data_length);
	ESP_LOGI(lora_messaging_log_prefix, "%d byte packet sent...speed %f byte/s", data_length, 
	(float)data_length/((float)(esp_timer_get_time()-starttime))*1000000);

	return ESP_OK;

}

void lora_messaging_init(char * _log_prefix){
    lora_messaging_log_prefix = _log_prefix;
}
