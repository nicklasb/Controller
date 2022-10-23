#include "orchestration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sdp_helpers.h"
#include "sdp_messaging.h"
#include "esp_log.h"
#include "sdp_def.h"

#include "sleep/sleep.h"

char *log_prefix;

int next_time;

/**
 * @brief Save the current time into RTC memory
 *
 */
void update_next_availability_window()
{
    // Note that this value will roll over at certain points (49.7 days?)
    /* Next time  = Last time we fell asleep + how long we slept + how long we should've been up + how long we will sleep */
    if (get_last_sleep_time() == 0)
    {
        next_time = SDP_CYCLE_UPTIME_uS + SDP_CYCLE_DELAY_uS;
    }
    else
    {
        next_time = get_last_sleep_time() + SDP_CYCLE_DELAY_uS + SDP_CYCLE_UPTIME_uS + SDP_CYCLE_DELAY_uS;
    }
    ESP_LOGI(log_prefix, "Next time we are available is at %i.", next_time);
}

void sdp_orchestration_parse_next_message(work_queue_item_t *queue_item)
{
    queue_item->peer->next_availability = get_time_since_start() + atoi(queue_item->parts[1]);
    ESP_LOGI(log_prefix, "Peer %s is available at %i.", queue_item->peer->name, queue_item->peer->next_availability);
}

/**
 * @brief Sends a "NEXT"-message to a peer informing on microseconds to next availability window
 * Please note the limited precision of the chrystals.
 * TODO: If having several clients, perhaps a slight delay would be good?
 *
 * @param peer The peer to send the message to
 * @return int
 */
int sdp_orchestration_send_next_message(work_queue_item_t *queue_item)
{

    int retval;

    uint8_t *next_msg = NULL;

    /* TODO: Handle the 32bit loop-around after 79 days ? */
    int delta_next = next_time - get_time_since_start();

    int next_length = add_to_message(&next_msg, "NEXT|%i|%i", delta_next, SDP_CYCLE_UPTIME_uS);

    if (next_length > 0)
    {
        retval = sdp_reply(*queue_item, ORCHESTRATION, next_msg, next_length);
    }
    else
    {
        // Returning the negative of the return value as that denotes an error.
        retval = -next_length;
    }
    free(next_msg);
    return retval;
}

/**
 * @brief Send a "When"-message that asks the peer to describe themselves
 *
 * @return int A handle to the created conversation
 */
int sdp_orchestration_send_when_message(sdp_peer *peer)
{
    char when_msg[5] = "WHEN\0";
    return start_conversation(peer, ORCHESTRATION, "Orchestration", &when_msg, 5);
}

void sleep_until_peer_available(sdp_peer *peer, int margin_us)
{
    if (peer->next_availability > 0)
    {
        int sleep_length = peer->next_availability - get_time_since_start() + margin_us;
        ESP_LOGI(log_prefix, "Going to sleep for %i microseconds.", sleep_length);
        goto_sleep_for_microseconds(sleep_length);
    }
}

void take_control()
{
    int wait_ms = SDP_CYCLE_UPTIME_uS / 1000;
    ESP_LOGI(log_prefix, "Orchestrator awaiting sleep for %i ms.", wait_ms);
    vTaskDelay(wait_ms / portTICK_PERIOD_MS);
    goto_sleep_for_microseconds(SDP_CYCLE_DELAY_uS);
}

void give_control(sdp_peer *peer)
{

    if (peer != NULL)
    {
        peer->next_availability = 0;
        // Ask for orchestration
        ESP_LOGI(log_prefix, "Asking for orchestration..");
        int retries = 0;
        while (retries < SDP_CYCLE_RETRY_COUNT)
        {
            sdp_orchestration_send_when_message(peer);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            if (peer->next_availability > 0)
            {
                break;
            }
            retries++;
        }
        if (retries == SDP_CYCLE_RETRY_COUNT)
        {
            ESP_LOGE(log_prefix, "Haven't gotten an availability time for peer \"%s\" ! Going to sleep for %i microseconds..",
                     peer->name, SDP_CYCLE_DELAY_uS);
            goto_sleep_for_microseconds(SDP_CYCLE_DELAY_uS);
        }
        else
        {
            ESP_LOGI(log_prefix, "Waiting for sleep..");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            sleep_until_peer_available(peer, SDP_CYCLE_MARGIN);
        }
    }
}

void orchestration_init(char *_log_prefix)
{
    log_prefix = _log_prefix;
    // Set the next available time.
    update_next_availability_window();
}