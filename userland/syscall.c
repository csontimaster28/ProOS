#include "../kernel/syscall.h"

int syscall3(int id, unsigned int a, unsigned int b, unsigned int c) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(id), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return ret;
}
