#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include "sms_sensor.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pham Minh Tung");
MODULE_DESCRIPTION("ASSIGNMENT-02: Sensor Monitoring System Kernel Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME "sms_sensor"
#define CLASS_NAME  "sms_sensor_class"
#define PROC_NAME   "sms_stats"

static int major_number;
static struct class  *sms_class  = NULL;
static struct device *sms_device = NULL;
static struct proc_dir_entry *proc_entry = NULL;

/* State Variables - Lưu hoàn toàn bằng số nguyên nhân 100 */
static unsigned int read_count = 0;
static unsigned int last_temp_x100 = 2500; /* Mặc định 25.00 C */
static unsigned int last_hum_x100  = 5000; /* Mặc định 50.00 % */
static unsigned int min_interval_ms = 100;

static ktime_t start_time;
static ktime_t last_read_time;
static DEFINE_MUTEX(sms_mutex);

/* ================= File Operations ================= */

static int sms_open(struct inode *inode, struct file *file) {
    return 0;
}

static int sms_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t sms_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    char data_str[128];
    int data_len;
    u32 rand_temp, rand_hum;
    ktime_t now;
    u64 elapsed_ms;
    u64 timestamp_ms;

    now = ktime_get();
    elapsed_ms = ktime_to_ms(ktime_sub(now, last_read_time));
    if (elapsed_ms < min_interval_ms) {
        msleep(min_interval_ms - elapsed_ms);
    }

    mutex_lock(&sms_mutex);

    /* Sinh ngẫu nhiên hoàn toàn bằng Integer Math */
    rand_temp = get_random_u32();
    last_temp_x100 = 1500 + (rand_temp % 3001); /* 1500 -> 4500 */

    rand_hum = get_random_u32();
    last_hum_x100 = 3000 + (rand_hum % 6001);   /* 3000 -> 9000 */

    read_count++;
    last_read_time = ktime_get();
    timestamp_ms = ktime_to_ms(last_read_time);

    /* Formatted chuỗi CSV: <timestamp_ms>,<temp_int>.<temp_dec>,<hum_int>.<hum_dec>\n */
    data_len = snprintf(data_str, sizeof(data_str), "%llu,%u.%02u,%u.%02u\n",
                        timestamp_ms,
                        last_temp_x100 / 100, last_temp_x100 % 100,
                        last_hum_x100 / 100, last_hum_x100 % 100);

    mutex_unlock(&sms_mutex);

    if (*offset >= data_len)
        return 0; /* EOF */

    if (len < data_len)
        data_len = len;

    if (copy_to_user(buf, data_str, data_len))
        return -EFAULT;

    *offset += data_len;
    return data_len;
}

static long sms_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case SMS_SET_INTERVAL:
            mutex_lock(&sms_mutex);
            min_interval_ms = (unsigned int)arg;
            mutex_unlock(&sms_mutex);
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .open           = sms_open,
    .release        = sms_release,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
};

/* ================= ProcFS (/proc/sms_stats) ================= */

static int sms_proc_show(struct seq_file *m, void *v) {
    u64 uptime_sec = ktime_divns(ktime_sub(ktime_get(), start_time), NSEC_PER_SEC);

    mutex_lock(&sms_mutex);
    seq_printf(m, "read_count: %u\n", read_count);
    seq_printf(m, "last_temperature: %u.%02u\n", last_temp_x100 / 100, last_temp_x100 % 100);
    seq_printf(m, "last_humidity: %u.%02u\n", last_hum_x100 / 100, last_hum_x100 % 100);
    seq_printf(m, "driver_uptime_s: %llu\n", uptime_sec);
    mutex_unlock(&sms_mutex);

    return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_fops = {
    .proc_open    = sms_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ================= Init & Exit ================= */

static int __init sms_driver_init(void) {
    start_time = ktime_get();
    last_read_time = start_time;

    major_number = register_chrdev(0, DEVICE_NAME, &sms_fops);
    if (major_number < 0) return major_number;

    sms_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(sms_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sms_class);
    }

    sms_device = device_create(sms_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(sms_device)) {
        class_destroy(sms_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sms_device);
    }

    proc_entry = proc_create(PROC_NAME, 0444, NULL, &sms_proc_fops);

    pr_info("sms_sensor: driver loaded successfully\n");
    return 0;
}

static void __exit sms_driver_exit(void) {
    if (proc_entry)
        proc_remove(proc_entry);

    device_destroy(sms_class, MKDEV(major_number, 0));
    class_destroy(sms_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("sms_sensor: driver unloaded\n");
}

module_init(sms_driver_init);
module_exit(sms_driver_exit);