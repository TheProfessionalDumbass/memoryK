ifneq ($(KERNELRELEASE),)
ifneq ($(KBUILD_EXTMOD),)
obj-m := miprotect.o
else
obj-$(CONFIG_MEMKERNEL) := miprotect.o
endif
miprotect-y := memory.o process.o entry.o input_hook.o
else
KERNEL_SRC ?= $(KDIR)
MODULE_DIR := $(abspath $(if $(M),$(M),$(CURDIR)))

.PHONY: all modules clean
all modules:
	@test -n "$(KERNEL_SRC)" || { echo "KERNEL_SRC or KDIR is required" >&2; exit 1; }
	$(MAKE) -C "$(KERNEL_SRC)" M="$(MODULE_DIR)" modules

clean:
	@test -n "$(KERNEL_SRC)" || { echo "KERNEL_SRC or KDIR is required" >&2; exit 1; }
	$(MAKE) -C "$(KERNEL_SRC)" M="$(MODULE_DIR)" clean
endif

