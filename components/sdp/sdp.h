/**
 * @file sdp.c
 * @author Nicklas Borjesson
 * @brief This is the sensor data protocol server header file and queue definitions
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _SDP_H_
#define _SDP_H_


#include <os/queue.h>
#include <nimble/ble.h>


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

/* Common error codes */
typedef enum sdp_error_codes
{
    /* An "error" code of 0x0 means success */
    SDP_OK = 0x00,
    /* A message failed to send for some reason */
    SDP_ERR_SEND_FAIL = 0x01,
    /* A one or more messages failed to send during a broadcast */
    SDP_ERR_SEND_SOME_FAIL = 0x02,
    /* There was an error adding a conversation to the conversation queue */
    SDP_ERR_CONV_QUEUE = 0x03,
    /* The conversation queue is full, too many concurrent conversations. TODO: Add setting? */
    SDP_ERR_CONV_QUEUE_FULL = 0x04,
    /* An identifier was not found */
    SDP_ERR_INVALID_ID = 0x05,
    /* Couldn't get a semaphore to successfully lock a resource for thread safe usage. */
    SDP_ERR_SEMAPHORE = 0x06,
    /* SDP failed in its initiation. */
    SDP_ERR_INIT_FAIL = 0x07,
    /* Incoming message filtered */
    SDP_ERR_MESSAGE_FILTERED = 0x08,
    /* Invalid input parameter */
    SDP_ERR_INVALID_PARAM = 0x09,
    /* Message to short to comply */
    SDP_ERR_MESSAGE_TOO_SHORT = 0x0a

} sdp_error_codes;

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

typedef enum work_type
{
    REQUEST = 0x00,
    DATA = 0x01,
    PRIORITY = 0x02
} work_type;

/**
 * @brief Supported media types
 * These are the media types, "ALL" is used as a way to broadcast over all types
 *
 */

typedef enum media_type
{
    BLE = 0x00,
    ESPNOW = 0x01,
    LoRa = 0x02,
    TCPIP = 0x03,
    TTL = 0x04,
    ALL = 0x05
} media_type;

/**
 * @brief This is the request queue
 * The queue is served by worker threads in a thread-safe manner.
 */
struct work_queue_item
{
    /* The type of work */
    enum work_type work_type;

    /* Hash of indata */
    uint16_t crc32;
    /* Protocol version of the request */
    uint8_t version;
    /* The conversation it belongs to */
    uint8_t conversation_id;
    /* The data */
    char *raw_data;
    /* The length of the data in bytes */
    uint16_t raw_data_length;
    /* The message parts as an array of null-terminated strings */
    char **parts;
    /* The number of message parts */
    int partcount;
    /* The underlying media type, avoid using this data to stay tech agnostic */
    enum media_type media_type;
    /* A handle to the underlying connection */
    uint16_t conn_handle;

    /* Queue reference */
    STAILQ_ENTRY(work_queue_item)
    items;
};

/* Work queue initialisation */
STAILQ_HEAD(work_queue, work_queue_item)
work_q;

/**
 * @brief The peer's list of conversations.
 */

struct conversation_list_item
{
    uint8_t conversation_id; // The conversation it belongs to
    char *reason;

    SLIST_ENTRY(conversation_list_item)
    items;

    enum media_type media_type; // The underlying media type
    uint16_t conn_handle;       // A handle to the underlying connection
};

/* Conversation list initialisation */
SLIST_HEAD(conversation_list, conversation_list_item)
conversation_l;

/**
 * These callbacks are implemented to handle the different
 * work types.
 */
/* Callbacks that handles incoming work items */
typedef void(work_callback)(struct work_queue_item *work_item);

/* Mandatory callback that handles incoming work items */
work_callback *on_work_cb;

/* Mandatory callback that handles incoming priority request immidiately */
work_callback *on_priority_cb;

/* Callbacks that act as filters on incoming work items */
typedef int(filter_callback)(struct work_queue_item *work_item);

/* Optional callback that intercepts before incoming request messages are added to the work queue */
filter_callback *on_filter_request_cb;
/* Optional callback that intercepts before incoming data messages are added to the work queue */
filter_callback *on_filter_data_cb;

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
int safe_add_work_queue(struct work_queue_item *new_item);
struct work_queue_item *safe_get_head_work_item(void);
int start_conversation(media_type media_type, int conn_handle, work_type work_type,
                       const char *reason, const void *data, int data_length);
int end_conversation(uint16_t conversation_id);
struct conversation_list_item *find_conversation(uint16_t conversation_id);
int sdp_reply(struct work_queue_item queue_item, enum work_type work_type, const void *data, int data_length);
int get_conversation_id(void);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

