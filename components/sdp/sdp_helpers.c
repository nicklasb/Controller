

#include <stdarg.h>
#include "sdp_helpers.h"

#include "sdp_messaging.h"
#include "sdp_def.h"

#include "string.h"

#include "esp_heap_caps.h"

#include <esp_log.h>

#define WHO_LENGTH 3

char *log_prefix;

/* Helpers */

/**
 * @brief A simple way to, using format strings, build a message.
 * However; as the result will have to be null-terminated, and we cannot put nulls in that string.
 * Instead, the pipe-symbol "|" is used to indicate the null-terminated sections.
 * Only one format is allowed per section: "%i Mhz|%s" is allowed. "%i Mhz, %i watts|%s" is not.
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
    ESP_LOGI(log_prefix, "Format string %s len %i", loc_format, format_len);

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

    ESP_LOGI(log_prefix, "2 %i", (break_count) * sizeof(char *));
    /* Now we now that our format array needs to be break_count long, allocate it */
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
    /* Now loop all formats, and put the message together, reallocating when enlarging */
    for (int k = 0; k < format_count; k++)
    {
        int value_length = asprintf(&value_str, format_array[k], va_arg(arg, __int64_t));
        new_length = new_length + value_length + 1;
        *message = realloc(*message, new_length);
        if (*message == NULL)
        {
            ESP_LOGE(log_prefix, "Realloc failed.");
        };
        memcpy((*message) + curr_pos, value_str, value_length);
        (*message)[new_length - 1] = (uint8_t)0x00;
        free(value_str);
        curr_pos = new_length;
    }
    free(loc_format);
    free(format_array);
    return new_length;
}


/**
 * @brief Add a preamble with general info in the beginning of a message
 * 
 * @param work_type 
 * @param conversation_id 
 * @param data 
 * @param data_length 
 * @return void* 
 */

void *sdp_add_preamble(e_work_type work_type, uint16_t conversation_id, const void *data, int data_length)
{
    char *new_data = malloc(data_length + SDP_PREAMBLE_LENGTH);
    new_data[0] = SDP_PROTOCOL_VERSION;
    new_data[1] = (uint8_t)(&conversation_id)[0];
    new_data[2] = (uint8_t)(&conversation_id)[1];
    new_data[3] = (uint8_t)work_type;
    memcpy(&(new_data[SDP_PREAMBLE_LENGTH]), data, (size_t)data_length);
    return new_data;
}

/**
 * @brief Make a "WHO"-message that asks the peer to describe their abilities
 * 
 * @return char* A pointer to the created 
 */
char * make_who_msg() {
    char msg[WHO_LENGTH] = "WHO";
    return sdp_add_preamble(HANDSHAKE, 0, &msg, 3);
}

/**
 * @brief Simply return a length for a whole who-message
 * 
 * @return int 
 */
int who_msg_length() {
    return SDP_PREAMBLE_LENGTH + WHO_LENGTH;
}


void sdp_helpers_init(char * _log_prefix) {
    log_prefix = _log_prefix;
}
