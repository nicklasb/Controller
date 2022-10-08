/**
 * @file sdp.c
 * @author Nicklas Borjesson
 * @brief This is the sensor data protocol server header file and queue definitions
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef _SDP_H_
#define _SDP_H_

#ifdef __cplusplus
extern "C"
{
#endif





#include <nimble/ble.h>

#include "sdp_def.h"
#include "sdp_worker.h"


/**
 * This is the definitions of the Sensor Data Protocol (SDP) implementation
 * To be decided is how much of the implementation should be here.
 */

// TODO: Break out all BLE-specifics, perhaps ifdef them.

/* The current protocol version */
#define SPD_PROTOCOL_VERSION 0

/* Lowest supported protocol version */
#define SPD_PROTOCOL_VERSION_MIN 0

/* The length, in bytes of the SDP preamble. */
#define SDP_PREAMBLE_LENGTH 4

/* The log prefix for all logging */
char *log_prefix;

/**
 * @brief Initialise the sdp subsystem
 *
 * @param work_cb The handling of incoming work items
 * @param priority_cb Handling of prioritized request
 * @param log_prefix How logs entries should be prefixed
 * @param is_controller Is this the controller?
 * @return int  Returns 0 value if successful.
 */
int sdp_init(work_callback work_cb, work_callback priority_cb, const char *_log_prefix, bool is_controller);



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
