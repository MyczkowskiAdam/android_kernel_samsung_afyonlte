
NAME                      := sec_hal
INCLUDES                   = -I$(src) -I$(src)/exports -I$(src)/rt/inc -I$(src)/toc/inc
obj-$(CONFIG_ARM_SEC_HAL)  = $(NAME).o
ccflags-y                 += $(INCLUDES) -DPUBLIC_ENVIRONMENT -Werror
ifneq ($(CONFIG_ARM_SEC_HAL_TRACE),)
ccflags-y                 += -DSEC_HAL_TRACE_TO_KERNEL
endif
asflags-y                 += $(INCLUDES)
$(NAME)-y                 := sec_hal_dev.o
$(NAME)-$(CONFIG_ARM_SEC_HAL_DRM_WVN) += rt/src/sec_hal_drm.o
$(NAME)-$(CONFIG_ARM_SEC_HAL_TEE) += rt/src/sec_hal_tee.o
$(NAME)-y                 += sec_hal_rpc.o
$(NAME)-y                 += sec_hal_icram0.o
$(NAME)-y                 += sec_hal_cnf.o
$(NAME)-y                 += rt/src/sec_hal_cmn.o
#legacy code ported from previous implementation.
$(NAME)-y                 += rt/src/sec_hal_rt.o
$(NAME)-y                 += rt/src/sec_msg.o
$(NAME)-y                 += rt/src/sec_hal_pm.o


ifeq ($(CONFIG_ARM_SEC_HAL_TEST_DISPATCHER),y) # select test dispatcher.
$(NAME)-y                 += rt/test/src/sec_hal_test_dispatcher.o
else # select real dispatcher.
# ARM specific implementation for entering and exiting TZ.
$(NAME)-y                 += rt/src/sec_dispatch.o
$(NAME)-y                 += rt/src/sec_bridge_tz.o
endif

ifeq ($(CONFIG_ARM_SEC_HAL_TOC),y) # toc read feature
$(NAME)-y                 += toc/src/sec_hal_toc.o
endif
$(NAME)-y                 += toc/src/sec_hal_toc.o
subdir-ccflags-y += -Werror
