ifeq ($(CONFIG_PDC),y)
	obj-$(CONFIG_PM)	+= sysfs.o generic_ops.o qos.o
else
	obj-$(CONFIG_PM)	+= sysfs.o generic_ops.o common.o qos.o
endif
obj-$(CONFIG_PM_SLEEP)	+= main.o wakeup.o
obj-$(CONFIG_PM_RUNTIME)	+= runtime.o
obj-$(CONFIG_PM_TRACE_RTC)	+= trace.o
obj-$(CONFIG_PM_OPP)	+= opp.o
ifeq ($(CONFIG_PDC),y)
else
	obj-$(CONFIG_PM_GENERIC_DOMAINS)	+=  domain.o domain_governor.o
	obj-$(CONFIG_HAVE_CLK)	+= clock_ops.o
endif

ccflags-$(CONFIG_DEBUG_DRIVER) := -DDEBUG
