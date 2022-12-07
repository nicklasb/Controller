#include "sdkconfig.h"
#ifdef CONFIG_SDP_LOAD_ESP_NOW

#include "espnow.h"

//#include "../secret/local_settings.h"


#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <sdp_def.h>

#include <espnow/espnow_messaging.h>
#include <espnow/espnow_peer.h>

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    #include <driver/adc.h>
#else
    #include <esp_adc/adc_oneshot.h>
#endif 

/* The log prefix for all logging */
char *log_prefix;

void init_wifi() {

    ESP_LOGI(log_prefix, "Initializing wifi (for ESP-NOW)");
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}



void espnow_shutdown() {
    ESP_LOGI(log_prefix, "Shutting down ESP-NOW:");
    ESP_LOGI(log_prefix, " - wifi stop");
    esp_wifi_stop();
    ESP_LOGI(log_prefix, " - wifi disconnect");
    esp_wifi_disconnect(); 
    esp_wifi_set_mode(WIFI_MODE_NULL);   
    ESP_LOGI(log_prefix, " - wifi deinit");   
    esp_wifi_deinit();
    ESP_LOGI(log_prefix, " - power of adc");   
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    adc_power_release();

#endif 

    
    


    ESP_LOGI(log_prefix, "ESP-NOW shut down.");
}

/**
 * @brief Initialize ESP-NOW
 * 
 * @param _log_prefix 
 * @param is_controller 
 */
void espnow_init(char * _log_prefix) {
    log_prefix = _log_prefix;
    ESP_LOGI(log_prefix, "Initializing ESP-NOW.");
    init_wifi();
    
    sdp_mac_address wifi_mac_addr;
    esp_wifi_get_mac(ESP_IF_WIFI_STA, wifi_mac_addr);
    ESP_LOGI(log_prefix, "WIFI base MAC address:");
    ESP_LOG_BUFFER_HEX(log_prefix, wifi_mac_addr, SDP_MAC_ADDR_LEN);  
    memcpy(sdp_host.base_mac_address, wifi_mac_addr, SDP_MAC_ADDR_LEN);
    espnow_messaging_init(_log_prefix);
    espnow_peer_init(_log_prefix);
    ESP_LOGI(log_prefix, "ESP-NOW initialized.");
}

#endif