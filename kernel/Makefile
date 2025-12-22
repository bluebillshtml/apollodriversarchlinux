# Apollo Twin Kernel Driver Makefile
# SPDX-License-Identifier: GPL-2.0-only

obj-m := apollo.o

apollo-objs := apollo_main.o apollo_pcm.o apollo_hw.o apollo_control.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

.PHONY: all clean install

