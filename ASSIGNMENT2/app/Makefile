CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -lrt -lm

SRCS = sms_app.c circular_buffer.c sensor_thread.c processor_thread.c alert_thread.c logger_thread.c
OBJS = $(SRCS:.c=.o)

all: sms_app_x86

sms_app_x86: $(SRCS)
	$(CC) $(CFLAGS) -I../driver $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o sms_app_x86 sms_app