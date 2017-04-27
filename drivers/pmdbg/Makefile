#/*
# * Copyright (C) 2013 Renesas Mobile Corporation
# *
# * This program is free software; you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation; version 2 of the License.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# *
# */

INC_PATH := $(srctree)
ccflags-y += -I$(INC_PATH)/drivers/sec_hal/exports
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_core.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_hw.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_sysc.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_cpg.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_pdc.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_dfs.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_idle.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_suspend.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_mem.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_dbgpin.o
obj-$(CONFIG_ARCH_R8A7373)		+= pmdbg_cpu.o
subdir-ccflags-y += -Werror
