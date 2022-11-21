

#include "sdkconfig.h"
#ifdef CONFIG_SDP_LOAD_ESP_NOW

#include "espnow_messaging.h"
#include <sdkconfig.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "esp_now.h"
#include "esp_crc.h"

#include "sdp_mesh.h"
#include "sdp_messaging.h"


#include <freertos/queue.h>

char *log_prefix;



#define ESPNOW_MAXDELAY 512

int unknown_counter = 0;

static QueueHandle_t s_espnow_queue;

static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint16_t s_espnow_seq[ESPNOW_DATA_MAX] = {0, 0};

static void espnow_deinit(espnow_send_param_t *send_param);

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGI(log_prefix, "In espnow_send_cb, send success.");
    }
    if (status == ESP_NOW_SEND_FAIL)
    {
        ESP_LOGW(log_prefix, "In espnow_send_cb, send failure, mac address:");
        ESP_LOG_BUFFER_HEX(log_prefix, mac_addr, SDP_MAC_ADDR_LEN);
    }
}

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{

    if (mac_addr == NULL || data == NULL || len <= 0)
    {
        ESP_LOGE(log_prefix, "In espnow_recv_cb, either parameter was NULL or 0.");
        return;
    }

    sdp_peer *peer = sdp_mesh_find_peer_by_base_mac_address(mac_addr);
    if (peer != NULL)
    {
        ESP_LOGI(log_prefix, "espnow_recv_cb got a message from a peer. Data:");
        ESP_LOG_BUFFER_HEX(log_prefix, data, len);

        handle_incoming(peer, data, len, SDP_MT_BLE);
    }
    else
    {
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


static esp_err_t espnow_init(void)
{

    s_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    if (s_espnow_queue == NULL)
    {
        ESP_LOGE(log_prefix, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
#if CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE
    ESP_ERROR_CHECK(esp_now_set_wake_window(65535));
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)(CONFIG_ESPNOW_PMK)));


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
int espnow_send_message(sdp_mac_address *dest_mac_address, void *data, int data_length)
{
    int rc = esp_now_send(dest_mac_address, data, data_length);
    if (rc != ESP_OK)
    {
        if (rc == ESP_ERR_ESPNOW_NOT_INIT)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NOT_INIT");
        }
        else if (rc == ESP_ERR_ESPNOW_ARG)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_ARG");
        }
        else if (rc == ESP_ERR_ESPNOW_NOT_FOUND)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NOT_FOUND");
        }
        else if (rc == ESP_ERR_ESPNOW_NO_MEM)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NO_MEM");
        }
        else if (rc == ESP_ERR_ESPNOW_FULL)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_FULL");
        }
        else if (rc == ESP_ERR_ESPNOW_NOT_FOUND)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_NOT_FOUND");
        }
        else if (rc == ESP_ERR_ESPNOW_INTERNAL)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_INTERNAL");
        }
        else if (rc == ESP_ERR_ESPNOW_EXIST)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_EXIST");
        }
        else if (rc == ESP_ERR_ESPNOW_IF)
        {
            ESP_LOGE(log_prefix, "ESP-NOW error: ESP_ERR_ESPNOW_IF");
        }
        else
        {
            ESP_LOGE(log_prefix, "ESP-NOW unknown error: %i", rc);
        }
        return -SDP_ERR_SEND_FAIL;
        // espnow_deinit(send_param);
        // vTaskDelete(NULL);
    }

    return rc;
}

void espnow_messaging_init(char *_log_prefix)
{
    log_prefix = _log_prefix;
    espnow_init();
}

#endif