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
#include "../sdp_work_queue.h"

/*********************
 *      DEFINES
 *********************/

#include <esp_err.h>

typedef struct lora_queue_item
{
    /* The data */
    char *data;
    /* The length of the data in bytes */
    uint16_t data_length;
    /* The peer */  
    struct sdp_peer *peer;

    /* Queue reference */
    STAILQ_ENTRY(lora_queue_item)
    items;
} lora_queue_item_t;

esp_err_t lora_safe_add_work_queue(sdp_peer *peer, char *data, int data_length);

esp_err_t lora_init_worker(work_callback work_cb, poll_callback poll_cb, char *_log_prefix);
void lora_set_queue_blocked(bool blocked);
void lora_shutdown_worker();
void lora_cleanup_queue_task(lora_queue_item_t *queue_item);

#endif

