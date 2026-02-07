// Capability-based security for ProOS v0.0.2a
// Capabilities are simple bitflags attached to processes at spawn.

#ifndef CAPABILITY_H
#define CAPABILITY_H

#include <stdint.h>

// Filesystem capabilities
#define CAP_FS_READ      (1u << 0)
#define CAP_FS_WRITE     (1u << 1)

// Process management
#define CAP_PROC_CONTROL (1u << 2)

// Networking
#define CAP_NET_SEND     (1u << 3)
#define CAP_NET_RECV     (1u << 4)

// Modules and introspection
#define CAP_MODULE_LOAD  (1u << 5)
#define CAP_INTROSPECT   (1u << 6)

// Helper: any capability
#define CAP_ANY          0xFFFFFFFFu

// Check capability in a mask
static inline int cap_has(uint32_t mask, uint32_t cap) {
    return (mask & cap) ? 1 : 0;
}

// Return human-readable name for debugging (not required to be exhaustive)
const char* cap_name(uint32_t cap);

#endif // CAPABILITY_H
