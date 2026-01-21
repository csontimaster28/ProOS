#include <stdint.h>
#include "idt.h"
#include "pic.h"
#include "scheduler.h"
#include "keyboard.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint8_t *vga = (uint8_t*)0xB8000;
static volatile int cursor_x = 0;
static volatile int cursor_y = 0;

// Forward declarations
void console_scroll(void);

void console_putchar(char c) {
    if (c == '\n') {
        cursor_y++;
        cursor_x = 0;
        if (cursor_y >= VGA_HEIGHT) {
            cursor_y = VGA_HEIGHT - 1;
            console_scroll();
        }
        return;
    }
    
    if (c == 8) {
        // Backspace - delete previous character
        if (cursor_x > 0) {
            cursor_x--;
            int offset = (cursor_y * VGA_WIDTH + cursor_x) * 2;
            vga[offset] = ' ';
            vga[offset + 1] = 0x0F;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
            int offset = (cursor_y * VGA_WIDTH + cursor_x) * 2;
            vga[offset] = ' ';
            vga[offset + 1] = 0x0F;
        }
        return;
    }
    
    if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) {
            cursor_y++;
            cursor_x = 0;
            if (cursor_y >= VGA_HEIGHT) {
                cursor_y = VGA_HEIGHT - 1;
                console_scroll();
            }
        }
        return;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_y++;
        cursor_x = 0;
        if (cursor_y >= VGA_HEIGHT) {
            cursor_y = VGA_HEIGHT - 1;
            console_scroll();
        }
    }
    
    int offset = (cursor_y * VGA_WIDTH + cursor_x) * 2;
    vga[offset] = c;
    vga[offset + 1] = 0x0F;
    cursor_x++;
}

void console_scroll(void) {
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        vga[i * 2] = vga[(i + VGA_WIDTH) * 2];
        vga[i * 2 + 1] = vga[(i + VGA_WIDTH) * 2 + 1];
    }
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = 0x0F;
    }
}

void console_puts(const char *s) {
    for (int i = 0; s[i]; i++) {
        console_putchar(s[i]);
    }
}

void console_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i * 2] = ' ';
        vga[i * 2 + 1] = 0x0F;
    }
    cursor_x = 0;
    cursor_y = 0;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i])
            return a[i] - b[i];
    }
    return 0;
}

int atoi(const char *str) {
    int result = 0;
    int negative = 0;
    
    if (*str == '-') {
        negative = 1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return negative ? -result : result;
}

void itoa(int num, char *str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int negative = 0;
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    int i = 0;
    while (num > 0) {
        str[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    if (negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    // Reverse string
    for (int j = 0; j < i / 2; j++) {
        char tmp = str[j];
        str[j] = str[i - 1 - j];
        str[i - 1 - j] = tmp;
    }
}

int evaluate_math(const char *expr, int *result) {
    // Parse simple math expressions like "2+3" or "10*5" or "20/4"
    int left = 0;
    char op = '+';
    int i = 0;
    
    // Parse first number
    while (expr[i] >= '0' && expr[i] <= '9') {
        left = left * 10 + (expr[i] - '0');
        i++;
    }
    
    // Process operations
    while (expr[i]) {
        if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/') {
            op = expr[i];
            i++;
        } else if (expr[i] >= '0' && expr[i] <= '9') {
            int right = 0;
            while (expr[i] >= '0' && expr[i] <= '9') {
                right = right * 10 + (expr[i] - '0');
                i++;
            }
            
            if (op == '+') {
                left = left + right;
            } else if (op == '-') {
                left = left - right;
            } else if (op == '*') {
                left = left * right;
            } else if (op == '/') {
                if (right == 0) {
                    return 0;  // Division by zero error
                }
                left = left / right;
            }
        } else {
            i++;
        }
    }
    
    *result = left;
    return 1;
}

void process_command(const char *input) {
    // Check for /math command
    if (strncmp(input, "/math ", 6) == 0) {
        const char *expr = &input[6];
        
        // Skip '=' if present
        if (*expr == '=') {
            expr++;
        }
        
        int result;
        if (evaluate_math(expr, &result)) {
            console_puts("Result: ");
            char result_str[32];
            itoa(result, result_str);
            console_puts(result_str);
            console_puts("\n");
        } else {
            console_puts("Error: Division by zero or invalid expression\n");
        }
        return;
    }
    
    // Check for /pr command
    if (strncmp(input, "/pr ", 4) == 0) {
        console_puts("Echo: ");
        console_puts(&input[4]);
        console_puts("\n");
        return;
    }
    
    if (strcmp(input, "/pr") == 0) {
        console_puts("Echo: \n");
        return;
    }
    
    if (strcmp(input, "help") == 0) {
        console_puts("Commands:\n");
        console_puts("  /pr <text>     - Echo text\n");
        console_puts("  /math <expr>   - Calculate math (e.g., /math =2+3)\n");
        console_puts("  help           - Show this help\n");
        return;
    }
    
    console_puts("Unknown command: ");
    console_puts(input);
    console_puts("\n");
}

void kernel_main(void) {
    // Initialize console
    console_clear();
    
    console_puts("=== MyOS Boot ===\n");
    console_puts("Initializing PIC...\n");
    pic_remap();
    
    console_puts("Initializing IDT...\n");
    idt_init();
    
    console_puts("Initializing scheduler...\n");
    scheduler_init();
    
    console_puts("Initializing PIT...\n");
    pit_init();
    
    console_puts("Initializing keyboard...\n");
    keyboard_init();
    
    // Set keyboard callback for real-time display
    keyboard_set_display_callback(console_putchar);
    
    console_puts("\nReady! Type /pr <text> to echo.\n");
    console_puts("> ");
    
    asm volatile("sti");

    while (1) {
        char *line = keyboard_get_line();
        if (line) {
            console_puts("\n");
            process_command(line);
            console_puts("> ");
        }
    }
}
