// Userland-visible framebuffer info for ProOS
#ifndef USER_FRAMEBUFFER_H
#define USER_FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch; // bytes per scanline
    uint32_t bpp;   // bits per pixel
    uint8_t double_buffered;
} framebuffer_info_t;

#endif // USER_FRAMEBUFFER_H
