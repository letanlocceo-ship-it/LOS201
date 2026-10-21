# Embedded Linux LAB01

Embedded Linux LAB01 running Linux 5.15 on an ARM Cortex-A9 platform using QEMU.

## Group Members

- Lê Tiến Thành
- Lê Tấn Lộc
- Nguyễn Quang Huy Đạt
- Trần Trương Tuấn Vũ
- Trần Đình Khôi Nguyên
## Environment

- Host OS: Ubuntu Linux
- Target architecture: ARMv7
- QEMU machine: vexpress-a9
- CPU: Cortex-A9
- Linux Kernel: 5.15
- BusyBox: 1.35.0
- U-Boot: 2022.04
- Cross compiler: arm-linux-gnueabihf-
- GCC: 13.3

## Repository Structure

- configs/: Kernel, BusyBox and U-Boot configuration files
- output/: Final boot artifacts
- patches/: Linux 5.15 ARMv7 build patch
- rootfs/initramfs/: Final BusyBox root filesystem

## Build Outputs

- output/zImage
- output/vexpress-v2p-ca9.dtb
- output/initramfs.cpio.gz
- output/u-boot

## Root Filesystem

The root filesystem is built using BusyBox 1.35.0 compiled statically for ARM.

During boot, /proc, /sys and /tmp are mounted automatically.

Boot message:

Boot completed! Welcome to Embedded Linux Lab 1 - Group members: Lê Tiến Thành / Lê Tấn Lộc / Nguyễn Quang Huy Đạt / Trần Vũ

## Run with QEMU

Run from the embedded_lab1 directory:

qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic -kernel output/zImage -dtb output/vexpress-v2p-ca9.dtb -initrd output/initramfs.cpio.gz -append "console=ttyAMA0,115200 rdinit=/sbin/init mem=512M"

After boot, test with:

uname -a
ls /
hostname

## Configuration Files

- configs/kernel.config
- configs/busybox.config
- configs/uboot.config

## Linux 5.15 ARMv7 Patch

The GCC 13.3 ARMv7 compatibility adjustment is stored in:

patches/linux-5.15-gcc13-armv7.patch

## Result

Linux 5.15 successfully boots on QEMU VExpress Cortex-A9 using the custom kernel, Device Tree and BusyBox initramfs.
