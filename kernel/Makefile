
# default location of linux kernel source code
KERNEL_DIR?=/usr/src/linux

obj-m=16aio.o

default:
	KCPPFLAGS= $(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules

all: default

clean:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(PWD) clean
