
#ifndef _SDP_MESSAGING_H_
#define _SDP_MESSAGING_H_

#include <os/queue.h>

#include "stdint.h"

#include "sdp_def.h"
#include "sdp_worker.h"
#include "sdp_peer.h"

/**
 * @brief The peer's list of conversations.
 */

struct conversation_list_item
{
    uint8_t conversation_id; // The conversation it belongs to
    char *reason;

    SLIST_ENTRY(conversation_list_item)
    items;

    sdp_peer *peer;         // A handle to the underlying connection
};

SLIST_HEAD(conversation_list, conversation_list_item) conversation_l;

void parse_message(struct work_queue_item *queue_item);

int handle_incoming(sdp_peer *peer, uint16_t attr_handle, const  uint8_t *data, int data_len, e_media_type media_type);

int broadcast_message(uint16_t conversation_id,
                      enum e_work_type work_type, const void *data, int data_length);

e_media_type send_message(struct sdp_peer *peer, const void *data, int data_length);

int start_conversation(struct sdp_peer *peer, e_work_type work_type,
                       const char *reason, const void *data, int data_length);
int end_conversation(uint16_t conversation_id);

struct conversation_list_item *find_conversation(uint16_t conversation_id);

int sdp_reply(struct work_queue_item queue_item, enum e_work_type work_type, const void *data, int data_length);
int get_conversation_id(void);

void init_messaging(char *_log_prefix);


#endif