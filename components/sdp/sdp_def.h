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
#define SDP_PREAMBLE_LENGTH 7
/* The length, in bytes of the SDP preamble. */
#define SDP_CRC_LENGTH 4
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
    ORCHESTRATION = 5,
    QOS = 6 
} e_work_type;

/**
 * @brief Supported media types
 * These are the media types, "ALL" is used as a way to broadcast over all types
 * Note: If more are needed, perhaps this could be split. 
 */

typedef enum e_media_type
{
    SDP_MT_NONE = 0,
    /* Wireless */
    SDP_MT_BLE = 1,
    SDP_MT_ESPNOW = 2,
    SDP_MT_LoRa = 4,
    SDP_MT_UMTS = 8, // TODO: Only implemented as a service. However UTMS isn't really a thing. PPP?
    
    /* Wired */
    SDP_MT_I2C = 16,
    SDP_MT_CANBUS = 32, // TODO: Not implemented
    SDP_MT_TTL = 64, // TODO: Probably this should be implemented in some way. Or onewire. 
    SDP_MT_ANY = 256
} e_media_type;

/**
 * TODO: UMTS is not implemented really. Probably it should not be TCP but something else(UMTS?) 
 * 
*/
/**
 * @brief This is a byte representing the medias supported
 * It is or'ed from the e_media_type enum values
 *
 */
typedef uint8_t sdp_media_types;

sdp_media_types get_host_supported_media_types();

void add_host_supported_media_type (e_media_type supported_media_type);

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


#define FAILURE_RATE_HISTORY_LENGTH 4

/**
 * @brief Media 
 * This is used by the adaptive transmission to find the optimal media
 * 
 */
struct sdp_peer_media_stats
{

    /* Last time peer was scored TODO: Unclear if this is used*/
    uint64_t last_score_time;
    /* Last score */
    float last_score;
    /* How many times score has been calculated since reset */
    float score_count;  
    // TODO: We really don't need this to be more than bytes. 0-255 is way enough detail.
    /* Length of the history */
    float failure_rate_history[FAILURE_RATE_HISTORY_LENGTH];

    /* Number of crc mismatches from the peer */
    uint32_t crc_mismatches;
        
    /* Supported speed bit/s */
    uint32_t theoretical_speed;
    /* Actual speed bit/s. 
    NOTE: Always lower than theoretical, and with small payloads; *much* lower */
    uint32_t actual_speed;

    /* Number of times we have failed sending to a peer since last check */
    uint32_t send_failures;
    /* Number of times we have failed receiving data from a peer since last check */
    uint32_t receive_failures;    
    /* Number of times we have succeeded sending to a peer since last check */
    uint32_t send_successes;
    /* Number of times we have succeeed eceiving data from a peer since last check */
    uint32_t receive_successes;    
    

};



/* This is the maximum number of peers */
/* NOTE: A list of relations, it relations+mac_addresses (4 + 6 * SDP_MAX_PEERS) is stored
* in RTC memory, so 32 peers mean 320 bytes of the 8K RTC memory
*/
#define SDP_MAX_PEERS 32

/* A peer */
typedef struct sdp_peer
{
    /* The name of the peer*/
    sdp_peer_name name;
    
    /**
     * @brief Following is the the 6-byte base MAC adress of the peer.
     * Note that the other MAC-adresses are offset, and in BLE´s case little-endian, for example. More info here:
     * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#mac-address
     */
    sdp_mac_address base_mac_address;

    /* Eight bits of the media types*/
    sdp_media_types supported_media_types;

    /* Protocol version*/
    uint8_t protocol_version;
    /* Minimum supported protocol version*/
    uint8_t min_protocol_version;
     /** A generated 32-bit crc32 of the peers and this peers mac addresses
      * Used by non-addressed and low-bandwith medias (LoRa) to economically resolve peers w
      */
    uint32_t relation_id;   
    /* The unique handle of the peer*/
    uint16_t peer_handle;    
    /* The peer state, if unknown, it cannot be used in many situations*/
    e_peer_state state;    
    /* Next availability (measured in mikroseconds from first boot)*/
    uint64_t next_availability;

    /* Media-specific statistics, used by transmission optimizer */
    #if CONFIG_SDP_LOAD_BLE
    struct sdp_peer_media_stats ble_stats;
    #endif

    #if CONFIG_SDP_LOAD_ESP_NOW
    struct sdp_peer_media_stats espnow_stats;
    bool espnow_peer_added;
    #endif

    #if CONFIG_SDP_LOAD_LORA
    struct sdp_peer_media_stats lora_stats;
    #endif
    #if CONFIG_SDP_LOAD_I2C
    struct sdp_peer_media_stats i2c_stats;
    uint8_t i2c_address;
    #endif
    
    SLIST_ENTRY(sdp_peer)
    next;

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
    uint32_t crc32;
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
typedef void (work_callback)(void *work_item);

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