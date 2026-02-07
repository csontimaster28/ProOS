// Minimal kernel framebuffer implementation (RAM-backed)
// Exposes a contiguous pixel buffer with optional double buffering.

#include "framebuffer.h"
#include "memory.h"
#include "logging.h"
#include <string.h>
#include <stdlib.h>

static framebuffer_info_t fb_info;
static uint8_t *fb_front = NULL;
static uint8_t *fb_back = NULL;

void framebuffer_init(uint32_t width, uint32_t height, uint32_t bpp, uint8_t double_buf) {
    fb_info.width = width;
    fb_info.height = height;
    fb_info.bpp = bpp;
    fb_info.pitch = (width * (bpp / 8));
    fb_info.double_buffered = double_buf ? 1 : 0;

    uint32_t bytes = fb_info.pitch * height;
    fb_front = (uint8_t *)malloc(bytes);
    if (!fb_front) {
        log_error("framebuffer_init: out of memory for front buffer");
        return;
    }
    memset(fb_front, 0, bytes);

    if (fb_info.double_buffered) {
        fb_back = (uint8_t *)malloc(bytes);
        if (!fb_back) {
            log_error("framebuffer_init: out of memory for back buffer");
            // fallback to single buffer
            fb_info.double_buffered = 0;
            fb_back = NULL;
        } else {
            memset(fb_back, 0, bytes);
        }
    }

    log_info("framebuffer_init: initialized");
}

void framebuffer_get_info(framebuffer_info_t *info) {
    if (!info) return;
    memcpy(info, &fb_info, sizeof(framebuffer_info_t));
}

void *framebuffer_get_front(void) { return fb_front; }
void *framebuffer_get_back(void) { return fb_info.double_buffered ? (void*)fb_back : (void*)fb_front; }

void framebuffer_swap(void) {
    if (!fb_front) return;
    uint32_t bytes = fb_info.pitch * fb_info.height;
    if (fb_info.double_buffered && fb_back) {
        // swap pointers
        uint8_t *t = fb_front; fb_front = fb_back; fb_back = t;
    } else {
        // copy back into front
        if (fb_back) memcpy(fb_front, fb_back, bytes);
    }

    // Simple fallback: also blit a low-res approximation into VGA text mode
    // so the user sees a graphical-like desktop in the terminal if no real
    // graphical mode is present. This samples the framebuffer and writes
    // block characters into the VGA text buffer at 0xB8000.
    volatile uint8_t *vga = (volatile uint8_t *)0xB8000;
    const int VGA_W = 80;
    const int VGA_H = 25;
    if (fb_info.width == 0 || fb_info.height == 0) return;
    int cell_w = fb_info.width / VGA_W;
    int cell_h = fb_info.height / VGA_H;
    if (cell_w < 1) cell_w = 1;
    if (cell_h < 1) cell_h = 1;
    for (int ty = 0; ty < VGA_H; ty++) {
        for (int tx = 0; tx < VGA_W; tx++) {
            int px = tx * cell_w + cell_w/2;
            int py = ty * cell_h + cell_h/2;
            if (px >= (int)fb_info.width) px = fb_info.width - 1;
            if (py >= (int)fb_info.height) py = fb_info.height - 1;
            uint32_t off = py * fb_info.pitch + px * (fb_info.bpp/8);
            uint8_t r = fb_front[off+0];
            uint8_t g = fb_front[off+1];
            uint8_t b = fb_front[off+2];
            // luminance
            uint8_t lum = (uint8_t)((uint32_t)r * 77 / 256 + (uint32_t)g * 151 / 256 + (uint32_t)b * 28 / 256);
            uint8_t attr = 0x07; // default
            if (lum > 200) attr = 0x0F; else if (lum > 150) attr = 0x0E; else if (lum > 100) attr = 0x0C; else attr = 0x08;
            size_t vpos = (ty * VGA_W + tx) * 2;
            vga[vpos] = 0xDB; // solid block
            vga[vpos+1] = attr;
        }
    }
}

void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_back && !fb_front) return;
    if (x >= fb_info.width || y >= fb_info.height) return;
    uint8_t *buf = fb_info.double_buffered && fb_back ? fb_back : fb_front;
    uint32_t offset = y * fb_info.pitch + x * (fb_info.bpp / 8);
    // assume 32bpp ARGB
    buf[offset + 0] = (color >> 16) & 0xFF; // R
    buf[offset + 1] = (color >> 8) & 0xFF;  // G
    buf[offset + 2] = (color >> 0) & 0xFF;  // B
    buf[offset + 3] = (color >> 24) & 0xFF; // A (unused)
}
