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
#include "module.h"
#include "capability.h"
#include "event.h"
extern void initd_main(void);
// Toggle for framebuffer console mode
static int fb_console_enabled = 1;
void keyboard_toggle_console(void) { fb_console_enabled = !fb_console_enabled; }

// Draw console into framebuffer rectangle when enabled
static void fb_console_putc(char c) {
    // Simple implementation: write into bottom-left corner pixels using framebuffer_put_pixel
    // For performance and simplicity we just ignore detailed cursor handling
    (void)c;
}

// Simple port I/O helpers for serial output
static inline uint8_t io_inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void serial_init(void) {
    // Initialize COM1 (0x3F8)
    io_outb(0x3F8+1, 0x00);    // Disable all interrupts
    io_outb(0x3F8+3, 0x80);    // Enable DLAB (set baud rate divisor)
    io_outb(0x3F8+0, 0x03);    // Divisor low byte (38400 baud)
    io_outb(0x3F8+1, 0x00);    // Divisor high byte
    io_outb(0x3F8+3, 0x03);    // 8 bits, no parity, one stop bit
    io_outb(0x3F8+2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    io_outb(0x3F8+4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static void serial_putc(char c) {
    // Wait for Transmitter Holding Register empty
    while ((io_inb(0x3F8+5) & 0x20) == 0) { asm volatile ("nop"); }
    io_outb(0x3F8, (uint8_t)c);
}

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint8_t *vga = (uint8_t*)0xB8000;
static volatile int cursor_x = 0;
static volatile int cursor_y = 0;
// Scrollback buffer to preserve lines beyond screen height
#define MAX_SCROLLBACK_LINES 512
static char scrollback[MAX_SCROLLBACK_LINES][VGA_WIDTH+1];
static uint32_t scrollback_index = 0; // next slot to write

// Forward declarations
void console_scroll(void);

void console_putchar(char c) {
    // Mirror console output to serial for logging
    serial_putc(c);
    if (fb_console_enabled) return;
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
    #include "profs.h"
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
    // Save the top-most line to scrollback before shifting up
    char top_line[VGA_WIDTH + 1];
    for (int c = 0; c < VGA_WIDTH; c++) {
        top_line[c] = vga[c * 2];
    }
    top_line[VGA_WIDTH] = '\0';
    // Store into ring buffer
    for (int i = 0; i < VGA_WIDTH; i++) scrollback[scrollback_index][i] = top_line[i];
    scrollback[scrollback_index][VGA_WIDTH] = '\0';
    scrollback_index = (scrollback_index + 1) % MAX_SCROLLBACK_LINES;

    // Shift screen up by one line
    for (int row = 0; row < (VGA_HEIGHT - 1); row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            int dst = (row * VGA_WIDTH + col) * 2;
            int src = ((row + 1) * VGA_WIDTH + col) * 2;
            vga[dst] = vga[src];
            vga[dst + 1] = vga[src + 1];
        }
    }
    // Clear last line
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

// Parse a signed floating point number from a string
// Returns pointer after number, and sets out value
static const char* parse_double(const char *s, double *out) {
    int neg = 0;
    if (*s == '+') s++; else if (*s == '-') { neg = 1; s++; }

    double val = 0.0;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        double place = 0.1;
        while (*s >= '0' && *s <= '9') { val += (*s - '0') * place; place *= 0.1; s++; }
    }
    *out = neg ? -val : val;
    return s;
}

// Evaluate simple expression: <num><op><num> with optional whitespace
int evaluate_math_double(const char *expr, double *result) {
    // Skip leading spaces
    while (*expr == ' ') expr++;
    double left = 0.0;
    expr = parse_double(expr, &left);
    // Skip spaces
    while (*expr == ' ') expr++;
    char op = *expr;
    if (op != '+' && op != '-' && op != '*' && op != '/') return 0;
    expr++;
    while (*expr == ' ') expr++;
    double right = 0.0;
    expr = parse_double(expr, &right);

    if (op == '+') *result = left + right;
    else if (op == '-') *result = left - right;
    else if (op == '*') *result = left * right;
    else if (op == '/') {
        if (right == 0.0) return 0;
        *result = left / right;
    }
    return 1;
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
    
    // Check for math command
    if (strncmp(input, "math ", 5) == 0) {
        const char *expr = &input[5];
        if (*expr == '=') expr++;

        double dres;
        if (evaluate_math_double(expr, &dres)) {
            console_puts("Result: ");
            // Convert double to string (simple)
            int whole = (int)dres;
            double frac = dres - (double)whole;
            itoa(whole, buffer);
            console_puts(buffer);
            if (frac != 0.0) {
                // print fractional part up to 4 decimals
                if (frac < 0) frac = -frac;
                int frac4 = (int)(frac * 10000.0 + 0.5);
                console_puts(".");
                char fb[8];
                itoa(frac4, fb);
                console_puts(fb);
            }
            console_puts("\n");
        } else {
            console_puts("Error: Division by zero or invalid expression\n");
        }
        return;
    }

    // echo command (replaces pr)
    if (strncmp(input, "echo ", 5) == 0) {
        console_puts("Echo: ");
        console_puts(&input[5]);
        console_puts("\n");
        return;
    }
    if (strcmp(input, "echo") == 0) {
        console_puts("Echo: \n");
        return;
    }
    
    if (strcmp(input, "memstat") == 0) {
        memory_print_stats();
        return;
    }
    
    if (strcmp(input, "procstat") == 0) {
        process_print_stats();
        return;
    }
    
    if (strcmp(input, "proclist") == 0) {
        process_print_processes();
        return;
    }
    
    if (strncmp(input, "procinfo ", 9) == 0) {
        const char *pid_str = &input[9];
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
    
    if (strcmp(input, "fsstat") == 0) {
        filesystem_print_stats();
        return;
    }
    
    if (strcmp(input, "ls") == 0) {
        fs_list_files();
        return;
    }
    
    if (strncmp(input, "cat ", 4) == 0) {
        const char *filename = &input[4];
        int32_t fd = fs_open(filename, FILE_MODE_READ, 0);
        if (fd < 0) {
            console_puts("Error: File not found or is a directory\n");
            return;
        }

        uint8_t read_buffer[256];
        while (1) {
            int bytes_read = fs_read(fd, read_buffer, sizeof(read_buffer) - 1);
            if (bytes_read < 0) {
                console_puts("Error: Could not read file\n");
                break;
            }
            if (bytes_read == 0) break;
            read_buffer[bytes_read] = '\0';
            console_puts((const char *)read_buffer);
        }
        console_puts("\n");
        fs_close(fd);
        return;
    }
    
    if (strncmp(input, "write ", 6) == 0) {
        // Parse: /write <filename> <text>
        const char *cmd = &input[6];
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

    if (strncmp(input, "cfold ", 6) == 0) {
        const char *name = &input[6];
        if (fs_create_folder(name) == 0) {
            console_puts("Folder created successfully\n");
        } else {
            console_puts("Error: Could not create folder\n");
        }
        return;
    }

    if (strncmp(input, "cd ", 3) == 0) {
        const char *name = &input[3];
        if (fs_change_dir(name) == 0) {
            console_puts("Directory changed\n");
        } else {
            console_puts("Error: Not a directory or not found\n");
        }
        return;
    }

    if (strncmp(input, "rename ", 7) == 0) {
        // rename old new
        const char *cmd = &input[7];
        char oldn[64]; char newn[64]; int i = 0;
        while (cmd[i] != ' ' && cmd[i] != '\0' && i < 63) { oldn[i] = cmd[i]; i++; }
        oldn[i] = '\0';
        if (cmd[i] == ' ') i++; const char *rest = &cmd[i];
        int j = 0; while (rest[j] != '\0' && j < 63) { newn[j] = rest[j]; j++; }
        newn[j] = '\0';
        if (fs_rename(oldn, newn) == 0) {
            console_puts("Renamed successfully\n");
        } else {
            console_puts("Error: Rename failed\n");
        }
        return;
    }

    if (strcmp(input, "clear") == 0) {
        console_clear();
        return;
    }
    
    if (strncmp(input, "rm ", 3) == 0) {
        const char *filename = &input[3];

        if (fs_delete(filename) == 0) {
            console_puts("File deleted successfully\n");
        } else {
            console_puts("Error: File not found\n");
        }
        return;
    }
    
    if (strcmp(input, "proc") == 0) {
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

    if (strncmp(input, "net ", 4) == 0) {
        const char *cmd = &input[4];
        if (strncmp(cmd, "status", 6) == 0) {
            console_puts("Network: minimal stack not loaded\n");
            return;
        } else if (strncmp(cmd, "ifconfig", 8) == 0) {
            console_puts("Interfaces: none\n");
            return;
        } else if (strncmp(cmd, "ping ", 5) == 0) {
            console_puts("Ping not implemented in this build\n");
            return;
        } else if (strncmp(cmd, "send ", 5) == 0) {
            console_puts("Net send not implemented - use netd when available\n");
            return;
        }
    }
    
    if (strcmp(input, "help") == 0) {
        console_puts("Available Commands:\n");
        console_puts("  echo <text>        - Echo text\n");
        console_puts("  math <expr>        - Calculate math (supports floats)\n");
        console_puts("  memstat           - Show memory statistics\n");
        console_puts("  procstat          - Show process/thread statistics\n");
        console_puts("  proclist          - List all processes and threads\n");
        console_puts("  procinfo <pid>    - Show process info\n");
        console_puts("  fsstat            - Show filesystem statistics\n");
        console_puts("  ls                - List files\n");
        console_puts("  cat <filename>    - Read file contents\n");
        console_puts("  write <file> <text> - Write to file\n");
        console_puts("  write <file> <text> - Write to file\n");
        console_puts("  rm <filename>      - Delete file\n");
        console_puts("  proc               - View /proc filesystem\n");
        console_puts("  cfold <name>       - Create folder in cwd\n");
        console_puts("  cd <name|..>       - Change directory\n");
        console_puts("  rename <old> <new> - Rename file or folder\n");
        console_puts("  clear              - Clear the terminal screen\n");
        console_puts("  top               - Show running processes\n");
        console_puts("  dmesg             - Show all kernel logs\n");
        console_puts("  dmesg <count>     - Show last N entries\n");
        console_puts("  help              - Show this help\n");
        return;
    }

    // Module management commands
    if (strncmp(input, "mod list", 8) == 0) {
        module_list();
        return;
    }
    if (strncmp(input, "mod load ", 9) == 0) {
        const char *name = &input[9];
        if (module_load(name) == 0) console_puts("Module loaded\n"); else console_puts("Module load failed\n");
        return;
    }
    if (strncmp(input, "mod unload ", 11) == 0) {
        const char *name = &input[11];
        if (module_unload(name) == 0) console_puts("Module unloaded\n"); else console_puts("Module unload failed\n");
        return;
    }

    if (strcmp(input, "testfaults") == 0) {
        // Simulate user process fault scenarios without crashing kernel.
        event_emit_text(EVENT_FS_ERROR, "faulttest: buffer overflow in user proc");
        event_emit_text(EVENT_PROC_EXIT, "faulttest: invalid syscall from user proc");
        event_emit_text(EVENT_OOM, "faulttest: null pointer deref simulated");
        console_puts("Fault tests emitted (kernel survived)\n");
        return;
    }
    
    console_puts("Unknown command: ");
    console_puts(input);
    console_puts("\n");
}

void kernel_main(void) {
    // Initialize console and serial logging
    console_clear();
    serial_init();
    
    console_puts("=== ProOS v0.0.2a Boot ===\n");
    console_puts("Initializing memory...\n");
    memory_init();
    
    console_puts("Initializing logging...\n");
    logging_init();
    log_info("Kernel initialization started");
    console_puts("Initializing event bus...\n");
    event_init();
    log_info("Event bus initialized");

    // Register built-in kernel modules (static descriptors)
    extern kernel_module_t debug_module;
    module_register(&debug_module);
    
    console_puts("Initializing filesystem...\n");
    filesystem_init();
    log_info("Filesystem initialized");
    
    console_puts("Initializing IPC...\n");
    ipc_init();
    log_info("IPC system initialized");
    
    console_puts("Initializing process manager...\n");
    process_manager_init();
    log_info("Process manager initialized");

    // Start initd (supervisor daemon)
    log_info("Starting initd");
    process_create(initd_main, 4096, "initd", CAP_PROC_CONTROL | CAP_MODULE_LOAD | CAP_INTROSPECT);
    
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
    
    // Set keyboard callback for real-time display (default to VGA)
    keyboard_set_display_callback(fb_console_enabled ? fb_console_putc : console_putchar);
    
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
        // yield CPU to let GUI daemons run
        asm volatile ("hlt");
    }
}
