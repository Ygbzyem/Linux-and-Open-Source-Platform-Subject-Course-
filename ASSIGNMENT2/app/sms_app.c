#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <mqueue.h>
#include <pthread.h>
#include "circular_buffer.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"

volatile int keep_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

int main(int argc, char *argv[]) {
    int interval_ms = 500;
    float temp_thresh = 40.0f;
    float humid_thresh = 80.0f;
    int duration = 60;
    int verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "i:t:h:d:v")) != -1) {
        switch (opt) {
            case 'i': interval_ms = atoi(optarg); break;
            case 't': temp_thresh = atof(optarg); break;
            case 'h': humid_thresh = atof(optarg); break;
            case 'd': duration = atoi(optarg); break;
            case 'v': verbose = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-i ms] [-t degC] [-h %%] [-d sec] [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(alarm_msg_t);
    attr.mq_curmsgs = 0;

    mq_unlink("/sms_alarm_mq");
    mqd_t alarm_mq = mq_open("/sms_alarm_mq", O_CREAT | O_RDWR, 0666, &attr);

    attr.mq_msgsize = 256;
    mq_unlink("/sms_log_mq");
    mqd_t log_mq = mq_open("/sms_log_mq", O_CREAT | O_RDWR, 0666, &attr);

    circular_buffer_t *buffer = cb_init(64);

    sensor_config_t s_cfg = {interval_ms, verbose, buffer, "/dev/sms_sensor"};
    processor_config_t p_cfg = {temp_thresh, humid_thresh, buffer, alarm_mq, log_mq};
    alert_config_t a_cfg = {alarm_mq, log_mq};
    logger_config_t l_cfg = {log_mq, "/tmp/sms.log"};

    pthread_t t_sensor, t_proc, t_alert, t_log;

    pthread_create(&t_log, NULL, logger_thread_func, &l_cfg);
    pthread_create(&t_alert, NULL, alert_thread_func, &a_cfg);
    pthread_create(&t_proc, NULL, processor_thread_func, &p_cfg);

    pthread_attr_t s_attr;
    pthread_attr_init(&s_attr);
    pthread_attr_setschedpolicy(&s_attr, SCHED_FIFO);
    struct sched_param param;
    param.sched_priority = 50;
    pthread_attr_setschedparam(&s_attr, &param);
    pthread_create(&t_sensor, &s_attr, sensor_thread_func, &s_cfg);
    pthread_attr_destroy(&s_attr);

    if (duration > 0) {
        sleep(duration);
        keep_running = 0;
    } else {
        while (keep_running) sleep(1);
    }

    pthread_join(t_sensor, NULL);
    pthread_join(t_proc, NULL);
    pthread_join(t_alert, NULL);
    pthread_join(t_log, NULL);

    cb_free(buffer);
    mq_close(alarm_mq);
    mq_close(log_mq);
    mq_unlink("/sms_alarm_mq");
    mq_unlink("/sms_log_mq");

    printf("SMS System shutdown complete.\n");
    return 0;
}