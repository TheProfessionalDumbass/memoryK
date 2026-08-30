#ifndef MEMKERNEL_COMM_H
#define MEMKERNEL_COMM_H

#include <linux/types.h>
#include <linux/pid.h>

// Structure for reading/writing memory
struct CopyMemory {
    pid_t pid;
    uintptr_t addr;
    void *buffer;
    size_t size;
};

// Structure for getting module base
struct ModuleBase {
    pid_t pid;
    char *name;
    uintptr_t base;
};

// NEW: Structure for Aimbot delta (sent from userspace)
struct AimDelta {
    int dx;
    int dy;
};

// IOCTL Commands
enum Operations {
    OP_READ_MEM = 0x801,
    OP_WRITE_MEM = 0x802,
    OP_MODULE_BASE = 0x803,
    OP_AIM_MOVE = 0x804,   // NEW: Inject touch delta
};

#endif
