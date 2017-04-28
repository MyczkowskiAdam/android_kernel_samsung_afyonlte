/*
 * sh/mdmloader/renesas_soc_modem_reset.c
 *
 * Copyright (c) 2011, Renesas Mobile Cooporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/spinlock_types.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <mach/common.h>
#include <mach/r8a7373.h>
#include <linux/d2153/d2153_battery.h>
#include <linux/pmic/pmic.h>


#define WPMCIF_EPMU_BASE		IO_ADDRESS(0xE6190000)
#define WPMCIF_EPMU_START_CR		(WPMCIF_EPMU_BASE + 0x0000)
#define WPMCIF_EPMU_ACC_CR		(WPMCIF_EPMU_BASE + 0x0004)
#define WPMCIF_EPMU_RES_CR		(WPMCIF_EPMU_BASE + 0x0008)
#define WPMCIF_EPMU_RES_FAC		(WPMCIF_EPMU_BASE + 0x000C)
#define WPMCIF_EPMU_PLL2_REALLY5_CR	(WPMCIF_EPMU_BASE + 0x0020)
#define WPMCIF_EPMU_RFCLK_CR		(WPMCIF_EPMU_BASE + 0x0024)
#define WPMCIF_EPMU_HPSSCLK_CR		(WPMCIF_EPMU_BASE + 0x0028)
#define WPMCIF_EPMU_INT_CR		(WPMCIF_EPMU_BASE + 0x00C4)
#define WPMCIF_EPMU_INT_FAC		(WPMCIF_EPMU_BASE + 0x00CC)
#define WPMCIF_EPMU_INT_FACMSK		(WPMCIF_EPMU_BASE + 0x00D4)
#define WPMCIF_EPMU_INT_FACCLR		(WPMCIF_EPMU_BASE + 0x00C8)
#define WPMCIF_EPMU_INT_MONREG		(WPMCIF_EPMU_BASE + 0x00E4)

/* define for INT_FACMSK register */
#define INT_FACMSK_MODEM_RESET_CLEAR_MASK 	0xFFFFFF7F
#define INT_FACMSK_MODEM_RESET_SET_MASK 	0x00000080

/* define for INT_FAC register */
#define INT_FAC_MODEM_RESET_MASK 		0x00000080

/* define for INT_FACCLR register */
#define INT_FACCLR_MODEM_RESET_SET_MASK 	0x00000080
#define INT_FACCLR_MODEM_RESET_CLEAR_MASK 	0xFFFFFF7F

/* define for INT_CR register */
#define INT_CR_ENABLE_EPMU_INT1_MASK 		0x00000001
#define INT_CR_CLEAR_EPMU_INT1_MASK 		0x00000100 /* Interrupt is clear */
#define INT_CR_CLEAR_EPMU_INT1_MASK_RESET	0xFFFFFEFF

#define SWRESET   1
#define KERNEL_PANIC  1


static dev_t 		rmc_reset_dev;
static struct 		cdev rmc_reset_cdev;
static int 		rmc_mdm_reset_char_major;	/* char major number */
static struct class *rmc_mdm_reset_char_class; 		/* char class during class_create */


struct rmc_device_node {
	struct cdev 		*cdev;
	unsigned long 		reg;
	int			irq;
	wait_queue_head_t 	wait;
	struct resource 	*res;
	int			open_count;	/* count the number of openers */
	atomic_t 		counter;
	spinlock_t		io_lock;
	spinlock_t 		irq_lock;
};

static struct rmc_device_node rmc_dev_node;

char devname[] = "rmc_mdm_reset";



static ssize_t rmc_reset_read(struct file *file, char *buf, size_t len,
				loff_t *ppos)
{
	printk(KERN_ALERT "rmc_reset_read()\n");
	return 0;
}

static ssize_t rmc_reset_write(struct file *file, const char __user *buf,
						size_t len, loff_t *ppos)
{
	printk(KERN_ALERT "rmc_reset_write()\n");
	return 0;
}


static long rmc_reset_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct rmc_device_node *dev = file->private_data;
	unsigned long curent_value = 0;

	if (!dev->open_count)
		return -ENODEV;

	switch (cmd) {

	case SWRESET:

		if(arg == KERNEL_PANIC){
			/* Set global variable panic_timeout to force a reboot
                          if panic_timeout = 0 the plateform does not reboot*/
			panic_timeout = 2;
			panic("CP Crash");
		}else{
#if 0 /*CONFIG_SEC_DEBUG*/
			panic("AP requested CP reboot");
#else
			curent_value = __raw_readl(WPMCIF_EPMU_ACC_CR);
			if (curent_value != 0x00000003){
				/* Host Access request */
				__raw_writel(0x00000002, WPMCIF_EPMU_ACC_CR);
			}

			while (0x00000003 != __raw_readl(WPMCIF_EPMU_ACC_CR) ) {
				/* Wait until Access OK */
				/* should be very quick.*/
			}

			/*Setting WRES bit will assert*/
			/* WGM_Recover_Req signal to modem*/
			__raw_writel(0x00000001, WPMCIF_EPMU_RES_CR);

			/* Clear WRES bit other wise WGM_Recover_Req */
			/* will be assert again */
			__raw_writel(0x00000000, WPMCIF_EPMU_RES_CR);
#endif
			pr_info("open WPMCIF_EPMU_INT_MONREG =0x%08lx\n",
				curent_value); /* tmp monreg */
		}
	break;

	default:
		pr_info("Wrong request only SW RESET is authorized \n");
	break;
	}
	return 0;

}

static unsigned int rmc_reset_poll(struct file *file, poll_table *wait)
{
 	struct rmc_device_node *dev = file->private_data;
	unsigned int mask = 0;

	printk(KERN_ALERT " %s : %d\n", __func__ , atomic_read(&dev->counter));
	wait_event_interruptible(dev->wait, atomic_read(&dev->counter));
	if (atomic_read(&dev->counter))
		mask = POLLIN | POLLRDNORM;

	printk(KERN_ALERT " %s :poll end: %d\n", __func__,
			atomic_read(&dev->counter));
	atomic_set(&dev->counter, 0);
	return mask;

}


static int rmc_reset_open(struct inode *inode, struct file *file)
{
	struct rmc_device_node *dev = &rmc_dev_node;
	int retval = 0;
	unsigned long curent_value = 0;

	pr_info("rmc_reset_open");

	/* lock the device to allow correctly handling errors
	 * in resumption */
	spin_lock(&dev->io_lock);

	/* allow opening only once */
	if (dev->open_count) {
		retval = -EBUSY;
		spin_unlock(&dev->io_lock);
		goto exit;
	}
	dev->open_count = 1;

	/*1. Clear Desired event bit in INT_FACMSK register.*/
	curent_value = __raw_readl(WPMCIF_EPMU_INT_FACMSK);
	__raw_writel(curent_value & INT_FACMSK_MODEM_RESET_CLEAR_MASK,
			WPMCIF_EPMU_INT_FACMSK);

	/* 2. Set INT_FACCLR = 0xFFFFFFFF*/
	/* (Clear all Interrupt Events before enabling the desired event)*/
	__raw_writel(0xFFFFFFFF, WPMCIF_EPMU_INT_FACCLR);

	curent_value =  __raw_readl(WPMCIF_EPMU_INT_FACCLR);
	__raw_writel(curent_value & INT_FACCLR_MODEM_RESET_CLEAR_MASK,
			WPMCIF_EPMU_INT_FACCLR); /* rpc tst */

	/* 3. Set INT_CR = 0x00000001 (Enable EPMU_Int1)*/
	curent_value = __raw_readl(WPMCIF_EPMU_INT_CR);
	__raw_writel(curent_value | INT_CR_ENABLE_EPMU_INT1_MASK,
			WPMCIF_EPMU_INT_CR);

	/* save our object in the file's private structure */
	file->private_data = dev;
	atomic_set(&dev->counter, 0);
	spin_unlock(&dev->io_lock);

exit:
	return retval;
}


static int rmc_reset_release(struct inode *inode, struct file *file)
{
	struct rmc_device_node *dev = file->private_data;

	pr_info("rmc_loader_release :  \n");

	if (dev == NULL)
		return -ENODEV;

	spin_lock(&dev->io_lock);
	dev->open_count = 0;
	cdev_del(&rmc_reset_cdev);
	spin_unlock(&dev->io_lock);

	return 0;
}


static const struct file_operations rmc_reset_fops = {
	.owner		= THIS_MODULE,
	.read		= rmc_reset_read,
	.write		= rmc_reset_write,
	.open		= rmc_reset_open,
	.release	= rmc_reset_release,
	.unlocked_ioctl	= rmc_reset_ioctl,
	.poll		= rmc_reset_poll,
};


static irqreturn_t rmc_interrupt_handler(int irq, void *dev_id)
{
	struct rmc_device_node *dev = &rmc_dev_node;
	unsigned long curent_value, int_fac_event,  int_facmsk_value;


	spin_lock(&dev->irq_lock);
	curent_value =  __raw_readl(WPMCIF_EPMU_INT_FAC); /* tmp */


	/* Mask all event factors by setting INT_FACMSK register*/
	int_facmsk_value = __raw_readl(WPMCIF_EPMU_INT_FACMSK);

	pr_info("rmc_interrupt_handler :INT_FAC =0x%08lx, int_facmsk_value =0x%08lx\n",
			curent_value, int_facmsk_value);

	__raw_writel(0xFFFFFFFF, WPMCIF_EPMU_INT_FACMSK);

	/* Read INT_FAC to know the Event factor*/
	curent_value =  __raw_readl(WPMCIF_EPMU_INT_FAC);
	int_fac_event = curent_value & INT_FAC_MODEM_RESET_MASK;
	if (!int_fac_event){
		pr_info("Wrong Interrupt :curent_value =0x%08lx\n",
				curent_value);
		spin_unlock(&dev->irq_lock);
		return IRQ_HANDLED;
	}

	/*Release HPB semaphore (HW sem + SW sem) if modem side doesn't release it*/

#if defined(CONFIG_PMIC_TPS80032_POW)
	tps80032_handle_modem_reset();
#elif defined(CONFIG_BATTERY_D2153)
	d2153_handle_modem_reset();
#else
#error PMIC modem reset not defined!!!
#endif

	/* Clear Event factor by setting corresponding bit */
	/* in INT_FACCLR register*/
	curent_value =  __raw_readl(WPMCIF_EPMU_INT_FACCLR);
	__raw_writel(curent_value | INT_FACCLR_MODEM_RESET_SET_MASK,
			WPMCIF_EPMU_INT_FACCLR);

	/* Clear the bit which is set in the above step.*/
	curent_value =  __raw_readl(WPMCIF_EPMU_INT_FACCLR);
	__raw_writel(curent_value & INT_FACCLR_MODEM_RESET_CLEAR_MASK,
			WPMCIF_EPMU_INT_FACCLR);

	/* Clear Interrupt by setting INT_CR[8] */
	curent_value =  __raw_readl(WPMCIF_EPMU_INT_CR);
	__raw_writel(curent_value | INT_CR_CLEAR_EPMU_INT1_MASK,
			WPMCIF_EPMU_INT_CR);
	/* Clear the bit which is set in the above step*/
	curent_value = __raw_readl(WPMCIF_EPMU_INT_CR);
	__raw_writel(curent_value & INT_CR_CLEAR_EPMU_INT1_MASK_RESET,
			WPMCIF_EPMU_INT_CR);

	/* Release the desired even factor */
	/* by clearing corresponding bit in INT_FACMSK*/
	__raw_writel(int_facmsk_value & INT_FACMSK_MODEM_RESET_CLEAR_MASK,
			WPMCIF_EPMU_INT_FACMSK);

	atomic_inc(&dev->counter);
	wake_up_interruptible(&dev->wait);
	spin_unlock(&dev->irq_lock);

	return IRQ_HANDLED;
}

static int __devinit rmc_reset_probe(struct platform_device *pdev)
{
	struct rmc_device_node *dev = &rmc_dev_node;
	struct resource *res;
	void __iomem *reg;
	int  irq, ret;

	pr_info("rmc_reset_probe ENTER()\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk(KERN_ERR "%s -platform_get_resource error.\n", __func__);
		return -EINVAL;
	}

	reg = ioremap(res->start, resource_size(res));
	if (!reg) {
		printk(KERN_ERR "%s -ioremap error.\n", __func__);
		return -ENOMEM;
	}
	dev->reg = (unsigned long)reg;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		printk(KERN_ERR "%s -platform_get_irq error.\n", __func__);
		return -EINVAL;
	}

	dev->irq = irq;

	init_waitqueue_head(&dev->wait);

	ret = request_irq(dev->irq, rmc_interrupt_handler,
			0, "rmc_wgm_reset_int" /*devname*/, pdev);
	if (ret) {
		printk(KERN_ERR "%s - request_irq erro\n", __func__);
		goto clean_up;
	}

	spin_lock_init(&dev->io_lock);
	spin_lock_init(&dev->irq_lock);
	pr_info("rmc_reset_probe OK\n");

	return 0;

clean_up:
	iounmap(&dev->reg);
	return ret;
}


static int __devexit rmc_reset_remove(struct platform_device *pdev)
{
	struct rmc_device_node *dev = &rmc_dev_node;
	int  irq;

	printk(KERN_DEBUG "rmc_reset_remove ENTER()\n");
	iounmap(&dev->reg);

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0)
		free_irq(irq, pdev);


	platform_set_drvdata(pdev, NULL);


	return 0;
}
static struct platform_driver rmc_reset_driver = {
	.probe		= rmc_reset_probe,
	.remove		= __devexit_p(rmc_reset_remove),
	.driver		= {
		.name	= "rmc_wgm_reset_int",
		.owner	= THIS_MODULE,
	},
};

static int __init rmc_reset_init(void)
{
	struct device *dev;
	int ret = -ENOMEM;


	printk(KERN_DEBUG "rmc_reset_init ENTER()\n");

	/* create Char driver */
	ret = alloc_chrdev_region(&rmc_reset_dev, 0, 1, devname);
	if (ret < 0) {
		printk(KERN_ALERT "rmc_reset_init() alloc_chrdev_region failed!\n");
		return ret;
	}

	cdev_init(&rmc_reset_cdev, &rmc_reset_fops);

	ret = cdev_add(&rmc_reset_cdev, rmc_reset_dev, 1);
	if (ret< 0) {
		printk(KERN_ALERT "rmc_reset_init() cdev_init/cdev_add failed!\n");
		return ret;
	}

	/* Export the _char device to user space*/
	rmc_mdm_reset_char_major = MAJOR(rmc_reset_dev);
	rmc_mdm_reset_char_class = class_create(THIS_MODULE, devname);
	if (IS_ERR(rmc_mdm_reset_char_class)) {
		pr_err("rmc_mdm_reset_char: Failed to create class\n");
		goto out_cdev_del;
	}


	pr_info( "rmc_reset_cdev Major %d\n",rmc_mdm_reset_char_major );

	dev = device_create(rmc_mdm_reset_char_class, NULL,
				MKDEV(rmc_mdm_reset_char_major, 0),
				NULL, "%s", devname);
	if (IS_ERR(dev)) {
		pr_err("Error in device_create rmc_mdm_reset \n");
		goto out_device_destroy;
	}

	rmc_dev_node.cdev = &rmc_reset_cdev;

	/* Create Platform driver*/
	ret = platform_driver_register(&rmc_reset_driver);
	return ret;

out_device_destroy:
	class_destroy(rmc_mdm_reset_char_class);
out_cdev_del:
	cdev_del(&rmc_reset_cdev);
	return ret;

}
module_init(rmc_reset_init);

static void __exit rmc_reset_exit(void)
{
	pr_info( "rmc_reset_exit\n");
	cdev_del(&rmc_reset_cdev);
        unregister_chrdev_region(rmc_reset_dev, 1);
	platform_driver_unregister(&rmc_reset_driver);
}
module_exit(rmc_reset_exit);

MODULE_AUTHOR("Ronan.Picard@renesasmobile.com");
MODULE_ALIAS("rmc_mdm_reset");
MODULE_DESCRIPTION("Renesas SoC Modem Reset");
MODULE_LICENSE("GPL v2");
