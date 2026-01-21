#pragma once
#include <stdint.h>

#define INPUT_BUFFER_SIZE 256

void keyboard_init(void);
void keyboard_handler(void);
char* keyboard_get_line(void);
void keyboard_set_display_callback(void (*callback)(char));