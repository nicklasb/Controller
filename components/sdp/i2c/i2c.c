#include "sdkconfig.h"
#if CONFIG_SDP_LOAD_I2C

#include "i2c.h"
#include "esp_log.h"

#include "string.h"
#include "esp32/rom/crc.h"
#include "inttypes.h"

#include "i2c_worker.h"
#include "i2c_messaging.h"
#include "i2c_peer.h"
#include "sdp_def.h"
#define I2C_TIMEOUT_MS 80

// TODO: What is the point of WRITE_BIT?



#define TEST_DATA_LENGTH_KB 10
#define TEST_DATA_MULTIPLIER 100

#define I2C_TX_BUF_KB (TEST_DATA_LENGTH_KB * TEST_DATA_MULTIPLIER) /*!< I2C master doesn't need buffer */
#define I2C_RX_BUF_KB (TEST_DATA_LENGTH_KB * TEST_DATA_MULTIPLIER) /*!< I2C master doesn't need buffer */

#define TEST_DELAY_MS 100
#if (TEST_DATA_LENGTH_KB * TEST_DATA_MULTIPLIER) % 10 != 0
#error "TEST_DATA_LENGTH_KB * TEST_DATA_MULTIPLIER must be divideable by 10!"
#endif

#if CONFIG_I2C_ADDR == -1
#error "I2C - An I2C address must be set in menuconfig!"
#endif

const char * i2c_log_prefix;


void i2c_shutdown() {
    ESP_LOGI(i2c_log_prefix, "Shutting down i2c:");
    //TODO: Something needed here?
    ESP_LOGI(i2c_log_prefix, "ESP-NOW shut down.");
}

/**
 * @brief Initialize i2c
 * 
 * @param _log_prefix 

 */
void i2c_init(char * _log_prefix) {
    i2c_log_prefix = _log_prefix;
    ESP_LOGI(i2c_log_prefix, "Initializing I2C");
    i2c_peer_init(i2c_log_prefix);
    i2c_messaging_init(i2c_log_prefix);
    
    if (i2c_init_worker(&i2c_do_on_work_cb, &i2c_do_on_poll_cb, i2c_log_prefix) != ESP_OK)
    {
       ESP_LOGE(i2c_log_prefix, "Failed initializing I2C"); 
       return;
    }
    i2c_set_queue_blocked(false);

    //i2c_receive();
    add_host_supported_media_type(SDP_MT_I2C);
    ESP_LOGI(i2c_log_prefix, "I2C initialized.");
}


#endif