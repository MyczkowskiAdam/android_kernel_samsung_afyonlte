#
# Makefile for the vibrator driver ICs with vibrator driver
#

obj-$(CONFIG_MOTOR_DRV_MAX8997)		+= max8997_vibrator.o
obj-$(CONFIG_MOTOR_DRV_MAX77693)	+= max77693_haptic.o
obj-$(CONFIG_MOTOR_DRV_ISA1200)		+= isa1200_vibrator.o
obj-$(CONFIG_VIBRATOR_ISA1000A)	+=isa1000a_haptic.o tspdrv.o
obj-$(CONFIG_VIBRATOR_SS)	+= ss_vibrator.o
obj-$(CONFIG_BCM_CLS_D_VIBRATOR)	+=ss_brcm_cls_d_haptic.o
subdir-ccflags-y += -Werror
