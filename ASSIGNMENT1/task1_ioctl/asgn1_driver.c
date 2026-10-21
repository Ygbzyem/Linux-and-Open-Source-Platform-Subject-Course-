#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pham Minh Tung SE204386 ");
MODULE_DESCRIPTION("Assignment_01: How to write a device driver ");
MODULE_VERSION("2.0");

#define DEVICE_NAME  "asgn1"
#define CLASS_NAME   "asgn1_class"
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 245

static int    major_number = DRIVER_MAJOR;
static char   device_buffer[BUFFER_SIZE];
static int    buffer_len = 0;
static int    open_count = 0;
static int    write_count = 0;
static int    read_count = 0;
static int    last_write_size = 0;
static struct class  *asgn1_class  = NULL;
static struct device *asgn1_device = NULL;
static DEFINE_MUTEX(asgn1_mutex);

/* ================= PART A: file_operations ================= */

static int asgn1_open(struct inode *inode, struct file *file) {
	
	int cnt;

    mutex_lock(&asgn1_mutex);
    open_count++;
    cnt = open_count;
    mutex_unlock(&asgn1_mutex);
    pr_info("asgn1_driver: device opened (count=%d)\n", cnt);
    return 0;
}

static int asgn1_release(struct inode *inode, struct file *file) {
    pr_info("asgn1_driver: device closed\n");
    return 0;
}

static ssize_t asgn1_read(struct file *file, char __user *buf,
                          size_t len, loff_t *offset)
{
    ssize_t ret;

    mutex_lock(&asgn1_mutex);

    if (*offset >= buffer_len) {          /* hết dữ liệu → EOF */
        ret = 0;
        goto out;
    }
    if (len > buffer_len - *offset)       /* cắt cho vừa phần còn lại */
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        pr_err("asgn1_driver: copy_to_user failed\n");
        ret = -EFAULT;
        goto out;
    }

    *offset += len;                       /* lần gọi sau đọc tiếp từ đây */
    read_count++;
    ret = len;
    pr_info("asgn1_driver: sent %zu bytes (read #%d)\n", len, read_count);

out:
    mutex_unlock(&asgn1_mutex);           /* lối ra DUY NHẤT */
    return ret;
}

static ssize_t asgn1_write(struct file *file, const char __user *buf,
                           size_t len, loff_t *offset)
{
    ssize_t ret;

    if (len > BUFFER_SIZE - 1) {          /* chừa 1 byte cho '\0' */
        pr_warn("asgn1_driver: write too large, truncating\n");
        len = BUFFER_SIZE - 1;
    }

    mutex_lock(&asgn1_mutex);

    if (copy_from_user(device_buffer, buf, len)) {
        pr_err("asgn1_driver: copy_from_user failed\n");
        ret = -EFAULT;
        goto out;                         /* lỗi: không đếm, không đổi len */
    }

    device_buffer[len] = '\0';            /* kết thúc theo độ dài MỚI */
    buffer_len      = len;
    write_count++;
    last_write_size = len;
    ret = len;
    pr_info("asgn1_driver: received %zu bytes (write #%d)\n", len, write_count);

out:
    mutex_unlock(&asgn1_mutex);
    return ret;
}

static struct file_operations asgn1_fops = {
    .owner   = THIS_MODULE,
    .open    = asgn1_open,
    .release = asgn1_release,
    .read    = asgn1_read,
    .write   = asgn1_write,
};

/* ================= PART B.1: procfs ================= */

static struct proc_dir_entry *proc_entry;

static int asgn1_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&asgn1_mutex);             /* in một "ảnh chụp" nhất quán */

    seq_printf(m, "ASSIGNMENT-01 Driver Statistics\n");
    seq_printf(m, "===============================\n");
    seq_printf(m, "Driver name     : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number    : %d\n", major_number);
    seq_printf(m, "Buffer size     : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length     : %d bytes\n", buffer_len);
    seq_printf(m, "Open count      : %d\n", open_count);
    seq_printf(m, "Write count     : %d\n", write_count);
    seq_printf(m, "Read count      : %d\n", read_count);
    seq_printf(m, "Last write size : %d bytes\n", last_write_size);
    if (buffer_len > 0)
        seq_printf(m, "Last data       : [%s]\n", device_buffer);

    mutex_unlock(&asgn1_mutex);
    return 0;
}

static int asgn1_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, asgn1_proc_show, NULL);
}

static const struct proc_ops asgn1_proc_fops = {
    .proc_open    = asgn1_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ================= PART B.2: sysfs ================= */

static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr, char *buf) {
    if (buffer_len > 0)
        return sprintf(buf, "%s", device_buffer);
    return sprintf(buf, "(empty)\n");
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *asgn1_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static struct attribute_group asgn1_attr_group = {
    .attrs = asgn1_attrs,
};

/* ================= init / exit ================= */

static int __init asgn1_init(void) {
    int ret;
    pr_info("asgn1_driver: initializing module\n");

    ret = register_chrdev(major_number, DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }

    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_class);
    }

    asgn1_device = device_create(asgn1_class, NULL,
                                MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(asgn1_device)) {
        class_destroy(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_device);
    }

    /* PART B.2: sysfs attributes */
    ret = sysfs_create_group(&asgn1_device->kobj, &asgn1_attr_group);
    if (ret)
        pr_warn("asgn1_driver: sysfs_create_group failed\n");

    /* PART B.1: /proc/asgn1_info */
    proc_entry = proc_create("asgn1_info", 0444, NULL, &asgn1_proc_fops);
    if (!proc_entry)
        pr_warn("asgn1_driver: failed to create /proc/asgn1_info\n");
    else
        pr_info("asgn1_driver: /proc/asgn1_info created\n");

    pr_info("asgn1_driver: loaded, major=%d\n", major_number);
    return 0;
}

static void __exit asgn1_exit(void) {
    if (proc_entry)
        proc_remove(proc_entry);
    sysfs_remove_group(&asgn1_device->kobj, &asgn1_attr_group);
    device_destroy(asgn1_class, MKDEV(major_number, 0));
    class_destroy(asgn1_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("asgn1_driver: module unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
