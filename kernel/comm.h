#ifndef MEMKERNEL_COMM_H
#define MEMKERNEL_COMM_H

#include <linux/types.h>

struct CopyMemory {
	pid_t pid;
	uintptr_t addr;
	void *buffer;
	size_t size;
};

struct ModuleBase {
	pid_t pid;
	char *name;
	uintptr_t base;
};

enum TouchAction {
	TOUCH_ACTION_DOWN = 0,
	TOUCH_ACTION_MOVE = 1,
	TOUCH_ACTION_UP = 2,
	TOUCH_ACTION_CANCEL = 3,
};

/*
 * One Type-B multitouch update. DOWN, MOVE, and UP address a slot in the
 * range 0..9. CANCEL releases every active slot and ignores x/y/slot.
 */
struct TouchCommand {
	__s32 action;
	__s32 slot;
	__s32 x;
	__s32 y;
};

enum Operations {
	OP_READ_MEM = 0x801,
	OP_WRITE_MEM = 0x802,
	OP_MODULE_BASE = 0x803,
	OP_TOUCH_EVENT = 0x804,
};

#endif
