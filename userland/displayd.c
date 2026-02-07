// displayd: manages surfaces and routes input to focused surface via IPC

#include "../kernel/syscall.h"
#include "gui_ipc.h"
#include <stdint.h>

extern int syscall3(int id, uint32_t a, uint32_t b, uint32_t c);

// Very small surface representation
typedef struct {
    uint32_t id;
    uint32_t x,y,w,h;
    uint32_t owner_pid;
    uint8_t *pixels; // not used directly — gfxd owns framebuffer
} surface_t;

static surface_t surfaces[16];
static uint32_t surface_count = 0;
static uint32_t focused_surface = 0;

int main(void) {
    // subscribe to input events via kernel event bus (not shown)
    // For MVP, displayd accepts create surface requests via IPC and maintains z-order.
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}
