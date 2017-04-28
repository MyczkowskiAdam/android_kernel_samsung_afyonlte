/*
 * drivers/memlog/memlog.c
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
#include <linux/io.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <memlog/memlog.h>
#include <mach/sec_debug.h>

static struct kobject *memlog_kobj;

static char *logdata;
unsigned long memlog_capture;

struct log_area {
	unsigned long index;
	unsigned long start;
	unsigned long limit;
};

static DEFINE_PER_CPU(struct log_area, proc_log_area);
static DEFINE_PER_CPU(struct log_area, irq_log_area);
static DEFINE_PER_CPU(struct log_area, func_log_area);
static DEFINE_PER_CPU(struct log_area, dump_log_area);

void memory_log_proc(const char *name, unsigned long pid)
{
	struct log_area *log;
	struct proc_log_entry proc_log;
	unsigned long index = 0;

	if (!logdata || !name || !memlog_capture
			|| !sec_debug_level.en.kernel_fault)
		return;

	memset(&proc_log, 0, PROC_ENTRY_SIZE);
	proc_log.time = local_clock();
	proc_log.pid = pid;
	snprintf((char *)&proc_log.data, 16, "%s", name);

	log = &get_cpu_var(proc_log_area);

	if (log->index + PROC_ENTRY_SIZE > log->limit)
		log->index = log->start;

	index = log->index;
	log->index += PROC_ENTRY_SIZE;

	put_cpu_var(proc_log_area);
	memcpy(logdata + index, &proc_log, PROC_ENTRY_SIZE);
}
EXPORT_SYMBOL_GPL(memory_log_proc);

void memory_log_worker(unsigned long func_addr, unsigned long pid, int in)
{
	char str[16];
	if (in)
		sprintf(str, " IN 0x%lx", func_addr);
	else
		sprintf(str, "OUT 0x%lx", func_addr);
	memory_log_proc(str, pid);
}
EXPORT_SYMBOL_GPL(memory_log_worker);

void memory_log_irq(unsigned int irq, int in)
{
	struct log_area *log;
	struct irq_log_entry irq_log;
	unsigned long index = 0;

	if (!logdata || !memlog_capture || !sec_debug_level.en.kernel_fault)
		return;

	memset(&irq_log, 0, IRQ_ENTRY_SIZE);
	irq_log.time = local_clock();
	irq_log.data = irq | (in ? IRQ_LOG_ENTRY_IN : 0);

	log = &get_cpu_var(irq_log_area);

	if (log->index + IRQ_ENTRY_SIZE > log->limit)
		log->index = log->start;

	index = log->index;
	log->index += IRQ_ENTRY_SIZE;

	put_cpu_var(irq_log_area);

	memcpy(logdata + index, &irq_log, IRQ_ENTRY_SIZE);
}
EXPORT_SYMBOL_GPL(memory_log_irq);

void memory_log_func(unsigned long func_id, int in)
{
	struct log_area *log;
	struct func_log_entry func_log;
	unsigned long index = 0;
	unsigned long flags = 0;

	if (!logdata || !memlog_capture || !sec_debug_level.en.kernel_fault)
		return;

	memset(&func_log, 0, FUNC_ENTRY_SIZE);
	func_log.time = local_clock();
	func_log.data = func_id | (in ? FUNC_LOG_ENTRY_IN : 0);

	local_irq_save(flags);

	log = this_cpu_ptr(&func_log_area);

	if (log->index + FUNC_ENTRY_SIZE > log->limit)
		log->index = log->start;

	index = log->index;
	log->index += FUNC_ENTRY_SIZE;

	local_irq_restore(flags);

	memcpy(logdata + index, &func_log, FUNC_ENTRY_SIZE);
}
EXPORT_SYMBOL_GPL(memory_log_func);

void memory_log_dump_int(unsigned char dump_id, int dump_data)
{
	struct log_area *log;
	struct dump_log_entry dump_log;
	unsigned long index = 0;
	unsigned long flags = 0;

	if (!logdata || !memlog_capture || !sec_debug_level.en.kernel_fault)
		return;

	memset(&dump_log, 0, DUMP_ENTRY_SIZE);
	dump_log.time = local_clock();
	dump_log.id = dump_id;
	dump_log.data = dump_data;

	local_irq_save(flags);

	log = this_cpu_ptr(&dump_log_area);

	if (log->index + DUMP_ENTRY_SIZE > log->limit)
		log->index = log->start;

	index = log->index;
	log->index += DUMP_ENTRY_SIZE;

	local_irq_restore(flags);

	memcpy(logdata + index, &dump_log, DUMP_ENTRY_SIZE);
}
EXPORT_SYMBOL_GPL(memory_log_dump_int);

void memory_log_timestamp(unsigned int id)
{
	int cpu;
	struct timestamp_entries *p =
			(struct timestamp_entries *)(logdata + TIMESTAMP_INDEX);

	if (!logdata || !memlog_capture || !sec_debug_level.en.kernel_fault)
			return;
	cpu = get_cpu();
	p->time[cpu][id] = local_clock();
	put_cpu();
}
EXPORT_SYMBOL_GPL(memory_log_timestamp);

void memory_log_init(void)
{
	struct memlog_header mh;
	BUILD_BUG_ON(MEMLOG_END > MEMLOG_SIZE);

	 /* init the starts and indexes */
	per_cpu(proc_log_area, 0).index = CPU0_PROC_START_INDEX;
	per_cpu(proc_log_area, 0).start = CPU0_PROC_START_INDEX;
	per_cpu(proc_log_area, 0).limit = CPU1_PROC_START_INDEX;

	per_cpu(proc_log_area, 1).index = CPU1_PROC_START_INDEX;
	per_cpu(proc_log_area, 1).start = CPU1_PROC_START_INDEX;
	per_cpu(proc_log_area, 1).limit = CPU0_IRQ_START_INDEX;

	per_cpu(irq_log_area, 0).index = CPU0_IRQ_START_INDEX;
	per_cpu(irq_log_area, 0).start = CPU0_IRQ_START_INDEX;
	per_cpu(irq_log_area, 0).limit = CPU1_IRQ_START_INDEX;

	per_cpu(irq_log_area, 1).index = CPU1_IRQ_START_INDEX;
	per_cpu(irq_log_area, 1).start = CPU1_IRQ_START_INDEX;
	per_cpu(irq_log_area, 1).limit = CPU0_FUNC_START_INDEX;

	per_cpu(func_log_area, 0).index = CPU0_FUNC_START_INDEX;
	per_cpu(func_log_area, 0).start = CPU0_FUNC_START_INDEX;
	per_cpu(func_log_area, 0).limit = CPU1_FUNC_START_INDEX;

	per_cpu(func_log_area, 1).index = CPU1_FUNC_START_INDEX;
	per_cpu(func_log_area, 1).start = CPU1_FUNC_START_INDEX;
	per_cpu(func_log_area, 1).limit = CPU0_DUMP_START_INDEX;

	per_cpu(dump_log_area, 0).index = CPU0_DUMP_START_INDEX;
	per_cpu(dump_log_area, 0).start = CPU0_DUMP_START_INDEX;
	per_cpu(dump_log_area, 0).limit = CPU1_DUMP_START_INDEX;

	per_cpu(dump_log_area, 1).index = CPU1_DUMP_START_INDEX;
	per_cpu(dump_log_area, 1).start = CPU1_DUMP_START_INDEX;
	per_cpu(dump_log_area, 1).limit = CPU0_PM_START_INDEX;

	memset(logdata, 0, MEMLOG_SIZE);

	mh.proc_size = PROC_ENTRY_SIZE;
	mh.proc_count = PROC_COUNT;
	mh.irq_size = IRQ_ENTRY_SIZE;
	mh.irq_count = IRQ_COUNT;
	mh.func_size = FUNC_ENTRY_SIZE;
	mh.func_count = FUNC_COUNT;
	mh.dump_size = DUMP_ENTRY_SIZE;
	mh.dump_count = DUMP_COUNT;
	mh.timestamp_size = sizeof(unsigned long long);
	mh.timestamp_count = COUNT_TIMESTAMP;
	mh.reserved1 = 0xFF;
	mh.reserved2 = 0xFF;
	memcpy(logdata, &mh, MEMLOG_HEADER_SIZE);

	smp_mb();
	memlog_capture = 1;
}

static ssize_t capture_show(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	return sprintf(buf, "%ld\n", memlog_capture);
}

static ssize_t capture_store(
	struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf,
	size_t count)
{
	int tmp = 0;
	sscanf(buf, "%du", &tmp);
	if (memlog_capture != tmp) {
		if (tmp == 0)
			memlog_capture = 0;
		else if (tmp == 1)
			memory_log_init();
	}
	return count;
}

static struct kobj_attribute capture_attribute =
	__ATTR(capture, 0644, capture_show, capture_store);

static ssize_t read_log(
	struct file *filp,
	struct kobject *kobj,
	struct bin_attribute *bin_attr,
	char *buf,
	loff_t pos,
	size_t count)
{
	unsigned long max_count = MEMLOG_SIZE - pos;
	if (count > max_count)
		count = max_count;

	memcpy(buf, logdata + pos, count);
	return count;
}

static struct bin_attribute log_attr = {
	.attr = {
		.name = "log",
		.mode = S_IRUGO,
	},
	.size = MEMLOG_SIZE,
	.read = read_log,
};

static int __init init_memlog(void)
{
	int ret = 0;

	memlog_kobj = kobject_create_and_add("memlog", NULL);
	if (!memlog_kobj) {
		printk(KERN_ERR "kobject_create_and_add: failed\n");
		ret = -ENOMEM;
		goto exit;
	}

	ret = sysfs_create_file(memlog_kobj, &capture_attribute.attr);
	if (ret) {
		printk(KERN_ERR "sysfs_create_file: failed (%d)", ret);
		goto kset_exit;
	}

	ret = sysfs_create_bin_file(memlog_kobj, &log_attr);
	if (ret) {
		printk(KERN_ERR "sysfs_create_bin_file: failed (%d)", ret);
		goto kset_exit;
	}

	if (MEMLOG_ADDRESS > SDRAM_KERNEL_END_ADDR)
		logdata = (char *)ioremap_nocache(MEMLOG_ADDRESS, MEMLOG_SIZE);
	else
		logdata = (char *)ioremap_wc(MEMLOG_ADDRESS, MEMLOG_SIZE);
	if (!logdata) {
		printk(KERN_ERR "ioremap: failed");
		ret = -ENOMEM;
		goto exit;
	}

	memory_log_init();
	return 0;

kset_exit:
	kobject_put(memlog_kobj);
exit:
	return ret;
}


static void __exit exit_memlog(void)
{
	kobject_put(memlog_kobj);
}

pure_initcall(init_memlog);
module_exit(exit_memlog);

MODULE_DESCRIPTION("Memory Log");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Renesas Mobile Corp.");

