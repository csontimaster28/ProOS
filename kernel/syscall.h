// Fixed syscall table for ProOS v0.0.2a
// Syscall numbers MUST NOT change once released.
// Userland should include this header and call syscalls using these IDs.

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// System call IDs - stable ABI
// Keep these numbers immutable across releases.
#define SYS_YIELD         0x01
#define SYS_EXIT          0x02
#define SYS_WRITE         0x03
#define SYS_READ          0x04
#define SYS_OPEN          0x05
#define SYS_CLOSE         0x06
#define SYS_GETPID        0x07
#define SYS_GETTIME       0x08
#define SYS_IPC_SEND      0x09
#define SYS_IPC_RECV      0x0A
#define SYS_EVENT_SUB     0x0B
#define SYS_EVENT_RECV    0x0C
#define SYS_FS_STAT       0x0D
#define SYS_DMESG         0x0E

// Framebuffer syscalls (reserved kernel range)
#define SYS_FRAMEBUF_INFO 0x10 // a = pointer to framebuffer_info_t
#define SYS_FRAMEBUF_PTR  0x11 // returns low-32 bits pointer to front/back buffer (b selects 0=front 1=back)
#define SYS_FRAMEBUF_SWAP 0x12 // swap buffers


// Reserved range: 0x10 - 0x7F for future kernel calls

// Error codes returned by syscalls
#define SYS_OK            0
#define SYS_ERR           -1
#define SYS_ERR_INVAL     -2
#define SYS_ERR_NOMEM     -3

#endif // SYSCALL_H
