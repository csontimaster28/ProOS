#include "module.h"
#include "logging.h"

static int debug_init(void) {
    log_info("debug_module initialized");
    return 0;
}

static int debug_unload(void) {
    log_info("debug_module unloaded");
    return 0;
}

kernel_module_t debug_module = {
    .name = "debug",
    .loaded = 0,
    .init = debug_init,
    .unload = debug_unload
};
