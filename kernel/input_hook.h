#ifndef MEMKERNEL_INPUT_HOOK_H
#define MEMKERNEL_INPUT_HOOK_H

#include "comm.h"

int touch_input_init(void);
void touch_input_exit(void);
int touch_input_event(const struct TouchCommand *command);

#endif
