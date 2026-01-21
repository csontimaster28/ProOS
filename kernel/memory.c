#include "memory.h"

// Memory pool
static uint8_t memory_pool[MEMORY_SIZE] __attribute__((section(".bss")));

// Pointer to the first memory block
static memory_block_t *memory_head = NULL;

// Memory statistics
static memory_stats_t mem_stats = {0};

// External function from kernel.c for console output
void console_puts(const char *s);
void console_putchar(char c);

void itoa(int num, char *str);

// Initialize memory management system
void memory_init(void) {
    // Initialize the memory statistics
    mem_stats.total_memory = MEMORY_SIZE;
    mem_stats.used_memory = 0;
    mem_stats.free_memory = MEMORY_SIZE;
    mem_stats.block_count = 0;
    mem_stats.allocation_count = 0;
    mem_stats.free_count = 0;
    
    // Clear the memory pool
    memset(memory_pool, 0, MEMORY_SIZE);
    
    // Create initial free block
    memory_head = (memory_block_t *)memory_pool;
    memory_head->size = MEMORY_SIZE - sizeof(memory_block_t);
    memory_head->capacity = memory_head->size - MEMORY_GUARD_SIZE;
    memory_head->used = 0;
    memory_head->is_free = 1;
    memory_head->guard_start = 0;
    memory_head->next = NULL;
    
    mem_stats.block_count = 1;
    mem_stats.free_memory = memory_head->size;
}

// Find a suitable memory block for allocation
static memory_block_t *find_free_block(size_t size) {
    memory_block_t *current = memory_head;
    
    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

// Allocate memory
void *malloc(size_t size) {
    if (size == 0 || size > 65536) {  // MAX_FILE_SIZE = 65536
        return NULL;  // Invalid size or too large
    }
    
    // Round up to minimum block size
    if (size < MEMORY_BLOCK_SIZE) {
        size = MEMORY_BLOCK_SIZE;
    }
    
    // Add space for guard bytes
    uint32_t total_size = size + MEMORY_GUARD_SIZE;
    
    // Find a free block
    memory_block_t *block = find_free_block(total_size);
    
    if (!block) {
        return NULL;  // Out of memory
    }
    
    // If block is larger than needed, split it
    if (block->size > total_size + sizeof(memory_block_t)) {
        memory_block_t *new_block = (memory_block_t *)((uint8_t *)block + sizeof(memory_block_t) + total_size);
        new_block->size = block->size - total_size - sizeof(memory_block_t);
        new_block->capacity = new_block->size - MEMORY_GUARD_SIZE;
        new_block->used = 0;
        new_block->is_free = 1;
        new_block->guard_start = 0;
        new_block->next = block->next;
        
        block->next = new_block;
        block->size = total_size;
        mem_stats.block_count++;
        mem_stats.free_memory -= (total_size + sizeof(memory_block_t));
    } else {
        mem_stats.free_memory -= block->size;
    }
    
    // Mark block as allocated and set capacity/used
    block->is_free = 0;
    block->capacity = size;
    block->used = 0;
    block->guard_start = MEMORY_GUARD_BYTE;
    
    // Add guard bytes at the end
    uint8_t *guard_ptr = (uint8_t *)block + sizeof(memory_block_t) + size;
    uint32_t *guard = (uint32_t *)guard_ptr;
    *guard = MEMORY_GUARD_BYTE;
    
    mem_stats.used_memory += block->size;
    mem_stats.allocation_count++;
    
    // Return pointer to memory after metadata
    return (void *)((uint8_t *)block + sizeof(memory_block_t));
}

// Free memory
void free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // Find the block metadata (it's right before the pointer)
    memory_block_t *block = (memory_block_t *)((uint8_t *)ptr - sizeof(memory_block_t));
    
    if (block->is_free) {
        return;  // Already free
    }
    
    // Mark as free
    block->is_free = 1;
    mem_stats.used_memory -= block->size;
    mem_stats.free_memory += block->size;
    mem_stats.free_count++;
    
    // Coalesce with next block if it's free
    if (block->next && block->next->is_free) {
        block->size += sizeof(memory_block_t) + block->next->size;
        block->next = block->next->next;
        mem_stats.block_count--;
    }
    
    // Coalesce with previous block if it's free
    if (memory_head != block) {
        memory_block_t *prev = memory_head;
        while (prev && prev->next != block) {
            prev = prev->next;
        }
        
        if (prev && prev->is_free) {
            prev->size += sizeof(memory_block_t) + block->size;
            prev->next = block->next;
            mem_stats.block_count--;
        }
    }
}

// Get memory statistics
void memory_get_stats(memory_stats_t *stats) {
    if (stats) {
        *stats = mem_stats;
    }
}

// Print memory statistics to console
void memory_print_stats(void) {
    char buffer[64];
    
    console_puts("\n=== Memory Statistics ===\n");
    
    console_puts("Total Memory:     ");
    itoa(mem_stats.total_memory / 1024, buffer);
    console_puts(buffer);
    console_puts(" KB\n");
    
    console_puts("Used Memory:      ");
    itoa(mem_stats.used_memory / 1024, buffer);
    console_puts(buffer);
    console_puts(" KB\n");
    
    console_puts("Free Memory:      ");
    itoa(mem_stats.free_memory / 1024, buffer);
    console_puts(buffer);
    console_puts(" KB\n");
    
    console_puts("Block Count:      ");
    itoa(mem_stats.block_count, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Allocations:      ");
    itoa(mem_stats.allocation_count, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Frees:            ");
    itoa(mem_stats.free_count, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    int usage_percent = (mem_stats.used_memory * 100) / mem_stats.total_memory;
    console_puts("Usage:            ");
    itoa(usage_percent, buffer);
    console_puts(buffer);
    console_puts("%\n");
}

// Initialize a memory region with a value
void *memset(void *ptr, int value, size_t size) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (uint8_t)value;
    }
    return ptr;
}

// Copy memory from source to destination
void *memcpy(void *dest, const void *src, size_t size) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

// Check if pointer is valid
int memory_is_valid_ptr(void *ptr) {
    if (!ptr) {
        return 0;
    }
    
    // Check if within memory pool
    uint8_t *p = (uint8_t *)ptr;
    uint8_t *pool_start = memory_pool;
    uint8_t *pool_end = memory_pool + MEMORY_SIZE;
    
    return (p >= pool_start && p < pool_end) ? 1 : 0;
}

// Check bounds for access
int memory_check_bounds(void *ptr, size_t offset) {
    if (!memory_is_valid_ptr(ptr)) {
        return 0;
    }
    
    // Get the block metadata
    memory_block_t *block = (memory_block_t *)((uint8_t *)ptr - sizeof(memory_block_t));
    
    // Check if access would exceed capacity
    if (offset > block->capacity) {
        return 0;  // Out of bounds
    }
    
    return 1;
}

// Check guard bytes for buffer overflow detection
int memory_check_guard(void *ptr) {
    if (!memory_is_valid_ptr(ptr)) {
        return 0;
    }
    
    memory_block_t *block = (memory_block_t *)((uint8_t *)ptr - sizeof(memory_block_t));
    
    // Check start guard
    if (block->guard_start != MEMORY_GUARD_BYTE) {
        return 0;  // Guard corrupted
    }
    
    // Check end guard
    uint8_t *guard_ptr = (uint8_t *)ptr + block->capacity;
    uint32_t *guard = (uint32_t *)guard_ptr;
    if (*guard != MEMORY_GUARD_BYTE) {
        return 0;  // Guard corrupted
    }
    
    return 1;
}
