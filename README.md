# SuDu
A utility that allows to dump correctly all mtdX partitions from Google Sooner prototypes.

# Prerequisites
Download a source code of SuDu first.
## Host System
Ubuntu Linux (tested on Ubuntu with gcc 4.6.3)

## Toolchain/Other
The device uses OABI (Old ABI) syscall convention (<code>__NR_SYSCALL_BASE = 0x900000</code>). Standard glibc-based toolchains will not work due to incompatibility with Linux 2.6.22. The dump tool must be compiled without any libc, using raw syscalls.

## Required compiler:
arm-linux-gnueabi-gcc (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3

## Install on Ubuntu:
<pre>
sudo apt install gcc-arm-linux-gnueabi binutils-arm-linux-gnueabi
</pre>
Linux Kernel Headers:<br>
Required for correct MTD ioctl values. Must use headers from Linux 2.6.22:
<pre>
wget https://cdn.kernel.org/pub/linux/kernel/v2.6/linux-2.6.22.tar.bz2
tar -xf linux-2.6.22.tar.bz2
cd linux-2.6.22
make ARCH=arm include/linux/version.h
</pre>
# Compilation process
Use the following command to compile this program:
<pre>
arm-linux-gnueabi-gcc \
  -march=armv5te \
  -mtune=arm926ej-s \
  -msoft-float \
  -marm \
  -nostdlib \
  -nostartfiles \
  -static \
  -fno-builtin \
  -o sudu sudu.c
</pre>
# Notes
The device runs Linux 2.6.22-omap1 compiled with gcc 4.1.1. Modern glibc (2.15+) uses <code>kernel_helpers</code> that do not exist in this kernel version, causing <code>illegal instruction</code> or hang on startup even with static binaries. So solution is to compile without any libc (<code>-nostdlib -nostartfiles</code>) and use raw OABI syscalls directly.
