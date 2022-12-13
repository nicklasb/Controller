/**
 * @file sdp_worker.h
 * @author Nicklas Borjesson 
 * @brief The worker reacts to items appearing in the worker queue 
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _GSM_WORKER_H_
#define _GSM_WORKER_H_
/*********************
 *      INCLUDES
 *********************/

#include "../sdp_def.h"
#include "esp_err.h"

/*********************
 *      DEFINES
 *********************/

esp_err_t gsm_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix);
esp_err_t gsm_safe_add_work_queue(work_queue_item_t *new_item);
void gsm_set_queue_blocked(bool blocked);
void gsm_shutdown_worker();
void gsm_cleanup_queue_task(work_queue_item_t *queue_item);

#endif

