
#ifndef _SDP_MESSAGING_H_
#define _SDP_MESSAGING_H_

#include <sys/queue.h> 

#include "stdint.h"

#include "sdp_def.h"

#include "sdp_peer.h"

/**
 * @brief The peer's list of conversations.
 */

struct conversation_list_item
{
    /* The conversation it belongs to */
    uint8_t conversation_id; 
    /* The reason for the conversation */ 
    char *reason;
    /* A pointer to the peer object */
    sdp_peer *peer;         
     /* Is it local? I.e. is this conversation*/
    bool local;     
           
    SLIST_ENTRY(conversation_list_item)
    items;


};

SLIST_HEAD(conversation_list, conversation_list_item) conversation_l;

void parse_message(work_queue_item_t *queue_item);

int handle_incoming(sdp_peer *peer, const  uint8_t *data, int data_len, e_media_type media_type);

int broadcast_message(uint16_t conversation_id,
                      enum e_work_type work_type, void *data, int data_length);

int start_conversation(sdp_peer *peer, e_work_type work_type,
                       const char *reason, const void *data, int data_length);
int end_conversation(uint16_t conversation_id);

struct conversation_list_item *find_conversation(sdp_peer *peer, uint16_t conversation_id);

int sdp_reply(work_queue_item_t queue_item, enum e_work_type work_type, const void *data, int data_length);

void sdp_init_messaging(char *_log_prefix);


#endif