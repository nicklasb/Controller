

#include "sdp.h"
#include "sdp_helpers.h"

#include "esp_log.h"

void add_to_message(uint8_t **message, int *curr_length, char *value)
{
    size_t value_length = (int)strlen(value);
    size_t new_length = value_length + *curr_length + 1;

    //    ESP_LOGI(log_prefix, "curr_length %i, new_length: %i, Value: %s, len: %i.", *curr_length, new_length, value, value_length);

    *message = realloc(*message, new_length);
    if (*message == NULL)
    {
        ESP_LOGE(log_prefix, "Realloc failed.");
    };
    //    ESP_LOGI(log_prefix, "Message efter realloc:  %i, new_length = %i", (int)*message, new_length);

    memcpy((*message) + *curr_length, value, value_length);
    (*message)[new_length - 1] = (uint8_t)0x00;

    *curr_length = new_length;

    free(value);
}

/* Helpers */



void cleanup_queue_task(struct work_queue_item *queue_item)
{
    free(queue_item->parts);
    free(queue_item->raw_data);
    free(queue_item);
    vTaskDelete(NULL);
}

