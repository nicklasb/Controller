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

#include "sdp_def.h"
#include "sdp_work_queue.h"

/*********************
 *      DEFINES
 *********************/


// The queue context
queue_context gsm_queue_context; 

esp_err_t gsm_safe_add_work_queue(work_queue_item_t *new_item);

esp_err_t gsm_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix);
void gsm_cleanup_queue_task(work_queue_item_t *queue_item);

#endif

