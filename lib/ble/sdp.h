#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "os/queue.h"


/** 
 * This is the definitions of the Sensor Data Protocol (SDP) implementation
 * To be decided is how much of the implementation should be here. 
 */

// TODO: Break out all BLE-specifics, perhaps ifdef them.


/* The current protocol version */
#define SPD_PROTOCOL_VERSION 0

/* Lowest supported protocol version */
#define SPD_PROTOCOL_VERSION_MIN 0


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

typedef enum work_type {REQUEST,DATA, PRIORITY} work_type;

/**
 * @brief Supported media types
 * These are the media types, "ALL" is used as a way to broadcast over all types 
 * 
 */

typedef enum media_type {BLE, LoRa, TCPIP, TTL, ALL} media_type;

/**
 * @brief This is the request queue
 * The queue is served by worker threads in a thread-safe manner. 
 */
struct work_queue_item {
    /* The type of work */
    enum work_type work_type;
    
    /* Hash of indata */
    uint16_t crc32; 
    /* Protocol version of the request */
    uint8_t version; 
    /* The conversation it belongs to */
    uint8_t conversation_id; 
    /* The data */
    void *data;
    /* The length of the data in bytes */
    uint16_t data_length;

    /* The underlying media type, avoid using this data to stay tech agnostic */
    enum media_type media_type; 
    /* A handle to the underlying connection */
    uint16_t conn_handle; 

    /* Queue reference */
    STAILQ_ENTRY(work_queue_item) items;   

};

/* Work queue initialisation */
STAILQ_HEAD(work_queue, work_queue_item) work_q;

/**
 * @brief The peer's list of conversations.
 */

struct conversation_list_item {
    uint8_t conversation_id; // The conversation it belongs to

    SLIST_ENTRY(conversation_list_item) items;   

    enum media_type media_type; // The underlying media type
    uint16_t conn_handle; // A handle to the underlying connection

};

/* Conversation list initialisation */
SLIST_HEAD(conversation_list, conversation_list_item) conversation_l;


/* Must be used when changing the work queue  */
SemaphoreHandle_t xQueue_Semaphore;

/** 
 * These callbacks are implemented to handle the different 
 * work types. 
 */
void (*on_request_cb)(struct work_queue_item queue_item);
void (*on_data_cb)(struct work_queue_item queue_item);
void (*on_priority_cb)(struct work_queue_item queue_item);

int safe_add_work_queue(struct work_queue_item *new_item, const char *log_tag);
int safe_add_conversation_queue(struct conversation_list_item *new_item, const char *log_tag);

#ifdef __cplusplus
} /* extern "C" */
#endif
