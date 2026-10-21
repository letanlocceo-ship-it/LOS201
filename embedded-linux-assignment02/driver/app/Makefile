CC_ARM = arm-linux-gnueabihf-gcc
CC_HOST = gcc
CFLAGS = -Wall -Wextra -g -O2 -fno-omit-frame-pointer -pthread
LDLIBS = -lrt -pthread

SRCS = sms_app.c circular_buffer.c sensor_thread.c processor_thread.c alert_thread.c logger_thread.c

all: arm host

arm:
	$(CC_ARM) $(CFLAGS) -static $(SRCS) -o sms_app $(LDLIBS)

host:
	$(CC_HOST) $(CFLAGS) $(SRCS) -o sms_app_x86 $(LDLIBS)

clean:
	rm -f sms_app sms_app_x86
