#include "i2c_messaging.h"
#include <sdkconfig.h>

#include "driver/i2c.h"
#include "esp32/rom/crc.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sdp_mesh.h>
#include <sdp_peer.h>
#include <sdp_messaging.h>

#include <string.h>

#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */


#define I2C_TIMEOUT_MS 1000

// TODO: What is the point of WRITE_BIT?



#define I2C_TX_BUF 1000 /*!< I2C master doesn't need buffer */
#define I2C_RX_BUF 1000 /*!< I2C master doesn't need buffer */


#if CONFIG_I2C_ADDR == -1
#error "I2C - An I2C address must be set in menuconfig!"
#endif

/* The log prefix for all logging */
char *i2c_messaging_log_prefix;

int i2c_unknown_counter = 0;

uint8_t *rcv_data;


/**
 * @brief i2c initialization
 */
static esp_err_t i2c_driver_init(bool is_master)
{

    if (is_master)
    {

        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = CONFIG_I2C_SDA_IO,
            .scl_io_num = CONFIG_I2C_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = CONFIG_I2C_MAX_FREQ_HZ,
            .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
        };
        ESP_ERROR_CHECK(i2c_param_config(CONFIG_I2C_CONTROLLER_NUM, &conf));

        ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - Installing driver");
        return i2c_driver_install(CONFIG_I2C_CONTROLLER_NUM, conf.mode, 0, 0, 0);
    }
    else
    {

        i2c_config_t conf = {
            .mode = I2C_MODE_SLAVE,
            .sda_io_num = CONFIG_I2C_SDA_IO,
            .scl_io_num = CONFIG_I2C_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            //.master.clk_speed = I2C_FREQ_HZ,
            .slave.slave_addr = CONFIG_I2C_ADDR,
            .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
        };
        ESP_ERROR_CHECK(i2c_param_config(CONFIG_I2C_CONTROLLER_NUM, &conf));

        ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - Installing driver");
        return i2c_driver_install(CONFIG_I2C_CONTROLLER_NUM, conf.mode, I2C_TX_BUF, I2C_RX_BUF, 0);
    }
}

/**
 * @brief  Calculate a reasonable wait time in milliseconds 
 * based on I2C frequency and expected response timeout
 * Note that logging may cause this to be too short
 * @param data_length Length of data to be transmitted
 * @return int The timeout needed.
 */
int calc_timeout_ms(uint32_t data_length) {
    int timeout_ms = (data_length / (CONFIG_I2C_MAX_FREQ_HZ/16) * 1000) + CONFIG_I2C_ACKNOWLEGMENT_TIMEOUT_MS;
    ESP_LOGI(i2c_messaging_log_prefix, "I2C - Timeout calculated to %i ms.", timeout_ms);
    return timeout_ms;
}


int i2c_send_message(sdp_peer *peer, char *data, int data_length) {

    ESP_LOGI(i2c_messaging_log_prefix, ">> I2C send message to %hhu,  %i bytes.", peer->i2c_address, data_length);
    unsigned int crc_in = crc32_be(0, (uint8_t *)data, data_length);
    esp_err_t ret = ESP_FAIL;
    
    ret = i2c_driver_delete(CONFIG_I2C_CONTROLLER_NUM);
    if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(i2c_messaging_log_prefix, ">> Deleting driver caused an invalid arg-error.");

    }

    // Init as a master.
    ESP_ERROR_CHECK(i2c_driver_init(true));

    uint64_t starttime;
    starttime = esp_timer_get_time();

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (peer->i2c_address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write(cmd, (uint8_t *)data, data_length, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    
    int retries = 0;
    // Check if SDA is HIGH (then we can send)
    if (gpio_get_level(CONFIG_I2C_SDA_IO) == 1)
    {
        gpio_set_level(CONFIG_I2C_SDA_IO, 0);
        
        ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - >> SDA was high, now set to low.");
        
        do {
            ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - >> Sending, try %i.", retries);
            ret = i2c_master_cmd_begin(CONFIG_I2C_CONTROLLER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
            if (ret != ESP_OK)
            {
                ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - >> Send failure, code %i.", ret);
                vTaskDelay(I2C_TIMEOUT_MS / 2 / portTICK_PERIOD_MS);
            }
            retries++;
        } while ((ret != 0) && (retries < 4));
        
            
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - >> %d byte packet sent...speed %f byte/s, air time: %lli , return value: %i", data_length,
                        (float)(data_length / ((float)(esp_timer_get_time() - starttime)) * 1000000), esp_timer_get_time() - starttime, ret);
            vTaskDelay(calc_timeout_ms(data_length)/ portTICK_PERIOD_MS);
            uint8_t *rcv_data = malloc(8);
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (peer->i2c_address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
            i2c_master_read(cmd, rcv_data, 3, ACK_VAL);
            i2c_master_read_byte(cmd, rcv_data + 3, NACK_VAL);
            i2c_master_stop(cmd);

            ESP_LOGI(i2c_messaging_log_prefix, "I2C Master  - << Reading from client.");
            int retries = 0;
            
            do {
                ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - << Reading, try %i.", retries + 1);
                ret = i2c_master_cmd_begin(CONFIG_I2C_CONTROLLER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - << Read failure, code %i. Waiting a short while.", ret);
                    vTaskDelay(I2C_TIMEOUT_MS / 2 / portTICK_PERIOD_MS);
                }
                retries++;
            } while ((ret != ESP_OK) && (retries < 3));

            i2c_cmd_link_delete(cmd);

            if (ret == ESP_OK)
            {
                unsigned int crc_response;
                memcpy(&crc_response, rcv_data, 4);
                if (crc_in == crc_response) {
                    ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - << Got a matching crc %u.", crc_response);
                } else {
                    ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - << Got a bad crc: %u! (correct would be %u",
                            crc_response, crc_in);
                    ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, rcv_data, 4, ESP_LOG_ERROR);
                }
                
                
            }
        }
        else
        {
            ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - >> Send failure after retries! Code: %i", ret);
        }
    } else {
        ESP_LOGW(i2c_messaging_log_prefix, "I2C Master - >> SDA was low, seems we have to listen first..");
    }
    ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - Deleting driver");
    i2c_driver_delete(CONFIG_I2C_CONTROLLER_NUM);
    // Go into slave mode
    ESP_ERROR_CHECK(i2c_driver_init(false));
    
    return ESP_OK;
}

void i2c_do_on_work_cb(i2c_queue_item_t *work_item) {
    ESP_LOGI(i2c_messaging_log_prefix, ">> In i2c work callback.");

    // TODO: Should we add a check for received here, or is it close enough after the poll?
    i2c_send_message(work_item->peer, work_item->data, work_item->data_length);

}

void i2c_do_on_poll_cb(queue_context *q_context) {

    //ESP_ERROR_CHECK(i2c_driver_init(false));
        
    //ESP_LOGE(i2c_messaging_log_prefix, ">> In I2Cdo on poll");
   
    int ret;

    //ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - Reading the buffer_");

    

    int read_so_far = 0;
    do
    {
   
        ret = i2c_slave_read_buffer(CONFIG_I2C_CONTROLLER_NUM, rcv_data + read_so_far, I2C_RX_BUF - read_so_far,
                                    20 / portTICK_PERIOD_MS);
        if (ret < 0) {
            ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - << Error %i reading buffer", ret);
        } 
        read_so_far = read_so_far + ret;
        if (read_so_far > I2C_RX_BUF)
        {
            // TODO: Add another buffer, chain them or something, obviously we need to be able to receive much more data
            ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - << Got too much data: %i bytes", read_so_far);
        }
    } while (ret > 0); // Continue as long as there is a result.

    if (read_so_far > 0)
    {
        unsigned int crc = crc32_be(0, rcv_data, read_so_far);
        ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - << Got %i bytes of data. crc32 : %u", read_so_far, crc);

        ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - >> Creating a response:");

        int response_len = read_so_far;

        ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, &crc, 4, ESP_LOG_INFO);
        
        ret = i2c_slave_write_buffer(CONFIG_I2C_CONTROLLER_NUM, &crc, 4,
                                        I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
        if (ret < 0)
        {
            ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - >> Got an error from sending back data: %i", ret);
        }
        else
        {
            ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - >> Sent a %i bytes with length of %i bytes and crc of %u. ", ret, response_len, crc);
        }
    }
    if (ret < 0)
    {
        ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - << Got an error from receiving data: %i", ret);
    }

    //ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - Deleting driver");
   // i2c_driver_delete(CONFIG_I2C_CONTROLLER_NUM);


    #if 0
    
    // TODO: Should the delay here should be related to the speed of the connection? 
    // Wait a moment to let any response in.
    
	if (i2c_received()) {
        uint8_t buf[256]; // Maximum Payload size of SX1276/77/78/79 is 256 bytes   
        int message_length = 0;
        int receive_len = 0;
        bool more_data = true;
        while (more_data) {
            receive_len = i2c_receive_packet(&buf + message_length, sizeof(buf));
            message_length += receive_len;
            // TODO: Here, the delay here should certainly be related to the speed of the connection
            // TODO: Test without retries, this entire while might be pointless as its always full packets?
            vTaskDelay(100/portTICK_PERIOD_MS);
            more_data = receive_len > 0;
        }
        
        ESP_LOGI(i2c_messaging_log_prefix, "<< In i2c POLL callback;i2c_received %i bytes.", message_length);
        ESP_LOGI(i2c_messaging_log_prefix, "<< Received data (including all) preamble): ");
        ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, &buf,message_length, ESP_LOG_INFO);  
        ESP_LOGI(i2c_messaging_log_prefix, "<< sdp_host.base_mac_address: ");
        ESP_LOG_BUFFER_HEX(i2c_messaging_log_prefix, &sdp_host.base_mac_address,SDP_MAC_ADDR_LEN);
        
        // TODO: Do some kind of better non-hardcoded length check. Perhaps it just has to be longer then the mac address?
        if (message_length > 9) {
            
            sdp_mac_address *src_mac_addr = NULL;
            uint8_t data_start = 0;
            // TODO: Add a near match on long ranges (all bits but two needs to match, emergency messaging?) 
            if (memcmp(&sdp_host.base_mac_address, &buf, SDP_MAC_ADDR_LEN) == 0)
            {
                // So, the first 
                src_mac_addr = buf + SDP_MAC_ADDR_LEN;
                data_start = (SDP_MAC_ADDR_LEN *2);
            } else {
                uint32_t relation_id = malloc(4);
                memcpy(&relation_id, buf, 4);
                ESP_LOGI(i2c_messaging_log_prefix, "Relation id in first bytes %u", relation_id);
                src_mac_addr = relation_id_to_mac_address(relation_id);
                if (src_mac_addr == NULL) {
                    ESP_LOGI(i2c_messaging_log_prefix, "<< %d byte packet to someone else received:[%.*s], RSSI %i", 
                        message_length, message_length, (char *)&buf , i2c_packet_rssi());
                    goto finish;
                }
                data_start = sizeof(uint32_t);
            }

            ESP_LOGI(i2c_messaging_log_prefix, "<< %d byte packet received:[%.*s], RSSI %i", 
            message_length, message_length, (char *)&buf , i2c_packet_rssi());

            sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address(src_mac_addr);
            if (!peer) {
                char *new_name;
                asprintf(&new_name, "UNKNOWN_%i", i2c_unknown_counter++);
                
                peer = sdp_add_init_new_peer(new_name, src_mac_addr, SDP_MT_i2c);
                
            }
            handle_incoming(peer, buf + data_start, message_length - data_start, SDP_MT_i2c);    
  
        }
        
    }

finish:
    i2c_receive(); 
     #endif   
}

void i2c_messaging_init(char * _log_prefix){
    i2c_messaging_log_prefix = _log_prefix;
    rcv_data = malloc(I2C_RX_BUF);
    ESP_ERROR_CHECK(i2c_driver_init(false));
}
