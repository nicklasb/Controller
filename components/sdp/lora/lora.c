/**
 * @file lora_init.c
 * @author Nicklas Borjesson (nicklasb@gmail.com)
 * @brief LoRa (from "long range") is a physical proprietary radio communication technique. 
 * See wikipedia: https://en.wikipedia.org/wiki/LoRa
 * @version 0.1
 * @date 2022-12-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#include "sdkconfig.h"
#ifdef CONFIG_SDP_LOAD_LORA

#include "lora.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdp_def.h"

//#include "../secret/local_settings.h"

#include <esp_log.h>
#ifdef CONFIG_LORA_SX127X
#include "lora_sx127x_lib.h"
#endif
#ifdef CONFIG_LORA_SX126X
#include "lora_sx126x_lib.h"
#endif
#include "lora_peer.h"
#include <sdp_def.h>
#include <sdp_peer.h>
#include "lora_messaging.h"
#include "lora_worker.h"




/* The log prefix for all logging */
char *lora_log_prefix;

/**
 * @brief Initiate LoRa
 * 
 */
int init_lora() {

    ESP_LOGI(lora_log_prefix, "Initializing LoRa");
    
    #ifdef CONFIG_LORA_SX126X
    LoRaInit();
    #endif
	
    #ifdef CONFIG_LORA_SX127X
	if (lora_init_local() == 0) {
		ESP_LOGE(lora_log_prefix, "Does not recognize the module");
        return ESP_FAIL;
	}
    #endif
    #if CONFIG_LORA_FREQUENCY_169MHZ
        ESP_LOGI(lora_log_prefix, "Frequency is 169MHz");
        long frequency = 169e6; // 169MHz
    #elif CONFIG_LORA_FREQUENCY_433MHZ
        ESP_LOGI(lora_log_prefix, "Frequency is 433MHz");
        long frequency = 433e6; // 433MHz
    #elif CONFIG_LORA_FREQUENCY_470MHZ
        ESP_LOGI(lora_log_prefix, "Frequency is 470MHz");
        long frequency = 470e6; // 470MHz
    #elif CONFIG_LORA_FREQUENCY_866MHZ
        ESP_LOGI(lora_log_prefix, "Frequency is 866MHz");
        long frequency = 866e6; // 866MHz
    #elif CONFIG_LORA_FREQUENCY_915MHZ
        ESP_LOGI(lora_log_prefix, "Frequency is 915MHz");
        long frequency = 915e6; // 915MHz
    #elif CONFIG_LORA_FREQUENCY_OTHER
        ESP_LOGI(lora_log_prefix, "Frequency is %d MHz", CONFIG_LORA_OTHER_FREQUENCY);
        long frequency = CONFIG_LORA_OTHER_FREQUENCY * 1000000;
  
    #endif


	//lora_enable_crc();

	int cr = 1;
	int bw = 7;
	int sf = 7;
#if CONFIG_LORA_ADVANCED
    ESP_LOGI(lora_log_prefix, "Advanced LoRa settings");
	cr = CONFIG_LORA_CODING_RATE;
	bw = CONFIG_LORA_BANDWIDTH;
	sf = CONFIG_LORA_SF_RATE;
#endif

    #ifdef CONFIG_LORA_SX126X
	
    float tcxoVoltage = 3.3; // use TCXO
	bool useRegulatorLDO = true; // use TCXO

    int8_t txPowerInDbm = 22;
    LoRaDebugPrint(true); // TODO: Check out why this is needed.
    int ret = LoRaBegin(frequency, txPowerInDbm, tcxoVoltage, useRegulatorLDO);
    LoRaDebugPrint(false);
	ESP_LOGI(lora_log_prefix, "LoRaBegin=%d", ret);
	if (ret != 0) {
		ESP_LOGE(lora_log_prefix, "Does not recognize the module");
		while(1) {
			vTaskDelay(1);
		}
	}	
    #endif
    
    #ifdef CONFIG_LORA_SX127X    
    lora_set_frequency(frequency);
    lora_set_tx_power(1);
	lora_set_coding_rate(cr);
	lora_set_bandwidth(bw);
	lora_set_spreading_factor(sf);    
    #endif

    ESP_LOGI(lora_log_prefix, "coding_rate=%d", cr);
	ESP_LOGI(lora_log_prefix, "bandwidth=%d", bw);
	ESP_LOGI(lora_log_prefix, "spreading_factor=%d", sf);

    #ifdef CONFIG_LORA_SX126X

	uint16_t preambleLength = 8;
	uint8_t payloadLen = 0;
	bool crcOn = true;
	bool invertIrq = false;
    
	LoRaConfig(sf, bw, cr, preambleLength, payloadLen, crcOn, invertIrq);

    #endif
    return ESP_OK;
}

void lora_shutdown() {
    ESP_LOGI(lora_log_prefix, "Shutting down LoRa:");
    //TODO: Something needed here?
    ESP_LOGI(lora_log_prefix, "ESP-NOW shut down.");
}

/**
 * @brief Initialize LoRa
 * 
 * @param _log_prefix 

 */
void lora_init(char * _log_prefix) {
    lora_log_prefix = _log_prefix;
    ESP_LOGI(lora_log_prefix, "Initializing LoRa");
    lora_peer_init(lora_log_prefix);
    lora_messaging_init(lora_log_prefix);

    if (init_lora() != ESP_OK) {
       goto fail;
    }
    if (lora_init_worker(&lora_do_on_work_cb, &lora_do_on_poll_cb, lora_log_prefix) != ESP_OK)
    {
       ESP_LOGE(lora_log_prefix, "Failed initializing LoRa"); 
       goto fail;
    }
    lora_set_queue_blocked(false);
    #ifdef CONFIG_LORA_SX127X
    lora_receive();
    #endif
    #ifdef CONFIG_LORA_SX126X
    #endif
    add_host_supported_media_type(SDP_MT_LoRa);
    ESP_LOGI(lora_log_prefix, "LoRa initialized.");
fail:
    ESP_LOGI(lora_log_prefix, "LoRa failed to initialize.");
    // TODO: All medias need to have this handling.
    


}

#endif