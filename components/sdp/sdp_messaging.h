
#ifndef _SDP_MESSAGING_H_
#define _SDP_MESSAGING_H_

#include <os/queue.h>

#include "sdp_def.h"
#include "sdp_worker.h"

/**
 * @brief The peer's list of conversations.
 */

struct conversation_list_item
{
    __uint8_t conversation_id; // The conversation it belongs to
    char *reason;

    SLIST_ENTRY(conversation_list_item)
    items;

    enum e_media_type media_type; // The underlying media type
    __uint16_t conn_handle;         // A handle to the underlying connection
};

SLIST_HEAD(conversation_list, conversation_list_item) conversation_l;

void parse_message(struct work_queue_item *queue_item);

int handle_incoming(__uint16_t conn_handle, __uint16_t attr_handle, char *data, int data_len, e_media_type media_type, void *arg);

int broadcast_message(__uint16_t conversation_id,
                      enum e_work_type work_type, const void *data, int data_length);

int send_message(__uint16_t conn_handle, __uint16_t conversation_id,
                 enum e_work_type work_type, const void *data, int data_length);

int start_conversation(e_media_type media_type, int conn_handle, e_work_type work_type,
                       const char *reason, const void *data, int data_length);
int end_conversation(__uint16_t conversation_id);

struct conversation_list_item *find_conversation(__uint16_t conversation_id);

int sdp_reply(struct work_queue_item queue_item, enum e_work_type work_type, const void *data, int data_length);
int get_conversation_id(void);

void init_messaging(char *_log_prefix);

#endif