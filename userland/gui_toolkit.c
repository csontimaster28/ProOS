// Minimal GUI toolkit implementations for userland apps
#include "gui_toolkit.h"
#include "framebuffer.h"
#include "../kernel/syscall.h"
#include <stdint.h>
#include <stddef.h>
// no libc available; implement minimal strncpy-like copy below

extern int syscall3(int id, uint32_t a, uint32_t b, uint32_t c);

// Simple static pools
static window_t windows[8];
static int window_used[8];

static button_t buttons[16];
static int button_used[16];

static framebuffer_info_t fb_info_cache;

static void refresh_fb_info(void) {
    framebuffer_info_t info;
    syscall3(SYS_FRAMEBUF_INFO, (uint32_t)&info, 0, 0);
    fb_info_cache = info;
}

static uint8_t *get_back_buffer(void) {
    return (uint8_t *) (uintptr_t) syscall3(SYS_FRAMEBUF_PTR, 1, 0, 0);
}

window_t* window_create(uint32_t w, uint32_t h) {
    for (int i = 0; i < 8; i++) {
        if (!window_used[i]) {
            window_used[i] = 1;
            windows[i].width = w;
            windows[i].height = h;
            windows[i].surface_id = i;
            windows[i].pixels = NULL;
            return &windows[i];
        }
    }
    return NULL;
}

void window_destroy(window_t *w) {
    if (!w) return;
    int idx = w->surface_id;
    if (idx >= 0 && idx < 8) window_used[idx] = 0;
}

// Draw a filled rect into the back buffer, window is centered on screen
void window_draw_rect(window_t *w, uint32_t x, uint32_t y, uint32_t wlen, uint32_t hlen, uint32_t color) {
    if (!w) return;
    refresh_fb_info();
    uint8_t *back = get_back_buffer();
    if (!back) return;
    uint32_t fbw = fb_info_cache.width;
    uint32_t fbh = fb_info_cache.height;
    uint32_t pitch = fb_info_cache.pitch;
    int origin_x = (int)((fbw > w->width) ? (fbw - w->width) / 2 : 0);
    int origin_y = (int)((fbh > w->height) ? (fbh - w->height) / 2 : 0);
    for (uint32_t yy = 0; yy < hlen && (origin_y + y + yy) < fbh; yy++) {
        for (uint32_t xx = 0; xx < wlen && (origin_x + x + xx) < fbw; xx++) {
            uint32_t off = (origin_y + y + yy) * pitch + (origin_x + x + xx) * (fb_info_cache.bpp/8);
            back[off + 0] = (color >> 16) & 0xFF;
            back[off + 1] = (color >> 8) & 0xFF;
            back[off + 2] = (color >> 0) & 0xFF;
            back[off + 3] = (color >> 24) & 0xFF;
        }
    }
}

// Very small text emulation: draw a small 1x1 pixel per char (placeholder)
void window_draw_text(window_t *w, uint32_t x, uint32_t y, const char *s, uint32_t color) {
    if (!w || !s) return;
    // Draw a thin horizontal bar per character
    uint32_t i = 0;
    while (s[i]) {
        window_draw_rect(w, x + i*8, y, 6, 10, color);
        i++;
    }
}

button_t* button_create(uint32_t x, uint32_t y, uint32_t wlen, uint32_t hlen, const char *label) {
    for (int i = 0; i < 16; i++) {
        if (!button_used[i]) {
            button_used[i] = 1;
            buttons[i].x = x; buttons[i].y = y; buttons[i].w = wlen; buttons[i].h = hlen;
            // manual copy to avoid libc dependency
            const char *src = label ? label : "";
            int j = 0;
            for (; j < (int)sizeof(buttons[i].label)-1 && src[j]; j++) buttons[i].label[j] = src[j];
            buttons[i].label[j] = '\0';
            return &buttons[i];
        }
    }
    return NULL;
}

void button_draw(button_t *b, window_t *w) {
    if (!b || !w) return;
    // Button background
    window_draw_rect(w, b->x, b->y, b->w, b->h, 0xFFAAAAAA);
    // Small label bar inside
    uint32_t lx = b->x + 4;
    uint32_t ly = b->y + (b->h/4);
    uint32_t lw = b->w - 8;
    uint32_t lh = b->h / 2;
    window_draw_rect(w, lx, ly, lw, lh, 0xFF404040);
}
