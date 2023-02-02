/**
 * @file i2c_worker.h
 * @author Nicklas Borjesson 
 * @brief The worker reacts to items appearing in the worker queue 
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _I2C_WORKER_H_
#define _I2C_WORKER_H_
/*********************
 *      INCLUDES
 *********************/

#include "../sdp_def.h"
#include "../sdp_work_queue.h"

/*********************
 *      DEFINES
 *********************/

#include <esp_err.h>
// TODO: We should be able to reuse the queue_item types
typedef struct i2c_queue_item
{
    /* The data */
    char *data;
    /* The length of the data in bytes */
    uint16_t data_length;
    /* The peer */  
    struct sdp_peer *peer;
    /* Continue to try using send_message and rescoring */
    bool try_rescoring;
    /* Queue reference */
    STAILQ_ENTRY(i2c_queue_item)
    items;
} i2c_queue_item_t;

esp_err_t i2c_safe_add_work_queue(sdp_peer *peer, char *data, int data_length, bool try_rescoring);

esp_err_t i2c_init_worker(work_callback work_cb, poll_callback poll_cb, char *_log_prefix);

queue_context *i2c_get_queue_context();

void i2c_set_queue_blocked(bool blocked);
void i2c_shutdown_worker();
void i2c_cleanup_queue_task(i2c_queue_item_t *queue_item);

#endif

