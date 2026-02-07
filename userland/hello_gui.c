// hello_gui: simple GUI app that opens a window, shows text and a button

#include "gui_toolkit.h"
#include <stdint.h>
#include <string.h>

int main(void) {
    window_t *w = window_create(320, 200);
    if (!w) return -1;
    window_draw_text(w, 10, 10, "Hello from ProOS GUI", 0xFFFFFFFF);
    button_t *btn = button_create(10, 40, 120, 28, "Change Text");
    button_draw(btn, w);
    // Wait for input loop (omitted)
    while (1) { asm volatile ("hlt"); }
    return 0;
}
