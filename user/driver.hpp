#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

#define DEVICE_NAME "/dev/miprotect"

class MemKDriver {
private:
	int fd;
	pid_t pid;

	struct CopyMemory
	{
		pid_t pid;
		uintptr_t addr;
		void *buffer;
		size_t size;
	};

	struct ModuleBase
	{
		pid_t pid;
		char *name;
		uintptr_t base;
	};

	struct TouchCommand
	{
		int32_t action;
		int32_t slot;
		int32_t x;
		int32_t y;
	};

	enum TouchAction {
		TOUCH_ACTION_DOWN = 0,
		TOUCH_ACTION_MOVE = 1,
		TOUCH_ACTION_UP = 2,
		TOUCH_ACTION_CANCEL = 3,
	};

	enum Operations {
		OP_READ_MEM = 0x801,
		OP_WRITE_MEM = 0x802,
		OP_MODULE_BASE = 0x803,
		OP_TOUCH_EVENT = 0x804,
	};

	inline bool send_touch(const TouchAction action, const int slot,
			       const int x, const int y) const {
		if (fd == -1) return false;

		TouchCommand command = {
			static_cast<int32_t>(action), slot, x, y
		};
		return ioctl(fd, OP_TOUCH_EVENT, &command) == 0;
	}

public:
	MemKDriver(const char* deviceName) : fd(open(deviceName, O_RDWR)) {
		if (fd == -1) {
			perror("[-] Failed to open driver");
		}
	}

	~MemKDriver() {
		if (fd > 0) {
			close(fd);
		}
	}

	inline void initialize(const pid_t new_pid) {
		pid = new_pid;
	}

	ssize_t read(const uintptr_t addr, void *buffer, const size_t size) const {
		if (!buffer || fd == -1) return -1;

		CopyMemory cm = { pid, addr, buffer, size };
		return ioctl(fd, OP_READ_MEM, &cm);
	}

	ssize_t write(const uintptr_t addr, const void *buffer, const size_t size) const {
		if (!buffer || fd == -1) return -1;

		CopyMemory cm = { pid, addr, const_cast<void *>(buffer), size };
		return ioctl(fd, OP_WRITE_MEM, &cm);
	}

	template <typename T>
	inline T read(const uintptr_t addr) const {
		T result{};
		read(addr, &result, sizeof(T));
		return result;
	}

	template <typename T>
	inline bool write(const uintptr_t addr, const T &value) const {
		return write(addr, &value, sizeof(T)) == sizeof(T);
	}

	inline uintptr_t get_module_base(const char *name) const {
		if (!name || fd == -1) return 0;

		ModuleBase mb = { pid, nullptr, 0 };
		mb.name = const_cast<char *>(name);

		return (ioctl(fd, OP_MODULE_BASE, &mb) == 0) ? mb.base : 0;
	}

	inline bool touch_down(const int x, const int y,
			       const int slot = 0) const {
		return send_touch(TOUCH_ACTION_DOWN, slot, x, y);
	}

	inline bool touch_move(const int x, const int y,
			       const int slot = 0) const {
		return send_touch(TOUCH_ACTION_MOVE, slot, x, y);
	}

	inline bool touch_up(const int slot = 0) const {
		return send_touch(TOUCH_ACTION_UP, slot, 0, 0);
	}

	inline bool touch_cancel() const {
		return send_touch(TOUCH_ACTION_CANCEL, 0, 0, 0);
	}

	inline bool touch_tap(const int x, const int y,
			      const useconds_t hold_us = 30000,
			      const int slot = 0) const {
		if (!touch_down(x, y, slot)) return false;
		usleep(hold_us);
		return touch_up(slot);
	}
};

static MemKDriver *driver = new MemKDriver(DEVICE_NAME);
