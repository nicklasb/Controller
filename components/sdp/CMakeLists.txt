FILE(GLOB sdp_sources 
"./*.*"
"sleep/sleep.h" "sleep/sleep.c"
"orchestration/orchestration.h" "orchestration/orchestration.c"
"monitor/monitor.h" "monitor/monitor.c"
"gsm/gsm.h" "gsm/gsm.c"
"espnow/espnow.h" "espnow/espnow.c"
"ble/ble_init.h" "ble/ble_init.c"
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

idf_component_register(SRCS ${sdp_sources} 
INCLUDE_DIRS . 
REQUIRES bt esp-modem mqtt esp_netif )
