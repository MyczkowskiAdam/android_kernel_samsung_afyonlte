# Makefile for rtapi driver

sh_mobile_core-y := appmem.o module.o
sh_mobile_composer-y := composer.o

obj-$(CONFIG_MISC_R_MOBILE_COMPOSER_APPMEM_SHAREMANGER) += sh_mobile_core.o
obj-$(CONFIG_MISC_R_MOBILE_COMPOSER)                  += sh_mobile_composer.o

ccflags-$(CONFIG_MISC_R_MOBILE_COMPOSER) += -Idrivers/char/composer
ccflags-$(CONFIG_MISC_R_MOBILE_COMPOSER) += -Werror
