#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <asm/ptrace.h>

// ------------------------------------------------------------
//  IMPORT GLOBALS FROM entry.c
// ------------------------------------------------------------
extern int g_aim_dx;
extern int g_aim_dy;
extern spinlock_t g_aim_lock;
extern int g_screen_width;
extern int g_screen_height;

// ------------------------------------------------------------
//  ARCHITECTURE-SPECIFIC REGISTER ACCESS (ARM64 / x86_64)
// ------------------------------------------------------------
#if defined(CONFIG_ARM64)
    #define GET_REG_DEV(regs)   ((unsigned long)regs->regs[0])
    #define GET_REG_TYPE(regs)  ((unsigned int)regs->regs[1])
    #define GET_REG_CODE(regs)  ((unsigned int)regs->regs[2])
    #define GET_REG_VALUE(regs) ((int)regs->regs[3])
    #define SET_REG_VALUE(regs, val) (regs->regs[3] = (unsigned long)(val))
#elif defined(CONFIG_X86_64)
    #define GET_REG_DEV(regs)   ((unsigned long)regs->di)
    #define GET_REG_TYPE(regs)  ((unsigned int)regs->si)
    #define GET_REG_CODE(regs)  ((unsigned int)regs->dx)
    #define GET_REG_VALUE(regs) ((int)regs->cx)
    #define SET_REG_VALUE(regs, val) (regs->cx = (unsigned long)(val))
#else
    #error "Unsupported architecture. Only ARM64 and x86_64 are supported."
#endif

// ------------------------------------------------------------
//  KPROBE PRE-HANDLER (Runs BEFORE input_event executes)
// ------------------------------------------------------------
static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
    unsigned long dev_addr = GET_REG_DEV(regs);
    unsigned int type = GET_REG_TYPE(regs);
    unsigned int code = GET_REG_CODE(regs);
    int value = GET_REG_VALUE(regs);

    // Safety check
    if (!dev_addr) return 0;

    struct input_dev *dev = (struct input_dev *)dev_addr;

    // Only intercept ABS touch events from the REAL touchscreen
    if (type == EV_ABS && dev && dev->name) {
        // Filter by typical touchscreen driver names (adjust if needed)
        if (strstr(dev->name, "synaptics") ||
            strstr(dev->name, "ft5x06") ||
            strstr(dev->name, "goodix") ||
            strstr(dev->name, "touch") ||
            strstr(dev->name, "silead") ||
            strstr(dev->name, "egalax") ||
            strstr(dev->name, "atmel")) {

            int new_val = value;
            int modified = 0;

            // Lock and consume the aim deltas
            spin_lock(&g_aim_lock);

            if (code == ABS_MT_POSITION_X && g_aim_dx != 0) {
                new_val += g_aim_dx;
                g_aim_dx = 0;   // Consume only once per touch frame
                modified = 1;
            }
            if (code == ABS_MT_POSITION_Y && g_aim_dy != 0) {
                new_val += g_aim_dy;
                g_aim_dy = 0;
                modified = 1;
            }

            spin_unlock(&g_aim_lock);

            if (modified) {
                // Clamp to prevent kernel crashes from out-of-bounds values
                if (new_val < 0) new_val = 0;
                if (code == ABS_MT_POSITION_X && new_val > g_screen_width)
                    new_val = g_screen_width;
                if (code == ABS_MT_POSITION_Y && new_val > g_screen_height)
                    new_val = g_screen_height;

                // Overwrite the register value before the kernel reads it
                SET_REG_VALUE(regs, new_val);
            }
        }
    }
    return 0; // Always return 0 to continue execution
}

// ------------------------------------------------------------
//  KPROBE STRUCTURE
// ------------------------------------------------------------
static struct kprobe kp = {
    .symbol_name = "input_event",   // Kernel function to hook
    .pre_handler = handler_pre,
};

// ------------------------------------------------------------
//  EXPORTED INSTALL / REMOVE FUNCTIONS
// ------------------------------------------------------------
int install_input_hook(void) {
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("miprotect: Failed to register kprobe on input_event (err=%d)\n", ret);
        return ret;
    }
    pr_info("miprotect: Kprobe installed on input_event (Kernel 5.10+)\n");
    return 0;
}

void remove_input_hook(void) {
    unregister_kprobe(&kp);
    pr_info("miprotect: Kprobe removed from input_event\n");
}
