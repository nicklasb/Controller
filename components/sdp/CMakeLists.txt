FILE(GLOB sdp_sources 
"./*.c" "./*.h"
"sleep/sleep.h" "sleep/sleep.c"
"orchestration/orchestration.h" "orchestration/orchestration.c"
"monitor/monitor.h" "monitor/monitor.c"
)

if (CONFIG_SDP_LOAD_UMTS)
    FILE(GLOB tmp_sources "gsm/*.*")
    LIST(APPEND sdp_sources ${tmp_sources})
endif()

if (CONFIG_SDP_LOAD_ESP_NOW)
    FILE(GLOB tmp_sources "espnow/*.*")
    LIST(APPEND sdp_sources ${tmp_sources})
endif()

if (CONFIG_SDP_LOAD_BLE)
    FILE(GLOB tmp_sources "ble/*.*")
    LIST(APPEND sdp_sources ${tmp_sources})
endif()

if (CONFIG_SDP_LOAD_LORA)
    FILE(GLOB tmp_sources "lora/*.*")
    LIST(APPEND sdp_sources ${tmp_sources})
endif()

idf_component_register(SRCS ${sdp_sources} 
INCLUDE_DIRS . 
REQUIRES driver esp_netif nvs_flash esp_timer esp_wifi #mqtt esp_modem esp_adc #bt
)
