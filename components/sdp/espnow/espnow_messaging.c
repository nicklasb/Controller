#include "espnow_messaging.h"
// #include <stdlib.h>
// #include <time.h>
// #include <string.h>
// #include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// #include "freertos/timers.h"
// #include "nvs_flash.h"
// #include "esp_random.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "esp_wifi.h"
#include "esp_log.h"
// #include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"

#include "sdp_mesh.h"
#include "sdp_messaging.h"
// #include "espnow_example.h"

#include <freertos/queue.h>

char *log_prefix;



/* ESPNOW Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/


#define ESPNOW_MAXDELAY 512

int unknown_counter = 0;

static QueueHandle_t s_espnow_queue;

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_espnow_seq[ESPNOW_DATA_MAX] = { 0, 0 };

static void espnow_deinit(espnow_send_param_t *send_param);



/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(log_prefix, "In espnow_send_cb, send success.");
    } if (status == ESP_NOW_SEND_FAIL) {
        ESP_LOGE(log_prefix, "In espnow_send_cb, send failure, mac address:");
        ESP_LOG_BUFFER_HEX(log_prefix, mac_addr, SDP_MAC_ADDR_LEN);
    }
    /*
    espnow_event_t evt;
    espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(log_prefix, "Send cb arg error");
        return;
    }

    evt.id = ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(log_prefix, "Send send queue fail");
    }
    */
}

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(log_prefix, "In espnow_recv_cb, either parameter was NULL or 0.");
        return;
    }

    sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address( mac_addr);
    if (peer != NULL) {
                ESP_LOGI(log_prefix, "espnow_recv_cb got a message from a peer. Data:");
        ESP_LOG_BUFFER_HEX(log_prefix, data, len);
        
        
        handle_incoming(peer, data, len, SDP_MT_BLE);

    } else {
        ESP_LOGI(log_prefix, "espnow_recv_cb got a message from an unknown peer. Data:");
        ESP_LOG_BUFFER_HEX(log_prefix, data, len);
        /* Remember peer. */
        esp_now_peer_info_t *espnow_peer = malloc(sizeof(esp_now_peer_info_t));
        /* TODO: These seem arbitrary, see how channel and such is handled*/
        espnow_peer->channel = 0;
        espnow_peer->encrypt = false;
        espnow_peer->ifidx = ESP_IF_WIFI_STA;
        memcpy(espnow_peer->peer_addr, mac_addr, SDP_MAC_ADDR_LEN); 

        char *new_name;
        asprintf(&new_name, "UNKNOWN_%i", unknown_counter);
        sdp_peer *s_peer = sdp_add_init_new_peer(new_name, mac_addr, SDP_MT_ESPNOW);

            
    }
}
#if 0
/* Parse received ESPNOW data. */
int espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, int *magic)
{
    espnow_data_t *buf = (espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(espnow_data_t)) {
        ESP_LOGE(log_prefix, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc) {
        return buf->type;
    }

    return -1;
}

/* Prepare ESPNOW data to be sent. */
void espnow_data_prepare(espnow_send_param_t *send_param)
{
    espnow_data_t *buf = (espnow_data_t *)send_param->buffer;

    assert(send_param->len >= sizeof(espnow_data_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? ESPNOW_DATA_BROADCAST : ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = s_espnow_seq[buf->type]++;
    buf->crc = 0;
    buf->magic = send_param->magic;
    /* Fill all remaining bytes after the data with random values */
    esp_fill_random(buf->payload, send_param->len - sizeof(espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}
#endif

static void espnow_task(void *pvParameter)
{
    #if 0
    espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    int recv_magic = 0;
    bool is_broadcast = false;
    int ret;


    while (xQueueReceive(s_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case ESPNOW_SEND_CB:
            {
                espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr);

                ESP_LOGD(log_prefix, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);
                /**/
                if (is_broadcast && (send_param->broadcast == false)) {
                    break;
                }

                if (!is_broadcast) {
                    send_param->count--;
                    if (send_param->count == 0) {
                        ESP_LOGI(log_prefix, "Send done");
                        espnow_deinit(send_param);
                        vTaskDelete(NULL);
                    }
                }

                /* Delay a while before sending the next data. */
                if (send_param->delay > 0) {
                    vTaskDelay(send_param->delay/portTICK_PERIOD_MS);
                }

                ESP_LOGI(log_prefix, "send data to "MACSTR"", MAC2STR(send_cb->mac_addr));

                memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
                espnow_data_prepare(send_param);

                /* Send the next data after the previous data is sent. */
                if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                    ESP_LOGE(log_prefix, "Send error");
                    espnow_deinit(send_param);
                    vTaskDelete(NULL);
                }
                break;
            }
            case ESPNOW_RECV_CB:
            {
                espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                ret = espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic);
                free(recv_cb->data);
                if (ret == ESPNOW_DATA_BROADCAST) {
                    ESP_LOGI(log_prefix, "Receive %dth broadcast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If MAC address does not exist in peer list, add it to peer list. */
                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false) {
                        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        if (peer == NULL) {
                            ESP_LOGE(log_prefix, "Malloc peer information fail");
                            espnow_deinit(send_param);
                            vTaskDelete(NULL);
                        }
                        memset(peer, 0, sizeof(esp_now_peer_info_t));
                        peer->channel = CONFIG_ESPNOW_CHANNEL;
                        peer->ifidx = ESPNOW_WIFI_IF;
                        peer->encrypt = true;
                        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                        memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                        free(peer);
                    }

                    /* Indicates that the device has received broadcast ESPNOW data. */
                    if (send_param->state == 0) {
                        send_param->state = 1;
                    }

                    /* If receive broadcast ESPNOW data which indicates that the other device has received
                     * broadcast ESPNOW data and the local magic number is bigger than that in the received
                     * broadcast ESPNOW data, stop sending broadcast ESPNOW data and start sending unicast
                     * ESPNOW data.
                     */
                    if (recv_state == 1) {
                        /* The device which has the bigger magic number sends ESPNOW data, the other one
                         * receives ESPNOW data.
                         */
                        if (send_param->unicast == false && send_param->magic >= recv_magic) {
                    	    ESP_LOGI(log_prefix, "Start sending unicast data");
                    	    ESP_LOGI(log_prefix, "send data to "MACSTR"", MAC2STR(recv_cb->mac_addr));

                    	    /* Start sending unicast ESPNOW data. */
                            memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                            espnow_data_prepare(send_param);
                            if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                                ESP_LOGE(log_prefix, "Send error");
                                espnow_deinit(send_param);
                                vTaskDelete(NULL);
                            }
                            else {
                                send_param->broadcast = false;
                                send_param->unicast = true;
                            }
                        }
                    }
                }
                else if (ret == ESPNOW_DATA_UNICAST) {
                    ESP_LOGI(log_prefix, "Receive %dth unicast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If receive unicast ESPNOW data, also stop sending broadcast ESPNOW data. */
                    send_param->broadcast = false;
                }
                else {
                    ESP_LOGI(log_prefix, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                }
                break;
            }
            default:
                ESP_LOGE(log_prefix, "Callback type error: %d", evt.id);
                break;
        }
    }
    #endif
}


static esp_err_t espnow_init(void)
{
    espnow_send_param_t *send_param;

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_espnow_queue == NULL) {
        ESP_LOGE(log_prefix, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );
#if CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE
    ESP_ERROR_CHECK( esp_now_set_wake_window(65535) );
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

#if 0
    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(log_prefix, "Malloc peer information fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);
/**/
    /* Initialize sending parameters. */
    send_param = malloc(sizeof(espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(log_prefix, "Malloc send parameter fail");
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_param, 0, sizeof(espnow_send_param_t));
    send_param->unicast = false;
    send_param->broadcast = true;
    send_param->state = 0;
    send_param->magic = esp_random();
    send_param->count = CONFIG_ESPNOW_SEND_COUNT;
    send_param->delay = CONFIG_ESPNOW_SEND_DELAY;
    send_param->len = CONFIG_ESPNOW_SEND_LEN;
    send_param->buffer = malloc(CONFIG_ESPNOW_SEND_LEN);
    if (send_param->buffer == NULL) {
        ESP_LOGE(log_prefix, "Malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(s_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->dest_mac, s_broadcast_mac, ESP_NOW_ETH_ALEN);
    espnow_data_prepare(send_param);
    // xTaskCreate(espnow_task, "espnow_task", 2048, send_param, 4, NULL);
    
#endif
    // xTaskCreate(espnow_task, "espnow_task", 2048, NULL, 4, NULL);

    return ESP_OK;
}
/**
 * @brief Deinitialization of ESP-NOW - 
 * TODO: Consider if this must be done before sleep modes
 * 
 * @param send_param 
 */
static void espnow_deinit(espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_espnow_queue);
    esp_now_deinit();
}


/**
 * @brief Sends a message through ESPNOW.
 */
int espnow_send_message(sdp_mac_address *dest_mac_address, void *data, int data_length) {
    int rc = esp_now_send(dest_mac_address, data, data_length); 
    if (rc != ESP_OK) {
        if (rc == ESP_ERR_ESPNOW_NOT_INIT) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NOT_INIT");
        } else
        if (rc == ESP_ERR_ESPNOW_ARG) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_ARG");
        } else
        if (rc == ESP_ERR_ESPNOW_NOT_FOUND) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NOT_FOUND");
        } else
        if (rc == ESP_ERR_ESPNOW_NO_MEM) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NO_MEM");
        } else
        if (rc == ESP_ERR_ESPNOW_FULL) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_FULL");
        } else
        if (rc == ESP_ERR_ESPNOW_NOT_FOUND) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NOT_FOUND");
        } else
        if (rc == ESP_ERR_ESPNOW_INTERNAL) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_INTERNAL");
        } else
        if (rc == ESP_ERR_ESPNOW_EXIST) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_EXIST");
        } else
        if (rc == ESP_ERR_ESPNOW_IF) {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_IF");
        } else {
            ESP_LOGE(log_prefix, "ESP-NOW unknown error: %i", rc);
        }
        return SDP_ERR_SEND_FAIL;
        //espnow_deinit(send_param);
        //vTaskDelete(NULL);
    }
    return rc;
}


void espnow_messaging_init(char * _log_prefix){
    log_prefix = _log_prefix;
    espnow_init();
}



