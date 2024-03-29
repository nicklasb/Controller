#include "i2c_messaging.h"
#include <sdkconfig.h>

#include "driver/i2c.h"
#include "esp32/rom/crc.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sdp_mesh.h>
#include <sdp_peer.h>
#include <sdp_messaging.h>
#include "i2c_peer.h"

#include <string.h>

#ifdef CONFIG_SDP_SIM
#include "i2c_simulate.h"
#endif

#define ACK_CHECK_EN 0x1  /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0       /*!< I2C ack value */
#define NACK_VAL 0x1      /*!< I2C nack value */

#define I2C_TIMEOUT_MS 1000


#define I2C_TX_BUF 1000 /*!< I2C master doesn't need buffer */
#define I2C_RX_BUF 1000 /*!< I2C master doesn't need buffer */

#if CONFIG_I2C_ADDR == -1
#error "I2C - An I2C address must be set in menuconfig!"
#endif

/* The log prefix for all logging */
char *i2c_messaging_log_prefix;

uint32_t i2c_unknown_counter = 0;
uint32_t i2c_unknown_failures = 0;
uint32_t i2c_crc_failures = 0;


uint8_t *rcv_data;

/**
 * @brief i2c mode setting initialization
 *
 * @param is_master To set to master mode, set this to true. False for slave mode.
 * @param dont_delete Don't delete the driver (likely because it is the first call)
 * @return esp_err_t 
 */
esp_err_t i2c_driver_set_master(bool is_master, bool dont_delete)
{
    if (is_master) {
        ESP_LOGI(i2c_messaging_log_prefix, "Setting I2C driver to master mode.");
    } else {
        ESP_LOGI(i2c_messaging_log_prefix, "Setting I2C driver to slave mode.");
    }
    
    if (!dont_delete) {
        esp_err_t delete_ret = ESP_FAIL;
        delete_ret = i2c_driver_delete(CONFIG_I2C_CONTROLLER_NUM);
        if (delete_ret == ESP_ERR_INVALID_ARG)
        {
            ESP_LOGE(i2c_messaging_log_prefix, ">> Deleting driver caused an invalid arg-error.");
            return ESP_ERR_INVALID_ARG;
        }
    }

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

        ESP_LOGD(i2c_messaging_log_prefix, "I2C Master - Installing driver");
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
            .slave.addr_10bit_en = false,
            .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
        };
        ESP_ERROR_CHECK(i2c_param_config(CONFIG_I2C_CONTROLLER_NUM, &conf));

        ESP_LOGD(i2c_messaging_log_prefix, "I2C Slave - Installing driver");
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
int calc_timeout_ms(uint32_t data_length)
{
    int timeout_ms = (data_length / (CONFIG_I2C_MAX_FREQ_HZ / 16) * 1000) + CONFIG_I2C_ACKNOWLEGMENT_TIMEOUT_MS;
    ESP_LOGI(i2c_messaging_log_prefix, "I2C - Timeout calculated to %i ms.", timeout_ms);
    return timeout_ms;
}


int i2c_send_message(sdp_peer *peer, char *data, int data_length, bool just_checking)
{

    int retval = ESP_FAIL;
    ESP_LOGI(i2c_messaging_log_prefix, ">> I2C send message to %hhu,  %i bytes.", peer->i2c_address, data_length);
    uint32_t crc_msg = crc32_be(0, (uint8_t *)data + 4, data_length - 4);
    ESP_ERROR_CHECK(i2c_driver_set_master(true, false));

    uint64_t starttime;
    starttime = esp_timer_get_time();

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (peer->i2c_address << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, CONFIG_I2C_ADDR, ACK_CHECK_EN);
    i2c_master_write(cmd, (uint8_t *)data, data_length, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    // Check if SDA is HIGH (then we can send)
    if (gpio_get_level(CONFIG_I2C_SDA_IO) == 1)
    {
        gpio_set_level(CONFIG_I2C_SDA_IO, 0);

        ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - >> SDA was high, now set to low, sending.");
        gpio_set_level(CONFIG_I2C_SDA_IO, 0);
        int send_retries = 0;
        esp_err_t send_ret = ESP_FAIL;
        do
        {
            ESP_LOGD(i2c_messaging_log_prefix, "I2C Master - >> Sending, try %i.", send_retries + 1);
            send_ret = i2c_master_cmd_begin(CONFIG_I2C_CONTROLLER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
            if (send_ret != ESP_OK)
            {
                ESP_LOGD(i2c_messaging_log_prefix, "I2C Master - >> Send failure, code %i.", send_ret);
                vTaskDelay((I2C_TIMEOUT_MS/4) / portTICK_PERIOD_MS);
                // TODO: We need to follow up these small failures
            }

            send_retries++;
        } while ((send_ret != ESP_OK) && (send_retries < 4));

        i2c_cmd_link_delete(cmd);

        if (send_ret == ESP_OK)
        {
            float speed = (float)(data_length / ((float)(esp_timer_get_time() - starttime)) * 1000000);
            ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - >> %d byte packet sent...speed %f byte/s, air time: %lli , return value: %i", 
                data_length, speed, esp_timer_get_time() - starttime, send_ret);
            peer->i2c_stats.actual_speed = (peer->i2c_stats.actual_speed + speed) / 2;
            peer->i2c_stats.theoretical_speed = (CONFIG_I2C_MAX_FREQ_HZ / 2);

            vTaskDelay(calc_timeout_ms(data_length) / portTICK_PERIOD_MS);
            uint8_t *rcv_data = malloc(6);
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (peer->i2c_address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
            i2c_master_read(cmd, rcv_data, 5, ACK_VAL);
            i2c_master_read_byte(cmd, rcv_data + 5, NACK_VAL);
            i2c_master_stop(cmd);

            int read_retries = 0;
            int read_ret = ESP_FAIL;
            
            // TODO: This should probably use CONFIG_SDP_RECEIPT_TIMEOUT_MS * 1000 in some way.
            // And SDP_RECEIPT must be recalculated based on transmission speeds.
            // Also, the delay here is slightly strange. 
            vTaskDelay(30 / portTICK_PERIOD_MS);
            do
            {
                ESP_LOGD(i2c_messaging_log_prefix, "I2C Master - << Reading receipt, try %i.", read_retries + 1);
                read_ret = i2c_master_cmd_begin(CONFIG_I2C_CONTROLLER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
                if (read_ret != ESP_OK)
                {
                    ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - << Read receipt failure, code %i. Waiting a short while.", read_ret);
                    vTaskDelay((CONFIG_SDP_RECEIPT_TIMEOUT_MS / 4) / portTICK_PERIOD_MS);
                }
                read_retries++;
            } while ((read_ret != ESP_OK) && (read_retries < 3)); // We always retry reading three times. TODO: Why?

            i2c_cmd_link_delete(cmd);

            if (read_ret == ESP_OK)
            {
                uint32_t crc_response;
                //
                if ((rcv_data[0] == 0xff) && (rcv_data[1] == 0x00)) {
                    ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - << Slave says it was correct");

                    memcpy(&crc_response, rcv_data + 2, 4);

                    #ifdef CONFIG_SDP_SIM_I2C_BAD_CRC
                        crc_response = sim_bad_crc(crc_response);
                    #endif

                    if (crc_msg == crc_response)
                    {
                        ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - << Got a matching crc %"PRIu32".", crc_response);
                        ESP_LOGI(i2c_messaging_log_prefix, "I2C Master - II %"PRIu32"", peer->i2c_stats.send_successes);
                        retval = ESP_OK;
                    }
                    else
                    {
                        ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - << Got a bad crc: %"PRIu32"! (correct would be %"PRIu32"",
                                crc_response, crc_msg);
                        ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, rcv_data, 6, ESP_LOG_ERROR);
                        i2c_crc_failures++;
                        retval = -SDP_ERR_SEND_FAIL;
                    }
                } else if ((rcv_data[0] == 0x0) && (rcv_data[1] == 0xff)) {
                    ESP_LOGW(i2c_messaging_log_prefix, "I2C Master - << Slave says it was wrong ");
                    ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, rcv_data, 6, ESP_LOG_INFO);
                    retval = -SDP_ERR_SEND_FAIL;
                } else {
                    ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - << We need look a bit closer ");
                    ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, rcv_data, 6, ESP_LOG_ERROR);
                    // TODO: Analyze the responses
                    // For now return send failed but later be more detailed, perhaps lower the speed.
                    retval = -SDP_ERR_SEND_FAIL;

                }
            }
            else
            {
                ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - >> Failed to read receipt after %i retries. Error code: %i.", 
                    read_retries, read_ret);
                retval = -SDP_ERR_SEND_FAIL;
            }
        }
        else
        {
            if (!just_checking) {
                ESP_LOGE(i2c_messaging_log_prefix, "I2C Master - >> Failed to send after %i retries . Code: %i", send_retries, send_ret);
            }
            
            retval = -SDP_ERR_SEND_FAIL;
        }
    }
    else
    {
        ESP_LOGW(i2c_messaging_log_prefix, "I2C Master - >> SDA was low, seems we have to listen first..");
    }

    if (retval != ESP_OK)
    {
        // We have failed sending data to this peer, report it.
        peer->i2c_stats.send_failures++;
    }
    else
    {
        peer->i2c_stats.send_successes++;
        ESP_LOGI(i2c_messaging_log_prefix, "I2C Master Peer name: %s - II 2 %"PRIu32"", peer->name, peer->i2c_stats.send_successes);
    }

    ESP_ERROR_CHECK(i2c_driver_set_master(false, false));

    return retval;
}


void i2c_do_on_poll_cb(queue_context *q_context)
{

    int retries = 0;

    int ret;
    int data_len = 0;
    do
    {
        if (retries > 0)
        {
            vTaskDelay(1);
        }
        do
        {
            ret = i2c_slave_read_buffer(CONFIG_I2C_CONTROLLER_NUM, rcv_data + data_len, I2C_RX_BUF - data_len,
                                        10 / portTICK_PERIOD_MS);
            if (ret < 0)
            {
                ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - << Error %i reading buffer", ret);
            }
            data_len = data_len + ret;
            if (data_len > I2C_RX_BUF)
            {
                // TODO: Add another buffer, chain them or something, obviously we need to be able to receive much more data
                ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - << Got too much data: %i bytes", data_len);
            }
        } while (ret > 0); // Continue as long as there is a result.
        retries++;
    } while ((data_len == 0) && (retries < 3)); // Continue as long as there is a result.

    // It has to be at least longer than the preamble and the address byte.
    if (data_len > SDP_PREAMBLE_LENGTH + 1)
    {
        ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - << Got data, length %i bytes.", data_len);
        uint8_t i2c_address = (uint8_t)rcv_data[0];
        uint32_t crc32_in = 0;
        memcpy(&crc32_in, rcv_data + 1, 4);
        uint32_t crc_calc = crc32_be(0, rcv_data + 5, data_len - 5);

        // TODO: It is not optimal to do this here, the lookup may be really fast, but the receipt should be immidiate. 
        // Probably the logging above needs to go as well.
        sdp_peer *peer = sdp_mesh_find_peer_by_i2c_address(i2c_address);

        uint8_t response[6];
        if (crc32_in != crc_calc)
        {
            ESP_LOGW(i2c_messaging_log_prefix, "I2C Slave - << CRC Mismatch crc32_in: %"PRIu32",crc_calc: %"PRIu32". Create response:", crc32_in, crc_calc);
            response[0] = 0x00;
            response[1] = 0xff;
            if (peer) {
                /**
                 * @brief We only log on an existing peer, as there is a number of peers/256 risk that 
                 * also the source address is wrong, as the message failed crc check. 
                 * However, the likelyhood of several conflicts is very low.
                 * Also, new peers are only added if the CRC is correct, and in that case it is unlikely that errors would just 
                 * concern the adress. Also, the HI message contains the mac_address. 
                 */
                peer->i2c_stats.receive_failures++;
                
            } else {
                i2c_unknown_failures++;

            }
            ret = ESP_FAIL;
        }  else {
            ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - << Got %i bytes of data from %hhu. crc32 : %"PRIu32", create response.", data_len, i2c_address, crc_calc);
            
            if (!peer)
            {
                char *new_name;
                asprintf(&new_name, "UNKNOWN_%"PRIu32"", i2c_unknown_counter++);
                ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - >> New peer, adding.");
                peer = sdp_add_init_new_peer_i2c(new_name, i2c_address);
            }
            peer->i2c_stats.receive_successes++;
            
            response[0] = 0xff;
            response[1] = 0x00;   
            ret = ESP_OK;
        }
        memcpy(&(response[2]) , &crc32_in, (size_t)4);
        ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, &response, 6, ESP_LOG_INFO);

        ret = i2c_slave_write_buffer(CONFIG_I2C_CONTROLLER_NUM, &response, 6,
                                     I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
        if (ret < 0)
        {
            ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - >> Got an error from sending back data: %i", ret);
            if (peer) {
                peer->i2c_stats.send_failures++; // TODO: Not sure how this should count, but probably it should
            }
            
        }
        else
        {
            ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - >> Sent a %i bytes with length of %i bytes and crc of %"PRIu32".", ret, data_len, crc_calc);
            peer->i2c_stats.send_successes++;       
        }


        handle_incoming(peer, rcv_data + 1, data_len - 1, SDP_MT_I2C);
    }
    else if (data_len > 0)
    {
        ESP_LOGE(i2c_messaging_log_prefix, "I2C Slave - << Got data of an invalid length &i");
        ESP_LOG_BUFFER_HEXDUMP(i2c_messaging_log_prefix, rcv_data, data_len, ESP_LOG_INFO);
        i2c_unknown_failures++;
    }
    if (ret < 0)
    {
        ESP_LOGI(i2c_messaging_log_prefix, "I2C Slave - << Got an error from receiving data: %i", ret);
        i2c_unknown_failures++;
    }
}

void i2c_do_on_work_cb(i2c_queue_item_t *work_item)
{
    ESP_LOGI(i2c_messaging_log_prefix, ">> In i2c work callback.");


    int retval = ESP_FAIL;
    int send_retries = 0;
    do
    {
        retval = i2c_send_message(work_item->peer, work_item->data, work_item->data_length, work_item->just_checking);
        if ((retval != ESP_OK) && (send_retries < CONFIG_I2C_RESEND_COUNT))
        {
            // Call the poll function as it was called by the queue to listen for response before retrying
            // TODO: There is no special reason why poll needs to have been called by the queue. 
            // We should probably remove queue context.
            ESP_LOGI(i2c_messaging_log_prefix, ">> Retry %i failed.", send_retries + 1);
            
        }
        i2c_do_on_poll_cb(i2c_get_queue_context());
        send_retries++;

    } while ((retval != ESP_OK) && (send_retries < CONFIG_I2C_RESEND_COUNT));
    // We have failed retrying, if we are supposed to, try resending (and rescoring)
    if ((retval != ESP_OK) && (!work_item->just_checking)) {
        sdp_send_message(work_item->peer, work_item->data, work_item->data_length);
    }
}




void i2c_messaging_init(char *_log_prefix)
{
    i2c_messaging_log_prefix = _log_prefix;
    rcv_data = malloc(I2C_RX_BUF);

    ESP_ERROR_CHECK(i2c_driver_set_master(false, true));
}
