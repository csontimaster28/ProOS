#include "filesystem.h"
#include "memory.h"

// Inode table
static inode_t inode_table[MAX_FILES];

// Open file descriptors
static file_descriptor_t *open_files[MAX_FILES];
static uint32_t open_file_count = 0;

// Filesystem state
static uint32_t next_inode_num = 1;
static uint32_t inode_count = 0;

// External function declarations
void console_puts(const char *s);
void console_putchar(char c);
void itoa(int num, char *str);

// Initialize filesystem
void filesystem_init(void) {
    inode_count = 0;
    next_inode_num = 1;
    open_file_count = 0;
    
    // Initialize inode table
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        inode_table[i].is_used = 0;
        inode_table[i].inode_num = i;
        inode_table[i].size = 0;
        inode_table[i].capacity = MAX_FILE_SIZE;
        inode_table[i].data = NULL;
        inode_table[i].filename[0] = '\0';
        open_files[i] = NULL;
    }
}

// Find inode by filename
static inode_t* find_inode_by_name(const char *filename) {
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].is_used) {
            // Simple string comparison
            const char *a = inode_table[i].filename;
            const char *b = filename;
            int match = 1;
            while (*a && *b) {
                if (*a != *b) {
                    match = 0;
                    break;
                }
                a++;
                b++;
            }
            if (match && *a == '\0' && *b == '\0') {
                return &inode_table[i];
            }
        }
    }
    return NULL;
}

// Find free inode
static inode_t* find_free_inode(void) {
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (!inode_table[i].is_used) {
            return &inode_table[i];
        }
    }
    return NULL;
}

// Open a file
int32_t fs_open(const char *filename, file_mode_t mode, uint32_t pid) {
    if (!filename || open_file_count >= MAX_FILES) {
        return -1;
    }
    
    inode_t *inode = NULL;
    file_descriptor_t *fd = NULL;
    
    // Handle read mode
    if (mode == FILE_MODE_READ) {
        inode = find_inode_by_name(filename);
        if (!inode) {
            return -1;  // File not found
        }
    } else if (mode == FILE_MODE_WRITE || mode == FILE_MODE_APPEND) {
        // Try to find existing file
        inode = find_inode_by_name(filename);
        
        if (!inode) {
            // Create new inode
            inode = find_free_inode();
            if (!inode) {
                return -1;  // No free inodes
            }
            
            // Initialize inode
            inode->is_used = 1;
            inode->inode_num = next_inode_num++;
            inode->size = 0;
            inode->capacity = MAX_FILE_SIZE;
            inode->created_ticks = 0;
            inode->modified_ticks = 0;
            
            // Copy filename (with bounds checking)
            const char *src = filename;
            char *dst = inode->filename;
            uint32_t i = 0;
            while (*src && i < MAX_FILENAME - 1) {
                *dst++ = *src++;
                i++;
            }
            *dst = '\0';  // Null terminator
            
            inode_count++;
        } else if (mode == FILE_MODE_WRITE) {
            // Clear existing file in write mode
            if (inode->data) {
                free(inode->data);
            }
            inode->data = NULL;
            inode->size = 0;
        }
    }
    
    // Allocate file descriptor
    fd = (file_descriptor_t *)malloc(sizeof(file_descriptor_t));
    if (!fd) {
        return -1;  // Out of memory
    }
    
    // Initialize file descriptor
    fd->inode_num = inode->inode_num;
    fd->size = inode->size;
    fd->capacity = inode->capacity;
    fd->state = FILE_STATE_OPEN;
    fd->owner_pid = pid;
    fd->read_pos = 0;
    fd->write_pos = (mode == FILE_MODE_APPEND) ? inode->size : 0;
    fd->data = inode->data;
    
    // Copy filename (with bounds checking)
    const char *src = inode->filename;
    char *dst = fd->filename;
    uint32_t i = 0;
    while (*src && i < MAX_FILENAME - 1) {
        *dst++ = *src++;
        i++;
    }
    *dst = '\0';  // Null terminator
    
    // Store file descriptor
    open_files[open_file_count] = fd;
    int32_t fd_index = open_file_count;
    open_file_count++;
    
    return fd_index;
}

// Close a file
int fs_close(int32_t fd) {
    if (fd < 0 || fd >= (int32_t)open_file_count || !open_files[fd]) {
        return -1;
    }
    
    // Get the inode and update it
    inode_t *inode = find_inode_by_name(open_files[fd]->filename);
    if (inode) {
        inode->size = open_files[fd]->size;
        inode->data = open_files[fd]->data;
        inode->capacity = open_files[fd]->capacity;
        inode->modified_ticks = 0;
    }
    
    free(open_files[fd]);
    open_files[fd] = NULL;
    
    // Shift remaining open files
    for (int32_t i = fd; i < (int32_t)open_file_count - 1; i++) {
        open_files[i] = open_files[i + 1];
    }
    
    open_file_count--;
    return 0;
}

// Read from file
int fs_read(int32_t fd, uint8_t *buffer, uint32_t size) {
    if (fd < 0 || fd >= (int32_t)open_file_count || !open_files[fd]) {
        return -1;
    }
    
    file_descriptor_t *file = open_files[fd];
    
    // Bounds checking
    if (!buffer || file->read_pos > file->size) {
        return -1;
    }
    
    if (file->read_pos >= file->size) {
        return 0;  // End of file
    }
    
    // Calculate how much to read
    uint32_t remaining = file->size - file->read_pos;
    uint32_t to_read = (size < remaining) ? size : remaining;
    
    // Bounds check on buffer access
    if (!memory_check_bounds(buffer, to_read - 1)) {
        return -1;  // Buffer bounds exceeded
    }
    
    // Copy data
    uint8_t *src = file->data + file->read_pos;
    for (uint32_t i = 0; i < to_read; i++) {
        buffer[i] = src[i];
    }
    
    file->read_pos += to_read;
    return to_read;
}

// Write to file
int fs_write(int32_t fd, const uint8_t *data, uint32_t size) {
    if (fd < 0 || fd >= (int32_t)open_file_count || !open_files[fd]) {
        return -1;
    }
    
    file_descriptor_t *file = open_files[fd];
    
    // Bounds checking - ensure write doesn't exceed capacity
    if (file->write_pos + size > file->capacity) {
        return -1;  // Would exceed capacity
    }
    
    // Check if we need more space
    uint32_t needed_size = file->write_pos + size;
    
    if (needed_size > file->size) {
        // Allocate more space
        uint8_t *new_data = (uint8_t *)malloc(needed_size + 1);  // +1 for null terminator
        if (!new_data) {
            return -1;  // Out of memory
        }
        
        // Copy old data
        if (file->data && file->size > 0) {
            for (uint32_t i = 0; i < file->size; i++) {
                new_data[i] = file->data[i];
            }
            free(file->data);
        }
        
        file->data = new_data;
        file->size = needed_size;
    }
    
    // Bounds check on input data
    if (!memory_check_bounds((void *)data, size - 1)) {
        return -1;  // Input buffer bounds exceeded
    }
    
    // Write data
    uint8_t *dst = file->data + file->write_pos;
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = data[i];
    }
    
    file->write_pos += size;
    
    // Add null terminator if this is text data
    if (file->data && file->write_pos < file->capacity) {
        file->data[file->write_pos] = '\0';
    }
    
    return size;
}

// Delete a file
int fs_delete(const char *filename) {
    inode_t *inode = find_inode_by_name(filename);
    if (!inode) {
        return -1;  // File not found
    }
    
    // Close any open file descriptors for this file
    for (uint32_t i = 0; i < open_file_count; i++) {
        if (open_files[i] && open_files[i]->inode_num == inode->inode_num) {
            fs_close(i);
        }
    }
    
    // Free file data
    if (inode->data) {
        free(inode->data);
    }
    
    // Mark inode as unused
    inode->is_used = 0;
    inode->size = 0;
    inode->capacity = MAX_FILE_SIZE;
    inode->data = NULL;
    inode_count--;
    
    return 0;
}

// Check if file exists
int fs_exists(const char *filename) {
    return find_inode_by_name(filename) != NULL ? 1 : 0;
}

// Get file size
uint32_t fs_filesize(const char *filename) {
    inode_t *inode = find_inode_by_name(filename);
    if (inode) {
        return inode->size;
    }
    return 0;
}

// List all files
void fs_list_files(void) {
    char buffer[64];
    
    console_puts("\n=== Filesystem - Files ===\n");
    
    if (inode_count == 0) {
        console_puts("No files\n");
        return;
    }
    
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].is_used) {
            console_puts("File: ");
            console_puts(inode_table[i].filename);
            console_puts(" | Size: ");
            itoa(inode_table[i].size, buffer);
            console_puts(buffer);
            console_puts(" | Cap: ");
            itoa(inode_table[i].capacity, buffer);
            console_puts(buffer);
            console_puts("\n");
        }
    }
}

// Get file descriptor
file_descriptor_t* fs_get_file(int32_t fd) {
    if (fd >= 0 && fd < (int32_t)open_file_count) {
        return open_files[fd];
    }
    return NULL;
}

// Get filesystem statistics
void filesystem_get_stats(filesystem_stats_t *stats) {
    if (!stats) {
        return;
    }
    
    stats->total_files = MAX_FILES;
    stats->used_files = inode_count;
    stats->total_space = MAX_FILES * MAX_FILE_SIZE;
    stats->open_files = open_file_count;
    
    // Calculate used and free space
    stats->used_space = 0;
    for (uint32_t i = 0; i < MAX_FILES; i++) {
        if (inode_table[i].is_used && inode_table[i].data) {
            stats->used_space += inode_table[i].size;
        }
    }
    
    stats->free_space = stats->total_space - stats->used_space;
}

// Print filesystem statistics
void filesystem_print_stats(void) {
    filesystem_stats_t stats = {0};
    filesystem_get_stats(&stats);
    
    char buffer[64];
    
    console_puts("\n=== Filesystem Statistics ===\n");
    
    console_puts("Total Files:          ");
    itoa(stats.total_files, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Used Files:           ");
    itoa(stats.used_files, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Open Files:           ");
    itoa(stats.open_files, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Used Space:           ");
    itoa(stats.used_space, buffer);
    console_puts(buffer);
    console_puts(" bytes\n");
    
    console_puts("Free Space:           ");
    itoa(stats.free_space / 1024, buffer);
    console_puts(buffer);
    console_puts(" KB\n");
}

// Read string from file
int io_read_string(int32_t fd, char *buffer, uint32_t max_size) {
    uint32_t pos = 0;
    uint8_t byte;
    
    while (pos < max_size - 1) {
        int result = fs_read(fd, &byte, 1);
        if (result <= 0) {
            break;
        }
        
        if (byte == '\0' || byte == '\n') {
            break;
        }
        
        buffer[pos++] = (char)byte;
    }
    
    buffer[pos] = '\0';  // Null terminator
    return pos;
}

// Write string to file
int io_write_string(int32_t fd, const char *str) {
    const uint8_t *data = (const uint8_t *)str;
    uint32_t size = 0;
    
    while (data[size] != '\0') {
        size++;
    }
    
    return fs_write(fd, data, size);
}
