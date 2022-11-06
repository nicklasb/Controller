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


#include "sdp_work_queue.h"

// The queue context
queue_context q_context; 

esp_err_t sdp_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix);


#endif

