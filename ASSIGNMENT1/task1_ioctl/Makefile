KDIR := $(HOME)/embedded_lab1/kernel/linux-5.15.221
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-
obj-m += asgn1_driver.o

all: modules app

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

app: asgn1_app.c asgn1_ioctl.h
	$(CROSS_COMPILE)gcc -static -Wall -O2 -o asgn1_app asgn1_app.c

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f asgn1_app

.PHONY: all modules app clean
