#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>

typedef struct kernel_module {
    const char *name;
    int loaded;
    // init/unload callbacks may be NULL
    int (*init)(void);
    int (*unload)(void);
} kernel_module_t;

// Register a static module descriptor (kernel-side)
int module_register(kernel_module_t *mod);

// Load/unload by name
int module_load(const char *name);
int module_unload(const char *name);

// List registered modules
void module_list(void);

#endif // MODULE_H
