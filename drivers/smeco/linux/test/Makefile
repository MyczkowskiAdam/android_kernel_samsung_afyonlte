
ifneq ($(KERNELRELEASE),)
# Invoked from kernel build system

#smctest-objs	:= smc_test_app.o
obj-$(CONFIG_SMECO_TEST) += smctest.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINCDIR=$(PWD) modules

endif

clean:
	rm -rf *.o *.ko