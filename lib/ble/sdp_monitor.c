#include "sdp.h"
#include "sdp_monitor.h"
#include "esp_log.h"

/* How often should we look */
#define MONITOR_DELAY 5000000

#define HISTORY_LENGTH 5

/* At what monitor count should the reference memory average be stored */
#define FIRST_AVG_POINT 7
#if (FIRST_AVG_POINT < HISTORY_LENGTH + 1)
#error "The first average cannot be before the history is populated, i.e. less than its length + 1. \
It is also not recommended to include the first sample as that is taken before system initialisation, \
and thus less helpful for finding memory leaks. "
#endif

/* Monitor timer & task */
esp_timer_handle_t monitor_timer;

/* Statistics */
int sample_count = 0;
/* As this is taken before SDP initialization it shows the dynamic allocation of SDP and subsystems */
int most_memory_available = 0;
/* This is the least memory available at any sample since startup */
int least_memory_available = 0;
/* This is the first calulated average (first done after FIRST_AVG_POINT samples) */
int first_average_memory_available = 0;

struct history_item
{
    int memory_available;
    int conversation_id;
    // int work_queue_length;
};
struct history_item history[HISTORY_LENGTH];

/**
 * @brief The monitor tasks periodically takes sampes of the current state
 * It uses that history data to perform some simple statistical calculations,
 * helping out with finding memory leaks.
 * TODO: Add an alarm callback to make it possible to raise the alarm if memory or power runs low.
 * 
 * @param arg 
 */

void monitor_task(void *arg)
{
    int curr_mem_avail = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int delta_mem_avail = 0;

    if (curr_mem_avail > most_memory_available)
    {
        most_memory_available = curr_mem_avail;
    }
    if (curr_mem_avail < least_memory_available || least_memory_available == 0)
    {
        least_memory_available = curr_mem_avail;
    }
    /* Loop history backwards, aggregate and push everything one step */
    int agg_avail = curr_mem_avail;
    for (int i = HISTORY_LENGTH - 1; i > -1; i--)
    {

        agg_avail = agg_avail + history[i].memory_available;

        if (i != 0)
        {
            /* Copy all except from the first to the next */
            history[i] = history[i - 1];
        }
        else
        {

            /* The first is filled with new data */
            history[0].memory_available = curr_mem_avail;
            history[0].conversation_id = get_conversation_id();

            /* TODO: Track queue lengths of conversations and work items? */
        }
    }
    float avg_mem_avail = 0;
    // In the beginning, we dont have data, and can't do aggregates
    if (sample_count > HISTORY_LENGTH)
    {
        /* We actually have HISTORY_LENGTH + 1 samples */
        avg_mem_avail = (float)agg_avail / (HISTORY_LENGTH + 1);
        delta_mem_avail = history[0].memory_available - history[1].memory_available;
    }
    // At a specified point, stored it for future reference
    if (sample_count == FIRST_AVG_POINT)
    {
        first_average_memory_available = avg_mem_avail;
    }

    ESP_LOGE(log_prefix, "Monitor reporting on cvailable resources. Memory:\nCurrently: %i, avg mem: %.0f bytes. \nDeltas - Avg vs 1st: %.0f, Last vs now: %i. \nExtremes - Least: %i, Most(before init): %i. ",
             curr_mem_avail, avg_mem_avail, avg_mem_avail - first_average_memory_available, delta_mem_avail, least_memory_available, most_memory_available);
    sample_count++;
    ESP_ERROR_CHECK(esp_timer_start_once(monitor_timer, MONITOR_DELAY));
}


void init_monitor(void)
{
    /* Init the monitor */
    const esp_timer_create_args_t monitor_timer_args = {
        .callback = &monitor_task,
        .name = "monitor"};

    ESP_ERROR_CHECK(esp_timer_create(&monitor_timer_args, &monitor_timer));

    ESP_LOGI(log_prefix, "Launching monitor, activate every %.2f seconds, history length: %i samples.", (float)MONITOR_DELAY/1000000, HISTORY_LENGTH);
    monitor_task(NULL);
}
