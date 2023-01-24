#include "i2c_peer.h"

#include <esp_log.h>


#include "sdp_def.h"
#include "esp_timer.h"

#include <string.h>

char * i2c_peer_log_prefix;
int i2c_unknown_counter = 0;
int i2c_unknown_failures = 0;
int i2c_crc_failures = 0;

/**
 * @brief Reset stats count.
 *
 * @param peer
 */
void i2c_stat_reset(sdp_peer *peer)
{
    i2c_unknown_failures = 0;
    i2c_crc_failures = 0;
    peer->i2c_stats.send_successes = 0;
    peer->i2c_stats.send_failures = 0;
    peer->i2c_stats.receive_successes = 0;
    peer->i2c_stats.receive_failures = 0;
    peer->i2c_stats.score_count = 0;
}

void i2c_peer_init_peer(sdp_peer *peer)
{
    memset(peer->i2c_stats.failure_rate_history, 0, sizeof(float) * FAILURE_RATE_HISTORY_LENGTH);

    i2c_stat_reset(peer);
}

/**
 * @brief Returns a connection score for the peer
 *
 * @param peer The peer to analyze
 * @param data_length The length of data to send
 * @return float The score = -100 don't use, +100 use
 */
float i2c_score_peer(sdp_peer *peer, int data_length)
{

    // I2C is not very fast, but not super-slow.
    // If there are many peers, per
    // Is there a level of usage that is "too much"
    // TODO: Add different demands, like "wired"? "secure", "fast", "roundtrip", or similar?

#if 0
    /* Supported speed bit/s */
    uint32_t theoretical_speed;
    /* Actual speed bit/s. 
    NOTE: Always lower than theoretical, and with small payloads; *much* lower */
    uint32_t actual_speed;

    /* Number of times we have failed sending to a peer since last check */
    uint32_t send_failures;
    /* Number of times we have failed receiving data from a peer since last check */
    uint32_t receive_failures;    
    /* Number of times we have succeeded sending to a peer since last check */
    uint32_t send_successes;
    /* Number of times we have succeeed eceiving data from a peer since last check */
    uint32_t receive_successes;
#endif
    // -50 if its way too long, +50 if its below 1000 bytes
    // TODO: Obviously, the length score should go down if we are forced to slow down, with a low actual speed.
    float length_score = 50 - ((data_length - 1000) * 0.05);
    if (length_score < -50)
    {
        length_score = -50;
    }
    ESP_LOGI(i2c_peer_log_prefix, "peer: %s ss: %i, rs: %i, sf: %i, rf: %i ", peer->name,
             peer->i2c_stats.send_successes, peer->i2c_stats.receive_successes,
             peer->i2c_stats.send_failures, peer->i2c_stats.receive_failures);
    // Success score

    float failure_rate = 0;

    if (peer->i2c_stats.send_successes + peer->i2c_stats.receive_successes > 0)
    {
        // Neutral if failures are lower than 0.01 of tries.
        failure_rate = ((float)peer->i2c_stats.send_failures + (float)peer->i2c_stats.receive_failures) /
                       ((float)peer->i2c_stats.send_successes + (float)peer->i2c_stats.receive_successes);
    }
    else if (peer->i2c_stats.send_failures + peer->i2c_stats.receive_failures > 0)
    {
        // If there are only failures, that causes a 1 as a failure fraction
        failure_rate = 1;
    }
    if (failure_rate > 1)
    {
        failure_rate = 1;
    }

    // A failure fraction of 0.1 - 0. No failures - 25. Anything over 0.5 returns -100.
    float success_score = 10 - (add_to_failure_rate_history(&(peer->i2c_stats), failure_rate) * 100);
    if (success_score < -100)
    {
        success_score = -100;
    }

    float total_score = length_score + success_score;
    if (total_score < -100)
    {
        total_score = -100;
    }

    ESP_LOGE(i2c_peer_log_prefix, "I2C - Scoring - peer: %s fr: %f:\nLength  : %f\nSuccess : %f\nTotal  = %f", peer->name,
             failure_rate, length_score, success_score, total_score);

    peer->i2c_stats.last_score = (total_score + peer->i2c_stats.last_score) / 2;
    peer->i2c_stats.last_score_time = esp_timer_get_time();

    i2c_stat_reset(peer);

    return peer->i2c_stats.last_score;
}

void i2c_peer_init(char * _log_prefix) {

    i2c_peer_log_prefix = _log_prefix;
};