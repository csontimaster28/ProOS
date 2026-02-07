#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include "ipc.h"

// Kernel event types
typedef enum {
    EVENT_PROC_START = 1,
    EVENT_PROC_EXIT = 2,
    EVENT_FS_ERROR = 3,
    EVENT_OOM = 4,
    EVENT_HIGH_CPU = 5
} event_type_t;
// Additional events for modules and networking
#define EVENT_MODULE_LOAD  0x10
#define EVENT_MODULE_CRASH 0x11
#define EVENT_NET_UP       0x20
#define EVENT_NET_DOWN     0x21
#define EVENT_NET_ERROR    0x22
#define EVENT_NET_PACKET_DROP 0x23

// Max subscribers
#define MAX_EVENT_SUBSCRIBERS 8

// Initialize event subsystem
void event_init(void);

// Subscribe a process to receive events (returns 0 on failure, queue id on success)
uint32_t event_subscribe(uint32_t pid);
int event_unsubscribe(uint32_t pid);

// Emit an event; `payload` may be NULL and `payload_size` may be 0
void event_emit(event_type_t type, const uint8_t *payload, uint32_t payload_size);

// Helper to post event as text
void event_emit_text(event_type_t type, const char *text);

#endif // EVENT_H
