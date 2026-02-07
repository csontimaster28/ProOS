#include "syscall.h"
#include "process.h"
#include "capability.h"
#include "logging.h"
#include "filesystem.h"
#include "logging.h"
#include "ipc.h"
#include "framebuffer.h"

// Generic syscall dispatcher helper
int handle_syscall(uint32_t id, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t pid = process_get_current_pid();
    switch (id) {
        case SYS_OPEN: {
            const char *name = (const char *)a;
            int mode = (int)b;
            return fs_open(name, mode, pid);
        }
        case SYS_READ: {
            int fd = (int)a;
            uint8_t *buf = (uint8_t *)b;
            uint32_t size = c;
            return fs_read(fd, buf, size);
        }
        case SYS_WRITE: {
            int fd = (int)a;
            const uint8_t *buf = (const uint8_t *)b;
            uint32_t size = c;
            // If fd==1 treat as console output (string)
            if (fd == 1) {
                // assume buf is a null-terminated string
                console_puts((const char *)buf);
                return SYS_OK;
            }
            return fs_write(fd, buf, size);
        }
        case SYS_CLOSE: {
            int fd = (int)a;
            return fs_close(fd);
        }
        case SYS_DMESG: {
            logging_print_all();
            return SYS_OK;
        }
        case SYS_EVENT_SUB: {
            return event_subscribe(pid);
        }
        case SYS_EVENT_RECV: {
            // a = pointer to ipc_message_t buffer
            ipc_message_t *msg = (ipc_message_t *)a;
            if (!msg) return SYS_ERR_INVAL;
            int r = ipc_receive_message(pid, msg);
            return r == 0 ? SYS_OK : SYS_ERR;
        }
        case SYS_FRAMEBUF_INFO: {
            framebuffer_info_t *info = (framebuffer_info_t *)a;
            if (!info) return SYS_ERR_INVAL;
            // validate user pointer
            if (!memory_check_bounds(info, sizeof(framebuffer_info_t) - 1)) return SYS_ERR_INVAL;
            framebuffer_get_info(info);
            return SYS_OK;
        }
        case SYS_FRAMEBUF_PTR: {
            // b == 0 -> front, 1 -> back
            uint32_t which = b;
            void *ptr = (which == 1) ? framebuffer_get_back() : framebuffer_get_front();
            return (int)ptr; // return kernel virtual pointer (assumes identity mapping for MVP)
        }
        case SYS_FRAMEBUF_SWAP: {
            framebuffer_swap();
            return SYS_OK;
        }
        default:
            log_warning("handle_syscall: invalid syscall id");
            return SYS_ERR_INVAL;
    }
}

// Simple syscall stubs for networking API; real implementation will be provided by network module
int sys_socket(int domain, int type, int protocol) {
    uint32_t pid = process_get_current_pid();
    if (!process_has_cap(pid, CAP_NET_SEND) && !process_has_cap(pid, CAP_NET_RECV)) {
        log_warning("sys_socket: missing network capabilities");
        return SYS_ERR_INVAL;
    }
    // Not implemented: return error for now
    return SYS_ERR;
}

int sys_bind(int sockfd, const void *addr, uint32_t addrlen) {
    uint32_t pid = process_get_current_pid();
    if (!process_has_cap(pid, CAP_NET_RECV)) return SYS_ERR_INVAL;
    return SYS_ERR;
}

int sys_connect(int sockfd, const void *addr, uint32_t addrlen) {
    uint32_t pid = process_get_current_pid();
    if (!process_has_cap(pid, CAP_NET_SEND)) return SYS_ERR_INVAL;
    return SYS_ERR;
}

int sys_send(int sockfd, const uint8_t *buf, uint32_t len) {
    uint32_t pid = process_get_current_pid();
    if (!process_has_cap(pid, CAP_NET_SEND)) return SYS_ERR_INVAL;
    return SYS_ERR;
}

int sys_recv(int sockfd, uint8_t *buf, uint32_t len) {
    uint32_t pid = process_get_current_pid();
    if (!process_has_cap(pid, CAP_NET_RECV)) return SYS_ERR_INVAL;
    return SYS_ERR;
}

int sys_close(int sockfd) {
    uint32_t pid = process_get_current_pid();
    (void)pid;
    return SYS_ERR;
}
