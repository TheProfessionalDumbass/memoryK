// SPDX-License-Identifier: GPL-2.0
/*
 * Memory operation and virtual-touch driver for Linux/Android.
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "comm.h"
#include "input_hook.h"
#include "memory.h"
#include "process.h"

#define DEVICE_NAME "miprotect"

static DEFINE_MUTEX(driver_mutex);

static int dispatch_open(struct inode *node, struct file *file)
{
	if (!mutex_trylock(&driver_mutex))
		return -EBUSY;

	return 0;
}

static int dispatch_close(struct inode *node, struct file *file)
{
	mutex_unlock(&driver_mutex);
	return 0;
}

static long dispatch_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct CopyMemory cm;
	struct ModuleBase mb;
	struct TouchCommand touch;
	char name[0x100] = { 0 };

	switch (cmd) {
	case OP_READ_MEM:
		if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
			return -EFAULT;

		return readwrite_process_memory(cm.pid, cm.addr, cm.buffer,
					cm.size, false);

	case OP_WRITE_MEM:
		if (copy_from_user(&cm, (void __user *)arg, sizeof(cm)))
			return -EFAULT;

		return readwrite_process_memory(cm.pid, cm.addr, cm.buffer,
					cm.size, true);

	case OP_MODULE_BASE:
		if (copy_from_user(&mb, (void __user *)arg, sizeof(mb)) ||
		    copy_from_user(name, (void __user *)mb.name,
				   sizeof(name) - 1))
			return -EFAULT;

		mb.base = get_module_base(mb.pid, name);
		if (copy_to_user((void __user *)arg, &mb, sizeof(mb)))
			return -EFAULT;

		return 0;

	case OP_TOUCH_EVENT:
		if (copy_from_user(&touch, (void __user *)arg, sizeof(touch)))
			return -EFAULT;

		return touch_input_event(&touch);

	default:
		return -ENOTTY;
	}
}

static const struct file_operations dispatch_functions = {
	.owner = THIS_MODULE,
	.open = dispatch_open,
	.release = dispatch_close,
	.unlocked_ioctl = dispatch_ioctl,
};

static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dispatch_functions,
};

static int __init memkernel_entry(void)
{
	int ret;

	ret = misc_register(&misc);
	if (ret)
		return ret;

	ret = touch_input_init();
	if (ret) {
		misc_deregister(&misc);
		return ret;
	}

	pr_info("miprotect: memory and virtual-touch driver loaded\n");
	return 0;
}

static void __exit memkernel_unload(void)
{
	touch_input_exit();
	misc_deregister(&misc);
	pr_info("miprotect: driver unloaded\n");
}

module_init(memkernel_entry);
module_exit(memkernel_unload);

MODULE_DESCRIPTION("Android memory operation and virtual-touch driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("memoryK contributors");
MODULE_VERSION("2.0");
