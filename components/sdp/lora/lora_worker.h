/**
 * @file sdp_worker.h
 * @author Nicklas Borjesson 
 * @brief The worker reacts to items appearing in the worker queue 
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _LORA_WORKER_H_
#define _LORA_WORKER_H_
/*********************
 *      INCLUDES
 *********************/

#include "../sdp_def.h"


/*********************
 *      DEFINES
 *********************/

#include <esp_err.h>

esp_err_t lora_safe_add_work_queue(work_queue_item_t *new_item);

esp_err_t lora_init_worker(work_callback work_cb, work_callback priority_cb, char *_log_prefix);
void lora_set_queue_blocked(bool blocked);
void lora_shutdown_worker();
void lora_cleanup_queue_task(work_queue_item_t *queue_item);

#endif

