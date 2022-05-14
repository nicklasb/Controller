#include "sdp.h"
#include "sdp_monitor.h"
#include "esp_log.h"

/* How often should we look */
#define MONITOR_DELAY 5000000

#define HISTORY_LENGTH 5

/* At what monitor count should the reference memory average be stored */
#define FIRST_AVG_POINT 6
#if (FIRST_AVG_POINT < HISTORY_LENGTH + 1)
#error "The first average cannot be before the history is populated, i.e. less than its length + 1."
#endif


/* Monitor */
esp_timer_handle_t monitor_timer;

void monitor_task(void *arg);

/* Statistics */
int most_memory_available = 0;
int least_memory_available = 0;
int first_average_memory_available = 0;

int monitor_count = 0;




struct history_item {
    int memory_available;
    int conversation_id;
    //int work_queue_length;
};
struct history_item history[HISTORY_LENGTH];


void init_monitor(void) {
    /* Init the monitor */
    const esp_timer_create_args_t monitor_timer_args = {
            .callback = &monitor_task,
            .name = "monitor"
    };

    ESP_ERROR_CHECK(esp_timer_create(&monitor_timer_args, &monitor_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(monitor_timer, MONITOR_DELAY));
}


void monitor_task(void *arg) {
    int curr_mem_avail = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int delta_mem_avail = 0;
    
    if (curr_mem_avail > most_memory_available) {
        most_memory_available = curr_mem_avail;
    }
    if (curr_mem_avail < least_memory_available) {
        least_memory_available = curr_mem_avail;
    }
    /* Loop history backwards, aggregate and push everything one step */
    int agg_avail = curr_mem_avail;
    for (int i = HISTORY_LENGTH-1; i > -1; i--) {

        agg_avail = agg_avail + history[i].memory_available;

        if (i != 0) {
            /* Copy all except from the first to the next */
            history[i] = history[i-1];
        } else {
            
            /* The first is filled with new data */
            history[0].memory_available = curr_mem_avail;
            history[0].conversation_id = get_conversation_id(); 
            

            /* TODO: Track queue lengths of conversations and work items? */
        }
    }
    float avg_mem_avail = 0;
    // In the beginning, we dont have data, and can't do aggregates 
    if (monitor_count > HISTORY_LENGTH) {
        /* We actually have HISTORY_LENGTH + 1 samples */
        avg_mem_avail = (float)agg_avail/(HISTORY_LENGTH + 1);
        delta_mem_avail = history[0].memory_available - history[1].memory_available;
    }
    // At a specified point, stored it for future reference 
    if (monitor_count == FIRST_AVG_POINT) {
        first_average_memory_available = avg_mem_avail;
    }


    ESP_LOGE(log_prefix, "Monitor: Avail. mem - Currently: %i, avg mem: %.0f. Deltas - avg vs 1st: %.0f, last vs now: %i. %i", 
        curr_mem_avail, avg_mem_avail, avg_mem_avail - first_average_memory_available, delta_mem_avail, sizeof(struct conversation_list_item));
    monitor_count++;
    ESP_ERROR_CHECK(esp_timer_start_once(monitor_timer, MONITOR_DELAY));
}
