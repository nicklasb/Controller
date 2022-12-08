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

#include "lora_init.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//#include "../secret/local_settings.h"

#include <esp_log.h>
#include "lora.h"
#include <sdp_def.h>
#include "lora_messaging.h"

/* The log prefix for all logging */
char *log_prefix;


/**
 * @brief Initiate LoRa
 * 
 */
void init_lora() {

    ESP_LOGI(log_prefix, "Initializing LoRa");
	if (lora_init_local() == 0) {
		ESP_LOGE(log_prefix, "Does not recognize the module");
		while(1) {
			vTaskDelay(1);
		}
	}
    #if CONFIG_LORA_FREQUENCY_169MHZ
        ESP_LOGI(log_prefix, "Frequency is 169MHz");
        lora_set_frequency(169e6); // 169MHz
    #elif CONFIG_LORA_FREQUENCY_433MHZ
        ESP_LOGI(log_prefix, "Frequency is 433MHz");
        lora_set_frequency(433e6); // 433MHz
    #elif CONFIG_LORA_FREQUENCY_470MHZ
        ESP_LOGI(log_prefix, "Frequency is 470MHz");
        lora_set_frequency(470e6); // 470MHz
    #elif CONFIG_LORA_FREQUENCY_866MHZ
        ESP_LOGI(log_prefix, "Frequency is 866MHz");
        lora_set_frequency(866e6); // 866MHz
    #elif CONFIG_LORA_FREQUENCY_915MHZ
        ESP_LOGI(log_prefix, "Frequency is 915MHz");
        lora_set_frequency(915e6); // 915MHz
    #elif CONFIG_LORA_FREQUENCY_OTHER
        ESP_LOGI(log_prefix, "Frequency is %d MHz", CONFIG_LORA_OTHER_FREQUENCY);
        long frequency = CONFIG_LORA_OTHER_FREQUENCY * 1000000;
        lora_set_frequency(frequency);
    #endif


	lora_enable_crc();

	int cr = 1;
	int bw = 7;
	int sf = 7;
#if CONFIG_LORA_ADVANCED
	cr = CONFIG_LORA_CODING_RATE;
	bw = CONFIG_LORA_BANDWIDTH;
	sf = CONFIG_LORA_SF_RATE;
#endif
	lora_set_tx_power(2);
	lora_set_coding_rate(cr);
	//lora_set_coding_rate(CONFIG_CODING_RATE);
	//cr = lora_get_coding_rate();
	ESP_LOGI(log_prefix, "coding_rate=%d", cr);

	lora_set_bandwidth(bw);
	//lora_set_bandwidth(CONFIG_BANDWIDTH);
	//int bw = lora_get_bandwidth();
	ESP_LOGI(log_prefix, "bandwidth=%d", bw);

	lora_set_spreading_factor(sf);
	//lora_set_spreading_factor(CONFIG_SF_RATE);
	//int sf = lora_get_spreading_factor();
	ESP_LOGI(log_prefix, "spreading_factor=%d", sf);

}



void lora_shutdown() {
    ESP_LOGI(log_prefix, "Shutting down LoRa:");

    ESP_LOGI(log_prefix, "ESP-NOW shut down.");
}

/**
 * @brief Initialize LoRa
 * 
 * @param _log_prefix 

 */
void lora_init(char * _log_prefix) {
    log_prefix = _log_prefix;
    ESP_LOGI(log_prefix, "Initializing LoRa");
    init_lora();
    lora_messaging_init(log_prefix);

    ESP_LOGI(log_prefix, "LoRa initialized.");
}

#endif