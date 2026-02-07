#include "module.h"
#include "logging.h"
#include "event.h"

#define MAX_MODULES 16

static kernel_module_t *modules[MAX_MODULES];

int module_register(kernel_module_t *mod) {
    if (!mod) return -1;
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i] == NULL) {
            modules[i] = mod;
            mod->loaded = 0;
            return 0;
        }
    }
    return -1;
}

int module_load(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i] && modules[i]->name && modules[i]->loaded == 0) {
            // Compare name
            const char *a = modules[i]->name;
            const char *b = name;
            int match = 1;
            while (*a && *b) { if (*a != *b) { match = 0; break; } a++; b++; }
            if (match && *a == '\0' && *b == '\0') {
                // Call init if present
                if (modules[i]->init) {
                    int r = modules[i]->init();
                    if (r != 0) {
                        log_error("module_load: init failed");
                        event_emit_text(EVENT_MODULE_CRASH, name);
                        return -1;
                    }
                }
                modules[i]->loaded = 1;
                event_emit_text(EVENT_MODULE_LOAD, name);
                return 0;
            }
        }
    }
    return -1;
}

int module_unload(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i] && modules[i]->name && modules[i]->loaded) {
            const char *a = modules[i]->name;
            const char *b = name;
            int match = 1;
            while (*a && *b) { if (*a != *b) { match = 0; break; } a++; b++; }
            if (match && *a == '\0' && *b == '\0') {
                if (modules[i]->unload) {
                    int r = modules[i]->unload();
                    if (r != 0) {
                        log_error("module_unload: unload failed");
                        return -1;
                    }
                }
                modules[i]->loaded = 0;
                return 0;
            }
        }
    }
    return -1;
}

void module_list(void) {
    console_puts("\n=== Modules ===\n");
    for (int i = 0; i < MAX_MODULES; i++) {
        if (modules[i]) {
            console_puts(modules[i]->name);
            console_puts(modules[i]->loaded ? " (loaded)\n" : " (unloaded)\n");
        }
    }
}

