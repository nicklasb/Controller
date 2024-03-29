#include "gsm_ip.h"

#include "esp_log.h"
#include "esp_netif_ppp.h"
#include "gsm_mqtt.h"

#include "esp_event.h"
#include "gsm_worker.h"
#include "inttypes.h"

#include "../orchestration/orchestration.h"

#include "gsm_def.h"

char *gsm_ip_log_prefix;
esp_netif_t *gsm_ip_esp_netif = NULL;

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    ESP_LOGI(gsm_ip_log_prefix, "PPP state changed event %li", event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = event_data;
        ESP_LOGW(gsm_ip_log_prefix, "User interrupted event from netif:%p", netif);
    }
    else
    {
        ESP_LOGW(gsm_ip_log_prefix, "Got Uncategorized ppp event! %li", event_id);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGD(gsm_ip_log_prefix, "IP event! %li", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        ESP_LOGI(gsm_ip_log_prefix, "* GOT ip event!!!");
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(gsm_ip_log_prefix, "Modem Connect to PPP Server");
        ESP_LOGI(gsm_ip_log_prefix, "~~~~~~~~~~~~~~");
        ESP_LOGI(gsm_ip_log_prefix, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(gsm_ip_log_prefix, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(gsm_ip_log_prefix, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(gsm_ip_log_prefix, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(gsm_ip_log_prefix, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(gsm_ip_log_prefix, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(gsm_event_group, GSM_CONNECT_BIT);

        
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGW(gsm_ip_log_prefix, "Modem Disconnect from PPP Server");
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(gsm_ip_log_prefix, "Got IPv6 event!");

        ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
        ESP_LOGI(gsm_ip_log_prefix, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
    else
    {
        ESP_LOGW(gsm_ip_log_prefix, "Got Uncategorized IP event! %li", event_id);
    }
}

void gsm_ip_cleanup() {

    if (gsm_ip_esp_netif) {
        gsm_mqtt_cleanup();
        ESP_LOGI(gsm_ip_log_prefix, "Cleaning up GSM IP");

        

        ESP_LOGI(gsm_ip_log_prefix, "- Unregistering IP/Netif events.");
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event);
        esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed);        
        ESP_LOGI(gsm_ip_log_prefix, "- Deinit netif");
        esp_netif_deinit();
        ESP_LOGI(gsm_ip_log_prefix, "- Destroying netif at %p.", gsm_ip_esp_netif);
        esp_netif_destroy(gsm_ip_esp_netif);
        
        gsm_ip_esp_netif = NULL;
        ESP_LOGI(gsm_ip_log_prefix, "GSM IP cleaned up.");
    } else {
        ESP_LOGW(gsm_ip_log_prefix, "GSM IP cleanup was told to cleanup while not initialized or already cleaned up.");
    }

}

int gsm_ip_enable_data_mode() {

    // Setting data mode.
    ESP_LOGI(gsm_ip_log_prefix, "esp_modem_set_data_mode(gsm_dce).%p ", gsm_dce);
    esp_err_t err = esp_modem_set_mode(gsm_dce, ESP_MODEM_MODE_DATA);
    if (err != ESP_OK)
    {
        ESP_LOGE(gsm_ip_log_prefix, "esp_modem_set_data_mode(gsm_dce) failed with %i", err);
        gsm_ip_cleanup();
        return ESP_FAIL;
    }
    ESP_LOGI(gsm_ip_log_prefix, "Data mode set");
    /* Wait for IP address */
    ESP_LOGI(gsm_ip_log_prefix, "Waiting for IP address");
    ask_for_time(5000000);
    EventBits_t uxBits;
    do
    {

        uxBits = xEventGroupWaitBits(gsm_event_group, GSM_CONNECT_BIT | GSM_SHUTTING_DOWN_BIT, pdFALSE, pdFALSE, 4000 / portTICK_PERIOD_MS); 
        if ((uxBits & GSM_SHUTTING_DOWN_BIT) != 0)
        {
            ESP_LOGW(gsm_ip_log_prefix, "Getting that we are shutting down, pausing indefinitely.");
            return GSM_SHUTTING_DOWN_BIT;
        }
        else if ((uxBits & GSM_CONNECT_BIT) != 0)
        {
            ESP_LOGI(gsm_ip_log_prefix, "Got an IP connection, great!");
            ask_for_time(5000000);
            return ESP_OK;
        }
        else
        {
            ESP_LOGI(gsm_ip_log_prefix, "Timed out. Continuing waiting for IP.");
            ask_for_time(5000000);
        }
    } while (1);
 
}

void gsm_ip_init(char *_log_prefix)
{
    
    gsm_ip_log_prefix = _log_prefix;
    ESP_LOGI(gsm_ip_log_prefix, "* Initiating gsm_ip");

    // Initialize the underlying TCP/IP stack  
    ESP_ERROR_CHECK(esp_netif_init());

    // Keeping this here to inform that the event loop is created in sdp_init, not here
    //    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(gsm_ip_log_prefix, " + Register IP event handlers.");
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP(); 
    ESP_LOGI(gsm_ip_log_prefix, " + Create netif object");
    gsm_ip_esp_netif = esp_netif_new(&netif_ppp_config);
    assert(gsm_ip_esp_netif); 
    ESP_LOGI(gsm_ip_log_prefix, "* gsm_ip_esp_netif assigned %p", gsm_ip_esp_netif);

}
