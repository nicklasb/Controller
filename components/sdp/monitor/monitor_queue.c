/**
 * @file monitor_queue.c
 * @author Nicklas Börjesson (nicklasb@gmail.com)
 * @brief Monitors the queues of the system.
 * @version 0.1
 * @date 2023-02-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "monitor_queue.h"

#include "sdp_worker.h"

void monitor_queue() {
    sdp_worker_on_monitor();
}