obj-m := hid-vxe.o

KDIR ?= /usr/lib/modules/$(shell uname -r)/build
BUILD_DIR := $(PWD)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
