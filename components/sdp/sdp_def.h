#ifndef _SDP_DEF_H_
#define _SDP_DEF_H_

#ifdef __cplusplus
extern "C"
{
#endif


#include "sdkconfig.h"

#include <sys/queue.h>
#include <stdint.h>
#include <stdbool.h>

/* The possible states of SDP */
enum sdp_states
{
    STARTING = 0,
    RUNNING = 1,
    SHUTTING_DOWN = 2
};

/* The current protocol version */
#define SDP_PROTOCOL_VERSION 0

/* Lowest supported protocol version */
#define SDP_PROTOCOL_VERSION_MIN 0

/* The length, in bytes of the SDP preamble. */
#define SDP_PREAMBLE_LENGTH 4

/* Define all the reasonable time */
#define SECOND 1000000
#define MINUTE 60000000
#define HOUR 3600000000

/* How long will we be running if no one extends our session */
#define SDP_AWAKE_TIME_uS 40 * SECOND
/* The most amount of time the peer gives itself until it goes to sleep */
#define SDP_AWAKE_TIMEBOX_uS SDP_AWAKE_TIME_uS * 2

/* How long will each cycle be*/
#define SDP_SLEEP_TIME_uS HOUR - SDP_AWAKE_TIME_uS

#if SDP_AWAKE_TIMEBOX_uS - SDP_SLEEP_TIME_uS > SDP_SLEEP_TIME_uS
#error "SDP_AWAKE_TIMEBOX - SDP_SLEEP_TIME_uS  cannot be longer than the SDP_SLEEP_TIME_uS"
#endif
/* Will we wait a little extra to avoid flooding? */
#define SDP_AWAKE_MARGIN_uS 2000000

/* How long should we sleep until next retry (in us) */
#define SDP_ORCHESTRATION_RETRY_WAIT_uS 30000000

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
    SDP_ERR_OS_ERROR = 14,
    /* Parsing error */
    SDP_ERR_PARSING_FAILED = 15,
    /* Message to long to comply */
    SDP_ERR_MESSAGE_TOO_LONG = 16,
    /* This feature is not supported */
    SDP_ERR_NOT_SUPPORTED = 17    
} e_sdp_error_codes;

/* Common warning codes */
typedef enum e_sdp_warning_codes
{
    /* No peers */
    SDP_WARN_NO_PEERS = 1
} e_sdp_warning_codes;

/**
 * The work types are:
 * HANDSHAKE: Initial communication between two peers.
 *
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
 *
 * ORCHESTRATION: This is about handling peers and controlling the mesh.
 * TODO: Move this into readme.md
 *
 * > 1000:
 *
 */

typedef enum e_work_type
{
    HANDSHAKE = 0,
    REQUEST = 1,
    REPLY = 2,
    DATA = 3,
    PRIORITY = 4,
    ORCHESTRATION = 5
} e_work_type;

/**
 * @brief Supported media types
 * These are the media types, "ALL" is used as a way to broadcast over all types
 *
 */

typedef enum e_media_type
{
    SDP_MT_NONE = 0,
    SDP_MT_BLE = 1,
    SDP_MT_ESPNOW = 2,
    SDP_MT_LoRa = 4,
    SDP_MT_TCPIP = 8, // TODO: Probably it should not be TCP but something else.
    SDP_MT_TTL = 16,
    SDP_MT_ANY = 128

} e_media_type;

/**
 * @brief This is a byte representing the medias supported
 * It is or'ed from the e_media_type enum values
 *
 */
typedef uint8_t sdp_media_types;

/* SDP peer stat, broadly categorizes the credibility of the peer */
typedef enum e_peer_state
{
    /* The peer is unknown */
    PEER_UNKNOWN = 0,
    /* The peer has presented itself, but isn't encrypted*/
    PEER_KNOWN_INSECURE = 1,
    /* The peer is both known and encrypted */
    PEER_KNOWN_SECURE = 2
} e_peer_state;

/* The length of MAC-addresses in SDP. DO NOT CHANGE.*/
#define SDP_MAC_ADDR_LEN 6

/* MAC-addresses should always be 6-byte values regardless of tech */

#if (ESP_NOW_ETH_ALEN && ESP_NOW_ETH_ALEN != 6) || SDP_MAC_ADDR_LEN != 6
#error ESP_NOW_ETH_ALEN or SDP_MAC_ADDR_LEN has been set to something else than six bytes. \
MAC-addresses are 6-byte values regardless of technology. \
This library assumes this and may fail using other lengths for this setting.
#endif

/* The SD MAC-address type */
typedef uint8_t sdp_mac_address[SDP_MAC_ADDR_LEN];

/* A peer name in SDP */
typedef char sdp_peer_name[CONFIG_SDP_PEER_NAME_LEN];

/** Information about *this* host.
 * This is used by all parts of the application.
 * Much intersects with the peer information, but is set during initialization.
 */
typedef struct sdp_host_t
{

    /* Protocol version*/
    uint8_t protocol_version;
    /* Minimum supported protocol version*/
    uint8_t min_protocol_version;
    /* The peer name*/
    sdp_peer_name sdp_host_name;

    /* The base mac adress handle of the system*/
    sdp_mac_address base_mac_address;

} sdp_host_t;

typedef struct sdp_peer
{
    SLIST_ENTRY(sdp_peer)
    next;

    /* The unique handle of the peer*/
    uint16_t peer_handle;
    /* The name of the peer*/
    sdp_peer_name name;
    /* Eight bits of the media types*/
    sdp_media_types supported_media_types;
    /* Last time heard from the peer*/
    uint64_t last_time_in;
    /* Last time we tried to contact the  peer*/
    uint64_t last_time_out;
    /* The peer state, if unknown, it cannot be used in many situations*/
    e_peer_state state;

    /* Protocol version*/
    uint8_t protocol_version;
    /* Minimum supported protocol version*/
    uint8_t min_protocol_version;

    /* Next availability (measured in mikroseconds from first boot)*/
    uint64_t next_availability;

    /**
     * @brief Following is the the 6-byte base MAC adress of the peer.
     * Note that the other MAC-adresses are offset, and in BLE´s case little-endian, for example. More info here:
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#mac-address
     */

    sdp_mac_address base_mac_address;

#ifdef CONFIG_SDP_LOAD_BLE
    /* The connection handle of the BLE connection, NimBLE has its own peer handling.*/
    int ble_conn_handle;
#endif

} sdp_peer;

/**
 * @brief This is the request queue
 * The queue is served by worker tasks in a thread-safe manner.
 */

typedef struct work_queue_item
{
    /* The type of work */
    enum e_work_type work_type;

    /* Hash of indata */
    uint16_t crc32;
    /* Protocol version of the request */
    uint8_t version;
    /* The conversation it belongs to */
    uint16_t conversation_id;
    /* The data */
    char *raw_data;
    /* The length of the data in bytes */
    uint16_t raw_data_length;
    /* The message parts as an array of null-terminated strings */
    char **parts;
    /* The number of message parts */
    int partcount;
    /* The underlying media type, avoid using this data to stay tech agnostic */
    enum e_media_type media_type;
    /* The peer */
    struct sdp_peer *peer;

    /* Queue reference */
    STAILQ_ENTRY(work_queue_item)
    items;
} work_queue_item_t;

/**
 * These callbacks are implemented to handle the different
 * work types.
 */
/* Callbacks that handles incoming work items */
typedef void(work_callback)(work_queue_item_t *work_item);

/* Mandatory callback that handles incoming work items */
extern work_callback *on_work_cb;

/* Mandatory callback that handles incoming priority request immidiately */
extern work_callback *on_priority_cb;

/* Callbacks that act as filters on incoming work items */
typedef int(filter_callback)(work_queue_item_t *work_item);


/* Callbacks that are called before sleeping, return true to stop going to sleep. */
typedef bool(before_sleep)();



#ifdef __cplusplus
} /* extern "C" */
#endif

#endif