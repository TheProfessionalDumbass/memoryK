# memoryK Features

`memoryK` is an Android/Linux kernel module and C++ client interface for
accessing another process's memory, finding mapped module addresses, and
injecting gestures through a virtual multi-touch input device.

The kernel module registers `/dev/miprotect`. User-space clients communicate
with it through IOCTL requests defined in `kernel/comm.h` and mirrored by
`user/driver.hpp`.

## Feature summary

| Area | Feature | Status |
| --- | --- | --- |
| Process memory | Read memory from a selected process | Existing |
| Process memory | Write memory to a selected process | Existing |
| Process inspection | Find the base address of a mapped library or executable | Existing |
| Client API | Typed and raw C++ read/write helpers | Existing |
| Touch input | Virtual Type-B multi-touchscreen | New |
| Touch input | Finger down, move, up, and cancel commands | New |
| Touch input | Tap, drag, and concurrent multi-finger gestures | New |
| Automation | Build the LKM when kernel sources change | Updated |

## Device and communication interface

The module creates the miscellaneous device:

```text
/dev/miprotect
```

Only one client can hold the device open at a time. A second open attempt
returns `-EBUSY` until the current client closes its file descriptor.

The supported IOCTL operation IDs are:

| Operation | Value | Payload | Purpose |
| --- | ---: | --- | --- |
| `OP_READ_MEM` | `0x801` | `CopyMemory` | Copy target-process memory to the client |
| `OP_WRITE_MEM` | `0x802` | `CopyMemory` | Copy client data into target-process memory |
| `OP_MODULE_BASE` | `0x803` | `ModuleBase` | Resolve a mapped module's base address |
| `OP_TOUCH_EVENT` | `0x804` | `TouchCommand` | Submit one virtual-touch update |
| `OP_TOUCH_BOUNDS` | `0x805` | `TouchBounds` | Set current display width and height in pixels |

## Process memory access

### Read process memory

`OP_READ_MEM` accepts a target PID, virtual address, destination buffer, and
byte count. The driver walks the target process's page tables, maps the
corresponding physical pages, and copies data back to user space.

Reads may span multiple pages. The implementation splits the request at page
boundaries and returns the number of bytes copied. It returns `-1` if no data
could be copied.

Raw client usage:

```cpp
std::uint8_t buffer[64]{};
ssize_t copied = driver->read(address, buffer, sizeof(buffer));
```

Typed client usage:

```cpp
std::uint64_t value = driver->read<std::uint64_t>(address);
```

### Write process memory

`OP_WRITE_MEM` uses the same page-aware translation and chunking logic, but
copies bytes from the client into the target process.

Raw client usage:

```cpp
std::uint8_t replacement[] = {0x01, 0x02, 0x03, 0x04};
ssize_t copied = driver->write(address, replacement, sizeof(replacement));
```

Typed client usage:

```cpp
std::uint32_t replacement = 1234;
bool written = driver->write(address, replacement);
```

Before using memory or module operations, select the target PID:

```cpp
driver->initialize(target_pid);
```

## Module-base lookup

`OP_MODULE_BASE` scans the target process's virtual-memory areas and compares
the basename of each file-backed mapping with the requested name. It returns
the starting address of the first matching mapping, or `0` when no match is
found.

```cpp
driver->initialize(target_pid);
std::uintptr_t base = driver->get_module_base("libunity.so");
```

The kernel copies at most 255 characters from the supplied module name.

## Virtual multi-touch input

The new input system registers a separate Linux input device named:

```text
miprotect-virtual-touch
```

It uses the Linux Type-B multi-touch protocol instead of intercepting or
modifying the physical touchscreen's `input_event()` path. As a result,
injected contacts can be controlled even when the physical touchscreen is
idle.

### Capabilities

- Up to 10 independently controlled touch slots (`0` through `9`).
- `DOWN`, `MOVE`, `UP`, and global `CANCEL` actions.
- Concurrent contacts for multi-finger gestures.
- `BTN_TOUCH` state automatically follows the number of active contacts.
- Input operations are serialized with a mutex.
- All active contacts are released when the module unloads.

### Touch commands

| Action | Required fields | Behavior |
| --- | --- | --- |
| `TOUCH_ACTION_DOWN` | `slot`, `x`, `y` | Starts a contact in an unused slot |
| `TOUCH_ACTION_MOVE` | `slot`, `x`, `y` | Moves an active contact |
| `TOUCH_ACTION_UP` | `slot` | Releases an active contact |
| `TOUCH_ACTION_CANCEL` | None | Releases every active contact |

Each successful command emits a synchronized input frame. Invalid state
transitions are rejected; for example, moving an inactive slot or pressing an
already-active slot does not silently alter the gesture.

### Display bounds

The virtual device advertises a fixed normalized `0..65535` axis to Android.
The driver converts caller-supplied pixel coordinates using the current display
bounds, so a resolution change does not require recompiling the `.ko`.
The initial bounds remain 1080 by 2400 pixels for older clients and can still
be supplied at module load:

```sh
insmod miprotect.ko touch_width=1440 touch_height=3200
```

New clients should set the actual dimensions after opening the device, and
again after rotation or a display-size change:

```cpp
driver->set_touch_bounds(display_width, display_height);
```

Changing bounds cancels active virtual contacts. Valid dimensions are 2..16384
pixels per axis; `DOWN` and `MOVE` outside those bounds return `-ERANGE`.
A user-space app must obtain the dimensions of its target display; the kernel
module cannot infer every device's display orientation reliably.

### Tap or click

The client provides `touch_tap()`, which presses a slot, waits for the supplied
hold duration, and releases it. The default hold is 30,000 microseconds.

```cpp
bool ok = driver->touch_tap(540, 1200);
```

A different duration and slot can be supplied:

```cpp
bool ok = driver->touch_tap(540, 1200, 50000, 2);
```

### Drag or swipe

A drag is a `DOWN`, one or more `MOVE` calls, and an `UP` using the same slot.
The client decides the path, timing, and number of intermediate points.

```cpp
if (driver->touch_down(200, 1600, 0)) {
    usleep(16000);
    driver->touch_move(350, 1400, 0);
    usleep(16000);
    driver->touch_move(500, 1200, 0);
    usleep(16000);
    driver->touch_up(0);
}
```

### Multi-finger gesture

Different slots may remain active at the same time. For example, a two-finger
gesture can be constructed as follows:

```cpp
driver->touch_down(400, 1200, 0);
driver->touch_down(680, 1200, 1);

driver->touch_move(300, 1200, 0);
driver->touch_move(780, 1200, 1);

driver->touch_up(0);
driver->touch_up(1);
```

If a client aborts a gesture or loses track of its active slots, it can release
all contacts at once:

```cpp
driver->touch_cancel();
```

### Touch error behavior

| Condition | Kernel result |
| --- | --- |
| Touch device is unavailable | `-ENODEV` |
| Slot is outside `0..9` | `-EINVAL` |
| `DOWN` or `MOVE` is outside the configured bounds | `-ERANGE` |
| `DOWN` targets an already-active slot | `-EALREADY` |
| `MOVE` or `UP` targets an inactive slot | `-EINVAL` |
| Action value is unknown | `-EINVAL` |

The current C++ convenience methods return `false` for any failed touch IOCTL.
Applications that need the exact kernel error can call `ioctl()` directly and
inspect `errno`.

## Module lifecycle

When the module loads, it:

1. Registers `/dev/miprotect`.
2. Allocates and configures the virtual touchscreen.
3. Registers the touchscreen with the Linux input subsystem.

If touch initialization fails, device registration is rolled back. During
unload, the driver releases active contacts, unregisters the virtual input
device, and removes `/dev/miprotect`.

## Kernel compatibility

The code includes compatibility paths for memory-map locking before and after
Linux 5.8, and for VMA iteration before and after Linux 6.1. The GKI workflow pins one dated Google release tag and common-kernel commit
per Android/LTS branch. It runs the eight selected builds with `max-parallel: 1`
and leaves kernel symbol/KMI checks enabled. A `.ko` is **not universal across
kernels or devices**: matching KMI, configuration, vermagic, exported symbols,
and any OEM restrictions must be verified on each target. A failed branch must
not be treated as a usable artifact.

## Build automation

The `Build LKM` workflow runs when:

- a commit touching `kernel/**` is pushed to `main`;
- a pull request targeting `main` changes `kernel/**`;
- the workflow file itself changes; or
- it is started manually with `workflow_dispatch`.

Each successful branch uploads a separately named `.ko`, its SHA-256,
vermagic, and the exact release tag/commit. Builds are queued one at a time.

## Repository layout

| Path | Responsibility |
| --- | --- |
| `kernel/comm.h` | Shared request structures, touch actions, and IOCTL IDs |
| `kernel/entry.c` | Device registration, IOCTL dispatch, and module lifecycle |
| `kernel/memory.c` | Page-table translation and process memory copying |
| `kernel/process.c` | Mapped-module base lookup |
| `kernel/input_hook.c` | Virtual Type-B multi-touch implementation |
| `kernel/input_hook.h` | Touch subsystem interface |
| `kernel/Makefile` | LKM objects and external-module build entry point |
| `kernel/Kconfig` | `CONFIG_MEMKERNEL` configuration |
| `user/driver.hpp` | C++ client wrapper for memory, module, and touch operations |
| `user/main.cpp` | Minimal PID, module-base, and memory-read example |

## Current scope and limitations

- The driver interface is intentionally single-client.
- The client must have permission to open `/dev/miprotect`.
- The caller is responsible for obtaining and selecting a valid target PID.
- Module lookup matches the basename of a file-backed mapping exactly.
- Touch motion interpolation and gesture timing are controlled by the client;
  the kernel processes individual touch updates rather than generating a path.
- The built-in `user/main.cpp` sample demonstrates module lookup and memory
  reading. Touch examples are provided in this document and through the helper
  methods in `user/driver.hpp`.
