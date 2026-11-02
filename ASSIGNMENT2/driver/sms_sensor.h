#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>

#define SMS_IOC_MAGIC 's'
#define SMS_SET_INTERVAL _IOW(SMS_IOC_MAGIC, 1, unsigned int)

typedef struct {
    unsigned long long timestamp_ms;
    float temperature;
    float humidity;
} sms_sensor_data_t;

#endif /* SMS_SENSOR_H */