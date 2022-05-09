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
 */

typedef enum work_type {REQUEST,DATA, PRIORITY} work_type;

typedef enum media_type {BLE, LoRa, TCPIP, TTL} media_type;

/**
 * @brief This is the request queue
 * The queue is served by worker threads in a thread-safe manner. 
 */


struct work_queue_item {
    enum work_type work_type;
    
    uint16_t crc32; // Hash of indata
    uint8_t version; // Version of request
    uint8_t conversation_id; // Version of request
    void *data;
    uint16_t data_length;
    STAILQ_ENTRY(work_queue_item) items;   

    // Information on communication media, avoid using this data to stay tech agnostic
    enum media_type media_type; // The underlying media type
    uint16_t conn_handle; // A handle to the underlying connection

};

STAILQ_HEAD(work_queue, work_queue_item) request_q;


/* Must be used when changing the work queue  */
SemaphoreHandle_t xQueue_Semaphore;

/** 
 * These callbacks are implemented to handle the different 
 * work types. 
 */
void (*on_request_cb)(struct work_queue_item queue_item);
void (*on_data_cb)(struct work_queue_item queue_item);
void (*on_priority_cb)(struct work_queue_item queue_item);


/* The current protocol version */
#define SPD_PROTOCOL_VERSION 0

/* Lowest supported protocol version */
#define SPD_PROTOCOL_VERSION_MIN 0



#ifdef __cplusplus
} /* extern "C" */
#endif
