# ASSIGNMENT-02: Sensor Monitoring System

> Vietnamese version: [README.vi.md](README.vi.md)

## Overview
The Sensor Monitoring System (SMS) is an embedded multi-threaded real-time monitoring application running on ARMv7 QEMU Linux. The system consists of:
- **Kernel Driver (`sms_sensor.ko`)**: Simulates an industrial temperature and humidity sensor. It exposes data via character device `/dev/sms_sensor`, provides statistics via `/proc/sms_stats`, and supports minimum reading interval configuration using `ioctl` (`SMS_SET_INTERVAL`).
- **User-space Application (`sms_app`)**: A POSIX multi-threaded C program comprising 4 synchronized threads:
  - `Sensor Thread`: Periodically reads CSV samples from `/dev/sms_sensor` using `SCHED_FIFO` real-time scheduling and pushes data into a thread-safe circular buffer.
  - `Processor Thread`: Consumes data from the buffer, calculates rolling statistics, and detects anomalies (temperature high, humidity high, temperature spike).
  - `Alert Thread`: Blocks on a POSIX message queue (`/sms_alarm_mq`) to receive and display warnings on `stderr`.
  - `Logger Thread`: Consumes sample/alert messages from `/sms_log_mq` and logs them asynchronously to `/tmp/sms.log` with file rotation support.

## Architecture
```
┌─────────────────────────────────────────────────────────────────────────┐
│ Kernel Space                                                            │
│  [sms_sensor.ko]                                                        │
│     ├── /dev/sms_sensor  ─── (read: CSV format, ioctl: SMS_SET_INTERVAL) │
│     └── /proc/sms_stats ─── (read: uptime, read_count, last values)   │
└───────────────────────────────────┬─────────────────────────────────────┘
                                    │ IPC / System Calls
┌───────────────────────────────────▼─────────────────────────────────────┐
│ User-space Application (sms_app)                                        │
│  [Sensor Thread] ──(Circular Buffer)──> [Processor Thread]             │
│        (SCHED_FIFO)                           │                         │
│                                      (POSIX Message Queues)             │
│                                               ├─> [Alert Thread]        │
│                                               └─> [Logger Thread] ──> /tmp/sms.log
└─────────────────────────────────────────────────────────────────────────┘
```

## Directory Structure
```
ASSIGNMENT2/
├── driver/
│   ├── sms_sensor.c       # Kernel character device driver source code
│   ├── sms_sensor.h       # Shared header (ioctl definitions, struct)
│   └── Makefile           # Kernel module cross-compile Makefile
├── app/
│   ├── circular_buffer.c/.h # Thread-safe circular buffer implementation
│   ├── sensor_thread.c/.h   # Sensor reader thread (SCHED_FIFO)
│   ├── processor_thread.c/.h# Data processing & anomaly detection thread
│   ├── alert_thread.c/.h    # Alert processing thread
│   ├── logger_thread.c/.h   # Asynchronous logger thread
│   ├── sms_app.c            # Main application entry point & signal handler
│   └── Makefile             # User-space cross-compile Makefile
├── logs/                  # Output logs and verification results
├── Makefile               # Top-level Makefile for full system build
├── README.vi.md           # Vietnamese documentation
└── README.md              # Project documentation
```

## Build
Cross-compile both kernel module and static user-space binary for ARMv7 target:

```bash
# Build kernel driver and userspace app
make -C driver
arm-linux-gnueabihf-gcc -Wall -Wextra -pthread -static -I../driver app/*.c -o app/sms_app -lrt -lm
```

## Run (on QEMU)
```bash
# 1. Load the kernel module
insmod /lib/modules/5.15.221/sms_sensor.ko
dmesg | tail -n 5

# 2. Verify device node creation
ls -l /dev/sms_sensor

# 3. Execute user-space monitoring application (500ms interval, 10s duration, verbose mode)
sms_app -i 500 -t 40.0 -h 80.0 -d 10 -v

# 4. Check generated log file and driver statistics
cat /tmp/sms.log
cat /proc/sms_stats

# 5. Unload kernel driver
rmmod sms_sensor
```

## Example Output
```text
/ # insmod /lib/modules/5.15.221/sms_sensor.ko
sms_sensor: driver loaded successfully
/ # ls -l /dev/sms_sensor
crw-------    1 root     0          248,   0 Nov  2 19:17 /dev/sms_sensor
/ # sms_app -i 500 -t 40.0 -h 80.0 -d 10 -v
[SENSOR] Time: 23788 ms | Temp: 41.92 C | Hum: 47.11 %
[ALERT][23788] TYPE: TEMP_HIGH VALUE: 41.92 THRESHOLD: 40.00
[SENSOR] Time: 24288 ms | Temp: 22.15 C | Hum: 65.40 %
...
SMS System shutdown complete.
/ # cat /proc/sms_stats
read_count: 20
last_temperature: 22.15
last_humidity: 65.40
driver_uptime_s: 45
```

## Key Concepts
- **Character Device Driver**: Implements `file_operations` (`read`, `open`, `release`, `unlocked_ioctl`) and dynamic major number assignment.
- **Kernel Integer Arithmetic**: Replaced floating-point math with scaled integers (`temp_x100`) to prevent FPU context switch overhead and ARM `modpost` `__aeabi` linker errors.
- **procfs Interface**: Exposes runtime statistics via `/proc/sms_stats` using `seq_file`.
- **POSIX Multi-threading (`pthreads`)**: Concurrent execution of 4 dedicated worker threads.
- **Thread Synchronization**: Circular buffer protected by `pthread_mutex_t` and `pthread_cond_t` producer-consumer signaling.
- **POSIX Message Queues (`mqueue`)**: Non-blocking/timed IPC messaging (`mq_timedsend`, `mq_timedreceive`) between processing and alert/logger threads to prevent deadlocks.
- **Real-Time Scheduling (`SCHED_FIFO`)**: Priority-based scheduling for Sensor Thread using `clock_nanosleep(CLOCK_MONOTONIC)` to minimize sampling jitter.
- **Graceful Shutdown**: Signal handling via `sigaction(SIGINT)` ensuring cleanly joined threads, flushed log files, and unlinked IPC resources.

## Notes
- **Known Limitations**:
  - The driver currently generates pseudo-random sensor values using `get_random_u32()` instead of interfacing with physical I2C/SPI hardware.
  - Circular buffer uses a fixed heap capacity (64 elements) initialized at startup.
- **Future Improvements**:
  - Integrate PREEMPT_RT kernel patches to further reduce task scheduling jitter below 100µs.
  - Implement dynamic memory re-allocation or ring-buffer memory mapping (`mmap`) for high-frequency sensor streams.
  - Add persistent log storage support using JFFS2/UBIFS flash file systems.