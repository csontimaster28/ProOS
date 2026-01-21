#include "process.h"
#include "memory.h"

// Global process manager
static process_manager_t pm = {0};

// External function declarations
void console_puts(const char *s);
void console_putchar(char c);
void itoa(int num, char *str);

// Initialize process manager
void process_manager_init(void) {
    pm.process_count = 0;
    pm.current_pid = 0;
    pm.current_tid = 0;
    pm.next_pid = 1;
    pm.next_tid = 1;
    pm.ready_queue = NULL;
    pm.current_thread = NULL;
}

// Create a new process
uint32_t process_create(void (*entry)(void), uint32_t memory_size, const char *name) {
    if (pm.process_count >= MAX_PROCESSES) {
        return 0;  // Too many processes
    }
    
    if (!entry) {
        return 0;  // Invalid entry point
    }
    
    // Allocate process memory
    void *proc_memory = malloc(memory_size);
    if (!proc_memory) {
        return 0;  // Out of memory
    }
    
    // Create process structure
    process_t *proc = &pm.processes[pm.process_count];
    proc->pid = pm.next_pid++;
    proc->state = PROC_CREATED;
    proc->memory_start = proc_memory;
    proc->memory_size = memory_size;
    proc->thread_count = 0;
    proc->created_ticks = 0;  // TODO: use actual tick count
    
    // Create main thread
    uint32_t tid = thread_create(proc->pid, entry, 5);
    if (tid == 0) {
        free(proc_memory);
        return 0;
    }
    
    proc->main_thread = thread_get_by_id(tid);
    proc->state = PROC_READY;
    pm.process_count++;
    
    return proc->pid;
}

// Terminate a process
int process_terminate(uint32_t pid) {
    process_t *proc = process_get_by_id(pid);
    if (!proc) {
        return 0;
    }
    
    // Terminate all threads
    for (uint32_t i = 0; i < proc->thread_count; i++) {
        if (proc->threads[i]) {
            proc->threads[i]->state = THREAD_TERMINATED;
        }
    }
    
    // Free process memory
    if (proc->memory_start) {
        free(proc->memory_start);
    }
    
    proc->state = PROC_TERMINATED;
    return 1;
}

// Get process by ID
process_t* process_get_by_id(uint32_t pid) {
    for (uint32_t i = 0; i < pm.process_count; i++) {
        if (pm.processes[i].pid == pid) {
            return &pm.processes[i];
        }
    }
    return NULL;
}

// Get process state
process_state_t process_get_state(uint32_t pid) {
    process_t *proc = process_get_by_id(pid);
    if (proc) {
        return proc->state;
    }
    return PROC_TERMINATED;
}

// Create a new thread
uint32_t thread_create(uint32_t pid, void (*entry)(void), uint32_t priority) {
    process_t *proc = process_get_by_id(pid);
    if (!proc) {
        return 0;  // Process not found
    }
    
    if (proc->thread_count >= MAX_THREADS_PER_PROCESS) {
        return 0;  // Too many threads
    }
    
    // Allocate thread structure
    thread_t *thread = (thread_t *)malloc(sizeof(thread_t));
    if (!thread) {
        return 0;  // Out of memory
    }
    
    // Allocate thread stack
    uint8_t *stack = (uint8_t *)malloc(THREAD_STACK_SIZE);
    if (!stack) {
        free(thread);
        return 0;  // Out of memory
    }
    
    // Initialize thread
    thread->tid = pm.next_tid++;
    thread->pid = pid;
    thread->state = THREAD_CREATED;
    thread->stack = stack;
    thread->stack_size = THREAD_STACK_SIZE;
    thread->esp = (uint32_t)(stack + THREAD_STACK_SIZE - 4);
    thread->ebp = thread->esp;
    thread->eip = (uint32_t)entry;
    thread->entry_point = entry;
    thread->priority = priority;
    thread->ticks = 0;
    thread->next = NULL;
    
    // Add thread to process
    proc->threads[proc->thread_count] = thread;
    proc->thread_count++;
    
    // Add to ready queue
    thread->state = THREAD_READY;
    if (!pm.ready_queue) {
        pm.ready_queue = thread;
        thread->next = NULL;
    } else {
        // Insert in priority order (higher priority first)
        thread_t *current = pm.ready_queue;
        thread_t *prev = NULL;
        
        while (current && current->priority >= thread->priority) {
            prev = current;
            current = current->next;
        }
        
        if (!prev) {
            thread->next = pm.ready_queue;
            pm.ready_queue = thread;
        } else {
            thread->next = current;
            prev->next = thread;
        }
    }
    
    return thread->tid;
}

// Terminate a thread
int thread_terminate(uint32_t tid) {
    thread_t *thread = thread_get_by_id(tid);
    if (!thread) {
        return 0;
    }
    
    thread->state = THREAD_TERMINATED;
    
    // Remove from ready queue if present
    if (pm.ready_queue == thread) {
        pm.ready_queue = thread->next;
    } else {
        thread_t *current = pm.ready_queue;
        while (current && current->next != thread) {
            current = current->next;
        }
        if (current) {
            current->next = thread->next;
        }
    }
    
    // Free thread stack
    if (thread->stack) {
        free(thread->stack);
    }
    
    // Free thread structure
    free(thread);
    
    return 1;
}

// Get thread by ID
thread_t* thread_get_by_id(uint32_t tid) {
    for (uint32_t i = 0; i < pm.process_count; i++) {
        process_t *proc = &pm.processes[i];
        for (uint32_t j = 0; j < proc->thread_count; j++) {
            if (proc->threads[j] && proc->threads[j]->tid == tid) {
                return proc->threads[j];
            }
        }
    }
    return NULL;
}

// Get thread state
thread_state_t thread_get_state(uint32_t tid) {
    thread_t *thread = thread_get_by_id(tid);
    if (thread) {
        return thread->state;
    }
    return THREAD_TERMINATED;
}

// Set thread priority
void thread_set_priority(uint32_t tid, uint32_t priority) {
    thread_t *thread = thread_get_by_id(tid);
    if (thread) {
        thread->priority = priority;
    }
}

// Get next thread from ready queue
thread_t* process_get_next_thread(void) {
    if (!pm.ready_queue) {
        return NULL;
    }
    
    thread_t *next_thread = pm.ready_queue;
    
    // Move to back of queue for round-robin within same priority
    if (next_thread && next_thread->next) {
        pm.ready_queue = next_thread->next;
        
        thread_t *current = pm.ready_queue;
        while (current->next) {
            current = current->next;
        }
        current->next = next_thread;
        next_thread->next = NULL;
    }
    
    return next_thread;
}

// Schedule next thread to run
void process_schedule(void) {
    pm.current_thread = process_get_next_thread();
    if (pm.current_thread) {
        pm.current_thread->state = THREAD_RUNNING;
        pm.current_pid = pm.current_thread->pid;
        pm.current_tid = pm.current_thread->tid;
    }
}

// Context switch (placeholder for actual assembly context switch)
void process_context_switch(void) {
    // In a real implementation, this would save current thread state
    // and restore the next thread's state
    process_schedule();
}

// Get process statistics
void process_get_stats(process_stats_t *stats) {
    if (!stats) {
        return;
    }
    
    stats->total_processes = pm.process_count;
    stats->running_processes = 0;
    stats->ready_processes = 0;
    stats->blocked_processes = 0;
    stats->terminated_processes = 0;
    stats->total_threads = 0;
    stats->ready_threads = 0;
    stats->running_threads = 0;
    
    for (uint32_t i = 0; i < pm.process_count; i++) {
        process_t *proc = &pm.processes[i];
        
        switch (proc->state) {
            case PROC_RUNNING:
                stats->running_processes++;
                break;
            case PROC_READY:
                stats->ready_processes++;
                break;
            case PROC_BLOCKED:
                stats->blocked_processes++;
                break;
            case PROC_TERMINATED:
                stats->terminated_processes++;
                break;
            default:
                break;
        }
        
        stats->total_threads += proc->thread_count;
        
        for (uint32_t j = 0; j < proc->thread_count; j++) {
            if (proc->threads[j]) {
                if (proc->threads[j]->state == THREAD_READY) {
                    stats->ready_threads++;
                } else if (proc->threads[j]->state == THREAD_RUNNING) {
                    stats->running_threads++;
                }
            }
        }
    }
}

// Print process statistics
void process_print_stats(void) {
    process_stats_t stats = {0};
    process_get_stats(&stats);
    
    char buffer[64];
    
    console_puts("\n=== Process & Thread Statistics ===\n");
    
    console_puts("Total Processes:      ");
    itoa(stats.total_processes, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Running Processes:    ");
    itoa(stats.running_processes, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Ready Processes:      ");
    itoa(stats.ready_processes, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Blocked Processes:    ");
    itoa(stats.blocked_processes, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Total Threads:        ");
    itoa(stats.total_threads, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Ready Threads:        ");
    itoa(stats.ready_threads, buffer);
    console_puts(buffer);
    console_puts("\n");
    
    console_puts("Running Threads:      ");
    itoa(stats.running_threads, buffer);
    console_puts(buffer);
    console_puts("\n");
}

// Print all processes and their threads
void process_print_processes(void) {
    char buffer[64];
    
    console_puts("\n=== Processes and Threads ===\n");
    
    for (uint32_t i = 0; i < pm.process_count; i++) {
        process_t *proc = &pm.processes[i];
        
        console_puts("PID ");
        itoa(proc->pid, buffer);
        console_puts(buffer);
        console_puts(" | State: ");
        
        switch (proc->state) {
            case PROC_RUNNING:
                console_puts("RUNNING");
                break;
            case PROC_READY:
                console_puts("READY");
                break;
            case PROC_BLOCKED:
                console_puts("BLOCKED");
                break;
            case PROC_TERMINATED:
                console_puts("TERMINATED");
                break;
            default:
                console_puts("UNKNOWN");
        }
        
        console_puts(" | Memory: ");
        itoa(proc->memory_size / 1024, buffer);
        console_puts(buffer);
        console_puts("KB | Threads: ");
        itoa(proc->thread_count, buffer);
        console_puts(buffer);
        console_puts("\n");
        
        // Print threads
        for (uint32_t j = 0; j < proc->thread_count; j++) {
            if (proc->threads[j]) {
                thread_t *thread = proc->threads[j];
                console_puts("  TID ");
                itoa(thread->tid, buffer);
                console_puts(buffer);
                console_puts(" | Priority: ");
                itoa(thread->priority, buffer);
                console_puts(buffer);
                console_puts(" | State: ");
                
                switch (thread->state) {
                    case THREAD_RUNNING:
                        console_puts("RUNNING");
                        break;
                    case THREAD_READY:
                        console_puts("READY");
                        break;
                    case THREAD_BLOCKED:
                        console_puts("BLOCKED");
                        break;
                    case THREAD_TERMINATED:
                        console_puts("TERMINATED");
                        break;
                    default:
                        console_puts("UNKNOWN");
                }
                
                console_puts("\n");
            }
        }
    }
}
