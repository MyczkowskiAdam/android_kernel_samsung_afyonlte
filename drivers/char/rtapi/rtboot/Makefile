
# Comment/uncomment the following line to disable/enable debugging
DEBUG = n

# Multimedia log output level INFO=0 LOW=1 MED=2 HIGH=3 ERROR=4 FATAL=5 NONE=255
LEVEL=4

# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

INC_PATH := $(srctree)
RTAPI_PATH := drivers/char/rtapi
U_INCLUDE := -I$(RTAPI_PATH)/common
U_INCLUDE += -I$(INC_PATH)/include/rtapi
U_INCLUDE += -I$(INC_PATH)/drivers/sec_hal/exports
ccflags-y += $(DEBFLAGS) $(U_INCLUDE) -DMSG_LEVEL=$(LEVEL)

ifneq ($(KERNELRELEASE),)
# call from kernel build system

rtboot-objs	:= ./rt_boot.o
rtboot-objs	+= ./rt_boot_sub.o

obj-y		+= rtboot.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINCDIR=$(PWD)/../include

endif
clean:
	rm -rf *.o *>  core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
