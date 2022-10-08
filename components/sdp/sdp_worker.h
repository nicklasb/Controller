/**
 * @file sdp_worker.h
 * @author Nicklas Borjesson 
 * @brief The worker reacts to items appearing in the worker queue 
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _SDP_WORKER_H_
#define _SDP_WORKER_H_
/*********************
 *      INCLUDES
 *********************/


/*********************
 *      DEFINES
 *********************/

#include "sdp_def.h"

#include "stdint.h"
#include <os/queue.h>
#include "sdp_peer.h"


/**
 * @brief This is the request queue
 * The queue is served by worker threads in a thread-safe manner.
 */
struct work_queue_item
{
    /* The type of work */
    enum e_work_type work_type;

    /* Hash of indata */
    uint16_t crc32;
    /* Protocol version of the request */
    uint8_t version;
    /* The conversation it belongs to */
    uint8_t conversation_id;
    /* The data */
    char *raw_data;
    /* The length of the data in bytes */
    uint16_t raw_data_length;
    /* The message parts as an array of null-terminated strings */
    char **parts;
    /* The number of message parts */
    int partcount;
    /* The underlying media type, avoid using this data to stay tech agnostic */
    enum e_media_type media_type;
    /* The peer */
    sdp_peer *peer;

    /* Queue reference */
    STAILQ_ENTRY(work_queue_item)
    items;
};

/* Expands to a declaration for the work queue */
STAILQ_HEAD(work_queue, work_queue_item) work_q;

/**
 * These callbacks are implemented to handle the different
 * work types.
 */
/* Callbacks that handles incoming work items */
typedef void(work_callback)(struct work_queue_item *work_item);

/* Mandatory callback that handles incoming work items */
work_callback *on_work_cb;

/* Mandatory callback that handles incoming priority request immidiately */
work_callback *on_priority_cb;

/* Callbacks that act as filters on incoming work items */
typedef int(filter_callback)(struct work_queue_item *work_item);

/* Optional callback that intercepts before incoming request messages are added to the work queue */
filter_callback *on_filter_request_cb;
/* Optional callback that intercepts before incoming data messages are added to the work queue */
filter_callback *on_filter_data_cb;


int init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix);

int safe_add_work_queue(struct work_queue_item *new_item);
struct work_queue_item *safe_get_head_work_item(void);

void cleanup_queue_task(struct work_queue_item *queue_item);

#endif

