#include "sdkconfig.h"
#if CONFIG_SDP_LOAD_I2C

#include "i2c.h"
#include "esp_log.h"

#include "string.h"
#include "esp32/rom/crc.h"
#include "inttypes.h"

#include "i2c_worker.h"
#include "i2c_messaging.h"
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


#if 0
void i2c_start()
{
    uint32_t send_len = TEST_DATA_LENGTH_KB * TEST_DATA_MULTIPLIER;
    uint8_t *send_data = malloc(send_len);
    char pattern[10] = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09";
    for (int i = 0; i < (send_len / 10); i++)
    {
        memcpy(send_data + (i * 10), &pattern, 10);
    }

    ESP_LOGI(i2c_log_prefix, "I2C - Initiated %i bytes of data, crc32 :%u", send_len, crc32_be(0, send_data, send_len));
    if (send_len <= 1000)
    {
        ESP_LOG_BUFFER_HEXDUMP(i2c_log_prefix, send_data, send_len, ESP_LOG_INFO);
    }

    uint8_t *rcv_data = malloc(send_len);
    int read_len = send_len;

    
    while (1)
    {
        // Init as a master.
        ESP_ERROR_CHECK(i2c_driver_init(true));

        uint64_t starttime;
        starttime = esp_timer_get_time();

        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (CONFIG_I2C_ADDR << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
        i2c_master_write(cmd, send_data, send_len, ACK_CHECK_EN);
        i2c_master_stop(cmd);

        esp_err_t ret = ESP_FAIL;
        int retries = 0;
        // Check if SDA is HIGH (then we can send)
        if (gpio_get_level(CONFIG_I2C_SDA_IO) == 1)
        {
            gpio_set_level(CONFIG_I2C_SDA_IO, 0);
            
            ESP_LOGI(i2c_log_prefix, "I2C Master - >> SDA was high, now set to low.");
            
            do {
                ESP_LOGI(i2c_log_prefix, "I2C Master - >> Sending, try %i.", retries);
                ret = i2c_master_cmd_begin(CONFIG_I2C_CONTROLLER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(i2c_log_prefix, "I2C Master - >> Send failure, code %i.", ret);
                    vTaskDelay(I2C_TIMEOUT_MS / 2 / portTICK_PERIOD_MS);
                }
                retries++;
            } while ((ret != 0) && (retries < 4));
            
             
            i2c_cmd_link_delete(cmd);

            if (ret == ESP_OK)
            {
                ESP_LOGI(i2c_log_prefix, "I2C Master - >> %d byte packet sent...speed %f byte/s, air time: %lli , return value: %i", send_len,
                         (float)(send_len / ((float)(esp_timer_get_time() - starttime)) * 1000000), esp_timer_get_time() - starttime, ret);
                vTaskDelay(calc_timeout_ms(send_len)/ portTICK_PERIOD_MS);

                i2c_cmd_handle_t cmd = i2c_cmd_link_create();
                i2c_master_start(cmd);
                i2c_master_write_byte(cmd, (CONFIG_I2C_ADDR << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
                i2c_master_read(cmd, rcv_data, 7, ACK_VAL);
                i2c_master_read_byte(cmd, rcv_data + 7, NACK_VAL);
                i2c_master_stop(cmd);
                //memset(rcv_data, 1, read_len);

                ESP_LOGI(i2c_log_prefix, "I2C Master  - << Reading from client.");
                int retries = 0;
                
                do {
                    ESP_LOGI(i2c_log_prefix, "I2C Master - << Reading, try %i.", retries + 1);
                    ret = i2c_master_cmd_begin(CONFIG_I2C_CONTROLLER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
                    if (ret != ESP_OK)
                    {
                        ESP_LOGE(i2c_log_prefix, "I2C Master - << Read failure, code %i. Waiting a short while.", ret);
                        vTaskDelay(I2C_TIMEOUT_MS / 2 / portTICK_PERIOD_MS);
                    }
                    retries++;
                } while ((ret != ESP_OK) && (retries < 3));

                i2c_cmd_link_delete(cmd);

                if (ret == ESP_OK)
                {
                    ESP_LOGI(i2c_log_prefix, "I2C Master - << Got a crc %u  and length %u.",
                             *(unsigned int *)&rcv_data[0], *(unsigned int *)&rcv_data[4]);
                    
                    if ((send_len) <= 1000)
                    {
                        ESP_LOG_BUFFER_HEXDUMP(i2c_log_prefix, rcv_data, 8, ESP_LOG_INFO);
                    }
                    
                }
            }
            else
            {
                ESP_LOGE(i2c_log_prefix, "I2C Master - >> Send failure after retries! Code: %i", ret);
            }
        } else {
            ESP_LOGW(i2c_log_prefix, "I2C Master - >> SDA was low, seems we have to listen first..");
        }
        ESP_LOGI(i2c_log_prefix, "I2C Master - Deleting driver");
        i2c_driver_delete(CONFIG_I2C_CONTROLLER_NUM);
        // Go into slave mode
        ESP_ERROR_CHECK(i2c_driver_init(false));
        
        for (int i = 0; i < 2; i++)
        {
            ESP_LOGI(i2c_log_prefix, "I2C Slave - Listening");
        
            int read_so_far = 0;
 
            do
            {
                ret = i2c_slave_read_buffer(CONFIG_I2C_CONTROLLER_NUM, rcv_data + read_so_far, read_len - read_so_far,
                                            I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
                if (ret < 0) {
                    ESP_LOGE(i2c_log_prefix, "I2C Slave - << Error %i reading buffer", ret);
                } 
                read_so_far = read_so_far + ret;
                if (read_so_far > read_len)
                {
                    ESP_LOGE(i2c_log_prefix, "I2C Slave - << Got too much data: %i bytes", read_so_far);
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            } while (ret > 0); // Continue as long as there is a result.

            if (read_so_far > 0)
            {
                unsigned int crc = crc32_be(0, rcv_data, read_so_far);
                ESP_LOGI(i2c_log_prefix, "I2C Slave - << Got %i bytes of data. crc32 : %u", read_so_far, crc);

                ESP_LOGI(i2c_log_prefix, "I2C Slave - >> Creating a response:");

                int response_len = read_so_far;
                unsigned int response_buf[2];
                response_buf[0] = crc;
                response_buf[1] = (unsigned int)read_so_far;
                ESP_LOG_BUFFER_HEXDUMP(i2c_log_prefix, &response_buf, 8, ESP_LOG_INFO);
                
                ret = i2c_slave_write_buffer(CONFIG_I2C_CONTROLLER_NUM, &response_buf, 8,
                                             I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
                if (ret < 0)
                {
                    ESP_LOGE(i2c_log_prefix, "I2C Slave - >> Got an error from sending back data: %i", ret);
                }
                else
                {
                    ESP_LOGI(i2c_log_prefix, "I2C Slave - >> Sent a %i bytes with length of %i bytes and crc of %u. ", ret, response_len, crc);
                }
            }
            if (ret < 0)
            {
                ESP_LOGI(i2c_log_prefix, "I2C Slave - << Got an error from receiving data: %i", ret);
            }
            vTaskDelay(TEST_DELAY_MS / portTICK_PERIOD_MS);
        }
        ESP_LOGI(i2c_log_prefix, "I2C Master - Deleting driver");
        i2c_driver_delete(CONFIG_I2C_CONTROLLER_NUM);
    }
};
#endif

void i2c_shutdown() {
    ESP_LOGI(i2c_log_prefix, "Shutting down i2c:");
    //TODO: Something needed here?
    ESP_LOGI(i2c_log_prefix, "ESP-NOW shut down.");
}


/**
 * @brief Initiate i2c
 * 
 */
void init_i2c() {

}

/**
 * @brief Initialize i2c
 * 
 * @param _log_prefix 

 */
void i2c_init(char * _log_prefix) {
    i2c_log_prefix = _log_prefix;
    ESP_LOGI(i2c_log_prefix, "Initializing i2c");
    i2c_messaging_init(i2c_log_prefix);
    init_i2c();
    if (i2c_init_worker(&i2c_do_on_work_cb, NULL, &i2c_do_on_poll_cb, i2c_log_prefix) != ESP_OK)
    {
       ESP_LOGE(i2c_log_prefix, "Failed initializing i2c"); 
       return;
    }
    i2c_set_queue_blocked(false);
    

    //i2c_receive();

    ESP_LOGI(i2c_log_prefix, "I2C initialized.");
}


#endif