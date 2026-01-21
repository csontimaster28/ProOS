#include "ipc.h"
#include "memory.h"

// Message queue table
static ipc_queue_t queue_table[MAX_MESSAGE_QUEUES];
static uint32_t queue_count = 0;
static uint32_t next_queue_id = 1;

// IPC statistics
static ipc_stats_t ipc_stats = {0};

// External function declarations
void console_puts(const char *s);
void itoa(int num, char *str);

// Initialize IPC system
void ipc_init(void) {
    queue_count = 0;
    next_queue_id = 1;
    
    // Initialize queue table
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        queue_table[i].is_used = 0;
        queue_table[i].queue_id = 0;
        queue_table[i].owner_pid = 0;
        queue_table[i].head = 0;
        queue_table[i].tail = 0;
        queue_table[i].count = 0;
    }
    
    ipc_stats.total_queues = MAX_MESSAGE_QUEUES;
    ipc_stats.active_queues = 0;
    ipc_stats.total_messages = 0;
    ipc_stats.total_sent = 0;
    ipc_stats.total_received = 0;
}

// Create a message queue
uint32_t ipc_create_queue(uint32_t owner_pid) {
    if (queue_count >= MAX_MESSAGE_QUEUES) {
        return 0;  // No free queues
    }
    
    // Find free queue
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (!queue_table[i].is_used) {
            queue_table[i].is_used = 1;
            queue_table[i].queue_id = next_queue_id++;
            queue_table[i].owner_pid = owner_pid;
            queue_table[i].head = 0;
            queue_table[i].tail = 0;
            queue_table[i].count = 0;
            
            queue_count++;
            ipc_stats.active_queues++;
            
            return queue_table[i].queue_id;
        }
    }
    
    return 0;
}

// Destroy a message queue
int ipc_destroy_queue(uint32_t queue_id) {
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (queue_table[i].is_used && queue_table[i].queue_id == queue_id) {
            queue_table[i].is_used = 0;
            queue_table[i].count = 0;
            queue_count--;
            ipc_stats.active_queues--;
            return 0;
        }
    }
    
    return -1;  // Queue not found
}

// Send a message
int ipc_send_message(uint32_t from_pid, uint32_t to_pid, const uint8_t *data, uint32_t size) {
    if (!data || size == 0 || size > MAX_MESSAGE_SIZE) {
        return -1;  // Invalid message
    }
    
    // Find receiver's queue
    ipc_queue_t *queue = NULL;
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (queue_table[i].is_used && queue_table[i].owner_pid == to_pid) {
            queue = &queue_table[i];
            break;
        }
    }
    
    if (!queue) {
        return -1;  // Receiver queue not found
    }
    
    // Check if queue is full
    if (queue->count >= MAX_MESSAGES_PER_QUEUE) {
        return -1;  // Queue full
    }
    
    // Add message to queue
    ipc_message_t *msg = &queue->messages[queue->tail];
    msg->from_pid = from_pid;
    msg->to_pid = to_pid;
    msg->timestamp = 0;  // TODO: use actual tick count
    msg->size = size;
    
    // Copy data
    for (uint32_t i = 0; i < size; i++) {
        msg->data[i] = data[i];
    }
    
    queue->tail = (queue->tail + 1) % MAX_MESSAGES_PER_QUEUE;
    queue->count++;
    ipc_stats.total_sent++;
    ipc_stats.total_messages++;
    
    return 0;
}

// Receive a message
int ipc_receive_message(uint32_t to_pid, ipc_message_t *msg) {
    if (!msg) {
        return -1;
    }
    
    // Find receiver's queue
    ipc_queue_t *queue = NULL;
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (queue_table[i].is_used && queue_table[i].owner_pid == to_pid) {
            queue = &queue_table[i];
            break;
        }
    }
    
    if (!queue || queue->count == 0) {
        return -1;  // No messages
    }
    
    // Get message from queue
    *msg = queue->messages[queue->head];
    queue->head = (queue->head + 1) % MAX_MESSAGES_PER_QUEUE;
    queue->count--;
    ipc_stats.total_received++;
    
    return 0;
}

// Check if queue exists
int ipc_queue_exists(uint32_t queue_id) {
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (queue_table[i].is_used && queue_table[i].queue_id == queue_id) {
            return 1;
        }
    }
    return 0;
}

// Get IPC statistics
void ipc_get_stats(ipc_stats_t *stats) {
    if (stats) {
        *stats = ipc_stats;
    }
}

// Print IPC statistics
void ipc_print_stats(void) {
    char buffer[64];
    
    console_puts("\n=== IPC Statistics ===\n");
    
    console_puts("Active Queues:        ");
    itoa(ipc_stats.active_queues, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Total Messages:       ");
    itoa(ipc_stats.total_messages, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Sent:                 ");
    itoa(ipc_stats.total_sent, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Received:             ");
    itoa(ipc_stats.total_received, buffer);
    console_puts(buffer);
    console_puts("\n");
}
