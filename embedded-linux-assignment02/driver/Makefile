obj-m += sms_sensor_driver.o

KDIR ?= /home/letienthanh/embedded_lab1/kernel/linux-5.15
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
