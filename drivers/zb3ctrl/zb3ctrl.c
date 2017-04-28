#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <mach/pm.h>

#include "zb3ctrl.h"

static int open_count;
atomic_t	pdwait_flag; /* video playback highload condition */

/* #define __DEBUG_PDWAIT */


struct zb3ctrl_info {
	struct file *file;
	int state;
};

static struct zb3ctrl_info info_list[ZB3_COUNT_MAX];

static void add_zb3ctrl_info(struct file *file)
{
	int i;
	for (i = 0; i < ZB3_COUNT_MAX ; i++) {
		if (!info_list[i].file) {
			info_list[i].file = file;
			info_list[i].state = ZB3_STATE_NONE;
			break;
		}
	}
}

static void del_zb3ctrl_info(struct file *file)
{
	int i;
	for (i = 0; i < ZB3_COUNT_MAX ; i++) {
		if (info_list[i].file == file) {
			info_list[i].file = NULL;
			info_list[i].state = ZB3_STATE_NONE;
			break;
		}
	}
}

static void set_zb3ctrl_info_state(struct file *file, int state)
{
	int i;
	for (i = 0; i < ZB3_COUNT_MAX ; i++) {
		if (info_list[i].file == file) {
			info_list[i].state = state;
			break;
		}
	}
}

static int get_zb3ctrl_info_state(void)
{
	int i;
	int state = ZB3_STATE_NONE;
	for (i = 0; i < ZB3_COUNT_MAX ; i++) {
		if (info_list[i].file) {
			state = info_list[i].state;
			break;
		}
	}
	return state;
}

static long zb3ctrl_ioctl(
struct file *file, unsigned int cmd, unsigned long arg)
{
	int val = -1;
	switch (cmd) {
	case ZB3_CTRL_REQ:
		get_user(val, (int __user *)arg);
		/* Low */
		if (val == 0) {
			set_zb3ctrl_info_state(file, ZB3_STATE_LOW);
			if (open_count == 1) {
				/* request change PDWAIT=0x08 */
				atomic_set(&pdwait_flag, 0);
				pdwait_judge();
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[zb3ctrl]: %s (zb3_state_low) set pdwait_flag=0\n", __func__);
#endif
				/* set 720p mode */
					movie_cpufreq(1);
			}
		/* High */
		} else {
			set_zb3ctrl_info_state(file, ZB3_STATE_HIGH);
			/* request change PDWAIT=0xFF */
			atomic_set(&pdwait_flag, 1);
			pdwait_judge();
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[zb3ctrl]: %s set pdwait_flag=1\n", __func__);
#endif
			/* set nomal mode */
			movie_cpufreq(0);
		}
		break;
	default:
		printk(KERN_ERR "%s: Unknown command 0x%x\n", __func__, cmd);
		break;
	}
	return 0 ;
}

static int zb3ctrl_open(struct inode *inode, struct file *file)
{
	add_zb3ctrl_info(file);
	open_count++;
	if (open_count >= 2) {
		atomic_set(&pdwait_flag, 1); /* request change PDWAIT=0xFF */
		pdwait_judge();
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[zb3ctrl]: %s set pdwait_flag=1\n", __func__);
#endif
	}
	/* set nomal mode */
	movie_cpufreq(0);
	return 0;
}

static int zb3ctrl_release(struct inode *inode, struct file *file)
{
	del_zb3ctrl_info(file);
	open_count--;
	if (open_count == 1) {
		if (get_zb3ctrl_info_state() == ZB3_STATE_LOW) {

			/*Have one 720p video playback */
			/* request change PDWAIT=0x08 */
			atomic_set(&pdwait_flag, 0);
			pdwait_judge();
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO"[zb3ctrl]: %s set pdwait_flag=0(open_count==1)\n", __func__);
#endif
			/* set 720p mode */
			movie_cpufreq(1);
		}
	} else {
		if (0 == open_count) {
			/* No 720p video playback */
			/* request change PDWAIT=0x08 */
			atomic_set(&pdwait_flag, 0);
			pdwait_judge();
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[zb3ctrl]: %s set pdwait_flag=0 (open_count==0)\n", __func__);
#endif
		}
		/* set nomal mode */
		movie_cpufreq(0);
	}
	return 0;
}

const struct file_operations zb3ctrl_fops = {
	.owner =          THIS_MODULE,
	.unlocked_ioctl = zb3ctrl_ioctl,
	.open =           zb3ctrl_open,
	.release =        zb3ctrl_release,
};

static struct cdev zb3ctrl_cdev;
static int major;
struct class *zb3ctrl_class;

static int __init init_zb3ctrl(void)
{
	int ret = 0;
	dev_t dev = 0;

	ret = alloc_chrdev_region(&dev, 0, 1, ZB3_DEV_NAME);
	if (ret < 0) {
		printk(KERN_ERR "%s() alloc_chrdev_region failed\n", __func__);
		return ret;
	}
	major = MAJOR(dev);
	open_count = 0;

	cdev_init(&zb3ctrl_cdev, &zb3ctrl_fops);
	zb3ctrl_cdev.owner = THIS_MODULE;
	zb3ctrl_cdev.ops = &zb3ctrl_fops;
	ret = cdev_add(&zb3ctrl_cdev, MKDEV(major, 0), 1);
	if (ret < 0) {
		printk(KERN_ERR "%s() cdev_add failed\n", __func__);
		return ret;
	}
	zb3ctrl_class = class_create(THIS_MODULE, "zb3ctrl");
	if (!zb3ctrl_class) {
		printk(KERN_ERR "%s() class_create\n", __func__);
		return -1;
	}
	device_create(zb3ctrl_class, NULL, MKDEV(major, 0), NULL, ZB3_DEV_NAME);
	memset(info_list, 0, sizeof(info_list));
	atomic_set(&pdwait_flag, 0);
	return 0;
}


static void __exit exit_zb3ctrl(void)
{
	device_destroy(zb3ctrl_class, MKDEV(major, 0));
	class_destroy(zb3ctrl_class);
	cdev_del(&zb3ctrl_cdev);
	unregister_chrdev_region(MKDEV(major, 0), 1);
}

module_init(init_zb3ctrl);
module_exit(exit_zb3ctrl);

MODULE_DESCRIPTION("zb3ctrl");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Renesas Mobile Corp.");

