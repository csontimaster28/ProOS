// gfxd: Graphics daemon — userland process that interacts with framebuffer syscalls
// Provides simple drawing primitives implemented in userland using direct framebuffer access.

#include "../kernel/syscall.h"
#include "framebuffer.h"
#include "gui_ipc.h"
#include <stdint.h>

// Minimal helpers for syscalls (platform-specific syscall mechanism omitted — assume library call)
extern int syscall3(int id, uint32_t a, uint32_t b, uint32_t c);

typedef struct {
    uint32_t width, height, pitch, bpp;
    uint8_t *fb_front;
    uint8_t *fb_back;
} gfxd_state_t;

static gfxd_state_t state;

static void gfxd_get_info(void) {
    framebuffer_info_t info;
    syscall3(SYS_FRAMEBUF_INFO, (uint32_t)&info, 0, 0);
    state.width = info.width; state.height = info.height; state.pitch = info.pitch; state.bpp = info.bpp;
    state.fb_front = (uint8_t *)syscall3(SYS_FRAMEBUF_PTR, 0, 0, 0);
    state.fb_back = (uint8_t *)syscall3(SYS_FRAMEBUF_PTR, 1, 0, 0);
}

static inline void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    // write into back buffer (ARGB 32)
    uint8_t *buf = state.fb_back;
    if (!buf) return;
    uint32_t offset = y * state.pitch + x * (state.bpp/8);
    buf[offset+0] = (color>>16)&0xFF;
    buf[offset+1] = (color>>8)&0xFF;
    buf[offset+2] = (color>>0)&0xFF;
    buf[offset+3] = (color>>24)&0xFF;
}

static void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t iy = y; iy < y+h && iy < state.height; iy++) {
        for (uint32_t ix = x; ix < x+w && ix < state.width; ix++) {
            put_pixel(ix, iy, color);
        }
    }
}

int main(void) {
    gfxd_get_info();
    // clear back buffer
    for (uint32_t y = 0; y < state.height; y++) {
        for (uint32_t x = 0; x < state.width; x++) put_pixel(x,y,0xFF000000);
    }
    // simple test pattern
    draw_rect(10,10,200,80,0xFFFF0000);

    // request buffer swap
    syscall3(SYS_FRAMEBUF_SWAP,0,0,0);

    // main loop: listen to IPC (omitted) and respond
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}
