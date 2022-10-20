#include "espnow_init.h"

//#include "../secret/local_settings.h"
#include "sdkconfig.h"
#ifdef CONFIG_SDP_LOAD_ESP_NOW

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include "sdp_def.h"

#include "espnow_messaging.h"
#include "espnow_peer.h"




/* The log prefix for all logging */
char *log_prefix;

void init_wifi() {

    ESP_LOGI(log_prefix, "Initializing wifi (for ESP-NOW)");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
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




/**
 * @brief Initialize ESP-NOW
 * 
 * @param _log_prefix 
 * @param is_controller 
 */
void espnow_init(char * _log_prefix, bool is_controller) {
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