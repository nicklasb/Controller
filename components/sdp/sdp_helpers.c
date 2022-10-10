

#include <stdarg.h>
#include "sdp_helpers.h"

#include "sdp_messaging.h"
#include "sdp_def.h"

#include "string.h"

#include "esp_heap_caps.h"

#include <esp_log.h>

#define WHO_LENGTH 4

char *log_prefix;

/* Helpers */

/**
 * @brief A simple way to, using format strings, build a message.
 * However; as the result will have to be null-terminated, and we cannot put nulls in that string.
 * Instead, the pipe-symbol "|" is used to indicate the null-terminated sections.
 * Only one format is allowed per section: "%i Mhz|%s" is allowed. "%i Mhz, %i watts|%s" is not.
 * Integers can be passed as is, 
 *
 * @param message A pointer to a pointer to the message structure, memory will be allocated to it.
 * @param format A format string where "|" indicates where null-separation is wanted and creates a section.
 * @param arg A list of arguments supplying data for the format.
 * @return int
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
    ESP_LOGD(log_prefix, "Format string %s len %i", loc_format, format_len);

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
    char *value_str;    
    int value_length = 0;
    /* Now loop all formats, and put the message together, reallocating when enlarging */
    for (int k = 0; k < format_count; k++)
    {
        
        
        if (strchr((char *)format_array[k], '%')) {
            void * value = va_arg(arg, void *);
            value_length = asprintf(&value_str, format_array[k], (void *)value);
            new_length+= value_length + 1;
        } else {
            
            value_str = format_array[k];
            value_length = strlen(format_array[k]);
            new_length+= value_length + 1;
        }

        // TODO: See if realloc has any significant performance impact, if so an allocations strategy might be useful
        
        if (k == 0) {
            *message = heap_caps_malloc(new_length, MALLOC_CAP_8BIT);
        } else {
            *message = heap_caps_realloc(*message, new_length, MALLOC_CAP_8BIT);
        }

        if (*message == NULL)
        {
            ESP_LOGE(log_prefix, "(Re)alloc failed.");
            return -1;
        };
        memcpy((*message) + curr_pos, value_str, value_length);   
        (*message)[new_length - 1] = (uint8_t)0x00;
        ESP_LOGD(log_prefix, "Message: %s, value_str: %s, new_length: %i.", (char *)*message, value_str, (int)new_length);
        curr_pos = new_length;

    }
    ESP_LOG_BUFFER_HEXDUMP(log_prefix, (char*)*message, new_length,  ESP_LOG_DEBUG);    
    va_end(arg);
    free(value_str);   
    free(loc_format);
    free(format_array);
    return new_length;
}

void sdp_helpers_init(char * _log_prefix) {
    log_prefix = _log_prefix;
}
