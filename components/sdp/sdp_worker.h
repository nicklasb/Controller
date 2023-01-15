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
#include "esp_err.h"

/*********************
 *      DEFINES
 *********************/



esp_err_t sdp_safe_add_work_queue(work_queue_item_t *new_item);
void sdp_cleanup_queue_task(work_queue_item_t *queue_item);

void sdp_set_queue_blocked(bool blocked);

void sdp_shutdown_worker();

esp_err_t sdp_init_worker(work_callback *work_cb, char *_log_prefix);


#endif

