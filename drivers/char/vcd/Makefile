ccflags-y +=  -D__PRINT_VCD__
ccflags-y +=  -D__VCD_DEBUG__
#ccflags-y +=  -D__VCD_SPUV_FW_FROM_SD_ENABLE__


ifneq ($(KERNELRELEASE),)
# call from kernel build system

vcd-objs    := ./interface/vcd.o
vcd-objs    += ./control/vcd_ctrl.o
vcd-objs    += ./control/vcd_ctrl_func.o
vcd-objs    += ./spuv/vcd_spuv.o
vcd-objs    += ./spuv/vcd_spuv_func.o
vcd-objs    += ./spuv/sh_resampler_core.o

obj-y       += vcd.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

default:
    $(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif
clean:
    $(MAKE) -C $(KERNELDIR) M=$(PWD) clean

