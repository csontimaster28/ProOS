#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

// Memory management constants
#define MEMORY_START 0x100000      // Start of heap at 1MB
#define MEMORY_SIZE 0x100000       // 1MB of heap memory
#define MEMORY_BLOCK_SIZE 16       // Minimum allocation size (bytes)
#define MEMORY_GUARD_BYTE 0xDEADBEEF  // Guard pattern for detecting overflow
#define MEMORY_GUARD_SIZE 4        // Size of guard bytes

// Memory block metadata structure
typedef struct memory_block {
    uint32_t size;                 // Actual allocated size (including capacity info)
    uint32_t capacity;             // Usable capacity
    uint32_t used;                 // Currently used bytes in this block
    uint8_t is_free;              // 1 if free, 0 if allocated
    uint32_t guard_start;          // Guard bytes at start (0xDEADBEEF)
    struct memory_block *next;    // Pointer to next block
} memory_block_t;

// Memory management statistics
typedef struct {
    uint32_t total_memory;        // Total memory available
    uint32_t used_memory;         // Currently used memory
    uint32_t free_memory;         // Currently free memory
    uint32_t block_count;         // Number of memory blocks
    uint32_t allocation_count;    // Number of allocations
    uint32_t free_count;          // Number of frees
} memory_stats_t;

// Initialize memory management system
void memory_init(void);

// Allocate memory (malloc-like)
void *malloc(size_t size);

// Free memory (free-like)
void free(void *ptr);

// Get memory statistics
void memory_get_stats(memory_stats_t *stats);

// Print memory statistics to console
void memory_print_stats(void);

// Initialize a memory region with a value
void *memset(void *ptr, int value, size_t size);

// Copy memory from source to destination
void *memcpy(void *dest, const void *src, size_t size);

// Safety functions
int memory_is_valid_ptr(void *ptr);
int memory_check_bounds(void *ptr, size_t offset);
int memory_check_guard(void *ptr);

#endif // MEMORY_H
