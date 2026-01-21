#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>

// Filesystem constants
#define MAX_FILES 32
#define MAX_FILENAME 64
#define MAX_FILE_SIZE 65536     // 64KB max file size
#define INODE_SIZE 256          // Size of inode metadata

// File modes
typedef enum {
    FILE_MODE_READ = 0x01,
    FILE_MODE_WRITE = 0x02,
    FILE_MODE_APPEND = 0x04
} file_mode_t;

// File state
typedef enum {
    FILE_STATE_CLOSED = 0,
    FILE_STATE_OPEN = 1
} file_state_t;

// File descriptor structure
typedef struct {
    uint32_t inode_num;         // Inode number
    char filename[MAX_FILENAME];  // File name
    uint32_t size;              // Current size (used bytes)
    uint32_t capacity;          // Maximum capacity
    uint32_t created_ticks;     // Creation time
    uint32_t modified_ticks;    // Modification time
    file_state_t state;         // File state
    uint32_t owner_pid;         // Owner process ID
    uint8_t *data;              // File data pointer
    uint32_t read_pos;          // Current read position
    uint32_t write_pos;         // Current write position
} file_descriptor_t;

// Inode structure
typedef struct {
    uint32_t inode_num;         // Inode number
    char filename[MAX_FILENAME];  // File name
    uint32_t size;              // Current size (used bytes)
    uint32_t capacity;          // Maximum capacity
    uint32_t created_ticks;     // Creation time
    uint32_t modified_ticks;    // Modification time
    uint8_t *data;              // Pointer to file data
    uint8_t is_used;            // 1 if inode is in use
} inode_t;

// Filesystem statistics
typedef struct {
    uint32_t total_files;
    uint32_t used_files;
    uint32_t total_space;
    uint32_t used_space;
    uint32_t free_space;
    uint32_t open_files;
} filesystem_stats_t;

// Initialize filesystem
void filesystem_init(void);

// File operations
int32_t fs_open(const char *filename, file_mode_t mode, uint32_t pid);
int fs_close(int32_t fd);
int fs_read(int32_t fd, uint8_t *buffer, uint32_t size);
int fs_write(int32_t fd, const uint8_t *data, uint32_t size);
int fs_delete(const char *filename);
int fs_exists(const char *filename);
uint32_t fs_filesize(const char *filename);

// Directory operations
void fs_list_files(void);
file_descriptor_t* fs_get_file(int32_t fd);

// Statistics
void filesystem_get_stats(filesystem_stats_t *stats);
void filesystem_print_stats(void);

// I/O operations
int io_read_string(int32_t fd, char *buffer, uint32_t max_size);
int io_write_string(int32_t fd, const char *str);

#endif // FILESYSTEM_H
