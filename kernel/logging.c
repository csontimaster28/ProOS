#include "logging.h"

// Log buffer
static log_entry_t log_buffer[MAX_LOG_ENTRIES];
static uint32_t log_count = 0;
static uint32_t log_index = 0;

// External function declarations
void console_puts(const char *s);
void console_putchar(char c);
void itoa(int num, char *str);

// Initialize logging system
void logging_init(void) {
    log_count = 0;
    log_index = 0;
    
    // Clear log buffer
    for (uint32_t i = 0; i < MAX_LOG_ENTRIES; i++) {
        log_buffer[i].timestamp = 0;
        log_buffer[i].level = LOG_INFO;
        log_buffer[i].message[0] = '\0';
    }
}

// Log a message
void log_message(log_level_t level, const char *message) {
    if (!message) {
        return;
    }
    
    // Get current entry
    log_entry_t *entry = &log_buffer[log_index];
    entry->timestamp = 0;  // TODO: use actual tick count
    entry->level = level;
    
    // Copy message with bounds checking
    const char *src = message;
    char *dst = entry->message;
    uint32_t i = 0;
    
    while (*src && i < MAX_LOG_MESSAGE - 1) {
        *dst++ = *src++;
        i++;
    }
    *dst = '\0';  // Null terminator
    
    // Advance log index
    log_index = (log_index + 1) % MAX_LOG_ENTRIES;
    
    // Update count
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

// Log info message
void log_info(const char *message) {
    log_message(LOG_INFO, message);
}

// Log warning message
void log_warning(const char *message) {
    log_message(LOG_WARNING, message);
}

// Log error message
void log_error(const char *message) {
    log_message(LOG_ERROR, message);
}

// Log debug message
void log_debug(const char *message) {
    log_message(LOG_DEBUG, message);
}

// Print all log entries
void logging_print_all(void) {
    char buffer[64];
    
    console_puts("\n=== System Log (dmesg) ===\n");
    
    if (log_count == 0) {
        console_puts("No log entries\n");
        return;
    }
    
    // Calculate starting index for circular buffer
    uint32_t start_idx = (log_count < MAX_LOG_ENTRIES) ? 0 : log_index;
    
    for (uint32_t i = 0; i < log_count; i++) {
        uint32_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
        log_entry_t *entry = &log_buffer[idx];
        
        console_puts("[");
        itoa(entry->timestamp, buffer);
        console_puts(buffer);
        console_puts("] ");
        
        // Print log level
        switch (entry->level) {
            case LOG_INFO:
                console_puts("INFO  ");
                break;
            case LOG_WARNING:
                console_puts("WARN  ");
                break;
            case LOG_ERROR:
                console_puts("ERROR ");
                break;
            case LOG_DEBUG:
                console_puts("DEBUG ");
                break;
        }
        
        console_puts(entry->message);
        console_puts("\n");
    }
}

// Print recent log entries
void logging_print_recent(uint32_t count) {
    char buffer[64];
    
    console_puts("\n=== Recent Log Entries ===\n");
    
    if (log_count == 0) {
        console_puts("No log entries\n");
        return;
    }
    
    // Limit count to actual log entries
    if (count > log_count) {
        count = log_count;
    }
    
    // Calculate starting index
    uint32_t start_idx = (log_count < MAX_LOG_ENTRIES) ? 0 : log_index;
    uint32_t start = (log_count > count) ? (log_count - count) : 0;
    
    for (uint32_t i = start; i < log_count; i++) {
        uint32_t idx = (start_idx + i) % MAX_LOG_ENTRIES;
        log_entry_t *entry = &log_buffer[idx];
        
        console_puts("[");
        itoa(entry->timestamp, buffer);
        console_puts(buffer);
        console_puts("] ");
        console_puts(entry->message);
        console_puts("\n");
    }
}

// Clear log buffer
void logging_clear(void) {
    log_count = 0;
    log_index = 0;
    
    for (uint32_t i = 0; i < MAX_LOG_ENTRIES; i++) {
        log_buffer[i].timestamp = 0;
        log_buffer[i].message[0] = '\0';
    }
}

// Get log count
uint32_t logging_get_count(void) {
    return log_count;
}
