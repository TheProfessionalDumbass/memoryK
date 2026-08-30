#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/spinlock.h>

extern int g_aim_dx, g_aim_dy;
extern spinlock_t g_aim_lock;
extern int g_screen_width, g_screen_height;

// Architecture-specific register offsets
#if defined(CONFIG_ARM64)
    #define REG_DEV   regs->regs[0]
    #define REG_TYPE  regs->regs[1]
    #define REG_CODE  regs->regs[2]
    #define REG_VALUE regs->regs[3]
#elif defined(CONFIG_X86_64)
    #define REG_DEV   regs->di
    #define REG_TYPE  regs->si
    #define REG_CODE  regs->dx
    #define REG_VALUE regs->cx
#else
    #error "Unsupported architecture"
#endif

static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
    struct input_dev *dev = (struct input_dev *)REG_DEV;
    unsigned int type = (unsigned int)REG_TYPE;
    unsigned int code = (unsigned int)REG_CODE;
    int value = (int)REG_VALUE;

    // Only intercept ABS touch events from the real touchscreen
    if (type == EV_ABS && dev && dev->name) {
        // Match your actual touchscreen driver name (check dmesg)
        if (strstr(dev->name, "touch") || strstr(dev->name, "synaptics") || strstr(dev->name, "ft5x06")) {
            int new_val = value;
            int modified = 0;

            spin_lock(&g_aim_lock);

            if (code == ABS_MT_POSITION_X && g_aim_dx != 0) {
                new_val += g_aim_dx;
                g_aim_dx = 0;
                modified = 1;
            }
            if (code == ABS_MT_POSITION_Y && g_aim_dy != 0) {
                new_val += g_aim_dy;
                g_aim_dy = 0;
                modified = 1;
            }

            spin_unlock(&g_aim_lock);

            if (modified) {
                // Clamp to prevent crashes
                if (new_val < 0) new_val = 0;
                if (code == ABS_MT_POSITION_X && new_val > g_screen_width) 
                    new_val = g_screen_width;
                if (code == ABS_MT_POSITION_Y && new_val > g_screen_height) 
                    new_val = g_screen_height;

                REG_VALUE = new_val;  // Inject the modified coordinate
            }
        }
    }
    return 0; // Continue to the real input_event
}

static struct kprobe kp = {
    .symbol_name = "input_event",
    .pre_handler = handler_pre,
};

int install_input_hook(void) {
    int ret = register_kprobe(&kp);
    if (ret < 0)
        printk(KERN_ERR "miprotect: kprobe failed: %d\n", ret);
    else
        printk(KERN_INFO "miprotect: input_event hooked\n");
    return ret;
}

void remove_input_hook(void) {
    unregister_kprobe(&kp);
}
