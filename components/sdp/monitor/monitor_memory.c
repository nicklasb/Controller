/**
 * @file monitor_memory.c
 * @author Nicklas Börjesson (nicklasb@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-02-04
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "monitor_memory.h"

#include <esp_heap_caps.h>
#include <esp_log.h>




#define CONFIG_SDP_MONITOR_HISTORY_LENGTH 5

/* Limits for built-in levels levels and warnings. */

/* Memory limits */

/* This much memory should not be used in any situation, a problem will be reported! */
#define CONFIG_SDP_MONITOR_DANGER_USAGE 120000
/* This is more memory than is used in normal operations, a warning will be sent*/
#define CONFIG_SDP_MONITOR_WARNING_USAGE 80000

/* TODO: Implement callbacks for external monitoring points and for problem and warning reporting */
/* TODO: Useful monitors: Queue length, stale conversations? UI? Too long-running work loads? */
/* TODO: Should callbaks return warning level? And be able to clear it? Store monitor list? */
/* TODO: Should this be a separate project? Like IoT monitor or something? */

/* At what monitor count should the reference memory average be stored?*/
#define CONFIG_SDP_MONITOR_FIRST_AVG_POINT 7
#if (CONFIG_SDP_MONITOR_FIRST_AVG_POINT < CONFIG_SDP_MONITOR_HISTORY_LENGTH + 1)
#error "The first average cannot be before the history is populated, i.e. less than its length + 1. \
It is also not recommended to include the first sample as that is taken before system initialisation, \
and thus less helpful for finding memory leaks. "
#endif

char * memory_monitor_log_prefix;


/* Statistics */
int sample_count = 0;
/* As this is taken before SDP initialization it shows the dynamic allocation of SDP and subsystems */
int most_memory_available = 0;
/* This is the least memory available at any sample since startup */
int least_memory_available = 0;
/* This is the first calulated average (first done after CONFIG_SDP_MONITOR_FIRST_AVG_POINT samples) */
int first_average_memory_available = 0;




struct history_item
{
    int memory_available;
    // An array of other values (this is dynamically allocated)
    int *other_values;
};
struct history_item history[CONFIG_SDP_MONITOR_HISTORY_LENGTH];

/**
 * The built-in functionality of the SDP monitoring is that of memory
 * This is considered essential.
 *
 */
void monitor_memory()
{

    int curr_mem_avail = heap_caps_get_free_size(MALLOC_CAP_EXEC);
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
    for (int i = CONFIG_SDP_MONITOR_HISTORY_LENGTH - 1; i > -1; i--)
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
        }
    }

    float avg_mem_avail = 0;
    // In the beginning, we dont have data, and can't do aggregates
    if (sample_count > CONFIG_SDP_MONITOR_HISTORY_LENGTH)
    {
        /* We actually have HISTORY_LENGTH + 1 samples */
        avg_mem_avail = (float)agg_avail / (CONFIG_SDP_MONITOR_HISTORY_LENGTH + 1);
        delta_mem_avail = history[0].memory_available - history[1].memory_available;
    }
    // At a specified point, stored it for future reference
    if (sample_count == CONFIG_SDP_MONITOR_FIRST_AVG_POINT)
    {
        first_average_memory_available = avg_mem_avail;
    }

    int level = ESP_LOG_INFO;
    if ((most_memory_available - curr_mem_avail) > CONFIG_SDP_MONITOR_DANGER_USAGE)
    {
        ESP_LOGE(memory_monitor_log_prefix, "Dangerously high memory usage at %i bytes! Will report!(CONFIG_SDP_MONITOR_DANGER_USAGE=%i)",
                 most_memory_available - curr_mem_avail, CONFIG_SDP_MONITOR_WARNING_USAGE);
        // TODO: Implement problem callback!
        level = ESP_LOG_ERROR;
    }
    else if ((most_memory_available - curr_mem_avail) > CONFIG_SDP_MONITOR_WARNING_USAGE)
    {
        ESP_LOGW(memory_monitor_log_prefix, "Unusually high memory usage at %i bytes(CONFIG_SDP_MONITOR_WARNING_USAGE=%i).",
                 most_memory_available - curr_mem_avail, CONFIG_SDP_MONITOR_WARNING_USAGE);
        // TODO: Implement warning callback!
        level = ESP_LOG_WARN;
    }
    sample_count++;
    ESP_LOG_LEVEL(level, memory_monitor_log_prefix, "Monitor reporting on available resources. Memory:\nCurrently: %i, avg mem: %.0f bytes. \nDeltas - Avg vs 1st: %.0f, Last vs now: %i. \nExtremes - Least: %i, Most(before init): %i. ",
                  curr_mem_avail, avg_mem_avail, avg_mem_avail - first_average_memory_available, delta_mem_avail, least_memory_available, most_memory_available);
}

void memory_monitor_init(char *_log_prefix) {
    memory_monitor_log_prefix = _log_prefix;
    ESP_LOGI(memory_monitor_log_prefix, "Launching memory monitor, history length: %i samples.", CONFIG_SDP_MONITOR_HISTORY_LENGTH);
}