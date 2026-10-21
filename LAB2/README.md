# LAB02 – Character Device Driver + procfs/sysfs + JFFS2 + BusyBox init

**Môi trường:** Ubuntu 26.04, GCC 15.2 (arm-linux-gnueabihf), QEMU vexpress-a9
**Kernel:** linux-5.15.221 (config: `../LAB1/embedded_lab1/configs/kernel.config`)

## Build lại
```bash
cd embedded_lab2/driver && make          # cần kernel đã build ở ../LAB1
../dev.sh                                # copy + đóng gói + boot QEMU
```

## Kết quả
- `logs/driver_test.txt`  – Phần A: read/write qua /dev/lab2
- `logs/procfs_test.txt`  – Phần B: /proc/lab2_info, /sys/class/lab2_class/lab2/
- `logs/mtd_jffs2.txt`    – Phần C: nandsim + JFFS2 persistence
- `logs/boot_log.txt`     – Phần D: auto-load module qua rcS
