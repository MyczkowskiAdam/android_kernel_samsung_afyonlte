/*
 * arch/arm/mach-shmobile/pm_boot_sysfs.c
 *
 *  Copyright (C) 2013 Renesas Mobile Corporation
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

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <sec_hal_cmn.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <mach/pm.h>
#include <mach/sbsc.h>

static int boot_stop;

static ssize_t pm_boot_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", boot_stop);
}

static ssize_t pm_boot_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int arbmode;
	sscanf(buf, "%du", &boot_stop);
	if (boot_stop == 1) {
		start_cpufreq();
		control_cpuidle(1);
		arbmode = SEC_HAL_CMN_ARBMODE_COUNTER;
		sec_hal_arbmode_set(&arbmode);
		pr_info("SAPRICR0 %x SAPRICR1 %x\n",
			shmobile_sbsc_read_reg32(SBSC_SAPRICR0),
			shmobile_sbsc_read_reg32(SBSC_SAPRICR1));
	} else if (boot_stop == 0) {
		(void)stop_cpufreq();
		control_cpuidle(0);
	}

	return count;
}

static struct kobj_attribute foo_attribute =
	__ATTR(boot_stop, 0644, pm_boot_show, pm_boot_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *attrs[] = {
	&foo_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *pm_boot_kobj;

static int __init pm_boot_init(void)
{
	int retval;

	boot_stop = 0;

	pm_boot_kobj = kobject_create_and_add("PowerManagement", kernel_kobj);
	if (!pm_boot_kobj)
		return -ENOMEM;

	/* Create the files associated with this kobject */
	retval = sysfs_create_group(pm_boot_kobj, &attr_group);
	if (retval)
		kobject_put(pm_boot_kobj);

	return retval;
}

static void __exit pm_boot_exit(void)
{
	kobject_put(pm_boot_kobj);
}

module_init(pm_boot_init);
module_exit(pm_boot_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Renesas Mobile");
