// SPDX-License-Identifier: GPL-2.0
/*
 * counter_driver.c - ASSIGNMENT-01 Task 2: misc device counter
 * Author: Pham Minh Tung - SE204386
 *
 * /dev/counter (misc device, major 10, dynamic minor)
 *   read  : returns the current value as "<value>\n", then the counter
 *           is incremented by 1 (one increment per read sequence, i.e. per
 *           "cat", no matter how many read() calls the reader makes)
 *   write : sets the counter to a non-negative decimal integer;
 *           negative or non-numeric input is rejected with -EINVAL
 * /proc/counter_info
 *   current value, total number of reads, total number of writes
 *
 * Concurrency: every shared value is an atomic_t, so no mutex is needed.
 * The per-open snapshot lives in file->private_data and is private to
 * the process that opened the file.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pham Minh Tung SE204386");
MODULE_DESCRIPTION("ASSIGNMENT-01 Task 2: misc device counter");
MODULE_VERSION("1.0");

#define DEVICE_NAME    "counter"
#define PROC_NAME      "counter_info"
#define VALUE_BUF_LEN  16   /* "-2147483648\n" is 12 chars + NUL */
#define WRITE_BUF_LEN  16   /* longest accepted input incl. '\n'   */

/* ================= data ================= */

static atomic_t counter      = ATOMIC_INIT(0);
static atomic_t total_reads  = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);

/*
 * Per-open state. The value is formatted once, when a read sequence
 * starts (*ppos == 0), and later read() calls of the same sequence are
 * served from this snapshot. Without it, a reader using a tiny buffer
 * (e.g. dd bs=1) would see a value that changes between calls.
 */
struct counter_file {
	char   text[VALUE_BUF_LEN];
	size_t len;
};

/* ================= file_operations ================= */

/*
 * counter_open - allocate the per-open snapshot
 * Return: 0 on success, -ENOMEM if allocation fails
 */
static int counter_open(struct inode *inode, struct file *file)
{
	struct counter_file *cf;

	cf = kzalloc(sizeof(*cf), GFP_KERNEL);
	if (!cf)
		return -ENOMEM;
	file->private_data = cf;
	return 0;
}

/*
 * counter_release - free the per-open snapshot
 * Return: 0
 */
static int counter_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

/*
 * counter_read - return "<value>\n" and increment the counter
 * @file: open file (private_data holds the snapshot)
 * @buf:  user buffer
 * @len:  size of the user buffer
 * @ppos: position inside the snapshot text
 *
 * At *ppos == 0 the current value is fetched and incremented in ONE
 * atomic operation (atomic_fetch_inc), so two concurrent readers can
 * never receive the same value.
 *
 * Return: bytes copied, 0 at end of the snapshot (EOF), -EFAULT on error
 */
static ssize_t counter_read(struct file *file, char __user *buf,
			    size_t len, loff_t *ppos)
{
	struct counter_file *cf = file->private_data;
	size_t avail;

	if (*ppos == 0) {
		int val = atomic_fetch_inc(&counter);

		cf->len = scnprintf(cf->text, sizeof(cf->text), "%d\n", val);
		atomic_inc(&total_reads);
		pr_info("counter: read value %d\n", val);
	}

	if (*ppos >= cf->len)
		return 0;                       /* whole snapshot delivered */

	avail = cf->len - *ppos;
	if (len > avail)
		len = avail;

	if (copy_to_user(buf, cf->text + *ppos, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

/*
 * counter_write - set the counter from a decimal string
 * @file: open file
 * @buf:  user data, e.g. "100\n" from echo
 * @len:  number of bytes written by the user
 * @ppos: unused
 *
 * Return: @len on success, -EINVAL for empty/too long/non-numeric/
 *         negative input, -EFAULT if the copy fails
 */
static ssize_t counter_write(struct file *file, const char __user *buf,
			     size_t len, loff_t *ppos)
{
	char kbuf[WRITE_BUF_LEN];
	int val;

	if (len == 0 || len >= sizeof(kbuf)) {
		pr_warn("counter: rejected write of %zu bytes\n", len);
		return -EINVAL;
	}

	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	/* kstrtoint accepts one trailing '\n' (added by echo) */
	if (kstrtoint(kbuf, 10, &val) || val < 0) {
		pr_warn("counter: rejected invalid value\n");
		return -EINVAL;
	}

	atomic_set(&counter, val);
	atomic_inc(&total_writes);
	pr_info("counter: set to %d\n", val);
	return len;
}

static const struct file_operations counter_fops = {
	.owner   = THIS_MODULE,
	.open    = counter_open,
	.release = counter_release,
	.read    = counter_read,
	.write   = counter_write,
};

static struct miscdevice counter_misc = {
	.minor = MISC_DYNAMIC_MINOR,   /* kernel picks a free minor */
	.name  = DEVICE_NAME,          /* -> /dev/counter           */
	.fops  = &counter_fops,
	.mode  = 0666,                 /* readable/writable by all  */
};

/* ================= procfs ================= */

static struct proc_dir_entry *proc_entry;

/*
 * counter_proc_show - generate /proc/counter_info
 * Return: 0
 */
static int counter_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Counter Driver Statistics\n");
	seq_printf(m, "=========================\n");
	seq_printf(m, "Device        : /dev/%s (misc, major 10, minor %d)\n",
		   DEVICE_NAME, counter_misc.minor);
	seq_printf(m, "Current value : %d\n", atomic_read(&counter));
	seq_printf(m, "Total reads   : %d\n", atomic_read(&total_reads));
	seq_printf(m, "Total writes  : %d\n", atomic_read(&total_writes));
	return 0;
}

static int counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
	.proc_open    = counter_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/* ================= init / exit ================= */

static int __init counter_init(void)
{
	int ret;

	ret = misc_register(&counter_misc);
	if (ret) {
		pr_err("counter: misc_register failed: %d\n", ret);
		return ret;
	}

	proc_entry = proc_create(PROC_NAME, 0444, NULL, &counter_proc_ops);
	if (!proc_entry) {
		pr_err("counter: failed to create /proc/%s\n", PROC_NAME);
		misc_deregister(&counter_misc);       /* undo step 1 */
		return -ENOMEM;
	}

	pr_info("counter: loaded, /dev/%s major 10 minor %d\n",
		DEVICE_NAME, counter_misc.minor);
	return 0;
}

static void __exit counter_exit(void)
{
	proc_remove(proc_entry);                     /* reverse order */
	misc_deregister(&counter_misc);
	pr_info("counter: unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);
