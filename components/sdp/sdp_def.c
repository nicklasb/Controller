#include "sdp_def.h"
#include "esp_log.h"
sdp_media_types host_supported_media_types = 0;

sdp_media_types get_host_supported_media_types() {
    return host_supported_media_types;
}

void add_host_supported_media_type (e_media_type supported_media_type) {
    host_supported_media_types |= supported_media_type;
}





