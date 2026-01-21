#include <stdint.h>
#include "keyboard.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static char keymap[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// Space bar is scancode 0x39
static char keymap_with_space[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static volatile char input_buffer[INPUT_BUFFER_SIZE];
static volatile int buffer_pos = 0;
static volatile int input_ready = 0;
static void (*display_callback)(char) = 0;

void keyboard_init(void) {
    buffer_pos = 0;
    input_ready = 0;
}

void keyboard_set_display_callback(void (*callback)(char)) {
    display_callback = callback;
}

char* keyboard_get_line(void) {
    if (input_ready) {
        input_ready = 0;
        return (char*)input_buffer;
    }
    return 0;
}

void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    
    // Skip key releases
    if (scancode & 0x80) return;

    // Skip unmapped keys
    if (scancode >= 128) return;
    
    char c = keymap[scancode];
    
    // Handle space bar (scancode 0x39)
    if (scancode == 0x39) {
        c = ' ';
    }
    
    if (!c) return;

    // Handle newline - mark buffer as ready
    if (c == '\n') {
        if (buffer_pos < INPUT_BUFFER_SIZE - 1) {
            input_buffer[buffer_pos] = '\0';
            input_ready = 1;
            buffer_pos = 0;
        }
        // Display newline
        if (display_callback) {
            display_callback('\n');
        }
        return;
    }
    
    // Handle backspace
    if (c == 8) {
        if (buffer_pos > 0) {
            buffer_pos--;
            // Display backspace (delete character)
            if (display_callback) {
                display_callback(8);
            }
        }
        return;
    }
    
    // Add character to buffer
    if (buffer_pos < INPUT_BUFFER_SIZE - 1) {
        input_buffer[buffer_pos++] = c;
        // Display character immediately
        if (display_callback) {
            display_callback(c);
        }
    }
}
