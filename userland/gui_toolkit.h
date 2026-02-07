// Minimal GUI toolkit API for ProOS userland apps
#ifndef GUI_TOOLKIT_H
#define GUI_TOOLKIT_H

#include <stdint.h>

typedef struct {
    uint32_t width, height;
    uint8_t *pixels; // pointer into back buffer via gfxd mapping
    uint32_t surface_id;
} window_t;

window_t* window_create(uint32_t w, uint32_t h);
void window_destroy(window_t *w);
void window_draw_text(window_t *w, uint32_t x, uint32_t y, const char *s, uint32_t color);
void window_draw_rect(window_t *w, uint32_t x, uint32_t y, uint32_t wlen, uint32_t hlen, uint32_t color);

// Simple button widget
typedef struct {
    uint32_t x,y,w,h;
    char label[32];
} button_t;

button_t* button_create(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const char *label);
void button_draw(button_t *b, window_t *w);

#endif // GUI_TOOLKIT_H
