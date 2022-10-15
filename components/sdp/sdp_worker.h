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


int init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix);

int safe_add_work_queue(work_queue_item_t *new_item);
work_queue_item_t *safe_get_head_work_item(void);

void cleanup_queue_task(work_queue_item_t *queue_item);

#endif

