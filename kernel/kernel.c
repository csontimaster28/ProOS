#include <stdint.h>
#include "idt.h"
#include "pic.h"
#include "scheduler.h"
#include "keyboard.h"
#include "memory.h"
#include "process.h"
#include "filesystem.h"
#include "ipc.h"
#include "logging.h"

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
    char buffer[64];
    
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
            itoa(result, buffer);
            console_puts(buffer);
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
    
    if (strcmp(input, "/memstat") == 0) {
        memory_print_stats();
        return;
    }
    
    if (strcmp(input, "/procstat") == 0) {
        process_print_stats();
        return;
    }
    
    if (strcmp(input, "/proclist") == 0) {
        process_print_processes();
        return;
    }
    
    if (strncmp(input, "/procinfo ", 10) == 0) {
        const char *pid_str = &input[10];
        int pid = atoi(pid_str);
        
        process_t *proc = process_get_by_id(pid);
        if (proc) {
            console_puts("Process ID: ");
            itoa(proc->pid, buffer);
            console_puts(buffer);
            console_puts("\nMemory: ");
            itoa(proc->memory_size / 1024, buffer);
            console_puts(buffer);
            console_puts("KB\nThreads: ");
            itoa(proc->thread_count, buffer);
            console_puts(buffer);
            console_puts("\n");
        } else {
            console_puts("Process not found\n");
        }
        return;
    }
    
    if (strcmp(input, "/fsstat") == 0) {
        filesystem_print_stats();
        return;
    }
    
    if (strcmp(input, "/ls") == 0) {
        fs_list_files();
        return;
    }
    
    if (strncmp(input, "/cat ", 5) == 0) {
        const char *filename = &input[5];
        
        int32_t fd = fs_open(filename, FILE_MODE_READ, 0);
        if (fd < 0) {
            console_puts("Error: File not found\n");
            return;
        }
        
        uint8_t read_buffer[1024];
        int bytes_read = fs_read(fd, read_buffer, sizeof(read_buffer) - 1);
        
        if (bytes_read > 0) {
            read_buffer[bytes_read] = '\0';
            console_puts((const char *)read_buffer);
            console_puts("\n");
        } else {
            console_puts("Error: Could not read file\n");
        }
        
        fs_close(fd);
        return;
    }
    
    if (strncmp(input, "/write ", 7) == 0) {
        // Parse: /write <filename> <text>
        const char *cmd = &input[7];
        char filename[64];
        int i = 0;
        
        // Extract filename
        while (cmd[i] != ' ' && cmd[i] != '\0' && i < 63) {
            filename[i] = cmd[i];
            i++;
        }
        filename[i] = '\0';
        
        // Skip space
        if (cmd[i] == ' ') {
            i++;
        }
        
        const char *text = &cmd[i];
        
        int32_t fd = fs_open(filename, FILE_MODE_WRITE, 0);
        if (fd < 0) {
            console_puts("Error: Could not create file\n");
            return;
        }
        
        int bytes = 0;
        while (text[bytes] != '\0') {
            bytes++;
        }
        fs_write(fd, (const uint8_t *)text, bytes);
        
        fs_close(fd);
        console_puts("File written successfully\n");
        return;
    }
    
    if (strncmp(input, "/rm ", 4) == 0) {
        const char *filename = &input[4];
        
        if (fs_delete(filename) == 0) {
            console_puts("File deleted successfully\n");
        } else {
            console_puts("Error: File not found\n");
        }
        return;
    }
    
    if (strcmp(input, "/proc") == 0) {
        console_puts("\n=== /proc - Process Information ===\n");
        process_print_processes();
        return;
    }
    
    if (strcmp(input, "top") == 0) {
        process_stats_t stats = {0};
        process_get_stats(&stats);
        
        console_puts("\n=== System Processes (top) ===\n");
        console_puts("Processes: ");
        itoa(stats.total_processes, buffer);
        console_puts(buffer);
        console_puts(" | Running: ");
        itoa(stats.running_processes, buffer);
        console_puts(buffer);
        console_puts(" | Ready: ");
        itoa(stats.ready_processes, buffer);
        console_puts(buffer);
        console_puts("\n");
        
        console_puts("Threads: ");
        itoa(stats.total_threads, buffer);
        console_puts(buffer);
        console_puts(" | Running: ");
        itoa(stats.running_threads, buffer);
        console_puts(buffer);
        console_puts(" | Ready: ");
        itoa(stats.ready_threads, buffer);
        console_puts(buffer);
        console_puts("\n");
        
        process_print_processes();
        return;
    }
    
    if (strcmp(input, "dmesg") == 0) {
        logging_print_all();
        return;
    }
    
    if (strncmp(input, "dmesg ", 6) == 0) {
        const char *count_str = &input[6];
        int count = atoi(count_str);
        if (count > 0) {
            logging_print_recent(count);
        } else {
            console_puts("Invalid count\n");
        }
        return;
    }
    
    if (strcmp(input, "help") == 0) {
        console_puts("Available Commands:\n");
        console_puts("  /pr <text>        - Echo text\n");
        console_puts("  /math <expr>      - Calculate math (e.g., /math =2+3)\n");
        console_puts("  /memstat          - Show memory statistics\n");
        console_puts("  /procstat         - Show process/thread statistics\n");
        console_puts("  /proclist         - List all processes and threads\n");
        console_puts("  /procinfo <pid>   - Show process info\n");
        console_puts("  /fsstat           - Show filesystem statistics\n");
        console_puts("  /ls               - List files\n");
        console_puts("  /cat <filename>   - Read file contents\n");
        console_puts("  /write <file> <text> - Write to file\n");
        console_puts("  /rm <filename>    - Delete file\n");
        console_puts("  /proc             - View /proc filesystem\n");
        console_puts("  top               - Show running processes\n");
        console_puts("  dmesg             - Show all kernel logs\n");
        console_puts("  dmesg <count>     - Show last N entries\n");
        console_puts("  help              - Show this help\n");
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
    console_puts("Initializing memory...\n");
    memory_init();
    
    console_puts("Initializing logging...\n");
    logging_init();
    log_info("Kernel initialization started");
    
    console_puts("Initializing filesystem...\n");
    filesystem_init();
    log_info("Filesystem initialized");
    
    console_puts("Initializing IPC...\n");
    ipc_init();
    log_info("IPC system initialized");
    
    console_puts("Initializing process manager...\n");
    process_manager_init();
    log_info("Process manager initialized");
    
    console_puts("Initializing PIC...\n");
    pic_remap();
    log_info("PIC remapped");
    
    console_puts("Initializing IDT...\n");
    idt_init();
    log_info("IDT initialized");
    
    console_puts("Initializing scheduler...\n");
    scheduler_init();
    log_info("Scheduler initialized");
    
    console_puts("Initializing PIT...\n");
    pit_init();
    log_info("PIT initialized");
    
    console_puts("Initializing keyboard...\n");
    keyboard_init();
    log_info("Keyboard initialized");
    
    // Set keyboard callback for real-time display
    keyboard_set_display_callback(console_putchar);
    
    console_puts("\nReady! Type 'help' for commands.\n");
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
