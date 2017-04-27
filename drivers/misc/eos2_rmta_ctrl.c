/*
 * eos2_rmta_control.c
 *
 *Copyright (C) 2013 Renesas Mobile Corporation
 *
 *This program is free software; you can redistribute it and/or
 *modify it under the terms of the GNU General Public License
 *version 2 as published by the Free Software Foundation.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program; if not, write to the Free Software
 *Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *MA  02110-1301, USA.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/smb328a_charger.h>

#define DEBUG_RMTA_INFO               0x00000001
#define DEBUG_DEFAULT_LEVEL           0

static unsigned long debug_level_set = DEBUG_DEFAULT_LEVEL;

#define pm_charger_info(fmt, ...)				\
	do {							 \
		if (debug_level_set & DEBUG_RMTA_INFO)		\
			pr_info(fmt, ##__VA_ARGS__);		\
	} while (0)

static ssize_t smb_charging_status_show(struct kobject *kobj,
					struct kobj_attribute \
					*attr, char *buf)
{
#if defined (CONFIG_MACH_LOGANLTE) || defined (CONFIG_MACH_AFYONLTE) || defined (CONFIG_MACH_HEATLTE)
	return sprintf(buf, "%d\n", smb328a_chip_status());
#else
	pm_charger_info("%s: Currently not supported\n", __func__);
#endif
}

static ssize_t smb_charging_status_store(struct kobject *kobj,
					struct kobj_attribute \
					*attr, const char *buf, size_t count)
{
	int charging_enable = 0;
	struct i2c_client *client = NULL;
	client = smb328a_chip_client();
	sscanf(buf, "%d", &charging_enable);
	pm_charger_info("%s: charger_status = %d\n", __func__,
			smb328a_chip_status());
	 if (client) {
		switch (charging_enable) {
		case 0:
#if defined (CONFIG_MACH_LOGANLTE) || defined (CONFIG_MACH_AFYONLTE) || defined (CONFIG_MACH_HEATLTE)
			smb328a_disable_charging(client);
			break;
#else
			pm_charger_info("%s: Currently not supported\n",
							__func__);
			break;
#endif
		case 1:
#if defined (CONFIG_MACH_LOGANLTE) || defined (CONFIG_MACH_AFYONLTE) || defined (CONFIG_MACH_HEATLTE)
			smb328a_enable_charging(client);
			break;
#else
			pm_charger_info("%s: Currently not supported\n",
							__func__);
			break;
#endif
		default:
			break;
		}
	}
	return count;
}

static struct kobj_attribute charging_status_attribute = __ATTR(charging_status,
				S_IRUGO | S_IWUSR, smb_charging_status_show ,
					smb_charging_status_store);

static struct attribute *charger_attributes[] = {
	&charging_status_attribute.attr,
	NULL,
};

static const struct attribute_group charger_group = {
	.attrs = charger_attributes,
};

static struct kobject *charger_kobj;
static int __init rmta_sysfs_init(void)
{
	int retval;

	printk(KERN_INFO "Creating charger sysfs entries ... ");
	charger_kobj = kobject_create_and_add("rmta_control", kernel_kobj);
	if (!charger_kobj) {
		printk(KERN_ERR"[FAILED]\n");
		return -ENOMEM;
	}

	retval = sysfs_create_group(charger_kobj, &charger_group);
	if (retval)
		kobject_put(charger_kobj);
	return retval;
}

static void __exit rmta_sysfs_exit(void)
{
	kobject_put(charger_kobj);
}

module_init(rmta_sysfs_init);
module_exit(rmta_sysfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Renesas Mobile Corporation");

