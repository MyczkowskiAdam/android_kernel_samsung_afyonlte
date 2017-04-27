/* mtd_trace.c
 *
 * Copyright (C) 2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Renesas design France Modem Trace Buffer
 */
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>

#define DRV_NAME	"mtd_trace"
#define DRV_VERSION	"0.0.1"
#define MTD_BUF_NUM	1

#define MTD_IOCTL_TYPE	0xBA
#define MTD_IOCTL_ADDR_NR 2
#define MTD_IOCTL_ADDR	_IOR(MTD_IOCTL_TYPE, 2, struct mtd_trace_buf)

struct mtd_trace_buf
{
	u_int32_t	phy_addr;
	size_t		size;
};

struct mtd_trace {			/* global module structure */
	struct resource	*mem;		/* address of trace buffer */
	struct platform_device	*pdev;
	struct device	*dev;
	struct cdev	cdev;
	dev_t		devt;
	struct class	*class;		/* char class during class_create */
};


/* open device */
static int mtd_buf_open(struct inode *inode, struct file *file)
{
	struct mtd_trace *mtd;
	unsigned minor = MINOR(inode->i_rdev);

	if (minor >= MTD_BUF_NUM)
		return -ENOSYS;

	/* look up device info for this device file */
	mtd = container_of(inode->i_cdev, struct mtd_trace, cdev);
	file->private_data = mtd;
	return 0;
} /* mtd_buf_open */

/* close device */
static int mtd_buf_release(struct inode *inode, struct file *file)
{
	return 0;
} /* mtd_buf_release */

static int mtd_buf_mmap(struct file *fp, struct vm_area_struct *vma)
{
	unsigned long addr;
	unsigned long size;
	unsigned long start;
	struct mtd_trace *mtd = (struct mtd_trace *)fp->private_data;
	int ret = 0;

	addr = (unsigned long)mtd->mem->start;
	size = mtd->mem->end - mtd->mem->start;

	if ((vma->vm_pgoff << PAGE_SHIFT) > size)
		return -ENXIO;
	addr = vma->vm_pgoff + (addr >> PAGE_SHIFT);

	start = vma->vm_start + (vma->vm_pgoff << PAGE_SHIFT);
	if (vma->vm_end - start < size)
		size = vma->vm_end - start;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = io_remap_pfn_range(vma,
				vma->vm_start,
				addr,
				size,
				vma->vm_page_prot);

	return ret;
}

static long mtd_buf_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct mtd_trace *mtd = (struct mtd_trace *)file->private_data;
	long ret;
	struct mtd_trace_buf __user *out;
	size_t size;
	u_int32_t addr;
	out = (struct mtd_trace_buf __user *)arg;

	if (MTD_IOCTL_TYPE != _IOC_TYPE(cmd)) {
		return -EPERM;
	}
	if (_IOC_NR(cmd) == MTD_IOCTL_ADDR_NR) {
		addr = (u_int32_t)mtd->mem->start;
		ret = put_user(addr, &out->phy_addr);
		size = mtd->mem->end - mtd->mem->start;
		if (!ret)
			ret = put_user(size, &out->size);
	} else
		return -EPERM;
	return ret;
}

static ssize_t mtd_buf_read(struct file *file,
				char *buffer,
				size_t len,
				loff_t *offset)
{
	struct mtd_trace *mtd = (struct mtd_trace *)file->private_data;
	struct resource *res = mtd->mem;
	void __user	*dst = buffer;
	void __iomem	*mem;
	void 		*src;
	ssize_t		ret = 0;

	if (*offset+len > resource_size(res))
		len = resource_size(res)-*offset;
	if (len)
	{
		/* map buffer to kernel space */
		mem = ioremap_nocache(res->start, resource_size(res));
		if (!mem) {
			dev_err(mtd->dev,"read unable to map memory\n");
			return -ENXIO;
		}
		src = mem+*offset;
		ret = copy_to_user(dst, src, len);
		if (!ret) {
			ret = len;
			*offset += len;
		}
		iounmap(mem);
	}
	return ret;
} /* mtd_buf_read */

static ssize_t mtd_buf_write(struct file *file ,const char *buffer,
				size_t len, loff_t *offset)
{
	struct mtd_trace *mtd = (struct mtd_trace *)file->private_data;
	struct resource *res = mtd->mem;
	void __user	*src = (void *)buffer;
	void __iomem	*mem;
	void		*dst;
	ssize_t		ret = 0;

	if (*offset+len > resource_size(res))
		len = resource_size(res)-*offset;
	if (len) {
		/* map buffer to kernel space */
		mem = ioremap_nocache(res->start, resource_size(res));
		if (!mem) {
			dev_err(mtd->dev,"write unable to map memory\n");
			return -ENXIO;
		}
		dst = mem+*offset;
		ret = copy_from_user(dst, src, len);
		if (!ret) {
			ret = len;
			*offset += ret;
		}
		iounmap(dst);
	}
	return ret;
} /* mtd_buf_write */

static const struct file_operations mtd_buf_fops = {
	.owner		= THIS_MODULE,
	.open		= mtd_buf_open,
	.release	= mtd_buf_release,
	.read		= mtd_buf_read,
	.write		= mtd_buf_write,
	.llseek		= default_llseek,
	.unlocked_ioctl	= mtd_buf_ioctl,
	.mmap		= mtd_buf_mmap,
};


void __exit mtd_buf_exit(struct mtd_trace *mtd)
{
	int major = MAJOR(mtd->devt);

	printk(KERN_INFO "mtd_buf_exit\n");
	device_destroy(mtd->class, MKDEV(major, 0));
	cdev_del(&mtd->cdev);
	unregister_chrdev_region(mtd->devt, 1);

}

static int __devinit mtd_trace_probe(struct platform_device *pdev)
{
	int error = 0;
	struct mtd_trace *mtd;
	struct device *dev;
	int major;

	mtd = (struct mtd_trace *)devm_kzalloc(&pdev->dev,
						sizeof(*mtd),
						GFP_KERNEL);

	if (!mtd) {
		dev_err(&pdev->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}
	memset((char *)mtd, 0, sizeof(*mtd));
	platform_set_drvdata(pdev, mtd);
	mtd->pdev = pdev;

	dev_info(&pdev->dev, "memory\n");
	mtd->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mtd->mem) {
		dev_err(&pdev->dev, "address unknown\n");
		return -ENODEV;
	}
	mtd->class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(mtd->class)) {
		dev_err(&pdev->dev, "Failed to create class\n");
		return error;
	}
	/* create Char driver */
	error = alloc_chrdev_region(&mtd->devt, 0,
						MTD_BUF_NUM, DRV_NAME);
	if (error < 0) {
		dev_err(&pdev->dev, "alloc_chrdev_region failed!\n");
		goto out_class_del;
	}

	cdev_init(&mtd->cdev, &mtd_buf_fops);

	error = cdev_add(&mtd->cdev, mtd->devt, MTD_BUF_NUM);
	if (error < 0) {
		dev_err(&pdev->dev, "cdev_add failed!\n");
		goto out_class_del;
	}

	/* Export the _char device to user space*/
	major = MAJOR(mtd->devt);
	dev_info(&pdev->dev, "Major %d\n", major);

	dev = device_create(mtd->class, &pdev->dev,
				MKDEV(major, 0),
				mtd, DRV_NAME);
	if (IS_ERR(dev)) {
		dev_err(&pdev->dev, "Error in device_create\n");
		error = PTR_ERR(dev);
		goto out_cdev_del;
	}
	mtd->dev = dev;
	return 0;

out_cdev_del:
	cdev_del(&mtd->cdev);
out_class_del:
	class_destroy(mtd->class);
	return error;
}

static int mtd_trace_remove(struct platform_device *pdev)
{
	struct mtd_trace *mtd;
	dev_info(&pdev->dev, "remove\n");

	mtd = platform_get_drvdata(pdev);
	device_destroy(mtd->class, mtd->devt);
	cdev_del(&mtd->cdev);
	class_destroy(mtd->class);
	return 0;
}


static struct platform_driver mtd_trace_driver = {
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= mtd_trace_probe,
	.remove	= mtd_trace_remove,
};

static int __init mtd_trace_init(void)
{
	printk(KERN_INFO "register " DRV_NAME " character device\n");
	/* Create Platform driver*/
	return platform_driver_register(&mtd_trace_driver);
}

static void __exit mtd_trace_exit(void)
{
	platform_driver_unregister(&mtd_trace_driver);
}

module_init(mtd_trace_init);
module_exit(mtd_trace_exit);

MODULE_DESCRIPTION("mtd_trace Controller driver");
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("Renesas Mobile Corporation");
MODULE_LICENSE("GPL");
