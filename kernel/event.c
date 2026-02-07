#include "event.h"
#include "logging.h"
#include "ipc.h"
#include "memory.h"

// Simple subscriber list: store pid -> queue_id
static uint32_t subscribers[MAX_EVENT_SUBSCRIBERS];

void event_init(void) {
    for (int i = 0; i < MAX_EVENT_SUBSCRIBERS; i++) {
        subscribers[i] = 0;
    }
}

uint32_t event_subscribe(uint32_t pid) {
    // Create an IPC queue for this pid if not present
    for (int i = 0; i < MAX_EVENT_SUBSCRIBERS; i++) {
        if (subscribers[i] == pid) {
            // Already subscribed
            return ipc_create_queue(pid);
        }
    }

    for (int i = 0; i < MAX_EVENT_SUBSCRIBERS; i++) {
        if (subscribers[i] == 0) {
            subscribers[i] = pid;
            uint32_t q = ipc_create_queue(pid);
            return q;
        }
    }
    return 0;
}

int event_unsubscribe(uint32_t pid) {
    for (int i = 0; i < MAX_EVENT_SUBSCRIBERS; i++) {
        if (subscribers[i] == pid) {
            subscribers[i] = 0;
            // Try to find and destroy queue for pid
            // ipc_destroy_queue requires queue id, we don't store it here; caller can destroy
            return 0;
        }
    }
    return -1;
}

void event_emit(event_type_t type, const uint8_t *payload, uint32_t payload_size) {
    char msg[128];
    switch (type) {
        case EVENT_PROC_START:
            log_info("EVENT: Process start");
            break;
        case EVENT_PROC_EXIT:
            log_info("EVENT: Process exit");
            break;
        case EVENT_FS_ERROR:
            log_error("EVENT: Filesystem error");
            break;
        case EVENT_OOM:
            log_error("EVENT: Out of memory");
            break;
        case EVENT_HIGH_CPU:
            log_warning("EVENT: High CPU usage");
            break;
        case EVENT_MODULE_LOAD:
            log_info("EVENT: Module loaded");
            break;
        case EVENT_MODULE_CRASH:
            log_error("EVENT: Module crash");
            break;
        case EVENT_NET_UP:
            log_info("EVENT: Network up");
            break;
        case EVENT_NET_DOWN:
            log_warning("EVENT: Network down");
            break;
        case EVENT_NET_ERROR:
            log_error("EVENT: Network error");
            break;
        case EVENT_NET_PACKET_DROP:
            log_warning("EVENT: Network packet drop");
            break;
        default:
            log_debug("EVENT: Unknown event");
            break;
    }

    // Deliver event to all subscribers via IPC messages
    for (int i = 0; i < MAX_EVENT_SUBSCRIBERS; i++) {
        uint32_t pid = subscribers[i];
        if (pid == 0) continue;

        // Build a small message: [event_type(1)] + payload (limited)
        uint8_t buffer[64];
        uint32_t written = 0;
        buffer[written++] = (uint8_t)type;
        if (payload && payload_size > 0) {
            uint32_t copy = (payload_size < (sizeof(buffer) - 1)) ? payload_size : (sizeof(buffer) - 1);
            for (uint32_t j = 0; j < copy; j++) buffer[written++] = payload[j];
        }

        // Send via IPC; ignore failure (queue might be full)
        ipc_send_message(0, pid, buffer, written);
    }
}

void event_emit_text(event_type_t type, const char *text) {
    if (!text) {
        event_emit(type, NULL, 0);
        return;
    }
    uint32_t len = 0;
    while (text[len]) len++;
    event_emit(type, (const uint8_t *)text, len);
}
