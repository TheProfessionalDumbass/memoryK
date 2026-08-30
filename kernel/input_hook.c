// SPDX-License-Identifier: GPL-2.0
/*
 * Virtual Type-B multitouch input device.
 *
 * This uses the input subsystem instead of altering input_event() with a
 * kprobe. It can create complete DOWN/MOVE/UP sequences while the physical
 * touchscreen is idle.
 */

#include <linux/errno.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include "input_hook.h"

#define MEMK_TOUCH_MAX_SLOTS 10
#define MEMK_DEFAULT_WIDTH 1080
#define MEMK_DEFAULT_HEIGHT 2400

static int touch_width = MEMK_DEFAULT_WIDTH;
module_param(touch_width, int, 0444);
MODULE_PARM_DESC(touch_width, "Virtual touchscreen width in pixels");

static int touch_height = MEMK_DEFAULT_HEIGHT;
module_param(touch_height, int, 0444);
MODULE_PARM_DESC(touch_height, "Virtual touchscreen height in pixels");

static DEFINE_MUTEX(touch_lock);
static struct input_dev *touch_device;
static bool active_slots[MEMK_TOUCH_MAX_SLOTS];
static unsigned int active_count;

static bool touch_coordinates_valid(const struct TouchCommand *command)
{
	return command->x >= 0 && command->y >= 0 &&
	       command->x < touch_width && command->y < touch_height;
}

static void touch_report_contact(struct input_dev *device, int slot,
				 bool active, int x, int y)
{
	input_mt_slot(device, slot);
	input_mt_report_slot_state(device, MT_TOOL_FINGER, active);

	if (active) {
		input_report_abs(device, ABS_MT_POSITION_X, x);
		input_report_abs(device, ABS_MT_POSITION_Y, y);
	}
}

static void touch_finish_frame(struct input_dev *device)
{
	input_report_key(device, BTN_TOUCH, active_count != 0);
	input_sync(device);
}

static void touch_cancel_all_locked(void)
{
	int slot;

	if (!touch_device)
		return;

	for (slot = 0; slot < MEMK_TOUCH_MAX_SLOTS; slot++) {
		if (!active_slots[slot])
			continue;

		touch_report_contact(touch_device, slot, false, 0, 0);
		active_slots[slot] = false;
	}

	active_count = 0;
	touch_finish_frame(touch_device);
}

int touch_input_event(const struct TouchCommand *command)
{
	int ret = 0;

	if (!command)
		return -EINVAL;

	if (command->action == TOUCH_ACTION_CANCEL) {
		mutex_lock(&touch_lock);
		if (!touch_device)
			ret = -ENODEV;
		else
			touch_cancel_all_locked();
		mutex_unlock(&touch_lock);
		return ret;
	}

	if (command->slot < 0 || command->slot >= MEMK_TOUCH_MAX_SLOTS)
		return -EINVAL;

	if (command->action != TOUCH_ACTION_UP &&
	    !touch_coordinates_valid(command))
		return -ERANGE;

	mutex_lock(&touch_lock);

	if (!touch_device) {
		ret = -ENODEV;
		goto out;
	}

	switch (command->action) {
	case TOUCH_ACTION_DOWN:
		if (active_slots[command->slot]) {
			ret = -EALREADY;
			break;
		}

		active_slots[command->slot] = true;
		active_count++;
		touch_report_contact(touch_device, command->slot, true,
				     command->x, command->y);
		touch_finish_frame(touch_device);
		break;

	case TOUCH_ACTION_MOVE:
		if (!active_slots[command->slot]) {
			ret = -EINVAL;
			break;
		}

		touch_report_contact(touch_device, command->slot, true,
				     command->x, command->y);
		touch_finish_frame(touch_device);
		break;

	case TOUCH_ACTION_UP:
		if (!active_slots[command->slot]) {
			ret = -EINVAL;
			break;
		}

		touch_report_contact(touch_device, command->slot, false, 0, 0);
		active_slots[command->slot] = false;
		active_count--;
		touch_finish_frame(touch_device);
		break;

	default:
		ret = -EINVAL;
		break;
	}

out:
	mutex_unlock(&touch_lock);
	return ret;
}

int touch_input_init(void)
{
	struct input_dev *device;
	int ret;

	if (touch_width <= 0 || touch_height <= 0)
		return -EINVAL;

	device = input_allocate_device();
	if (!device)
		return -ENOMEM;

	device->name = "miprotect-virtual-touch";
	device->phys = "miprotect/input0";
	device->id.bustype = BUS_VIRTUAL;
	device->id.vendor = 0x0001;
	device->id.product = 0x0001;
	device->id.version = 0x0100;

	input_set_abs_params(device, ABS_MT_POSITION_X, 0,
			     touch_width - 1, 0, 0);
	input_set_abs_params(device, ABS_MT_POSITION_Y, 0,
			     touch_height - 1, 0, 0);

	ret = input_mt_init_slots(device, MEMK_TOUCH_MAX_SLOTS,
				  INPUT_MT_DIRECT);
	if (ret)
		goto free_device;

	ret = input_register_device(device);
	if (ret)
		goto free_device;

	mutex_lock(&touch_lock);
	touch_device = device;
	memset(active_slots, 0, sizeof(active_slots));
	active_count = 0;
	mutex_unlock(&touch_lock);

	pr_info("miprotect: virtual touchscreen registered (%dx%d, %u slots)\n",
		touch_width, touch_height, MEMK_TOUCH_MAX_SLOTS);
	return 0;

free_device:
	input_free_device(device);
	return ret;
}

void touch_input_exit(void)
{
	struct input_dev *device;

	mutex_lock(&touch_lock);
	device = touch_device;
	if (device)
		touch_cancel_all_locked();
	touch_device = NULL;
	mutex_unlock(&touch_lock);

	if (device)
		input_unregister_device(device);
}
