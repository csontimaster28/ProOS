#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include <stddef.h>

// IPC message queue constants
#define MAX_MESSAGE_QUEUES 8
#define MAX_MESSAGES_PER_QUEUE 32
#define MAX_MESSAGE_SIZE 256

// Message structure
typedef struct {
    uint32_t from_pid;          // Sender process ID
    uint32_t to_pid;            // Receiver process ID
    uint32_t timestamp;         // Message timestamp
    uint32_t size;              // Message size
    uint8_t data[MAX_MESSAGE_SIZE];  // Message data
} ipc_message_t;

// Message queue structure
typedef struct {
    uint32_t queue_id;          // Queue ID
    uint32_t owner_pid;         // Owner process ID
    ipc_message_t messages[MAX_MESSAGES_PER_QUEUE];  // Message buffer
    uint32_t head;              // Head index
    uint32_t tail;              // Tail index
    uint32_t count;             // Message count
    uint8_t is_used;            // 1 if queue is active
} ipc_queue_t;

// IPC statistics
typedef struct {
    uint32_t total_queues;
    uint32_t active_queues;
    uint32_t total_messages;
    uint32_t total_sent;
    uint32_t total_received;
} ipc_stats_t;

// Initialize IPC system
void ipc_init(void);

// Queue operations
uint32_t ipc_create_queue(uint32_t owner_pid);
int ipc_destroy_queue(uint32_t queue_id);
int ipc_send_message(uint32_t from_pid, uint32_t to_pid, const uint8_t *data, uint32_t size);
int ipc_receive_message(uint32_t to_pid, ipc_message_t *msg);
int ipc_queue_exists(uint32_t queue_id);

// Statistics
void ipc_get_stats(ipc_stats_t *stats);
void ipc_print_stats(void);

#endif // IPC_H
