#pragma once
#include <stdint.h>

#define MAX_TASKS 16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
} task_state_t;

typedef struct {
    int id;
    task_state_t state;
} task_t;

void scheduler_init(void);
void scheduler_add_task(void (*entry)(void));
void scheduler_switch_task(void);
task_t* scheduler_get_current_task(void);
void pit_init(void);
uint32_t scheduler_get_ticks(void);
void scheduler_tick(void);
