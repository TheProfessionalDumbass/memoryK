# MemKernel
MemKernel is a kernel driver for Android. Originally created by [Jiang-Night](https://github.com/Jiang-Night/Kernel_driver_hack), it has been modified and fixed according to my personal needs. This driver reads and writes physical memory of the target process, effectively bypassing anti-cheats.

## Integration
2 ways you can integrate this driver to your kernel source (for compilation) using setup script:
* __Y__ : To build the driver as part of the kernel. (statically build within kernel).
```
curl -LSs "https://raw.githubusercontent.com/Poko-Apps/MemKernel/main/kernel/setup.sh" | bash -s Y
```
* __M__ : To build the driver as lkm (loadable kernel module).
```
curl -LSs "https://raw.githubusercontent.com/Poko-Apps/MemKernel/main/kernel/setup.sh" | bash -s M
```

**TIP** : By default the setup script generates random name for the driver (/dev/*randomname*) and as well as for the lkm (*randomname*_memk.ko), this is done to bypass existency check done via [*access(2)*](https://man7.org/linux/man-pages/man2/access.2.html) syscall. But you can override this behaviour by providing 2nd argument to the setup script like this:

```curl -LSs "https://raw.githubusercontent.com/Poko-Apps/MemKernel/main/kernel/setup.sh" | bash -s M myname```
