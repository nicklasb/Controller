

#include "sdp_helpers.h"
#include <stdarg.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/adc.h>


#include "sdp_def.h"



#define WHO_LENGTH 4

char *helpers_log_prefix;

/* Helpers */

/**
 * @brief A simple way to, using format strings, build a message.
 * However; as the result will have to be null-terminated, and we cannot put nulls in that string.
 * Instead, the pipe-symbol "|" is used to indicate the null-terminated sections.
 * Only one format is allowed per section: "%i Mhz|%s" is allowed. "%i Mhz, %i watts|%s" is not.
 * Integers can be passed as is. An additional format type is %b, which denotes a
 * IMPORTANT: Do not send floating point values, format them ahead and send them as null-terminated strings.
 * This is because of how variadic functions promotes and handles these data types.
 *
 * @param message A pointer to a pointer to the message structure, memory will be allocated to it.
 * @param format A format string where "|" indicates where null-separation is wanted and creates a section.
 * @param arg A list of arguments supplying data for the format.
 * @return int Length of message
 *
 * @note An evolution would be to identify the number of format strings in each "|"-section to enable full support of format.
 * Unclear when that woud be needed, though, perhaps if compiling error texts.
 */
int add_to_message(uint8_t **message, const char *format, ...)
{

    va_list arg;
    va_start(arg, format);

    int format_len = strlen(format);
    
    char *loc_format = heap_caps_malloc(format_len + 1, MALLOC_CAP_8BIT);
    strcpy(loc_format, format);
    ESP_LOGD(helpers_log_prefix, "Format string %s len %i", loc_format, format_len);

    int break_count = 1;
    /* Make a pass to count pipes and replace them with nulls */
    for (int i = 0; i < format_len; i++)
    {
        if ((int)(loc_format[i]) == 124)
        {
            loc_format[i] = 0;
            break_count++;
        }
    }
    // Ensure null-termination (alloc is 1 longer than format len)
    ESP_LOGD(helpers_log_prefix, "format_len %i", format_len);
    loc_format[format_len] = 0;
    /* Now we know that our format array needs to be break_count long, allocate it */
    char **format_array = heap_caps_malloc(break_count * sizeof(char *), MALLOC_CAP_8BIT);

    format_array[0] = loc_format;
    int format_count = 1;
    for (int j = 0; j < format_len; j++)
    {
        if ((int)(loc_format[j]) == 0)
        {

            format_array[format_count] = (char *)(loc_format + j + 1);
            format_count++;
        }
    }
    int curr_pos = 0;
    size_t new_length = 0;
    char *value_str = NULL;   
    int value_length = 0;
    /* Now loop all formats, and put the message together, reallocating when enlarging */
    char * curr_format;
    for (int k = 0; k < format_count; k++)
    {
        curr_format = (char *)(format_array[k]);
        // Is there any formatting at all in the format string?
        if (strchr(curr_format, '%') != NULL) {
            // Fetch the next parameter.
            void * value = va_arg(arg, void *);
            // Handle fixed length byte arrays "%b"
            if (curr_format[1] == 'b') {
                int n=2;
                while (((int)(curr_format[n]) != 0) && (n < 10)) { n++; }
                if (n > 9 || n < 3) {
                    ESP_LOGE(helpers_log_prefix, "Bad byte parameter on location %i, len %i in format string: %s", k, n-2, curr_format);
                    new_length = -SDP_ERR_PARSING_FAILED;
                    goto cleanup;                    
                }
                ESP_LOGD(helpers_log_prefix, "Found null value at %i in %s", n, curr_format);
                value_length = atoi((char *)&(curr_format[2]));
                ESP_LOGD(helpers_log_prefix, "value_length parsed %i", value_length);
                value_str = malloc(value_length);
                memcpy(value_str,(void *)value, value_length);
                
            } else {
                // Used sprintf formatting
                value_length = asprintf(&value_str, curr_format, (void *)value);

                /*double test = 0;
                memcpy(&test, value, 8);

                ESP_LOGI(helpers_log_prefix, "Formatted value = %f with %s, into %s", test, curr_format, value_str);*/
            }
        } else {
            // If the format string doesn't start with a %-character, just use the format for value
            // TODO: Checking if it *contains* a %-character should be more flexible
            value_length = strlen(curr_format);
            value_str = malloc(value_length);
            strncpy(value_str, curr_format, value_length);
            
        }
        new_length+= value_length + 1;

        // TODO: See if realloc has any significant performance impact, if so an allocations strategy might be useful
        
        if (k == 0) {
            *message = heap_caps_malloc(new_length, MALLOC_CAP_8BIT);
        } else {
            *message = heap_caps_realloc(*message, new_length, MALLOC_CAP_8BIT);
        }

        if (*message == NULL)
        {
            ESP_LOGE(helpers_log_prefix, "(Re)alloc failed.");
            new_length = -SDP_ERR_OUT_OF_MEMORY;
            goto cleanup;
        };
        memcpy((*message) + curr_pos, value_str, value_length);   


        (*message)[new_length - 1] = (uint8_t)0x00;
        ESP_LOGD(helpers_log_prefix, "Message: %s, value_str: %s, new_length: %i.", (char *)*message, value_str, (int)new_length);
        free(value_str);
        value_str = NULL;
        curr_pos = new_length;

    }
    ESP_LOG_BUFFER_HEXDUMP(helpers_log_prefix, (char*)*message, new_length,  ESP_LOG_DEBUG);    
 cleanup:
    va_end(arg);
    free(value_str);   
    free(loc_format);
    free(format_array);
    return new_length;
}

void sdp_blink_led(gpio_num_t gpio_num, uint16_t time_on, uint16_t time_off, uint16_t times) {

    int count = 0;
    int pre_level = gpio_get_level(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
    while (count < times)
    {
        gpio_set_level(gpio_num, 1); 
        vTaskDelay(time_on / portTICK_PERIOD_MS);
        gpio_set_level(gpio_num, 0); 
        vTaskDelay(time_off / portTICK_PERIOD_MS);    
        count++;    
    }
    gpio_set_level(gpio_num, pre_level);


}

float sdp_read_battery()
{

    int volt;

    adc2_get_raw(ADC2_CHANNEL_7, ADC_WIDTH_BIT_12, &volt);
    float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 ;
    return battery_voltage;
}


void log_peer_info(char * _log_prefix, sdp_peer *peer) {
    ESP_LOGI(_log_prefix, "Peer info:");
    ESP_LOGI(_log_prefix, "Name:                  %s", peer->name);
    ESP_LOGI(_log_prefix, "Base Mac address:      %02X:%02X:%02X:%02X:%02X:%02X", peer->base_mac_address[0],
    peer->base_mac_address[1],peer->base_mac_address[2],peer->base_mac_address[3],peer->base_mac_address[4],peer->base_mac_address[5]);
    ESP_LOGI(_log_prefix, "State:                 %hhx", peer->state);
    ESP_LOGI(_log_prefix, "Supported media types: %hhx", peer->supported_media_types);
    ESP_LOGI(_log_prefix, "Protocol version:      %i", peer->protocol_version);
    ESP_LOGI(_log_prefix, "Next availability:     %lli", peer->next_availability);
    ESP_LOGI(_log_prefix, "Handle:                %i", peer->peer_handle);
}


void sdp_helpers_init(char * _log_prefix) {
    helpers_log_prefix = _log_prefix;
}
