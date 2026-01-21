#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>
#include <stddef.h>

// Logging constants
#define MAX_LOG_ENTRIES 256
#define MAX_LOG_MESSAGE 128

// Log levels
typedef enum {
    LOG_INFO = 0,
    LOG_WARNING = 1,
    LOG_ERROR = 2,
    LOG_DEBUG = 3
} log_level_t;

// Log entry structure
typedef struct {
    uint32_t timestamp;         // Entry timestamp
    log_level_t level;          // Log level
    char message[MAX_LOG_MESSAGE];  // Log message
} log_entry_t;

// Initialize logging system
void logging_init(void);

// Log functions
void log_message(log_level_t level, const char *message);
void log_info(const char *message);
void log_warning(const char *message);
void log_error(const char *message);
void log_debug(const char *message);

// Display logs
void logging_print_all(void);
void logging_print_recent(uint32_t count);
void logging_clear(void);

// Get statistics
uint32_t logging_get_count(void);

#endif // LOGGING_H
