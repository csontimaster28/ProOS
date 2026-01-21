#include "scheduler.h"

static task_t tasks[MAX_TASKS];
static int task_count = 0;
static int current_task_id = 0;
static volatile uint32_t ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pit_init(void) {
    // Programmable Interval Timer setup
    // Channel 0, lobyte/hibyte, mode 2 (rate generator), binary
    outb(0x43, 0x34);
    
    // Frequency divisor for ~100 Hz scheduling (1193182 Hz / 11932 ≈ 100 Hz)
    uint16_t divisor = 11932;
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void scheduler_init(void) {
    task_count = 0;
    current_task_id = 0;
    ticks = 0;
}

void scheduler_add_task(void (*entry)(void)) {
    if (task_count >= MAX_TASKS) return;
    
    task_t *task = &tasks[task_count];
    task->id = task_count;
    task->state = TASK_READY;
    
    task_count++;
}

task_t* scheduler_get_current_task(void) {
    return &tasks[current_task_id];
}

void scheduler_switch_task(void) {
    // Simple round-robin - just increment to next task
    if (task_count > 0) {
        current_task_id = (current_task_id + 1) % task_count;
    }
}

uint32_t scheduler_get_ticks(void) {
    return ticks;
}

void scheduler_tick(void) {
    ticks++;
}
