ifneq ($(KERNELRELEASE),)
ifneq ($(KBUILD_EXTMOD),)
obj-m := miprotect.o
else
obj-$(CONFIG_MEMKERNEL) := miprotect.o
endif
miprotect-y := memory.o process.o entry.o input_hook.o
else
KERNEL_SRC ?= $(KDIR)
MODULE_DIR := $(if $(M),$(M),$(CURDIR))

# External LKMs remain unsigned until signed by a device/OEM key. Keep KMI and
# modpost export checks intact; only skip the unavailable CI signing key.
.PHONY: all modules modules_install clean
all modules modules_install:
	@test -n "$(KERNEL_SRC)" || { echo "KERNEL_SRC or KDIR is required" >&2; exit 1; }
	$(MAKE) -C "$(KERNEL_SRC)" M="$(MODULE_DIR)" $(if $(filter modules_install,$@),CONFIG_MODULE_SIG_ALL=n) $(if $(filter all,$@),modules,$@)

clean:
	@test -n "$(KERNEL_SRC)" || { echo "KERNEL_SRC or KDIR is required" >&2; exit 1; }
	$(MAKE) -C "$(KERNEL_SRC)" M="$(MODULE_DIR)" clean
endif

