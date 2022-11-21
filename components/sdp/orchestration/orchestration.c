/**
 * @file orchestration.c
 * @author Nicklas Borjesson (nicklasb@gmail.com)
 * @brief Orchestration refers to process-level operations, like scheduling and timing sleep and timeboxing wake time
 * @version 0.1
 * @date 2022-11-20
 * 
 * @copyright Copyright Nicklas Borjesson(c) 2022
 * 
 */

#include "orchestration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sdp_helpers.h"
#include "sdp_messaging.h"
#include "esp_log.h"
#include "sdp_def.h"
#include "inttypes.h"

#include "sleep/sleep.h"

char *log_prefix;

uint64_t next_time;
uint64_t wait_for_sleep_started;
uint64_t wait_time = 0;
uint64_t requested_time = 0;

RTC_DATA_ATTR int availibility_retry_count;



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
        next_time = SDP_AWAKE_TIME_uS + SDP_SLEEP_TIME_uS;
    }
    else
    {
        next_time = get_last_sleep_time() + SDP_SLEEP_TIME_uS + SDP_AWAKE_TIME_uS + SDP_SLEEP_TIME_uS;
    }
    ESP_LOGI(log_prefix, "Next time we are available is at %"PRIu64".", next_time);
}

void sdp_orchestration_parse_next_message(work_queue_item_t *queue_item)
{
    queue_item->peer->next_availability = get_time_since_start() + atoll(queue_item->parts[1]);
    ESP_LOGI(log_prefix, "Peer %s is available at %"PRIu64".", queue_item->peer->name, queue_item->peer->next_availability);
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

    ESP_LOGI(log_prefix, "BEFORE NEXT get_time_since_start() = %"PRIu64, get_time_since_start());
    /* TODO: Handle the 64bit loop-around after 79 days ? */
    uint64_t delta_next = next_time - get_time_since_start();
    ESP_LOGI(log_prefix, "BEFORE NEXT delta_next = %"PRIu64, delta_next);

    /*  Cannot send uint64_t into va_args in add_to_message */
    char * c_delta_next;
    asprintf(&c_delta_next, "%"PRIu64, delta_next);

    int next_length = add_to_message(&next_msg, "NEXT|%s|%i", c_delta_next, SDP_AWAKE_TIME_uS);

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

void sleep_until_peer_available(sdp_peer *peer, uint64_t margin_us)
{
    if (peer->next_availability > 0)
    {
        uint64_t sleep_length = peer->next_availability - get_time_since_start() + margin_us;
        ESP_LOGI(log_prefix, "Going to sleep for %"PRIu64" microseconds.", sleep_length);
        goto_sleep_for_microseconds(sleep_length);
    }
}

/**
 * @brief Ask to wait with sleep for a specific amount of time from now 
 * @param ask Returns false if request is denied
 */
bool ask_for_time(uint64_t ask) {
    uint64_t wait_time_left = wait_time - esp_timer_get_time() + wait_for_sleep_started;
    
    // Only request for more time if it is more than being available and already requested
    if ((wait_time_left < ask) && (requested_time < ask - wait_time_left)) {
        /* Only allow for requests that fit into the awake timebox */
        if (esp_timer_get_time() + wait_time_left + ask < SDP_AWAKE_TIMEBOX_uS) {
            requested_time = ask - wait_time_left;
            ESP_LOGI(log_prefix, "Orchestrator granted an extra %"PRIu64" ms of awakeness.", ask/1000);
            return true;
        } else {
            ESP_LOGI(log_prefix, "Orchestrator denied %"PRIu64" ms of awakeness because it would violate the timebox.", ask/1000);
            return false;      
        }
    } 
    ESP_LOGI(log_prefix, "Orchestrator got an unnessary request for %"PRIu64" ms of awakeness.", ask/1000);
    return true;

}

void take_control()
{
    /* Wait for the awake period*/
    wait_time = SDP_AWAKE_TIME_uS;
    int64_t wait_ms;
    while (1) {
        
        wait_ms = wait_time / 1000;
        ESP_LOGI(log_prefix, "Orchestrator awaiting sleep for %"PRIu64" ms.", wait_ms);
        wait_for_sleep_started = esp_timer_get_time();
        vTaskDelay(wait_ms / portTICK_PERIOD_MS);
        if (requested_time > 0) {
            wait_time = requested_time;
            ESP_LOGW(log_prefix, "Orchestrator extending the awake phase.");
            requested_time = 0;
        } else {
            ESP_LOGI(log_prefix, "Orchestrator done waiting, going to sleep. zzZzzzzZzz");
            break;
        }

    }
    // TODO: Add this on other side as well
    if (on_before_sleep_cb)
    {
        ESP_LOGI(log_prefix, "Calling before sleep callback");
        if (!on_before_sleep_cb())
        {
            ESP_LOGW(log_prefix, "Stopped from going to sleep by callback!");
            return;
        }
    }
    goto_sleep_for_microseconds(SDP_SLEEP_TIME_uS - (esp_timer_get_time() - SDP_AWAKE_TIME_uS));
}
/**
 * @brief Check with the peer when its available next, and goes to sleep until then.
 * 
 * @param peer The peer that one wants to follow
 */
void give_control(sdp_peer *peer)
{

    if (peer != NULL)
    {
        peer->next_availability = 0;
        // Ask for orchestration
        ESP_LOGI(log_prefix, "Asking for orchestration..");
        int retries = 0;
        // Waiting a while for a response.
        while (retries < 10)
        {
            sdp_orchestration_send_when_message(peer);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            
            if (peer->next_availability > 0)
            {
                break;
            }

            retries++;
        }
        if (retries == 10)
        {
            ESP_LOGE(log_prefix, "Haven't gotten an availability time for peer \"%s\" ! Tried %i times. Going to sleep for %i microseconds..",
                     peer->name, availibility_retry_count++, SDP_ORCHESTRATION_RETRY_WAIT_uS);
            
            goto_sleep_for_microseconds(SDP_ORCHESTRATION_RETRY_WAIT_uS);
        }
        else
        {
            availibility_retry_count = 0;
            ESP_LOGI(log_prefix, "Waiting for sleep..");
            /* TODO: Add a sdp task concept instead and wait for a task count to reach zero (within ) */
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            sleep_until_peer_available(peer, SDP_AWAKE_MARGIN_uS);
        }
    }
}

void orchestration_init(char *_log_prefix)
{
    log_prefix = _log_prefix;
    // Set the next available time.
    update_next_availability_window();
}