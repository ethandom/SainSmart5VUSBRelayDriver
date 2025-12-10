# Makefile for relay_driver kernel module (out-of-tree)

ifneq ($(KERNELRELEASE),)

# This tells kbuild to build relay_driver.ko from relay_driver.c
obj-m := relay_driver.o

else

# Path to the kernel build directory (can be overridden from the env/cmdline)
KDIR ?= /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
