#include "capability.h"

const char* cap_name(uint32_t cap) {
    switch (cap) {
        case CAP_FS_READ: return "CAP_FS_READ";
        case CAP_FS_WRITE: return "CAP_FS_WRITE";
        case CAP_PROC_CONTROL: return "CAP_PROC_CONTROL";
        case CAP_NET_SEND: return "CAP_NET_SEND";
        case CAP_NET_RECV: return "CAP_NET_RECV";
        case CAP_MODULE_LOAD: return "CAP_MODULE_LOAD";
        case CAP_INTROSPECT: return "CAP_INTROSPECT";
        default: return "CAP_UNKNOWN";
    }
}
