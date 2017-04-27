# subdir-ccflags-$(CONFIG_LOW_MEM_SNPR) += -DDEBUG
subdir-ccflags-$(CONFIG_LOW_MEM_SNPR) += -Dxprint0=pr_debug
subdir-ccflags-$(CONFIG_LOW_MEM_SNPR) += -Dxfree0=kfree
subdir-ccflags-$(CONFIG_LOW_MEM_SNPR) += -Werror

obj-$(CONFIG_LOW_MEM_SNPR) += lowmemsnpr.o

ifeq ($(CONFIG_LOW_MEM_SNPR_DEFAULT),y)
obj-$(CONFIG_LOW_MEM_SNPR_DEFAULT) += lowmemsnpr_default.o
endif

ifeq ($(CONFIG_LOW_MEM_SNPR_RTDS),y)
subdir-ccflags-$(CONFIG_LOW_MEM_SNPR_RTDS) += -Idrivers/char/rtapi/common
subdir-ccflags-$(CONFIG_LOW_MEM_SNPR_RTDS) += -Idrivers/char/rtapi/rtds

obj-$(CONFIG_LOW_MEM_SNPR_RTDS) += lowmemsnpr_rtds.o
endif
