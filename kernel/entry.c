#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include "comm.h"

#define DEVICE_NAME "miprotect"
#define CLASS_NAME "miprotect_class"

static int major_num;
static struct class *device_class = NULL;
static struct device *device_obj = NULL;

// ------------------------------------------------------------
//  GLOBALS FOR AIMBOT (shared with input_hook.c)
// ------------------------------------------------------------
int g_aim_dx = 0;
int g_aim_dy = 0;
DEFINE_SPINLOCK(g_aim_lock);

// Default screen resolution (you can update these dynamically via IOCTL if needed)
int g_screen_width = 1080;
int g_screen_height = 2400;

// ------------------------------------------------------------
//  EXTERNAL FUNCTIONS (implemented in memory.c / process.c)
// ------------------------------------------------------------
extern int read_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size);
extern int write_process_memory(pid_t pid, uintptr_t addr, void *buffer, size_t size);
extern uintptr_t get_module_base(pid_t pid, char *name);

// ------------------------------------------------------------
//  EXTERNAL HOOK FUNCTIONS (implemented in input_hook.c)
// ------------------------------------------------------------
extern int install_input_hook(void);
extern void remove_input_hook(void);

// ------------------------------------------------------------
//  IOCTL DISPATCHER
// ------------------------------------------------------------
static long dispatch_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case OP_READ_MEM: {
            struct CopyMemory cm;
            if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
                return -EFAULT;
            if (read_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) < 0)
                return -EIO;
            break;
        }
        case OP_WRITE_MEM: {
            struct CopyMemory cm;
            if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
                return -EFAULT;
            if (write_process_memory(cm.pid, cm.addr, cm.buffer, cm.size) < 0)
                return -EIO;
            break;
        }
        case OP_MODULE_BASE: {
            struct ModuleBase mb;
            if (copy_from_user(&mb, (void __user *)arg, sizeof(mb)))
                return -EFAULT;
            mb.base = get_module_base(mb.pid, mb.name);
            if (copy_to_user((void __user *)arg, &mb, sizeof(mb)))
                return -EFAULT;
            break;
        }
        // ------------------------------------------------------------
        //  NEW: Handle Aimbot delta from userspace
        // ------------------------------------------------------------
        case OP_AIM_MOVE: {
            struct AimDelta delta;
            if (copy_from_user(&delta, (void __user *)arg, sizeof(delta)))
                return -EFAULT;

            spin_lock(&g_aim_lock);
            g_aim_dx += delta.dx;
            g_aim_dy += delta.dy;
            spin_unlock(&g_aim_lock);
            break;
        }
        default:
            return -ENOTTY;
    }
    return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = dispatch_ioctl,
    .open = NULL,
    .release = NULL,
};

// ------------------------------------------------------------
//  INIT / EXIT
// ------------------------------------------------------------
static int __init memkernel_init(void) {
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0) {
        pr_err("Failed to register device\n");
        return major_num;
    }

    device_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(device_class)) {
        unregister_chrdev(major_num, DEVICE_NAME);
        return PTR_ERR(device_class);
    }

    device_obj = device_create(device_class, NULL, MKDEV(major_num, 0), NULL, DEVICE_NAME);
    if (IS_ERR(device_obj)) {
        class_destroy(device_class);
        unregister_chrdev(major_num, DEVICE_NAME);
        return PTR_ERR(device_obj);
    }

    // ------------------------------------------------------------
    //  INSTALL THE input_event KPROBE HOOK
    // ------------------------------------------------------------
    install_input_hook();

    pr_info("miprotect: Driver loaded successfully (major %d)\n", major_num);
    return 0;
}

static void __exit memkernel_exit(void) {
    // ------------------------------------------------------------
    //  REMOVE THE HOOK FIRST (to avoid use-after-free)
    // ------------------------------------------------------------
    remove_input_hook();

    device_destroy(device_class, MKDEV(major_num, 0));
    class_destroy(device_class);
    unregister_chrdev(major_num, DEVICE_NAME);
    pr_info("miprotect: Driver unloaded\n");
}

module_init(memkernel_init);
module_exit(memkernel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("memoryK");
MODULE_DESCRIPTION("Kernel driver with memory access + touch injection (Kernel 5.10)");
MODULE_VERSION("2.0");
