/*
 * arch/arm/mach-shmobile/ramdump.c
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kmsg_dump.h>
#include <linux/kdebug.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <asm/system_misc.h>
#include <asm/cacheflush.h>
#include <linux/atomic.h>
#include <mach/ramdump.h>
#include <asm/mach/map.h>
#include <memlog/memlog.h>

#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif
static int debug_enabled;
module_param_named(debug_enabled, debug_enabled, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

#define dprintk(fmt, arg...) \
	do { \
		if (debug_enabled) \
			printk(KERN_INFO fmt, ##arg); \
	} while (0)


struct hw_registers {
	void __iomem *start;
	void __iomem *end;
	phys_addr_t p_start;
	enum hw_register_width width;
	/* Power area which needs to be on before reading registers
	 * This is PTSR register bit mask */
	unsigned int pa;
	/* This one of the module stop registers */
	void __iomem *msr;
	/* This is module stop register bit mask */
	unsigned int msb;
};

struct ramdump {
	/* physical base address for trace32 scripts */
	unsigned long reg_dump_base_phys;

	/* base address & size for register dump area */
	void __iomem *reg_dump_base;
	size_t reg_dump_size;

	/* base address for core register dump */
	void *core_reg_dump_base;
	/* size reserved for each core */
	size_t core_reg_dump_size;

	/* offset for exception registers */
	unsigned int excp_reg_offset;

	/* base address & size for hw register dump */
	void *lsi_reg_dump_base;
	size_t lsi_reg_dump_size;

	struct hw_registers *hw_registers;
	unsigned int num_hw_registers;

	struct hw_register_dump *hw_register_dump;
	atomic_t hw_registers_saved;
};

static struct ramdump *ramdump;

/* This is the magic marker used by trace32 to find the kernel base address */
static const char *magic_marker = "k3rneL_*_p0hj4Os0it3";


/**
 *  my_emergency_restart - reboot the system
 *
 *  This is a hacked version of emergency_restart to avoid stopping
 *  peripherals etc which should not happen in emergency restart (machine
 *  shutdown calls shmobile_pm_stop_peripheral_devices in ARCH_R8A73734).
 */
static void my_emergency_restart(void)
{
	dprintk("%s\n", __func__);
	kmsg_dump(KMSG_DUMP_EMERG);

	arm_pm_restart('h', NULL);

	/* reboot failed, let's wait watchdog */
	while (1);
}

/*
 * For analysis tool compatibility, the layout should only be extended, not
 * changed. Layout is:
 * +000: R0
 * +004: R1
 * +008: R2
 * +00C: R3
 * +010: R4
 * +014: R5
 * +018: R6
 * +01C: R7
 * +020: R8
 * +024: R9
 * +028: R10
 * +02C: R11
 * +030: R12
 * +034: R13_usr
 * +038: R14_usr
 * +03C: R15
 * +040: CPSR
 * +044: SPSR_svc
 * +04C: R14_svc
 * +050: SPSR_abt
 * +054: R13_abt
 * +058: R14_abt
 * +05C: SPSR_und
 * +060: R13_und
 * +064: R14_und
 * +068: SPSR_irq
 * +06C: R13_irq
 * +070: R14_irq
 * +074: SPSR_fiq
 * +078: R8_fiq
 * +07C: R9_fiq
 * +080: R10_fiq
 * +084: R11_fiq
 * +088: R12_fiq
 * +08C: R13_fiq
 * +090: R14_fiq
 * +094: MPIDR
 * +098: SCTLR
 * +09C: ACTLR
 * +0A0: TTBR0
 * +0A4: TTBR1
 * +0A8: TTBCR
 * +0AC: DACR
 * +0B0: DFSR
 * +0B4: IFSR
 * +0B8: DFAR
 * +0BC: IFAR
 * +0C0: PRRR
 * +0C4: NMRR
 */
static void *save_core_registers(void *ptr)
{
	register void *p asm("r0") = ptr;

	/* Routine assumes it is entered in SVC mode. */
	asm volatile(
		"STMIA	r0, {r0-r15}^\n\t"	/* User registers */
		"ADD	r0, r0, #64\n\t"
		"MRS	r1, cpsr\n\t"
		"MRS	r2, spsr\n\t"
		"STMIA	r0!, {r1,r2,r13,r14}\n\t"/* SVC SPSR,R13,R14 */
		"CPSID	a, #0x17\n\t"
		"MRS	r2, spsr\n\t"
		"STMIA	r0!, {r2,r13,r14}\n\t"	/* ABT SPSR,R13,R14 */
		"CPS	#0x1B\n\t"
		"MRS	r2, spsr\n\t"
		"STMIA	r0!, {r2,r13,r14}\n\t"	/* UND SPSR,R13,R14 */
		"CPSID	i, #0x12\n\t"
		"MRS	r2, spsr\n\t"
		"STMIA	r0!, {r2,r13,r14}\n\t"	/* IRQ SPSR,R13,R14 */
		"CPSID	f, #0x11\n\t"
		"MRS	r2, spsr\n\t"
		"STMIA	r0!, {r2,r8-r14}\n\t"	/* FIQ SPSR,R8-R14 */
		"MSR	cpsr_cx, r1\n\t"	/* Restore SVC mode+interrupt status */

		"MRC	p15, 0, r1, c0, c0, 5\n\t" /* MPIDR (Multiprocessor Affinity) */
		"MRC	p15, 0, r2, c1, c0, 0\n\t" /* SCTLR (System Control) */
		"MRC	p15, 0, r3, c1, c0, 1\n\t" /* ACTLR (Auxiliary Control) */
		"STMIA	r0!, {r1-r3}\n\t"
		"MRC	p15, 0, r1, c2, c0, 0\n\t" /* TTBR0 (Translation Table Base 0) */
		"MRC	p15, 0, r2, c2, c0, 1\n\t" /* TTBR1 */
		"MRC	p15, 0, r3, c2, c0, 2\n\t" /* TTBCR (Translation Table Base Control) */
		"MRC	p15, 0, r12, c3, c0, 0\n\t" /* DACR (Domain Access Control) */
		"STMIA	r0!, {r1-r3,r12}\n\t"
		"MRC	p15, 0, r1, c5, c0, 0\n\t" /* DFSR (Data Fault Status) */
		"MRC	p15, 0, r2, c5, c0, 1\n\t" /* IFSR (Instruction Fault Status) */
		"MRC	p15, 0, r3, c6, c0, 0\n\t" /* DFAR (Data Fault Address) */
		"MRC	p15, 0, r12, c6, c0, 2\n\t" /* IFAR (Instruction Fault Address) */
		"STMIA r0!, {r1-r3,r12}\n\t"
		"MRC p15, 0, r1, c10, c2, 0\n\t" /* PRRR (Primary Region Remap) */
		"MRC p15, 0, r2, c10, c2, 1\n\t" /* NMRR (Normal Memory Remap) */
		"STMIA r0!, {r1-r2}"

		: "+r" (p) /* output */
		:
		: "r1", "r2", "r3", "r12", "memory" /* clobber */
	);
	return p;
}

static unsigned int *read_register_range(unsigned int *write_ptr,
		struct hw_registers *hwr)
{
	/* todo: read address is increment by 4 byte steps.
	 * if some range has width 8bit and the addresses needs to be
	 * incremented by byte step then this needs to be fixes. */
	unsigned int __iomem *start_addr = NULL, *end_addr = NULL;
	unsigned int msr = 0;
	start_addr = hwr->start;
	end_addr = hwr->end;
	if ((NULL != write_ptr) && (NULL != start_addr) && (NULL != end_addr)) {

		/* Check if register have power area to be checked before
		 * reading register */
		if (hwr->pa &&
			(ramdump->hw_register_dump->SYSCPSTR & hwr->pa) == 0) {
			/* power are down -> skip registers */
			dprintk("power area status 0x%08lx -> area 0x%08x " \
					"down -> skip reading register range\n",
					ramdump->hw_register_dump->SYSCPSTR,
					hwr->pa);
			write_ptr = write_ptr + (end_addr - start_addr);
			write_ptr++; /* skip also end address */
			return write_ptr;
		}

		/* Check if register have module stop condition be checked
		 * before reading register */
		if (hwr->msr && hwr->msb) {
			msr = __raw_readl(hwr->msr);
			if ((msr & hwr->msb) == hwr->msb) {
				dprintk("msr 0x%p:0x%08x -> msb %x stopped" \
					"-> skip reading register range\n",
					hwr->msr, msr, hwr->msb);
				write_ptr = write_ptr + (end_addr - start_addr);
				write_ptr++; /* skip also end address */
				return write_ptr;
			}
		}

		while (start_addr <= end_addr) {
			switch (hwr->width) {
			case HW_REG_8BIT:
				*write_ptr = (unsigned int)__raw_readb(
					(unsigned char __iomem *)start_addr);
				break;
			case HW_REG_16BIT:
				*write_ptr = (unsigned int)__raw_readw(
					(unsigned short __iomem *)start_addr);
				break;
			case HW_REG_32BIT:
				*write_ptr = __raw_readl(start_addr);
				break;
			}


			dprintk("%x:%d, %p : %08x\n", hwr->p_start +
					((unsigned int)start_addr - (
					unsigned int)hwr->start), hwr->width,
					write_ptr, *write_ptr);
			write_ptr++;
			start_addr++;
		}
	}
	return write_ptr;
}

/**
 * save_hw_registers_to_ram - Write logs to eMMC
 * @return : none
 */
static void save_hw_registers_to_ram(void)
{
	int i = 0;
	unsigned int *p = NULL;
	struct hw_registers *hw_regs = NULL;
	dprintk("--- Start of %s() ---\n", __func__);

	/* Check if we already have dumped the LSI registers */
	if (!atomic_inc_and_test(&ramdump->hw_registers_saved)) {
		dprintk("%s HW registers already dumped\n", __func__);
		return;
	}

	p = ramdump->lsi_reg_dump_base;

	for (i = 0; i < ramdump->num_hw_registers; i++) {
		hw_regs = &ramdump->hw_registers[i];
		p = read_register_range(p, hw_regs);
	}
}



static void ramdump_kmsg_dump_handler(struct kmsg_dumper *dumper,
				      enum kmsg_dump_reason reason,
				      const char *s1, unsigned long l1,
				      const char *s2, unsigned long l2)
{
	dprintk("%s reason = %d\n", __func__, reason);
	switch (reason) {
	case KMSG_DUMP_HALT:
	case KMSG_DUMP_RESTART:
	case KMSG_DUMP_POWEROFF:
		/* No action on ordinary shutdown */
		return;
	case KMSG_DUMP_PANIC:
		save_hw_registers_to_ram();
	case KMSG_DUMP_EMERG:
	case KMSG_DUMP_OOPS:
	default: /* And anything else we don't recognise */
		break;
	}

	/*
	* Only action we take at the moment is to flush as much cache as
	* we can to try to ensure RAM dumps are useful. Flush everything,
	* because we want to see not only the kmsg dump, but all other data.
	* (We only need a clean, not a flush, but no API for that).
	*/
	flush_cache_all();
	outer_flush_all();
}

static struct kmsg_dumper kmsg_dump_block = {
	.dump = ramdump_kmsg_dump_handler,
};




static int ramdump_die_notifier(struct notifier_block *unused,
		     unsigned long val, void *data)
{
	void *p = NULL;
	struct pt_regs *regs = NULL;
	struct die_args *args = data;
	dprintk("%s val %lu\n", __func__, val);

	memlog_capture = 0;

	/* find the exception register dump location */
	p = ramdump->core_reg_dump_base
			+ ramdump->core_reg_dump_size*smp_processor_id()
			+ ramdump->excp_reg_offset;

	/* Check if exceptions registers are already saved and exit if they are.
	 * Only the first set of exception registers are saved to catch the
	 * problem which occurred first */
	regs = (struct pt_regs *)p;
	if (regs->ARM_cpsr != 0) {
		dprintk("%s exception registers already saved CPSR=%lx\n",
				__func__, regs->ARM_cpsr);
		return NOTIFY_DONE;
		}

	/* save exception registers to the end of dump area */
	memcpy(p, args->regs, sizeof(struct pt_regs));

	return NOTIFY_DONE;
}


static struct notifier_block ramdump_die_notifier_nb = {
	.notifier_call = ramdump_die_notifier,
	.priority = INT_MAX /* we want to be first to save excp regs */
};

static int ramdump_panic_handler(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	void *p = NULL;
	dprintk("%s event %lu\n", __func__, event);

	memlog_capture = 0;

	/* save core registers to the beginning of the dump area */
	p = ramdump->core_reg_dump_base +
			ramdump->core_reg_dump_size*smp_processor_id();
	dprintk("core reg dump start=%p\n", p);
	save_core_registers(p);

	/* cache cleaning will happen in kmsg dump handler */
	my_emergency_restart();

	return NOTIFY_DONE;
}

static struct notifier_block rmc_panic_block = {
	.notifier_call = ramdump_panic_handler,
	.priority = -INT_MAX /* we do restart so be last in list */
};

/* ramdump_remap_resources does not return error because even it fails,
 * the rest of the ramdum functionality needs to be initialized to get
 * cache flushes and core registers dumps from crashes
 *
 * @return size of the register dump
 * */
static resource_size_t ramdump_remap_resources(struct ramdump_plat_data *pdata)
{
	resource_size_t total_size = 0;
	int i;

	dprintk("%s\n", __func__);

	for (i = 0; i < pdata->num_resources; i++) {
		struct hw_register_range *hwr = &pdata->hw_register_range[i];
		phys_addr_t h_end = hwr->end + hwr->width - 1;
		resource_size_t size = h_end - hwr->start + 1;
		unsigned char __iomem *v_start = NULL, *v_end = NULL;
		int j;

		/* registers are saved as unsigned int each */
		total_size += hwr->end - hwr->start + sizeof(unsigned int);
		/* if the addresses are already mapped, no need to map again */
		for (j = 0; j < pdata->io_desc_size; j++) {
			struct map_desc *map = &pdata->io_desc[j];

			phys_addr_t p_start = __pfn_to_phys(map->pfn);
			phys_addr_t p_end = p_start + map->length - 1;

			if (hwr->start >= p_start && h_end <= p_end) {
				/* physical address range found from permanent
				 * mappings */
				v_start = (unsigned char __iomem *)
					(map->virtual + (hwr->start - p_start));
				v_end = (unsigned char __iomem *)
					(map->virtual + (h_end - p_start));
				break;
			}
		}

		/* address range not mapped permanently */
		if (!v_start) {
			v_start = ioremap_nocache(hwr->start, size);
			if (!v_start) {
				printk(KERN_ERR "%s ioremapping %x-%x failed!\n",
						__func__, hwr->start, h_end);
				/* even one mapping one register range fails,
				 * continue */
				continue;
			}
			v_end = v_start + size - 1;
		}

		ramdump->hw_registers[i].start = v_start;
		ramdump->hw_registers[i].end = v_end;
		ramdump->hw_registers[i].width = hwr->width;
		ramdump->hw_registers[i].pa = hwr->pa;
		ramdump->hw_registers[i].msr = hwr->msr;
		ramdump->hw_registers[i].msb = hwr->msb;
		ramdump->hw_registers[i].p_start = hwr->start;
		dprintk("%x-%x -> %p-%p\n", hwr->start, h_end,
				ramdump->hw_registers[i].start,
				ramdump->hw_registers[i].end);
	}
	return total_size;
}

static int __devinit ramdump_probe(struct platform_device *pd)
{
	int ret = 0;
	char *mm = NULL;
	resource_size_t hw_reg_dump_size = 0;
	struct ramdump_plat_data *pdata = pd->dev.platform_data;
	pd->dev.platform_data = NULL;

	ramdump = kzalloc(sizeof(*ramdump), GFP_KERNEL);
	if (!ramdump) {
		dev_err(&pd->dev, "%s kzalloc: failed", __func__);
		return -ENOMEM;
	}

	atomic_set(&ramdump->hw_registers_saved, -1);

	ramdump->reg_dump_base_phys = pdata->reg_dump_base;

	dprintk("%s reg_dump_base phys 0x%lx, size %d\n", __func__,
				pdata->reg_dump_base, pdata->reg_dump_size);
	ramdump->reg_dump_base = ioremap_nocache(pdata->reg_dump_base,
			pdata->reg_dump_size);
	if (!ramdump->reg_dump_base) {
		dev_err(&pd->dev, "%s ioremap_nocache: failed", __func__);
		goto ioremap_regdump_failed;
	}
	ramdump->reg_dump_size = pdata->reg_dump_size;
	memset((void __force *) ramdump->reg_dump_base, 0,
			ramdump->reg_dump_size);
	dprintk("%s reg_dump_base virt 0x%p\n", __func__,
			ramdump->reg_dump_base);

	/* store kernel base address as a last address and store magic marker
	 * before it so that trace32 scripts can find it */
	mm = (void __force *) ramdump->reg_dump_base + ramdump->reg_dump_size -
			sizeof(unsigned int);
	*((unsigned int *)mm) = (unsigned int)CONFIG_MEMORY_START;
	mm = mm - strlen(magic_marker);
	strcpy(mm, magic_marker);

	/* setup register dump area */
	ramdump->core_reg_dump_base = (void __force *) ramdump->reg_dump_base;
	ramdump->core_reg_dump_size = pdata->core_reg_dump_size;
	ramdump->excp_reg_offset =
			ramdump->core_reg_dump_size - sizeof(struct pt_regs);

	/* setup hw register dump area. it starts after core reg dump areas
	 * and stops just before magic marker */
	ramdump->lsi_reg_dump_base = ramdump->core_reg_dump_base +
			ramdump->core_reg_dump_size*CONFIG_NR_CPUS;
	ramdump->lsi_reg_dump_size = pdata->reg_dump_size -
			ramdump->core_reg_dump_size*CONFIG_NR_CPUS -
			strlen(magic_marker) - sizeof(unsigned int);
	dprintk("%s lsi_reg_dump_base virt 0x%p, size %d\n", __func__,
			ramdump->lsi_reg_dump_base,
			ramdump->lsi_reg_dump_size);

	/* set the struct pointer to lsi reg dump base */
	ramdump->hw_register_dump =
			(struct hw_register_dump *)ramdump->lsi_reg_dump_base;

	/* allocate mem for register ranges */
	ramdump->hw_registers = kzalloc(pdata->num_resources *
			sizeof(struct hw_registers), GFP_KERNEL);
	if (!ramdump->hw_registers) {
		dev_err(&pd->dev, "%s kzalloc: failed", __func__);
		goto alloc_registers_failed;
	}
	ramdump->num_hw_registers = pdata->num_resources;

	hw_reg_dump_size = ramdump_remap_resources(pdata);
	dprintk("hw register dump takes %d bytes\n", hw_reg_dump_size);
	if (hw_reg_dump_size > ramdump->lsi_reg_dump_size) {
		dev_err(&pd->dev, "HW REGISTER DUMP TOO BIG, please check" \
			"content of hw_register_range or reserve more space" \
			"for register dump\n");
		goto reg_dump_too_big;
	}

	/* register system callbacks */
	ret = register_die_notifier(&ramdump_die_notifier_nb);
	if (ret) {
		dev_err(&pd->dev, "%s register_die_notifier: failed %d\n",
				__func__, ret);
		goto die_notifier_register_failed;
	}

	ret = kmsg_dump_register(&kmsg_dump_block);
	if (ret) {
		dev_err(&pd->dev, "%s kmsg_dump_register: failed %d\n", __func__,
				ret);
		goto kmsg_notifier_register_failed;
	}

	ret = atomic_notifier_chain_register(&panic_notifier_list,
			&rmc_panic_block);
	if (ret) {
		dev_err(&pd->dev, "%s atomic_notifier_chain_register: failed %d\n",
				__func__, ret);
		goto panic_notifier_register_failed;
	}

	return 0;

panic_notifier_register_failed:
	if (kmsg_dump_unregister(&kmsg_dump_block))
		dev_err(&pd->dev, "%s: kmsg_dump_unregister failed\n",
				__func__);
kmsg_notifier_register_failed:
	if (unregister_die_notifier(&ramdump_die_notifier_nb))
		dev_err(&pd->dev, "%s: unregister_die_notifier failed\n",
				__func__);
die_notifier_register_failed:
	kfree(ramdump->hw_registers);
reg_dump_too_big:
	/*todo: unmap registers*/
alloc_registers_failed:
	iounmap(ramdump->reg_dump_base);
ioremap_regdump_failed:
	kfree(ramdump);
	return ret;
}

static int __exit ramdump_remove(struct platform_device *pd)
{
	/* Should not happen */
	int ret = 0;
	dev_err(&pd->dev, "%s ***RAMDUMP feature being removed***\n", __func__);

	/*todo: unmap registers*/

	ret = atomic_notifier_chain_unregister(&panic_notifier_list,
			&rmc_panic_block);
	if (ret)
		dev_err(&pd->dev, "%s: atomic_notifier_chain_unregister " \
				" failed %d\n", __func__, ret);

	if (kmsg_dump_unregister(&kmsg_dump_block))
		dev_err(&pd->dev, "%s: kmsg_dump_unregister failed\n",
				__func__);

	if (unregister_die_notifier(&ramdump_die_notifier_nb))
		dev_err(&pd->dev, "%s: unregister_die_notifier failed\n",
				__func__);

	iounmap(ramdump->reg_dump_base);
	kfree(ramdump->hw_registers);
	kfree(ramdump);
	ramdump = NULL;
	return 0;
}

static struct platform_driver ramdump_pdriver = {
	.probe	=  ramdump_probe,
	.remove	= __exit_p(ramdump_remove),
	.driver	= {
			.name	= "ramdump",
			.owner	= THIS_MODULE,
	},
};

static int __init init_ramdump(void)
{
	int ret = 0;
#ifdef CONFIG_SEC_DEBUG
	if (!sec_debug_level.en.kernel_fault)
		return -1;
#endif
	dprintk("%s\n", __func__);

	ret = platform_driver_register(&ramdump_pdriver);
	if (ret) {
		printk(KERN_ERR "%s: platform_driver_unregister failed  %d\n",
				__func__, ret);
	}

	return ret;
}


static void __exit exit_ramdump(void)
{
	/* Should not happen */
	printk(KERN_ERR "%s ***RAMDUMP feature being removed***\n", __func__);
	platform_driver_unregister(&ramdump_pdriver);
}

module_init(init_ramdump);
module_exit(exit_ramdump);

MODULE_DESCRIPTION("RMC Ramdump");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Renesas Mobile Corp.");
