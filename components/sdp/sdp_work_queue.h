/**
 * @file sdp_worker.h
 * @author Nicklas Borjesson
 * @brief A generalized implementation for handling worker queues
 *  It is callback-based because of the cumbersomeness of passing pointers to queues
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _SDP_WORK_QUEUE_H_
#define _SDP_WORK_QUEUE_H_
/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "sdp_def.h"

#include "stdint.h"
#include <sys/queue.h>

typedef void *(first_queueitem)();
typedef void(remove_first_queueitem)();

typedef void(insert_tail)(void *new_item);

typedef void(poll_callback)(void *q_context);

typedef struct queue_context
{
  /* Queue management callbacks, needed because of the difficulties in passing queues as pointers */
  first_queueitem *first_queue_item_cb;
  remove_first_queueitem *remove_first_queueitem_cb;
  insert_tail *insert_tail_cb;

  /* Mandatory callback that handles incoming work items */
  work_callback *on_work_cb;

  /* Mandatory callback that handles incoming priority request immidiately - TODO: This should not be here*/
  work_callback *on_priority_cb;
  /* Optional callback that is called each poll period. T
  Note: This is run in the queue task, and might conflict with multitasking. */
  poll_callback *on_poll_cb;
  /* Max number of concurrent tasks. (0 = unlimited) */
  uint max_task_count;
  /* Current number of running tasks */
  uint task_count;
  /* Create separate tasks for each work. 
  Do not do this if the functionality isn't thread safe. 
  Also, what is gained in isolation, might be lost in performance. */
  bool multitasking;

  /* Worker task */
  TaskHandle_t worker_task_handle;
  /* Worker task name*/
  char worker_task_name[50];

  /* If set, the queue will not process any items */
  bool blocked;
  /* If set, worker will shut down */
  bool shutdown;
  /* Internal semaphores managed by the queue implementation - Do not set. */
  SemaphoreHandle_t __x_queue_semaphore;      // Thread-safe the queue
  SemaphoreHandle_t __x_task_state_semaphore; // Thread-safe the tasks

} queue_context;



esp_err_t safe_add_work_queue(queue_context *q_context, void *new_item);

esp_err_t init_work_queue(queue_context *q_context, char *_log_prefix, const char *queue_name);

void set_queue_blocked(queue_context *q_context, bool blocked);

void cleanup_queue_task(queue_context *q_context);

#endif
