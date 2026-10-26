# 06 – Troubleshooting

**English** | [Tiếng Việt](vi/06-loi-thuong-gap.md) · [← Home](../README.md)

Every error below **actually happened** while doing the labs. Search by symptom.

**Reading `make` errors:** find the **first** line containing `error:` or `***` – later lines
are usually consequences. `file.c:38:5: error:` = file, line 38, column 5.

---

## 1. Build errors (`make`)

| Symptom | Cause | Fix |
|---|---|---|
| `KDIR…: No such file or directory. Stop.` | `KDIR` in the Makefile is wrong | `KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15.221`. Use `$(HOME)`, not `~` |
| `No rule to make target 'X.o'` | Name in `obj-m` does not match the `.c` file | `grep obj-m Makefile` – must match the file name **exactly** |
| `missing separator. Stop.` / `No targets` | Recipe line indented with spaces instead of a **TAB** | `cat -A Makefile` – recipe lines must start with `^I`. Recreate with `printf '\t…'` |
| `error: 'ret' undeclared` | Variable used before declaration | Add `ssize_t ret;` at the **top of the function body** |
| `label 'out' used but not defined` | `goto out` without the label | Add `out:` before the final `mutex_unlock` |
| `'x' defined but not used` (warning) | Declared but never used | Use it or remove it |
| `implicit declaration of function 'toupper'` | Missing header | Add `#include <linux/ctype.h>` (headers in [05](05-kernel-api.md)) |
| `too many arguments to function 'class_create'` | Code written for kernel ≥ 6.4 | Kernel 5.15 uses `class_create(THIS_MODULE, name)` |
| `make` prints nothing, no `.ko` | `make 2>&1 \| grep …` hid the real error | Run plain `make` to see everything |

---

## 2. Packaging / boot errors

| Symptom | Cause | Fix |
|---|---|---|
| In QEMU: `xxx: not found` although it was built | **initramfs not repacked** | Always use `dev.sh`; check its "Kiem tra trong goi" list before boot |
| Driver log in QEMU still shows old messages | Ran `qemu-system-arm` directly → old package booted | Use only `dev.sh` after editing code |
| `asgn1_app: not found` although it is in `/usr/bin` | Binary linked **dynamically** – rootfs has no libraries | Build with `-static`; `file app` must say `statically linked` |
| `dev.sh` finishes without printing anything | `dev.sh` was truncated to 0 bytes | `wc -c tools/dev.sh`; restore with `git checkout -- tools/dev.sh` |
| `LOI: khong thay Makefile` | `dev.sh` run from the wrong directory | `cd` into the driver directory with the Makefile |
| `Warning: unable to open an initial console` | Rootfs has no `/dev/console` | `sudo mknod -m 600 $R/dev/console c 5 1`, then repack |

Trace **source → `.ko` → initramfs** – the first stage missing your new string is where it broke:

```bash
grep -n 'new_string' mydrv.c
strings mydrv.ko | grep 'new_string'
zcat ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  | cpio -i --to-stdout lib/modules/5.15.221/mydrv.ko 2>/dev/null | strings | grep 'new_string'
```

---

## 3. Load / runtime errors

| Symptom | Cause | Fix |
|---|---|---|
| `insmod: … File exists` | Module already loaded (often by `rcS`) | `rmmod name` first, or `grep -q '^name ' /proc/modules \|\| insmod …` |
| `insmod: … Invalid module format` | vermagic mismatch – built against another kernel | `modinfo x.ko \| grep vermagic` must be `5.15.221 SMP mod_unload ARMv7 p2v8`; check `KDIR` |
| `insmod: … Device or resource busy` | Major already taken by another driver | `cat /proc/devices`, pick another (241–254) |
| `cat /dev/x` never stops | `read` does not update `*offset` | Add `*offset += len;` |
| Second operation on the device **hangs** | A `return` path skipped `mutex_unlock` | Rewrite with the `goto out` pattern |
| Long write: buffer keeps only the **tail** | `write` returned less than `len` → shell retried and overwrote | `write` always returns `len` (bytes the user sent); truncate internally |
| Short write after a long one shows old garbage | `'\0'` placed at the **old** length (`buf[buffer_len]` before updating it) | Terminate at the **new** length |
| `ioctl: Inappropriate ioctl for device` | `.unlocked_ioctl` not set, or headers differ between sides | Check `fops`; rebuild **both** module and app |
| Struct returned by ioctl contains garbage | `sizeof` in `_IOR` differs from the real struct | One shared header for both sides |
| `echo 1 > /sys/.../attr` loops / fails | `_store` does not return `count` | `return count;` |

---

## 4. Terminal / shell errors

| Symptom | Cause | Fix |
|---|---|---|
| Dozens of `syntax error near unexpected token` | **C code pasted into the terminal** instead of the editor | C code goes into the editor; the terminal only gets commands |
| Strange files appear: `-append`, `-kernel`, `EOF`, `echo` | A multi-line paste broke | Remove with `rm -- -append …` (`--` stops `-` names being read as options). Check `git status` before committing |
| `^[[200~` when pasting | Bracketed paste | `bind 'set enable-bracketed-paste off'` |
| `sudo: Authentication failed` although typed correctly | Vietnamese input method on / Caps Lock | Switch to an English layout, type (don't paste) |
| `lsmod` lists hundreds of modules (nvidia, i915…) | You are on the **laptop**, not QEMU | Check the prompt: QEMU is `/ # ` |
| heredoc + `sudo` swallows commands | Pasting a large block with `sudo` inside | Run `sudo -v` first, then one command at a time |

---

## 5. Git errors

| Symptom | Cause | Fix |
|---|---|---|
| GitHub rejects the push (file > 100 MB) | Committed a `.tar.gz` / kernel source | `git reset --soft origin/main`, add to `.gitignore`, commit again |
| Commit contains `.ko`, `.o`, binaries | Used `git add -A` without checking | Always `git status --short` first; add specific paths |
| `deleted: …` in `git status` after removing files | The files were committed before | `git rm -- <file>` then commit |

---

## 6. ⚠️ Dangerous – MTD on the laptop

The laptop already has a real MTD device: **the BIOS flash chip** (`/proc/mtd` → `mtd0: "BIOS"`).
Writing/erasing it by mistake can leave the machine unbootable.

**Never hard-code `mtd0`.** Always look up the simulated NAND:

```bash
MTD=$(grep 'NAND simulator' /proc/mtd | head -1 | cut -d: -f1)
echo "$MTD"     # must be mtd1 (or anything but mtd0). Empty → STOP
```

---

**Next:** [07 – Viva Q&A](07-viva-qa.md)
