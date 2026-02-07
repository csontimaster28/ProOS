// Simple framebuffer interface for ProOS (kernel-side)
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch; // bytes per scanline
    uint32_t bpp;   // bits per pixel
    uint8_t double_buffered; // 1 if double buffered
} framebuffer_info_t;

// Initialize the framebuffer (called at boot)
void framebuffer_init(uint32_t width, uint32_t height, uint32_t bpp, uint8_t double_buf);

// Get framebuffer info
void framebuffer_get_info(framebuffer_info_t *info);

// Get pointer to front/back buffer (kernel virtual address)
void *framebuffer_get_front(void);
void *framebuffer_get_back(void);

// Swap back -> front. If double-buffered, this swaps pointers; otherwise copy.
void framebuffer_swap(void);

// Draw pixel into back buffer (x,y) with 32-bit ARGB color
void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color);

#endif // FRAMEBUFFER_H
