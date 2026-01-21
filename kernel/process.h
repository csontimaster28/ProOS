#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stddef.h>

// Process and Thread limits
#define MAX_PROCESSES 8
#define MAX_THREADS_PER_PROCESS 4
#define THREAD_STACK_SIZE 4096

// Process and Thread states
typedef enum {
    PROC_CREATED,
    PROC_RUNNING,
    PROC_READY,
    PROC_BLOCKED,
    PROC_TERMINATED
} process_state_t;

typedef enum {
    THREAD_CREATED,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

// Thread structure
typedef struct thread {
    uint32_t tid;                      // Thread ID
    uint32_t pid;                      // Parent Process ID
    thread_state_t state;              // Thread state
    uint32_t esp;                      // Stack pointer
    uint32_t ebp;                      // Base pointer
    uint32_t eip;                      // Instruction pointer
    uint8_t *stack;                    // Stack memory
    uint32_t stack_size;               // Stack size
    uint32_t ticks;                    // Execution ticks
    uint32_t priority;                 // Thread priority (0=low, 10=high)
    void (*entry_point)(void);         // Thread entry point
    struct thread *next;               // Linked list for scheduling
} thread_t;

// Process structure
typedef struct {
    uint32_t pid;                      // Process ID
    process_state_t state;             // Process state
    void *memory_start;                // Process memory start
    uint32_t memory_size;              // Process memory size
    thread_t *main_thread;             // Main thread
    thread_t *threads[MAX_THREADS_PER_PROCESS];  // Thread list
    uint32_t thread_count;             // Number of threads
    uint32_t created_ticks;            // Creation time
    uint32_t terminated_ticks;         // Termination time
    uint32_t total_ticks;              // Total execution time
} process_t;

// Process and Thread management structure
typedef struct {
    process_t processes[MAX_PROCESSES];
    uint32_t process_count;
    uint32_t current_pid;
    uint32_t current_tid;
    uint32_t next_pid;
    uint32_t next_tid;
    thread_t *ready_queue;             // Head of ready thread queue
    thread_t *current_thread;          // Currently executing thread
} process_manager_t;

// Initialize process manager
void process_manager_init(void);

// Process management
uint32_t process_create(void (*entry)(void), uint32_t memory_size, const char *name);
int process_terminate(uint32_t pid);
process_t* process_get_by_id(uint32_t pid);
process_state_t process_get_state(uint32_t pid);

// Thread management
uint32_t thread_create(uint32_t pid, void (*entry)(void), uint32_t priority);
int thread_terminate(uint32_t tid);
thread_t* thread_get_by_id(uint32_t tid);
thread_state_t thread_get_state(uint32_t tid);
void thread_set_priority(uint32_t tid, uint32_t priority);

// Scheduling
void process_schedule(void);
thread_t* process_get_next_thread(void);
void process_context_switch(void);

// Statistics
typedef struct {
    uint32_t total_processes;
    uint32_t running_processes;
    uint32_t ready_processes;
    uint32_t blocked_processes;
    uint32_t terminated_processes;
    uint32_t total_threads;
    uint32_t ready_threads;
    uint32_t running_threads;
} process_stats_t;

void process_get_stats(process_stats_t *stats);
void process_print_stats(void);
void process_print_processes(void);

#endif // PROCESS_H
