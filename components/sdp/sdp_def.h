
#ifndef _SDP_DEF_H_
#define _SDP_DEF_H_

/* Common error codes */
typedef enum e_sdp_error_codes
{
    /* An "error" code of 0x0 means success */
    SDP_OK = 0,
    /* A message failed to send for some reason */
    SDP_ERR_SEND_FAIL = 1,
    /* A one or more messages failed to send during a broadcast */
    SDP_ERR_SEND_SOME_FAIL = 2,
    /* There was an error adding a conversation to the conversation queue */
    SDP_ERR_CONV_QUEUE = 3,
    /* The conversation queue is full, too many concurrent conversations. TODO: Add setting? */
    SDP_ERR_CONV_QUEUE_FULL = 4,
    /* An identifier was not found */
    SDP_ERR_INVALID_ID = 5,
    /* Couldn't get a semaphore to successfully lock a resource for thread safe usage. */
    SDP_ERR_SEMAPHORE = 6,
    /* SDP failed in its initiation. */
    SDP_ERR_INIT_FAIL = 7,
    /* Incoming message filtered */
    SDP_ERR_MESSAGE_FILTERED = 8,
    /* Invalid input parameter */
    SDP_ERR_INVALID_PARAM = 9,
    /* Message to short to comply */
    SDP_ERR_MESSAGE_TOO_SHORT = 10,
    /* Peer not found (handle or name not found) */
    SDP_ERR_PEER_NOT_FOUND = 11,
    /* Peer already exists */
    SDP_ERR_PEER_EXISTS = 12,
     /* Out of memory */
    SDP_ERR_OUT_OF_MEMORY = 13,
    /* OS error.  See enum os_error in os/os_error.h for meaning of values when debugging */
    SDP_ERR_OS_ERROR = 14
} e_sdp_error_codes;

/**
 * The work types are:
 * REQUEST: A peer have a request.
 * Requests are put on the work queue for consumption by the worker.
 *
 * DATA: Data, a response from a request.
 * A request can generate multible DATA responses.
 * Like requests, they are put on the work queue for consumption by the worker.
 *
 * PRIORITY: A priority message.
 * These are not put on the queue, but handled immidiately.
 * Examples may be message about an urgent problem, an emergency message (alarm) or an explicit instruction.
 * Immidiately, the peer responds with the CRC32 of the message, to prove to the reporter
 * that the message has been received before it invokes the callback.
 * In some cases it needs to know this to be able to start saving power or stop trying to send the message.
 * Emergency messages will be retried if the CRC32 doesn't match.
 * TODO: Move this into readme.md
 *
 */

typedef enum e_work_type
{
    REQUEST = 0x00,
    DATA = 0x01,
    PRIORITY = 0x02
} e_work_type;

/**
 * @brief Supported media types
 * These are the media types, "ALL" is used as a way to broadcast over all types
 *
 */

typedef enum e_media_type
{
    BLE = 0x00,
    ESPNOW = 0x01,
    LoRa = 0x02,
    TCPIP = 0x03,
    TTL = 0x04,
    ALL = 0x05
} e_media_type;

#endif