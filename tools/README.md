# tools/

**English** | [Tiếng Việt](README.vi.md) · [← Home](../README.md)

## `dev.sh` – build → copy → repack → boot

Run it from a **directory containing the driver's Makefile**:

```bash
cd ASSIGNMENT1/task1_ioctl
../../tools/dev.sh
# or through the symlink:  ~/embedded_lab2/dev.sh
```

| Step | Action | Details |
|---|---|---|
| — | Sanity check | Stops immediately if there is no `Makefile` in the current directory |
| — | `sudo -v` | Ask for the password once for the whole script |
| [2] | Build | `make` (stops on error thanks to `set -e`) |
| [3] | Copy into rootfs | `*.ko` → `lib/modules/5.15.221/`; `*.sh` → `usr/bin/`; every **static ARM** binary → `usr/bin/` |
| [4] | Repack | `find . \| cpio -H newc -o \| gzip` → `~/embedded_lab2/output/initramfs_lab2.cpio.gz`, then lists the `.ko` files and scripts inside |
| [5] | Boot | `qemu-system-arm -M vexpress-a9 …` (quit: **Ctrl+A, then X**) |

Configuration variables at the top of the file (change them if your environment lives elsewhere):

```bash
R=$HOME/embedded_lab2/rootfs/initramfs
OUT=$HOME/embedded_lab2/output/initramfs_lab2.cpio.gz
KVER=5.15.221
```

Record the whole session as a log:

```bash
script -q -c "$HOME/embedded_lab2/dev.sh" log.txt
```

Install the symlink (once):

```bash
ln -sf "$PWD/tools/dev.sh" ~/embedded_lab2/dev.sh
```
