/*
 * I2C Exclusive Access driver for Dialog D2153 I2C
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * Author: Jesu Anuroop Suresh
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <linux/atomic.h>
#include <mach/r8a7373.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/hwspinlock.h>
#include <linux/kthread.h>
#include <linux/d2153/core.h>

/* Static Variable Declaration */
struct hwspinlock *hwlock_adc;
struct hwspinlock *hwlock_i2c_hw0;
struct hwspinlock *hwlock_i2c_hw1;
static wait_queue_head_t d2153_modem_reset_event;
static struct task_struct *d2153_modem_reset_thread;
static atomic_t modem_reset_handing = ATOMIC_INIT(0);

#define D2153_APE_MODEM_HW_LOCK_RETRY	5		/* Retry max count for hwsem acquire */
#define EOS2_HWSEM_ID_SHIFT		24
#define EOS2_HWSEM_LEN			4		/* Register Length for ioremap */
#define EOS2_HWSEM_MASTER_ON		0xC0000000	/* Enable the master access for acquisition of HW sen */
#define EOS2_HWSEM_MASTER_OFF		0x0		/* Disable the master access for acquisition of HW sen */

/*
 * d2153_get_hw_sem_timeout() - lock an hwspinlock with timeout limit
 * @hwlock: the hwspinlock to be locked
 * @timeout: timeout value in msecs
 */
static int d2153_get_hwsem_timeout(struct hwspinlock *hwlock,
		unsigned int time_out)
{
	int ret;
	unsigned long expire;

	expire = msecs_to_jiffies(time_out) + jiffies;

	for (;;) {
		/* Try to take the hwspinlock */
		ret = hwspin_trylock_nospin(hwlock);

		if (ret == 0)
			break;

		/*
		 * The lock is already taken, try again
		 */
		if (time_is_before_eq_jiffies(expire))
			return -ETIMEDOUT;

		/*
		 * Wait 1 millisecond for another round
		 */
		mdelay(1);
	}

	return ret;
}

static int d2153_init_hwsem(unsigned int id)
{
	int ret = 0;
	struct hwspinlock *hwlock = NULL;

	hwlock = hwspin_lock_request_specific(id);

	if (hwlock == NULL) {
		pr_err("%s Unable to register hw spinlock for pmic driver\n",
			 __func__);
		ret = -EIO;
	} else {
		if (id == SMGP000)
			hwlock_adc = hwlock;
		else if (id == SMGP002)
			hwlock_i2c_hw0 = hwlock;
		else if (id == SMGP102)
			hwlock_i2c_hw1 = hwlock;
	}
	return ret;
}

static int d2153_get_hwsem(struct hwspinlock *hwlock, unsigned int id)
{
	int ret = -EBUSY;
	int count = D2153_APE_MODEM_HW_LOCK_RETRY;

	if (hwlock == NULL) {
		ret = d2153_init_hwsem(id);

		if (ret != 0 || hwlock == NULL)
			return ret;
	}

	for (;;) {
		ret = d2153_get_hwsem_timeout(hwlock, CONST_HPB_WAIT);

		if (ret == 0)
			break;
		if (count == 0) {
			pr_err("%s: HW Sem[%d] Acquired Failed\r\n",
				__func__, id);
			return -EBUSY;
		}
		count--;
		mdelay(1);
	}
	return ret;
}

static void d2153_put_hwsem(struct hwspinlock *hwlock)
{
	if (hwlock)
		hwspin_unlock_nospin(hwlock);
}

int d2153_get_adc_hwsem(void)
{
	return d2153_get_hwsem(hwlock_adc, SMGP000);
}
EXPORT_SYMBOL(d2153_get_adc_hwsem);

int d2153_get_i2c_hwsem(void)
{
	int ret;
	ret = d2153_get_hwsem(hwlock_i2c_hw0, SMGP002);

	if (ret == 0) {
		ret = d2153_get_hwsem(hwlock_i2c_hw1, SMGP102);

		if (ret != 0)
			d2153_put_hwsem(hwlock_i2c_hw0);
	}
	return ret;
}
EXPORT_SYMBOL(d2153_get_i2c_hwsem);

void d2153_put_adc_hwsem(void)
{
	d2153_put_hwsem(hwlock_adc);
}
EXPORT_SYMBOL(d2153_put_adc_hwsem);

void d2153_put_i2c_hwsem(void)
{
	d2153_put_hwsem(hwlock_i2c_hw0);
	d2153_put_hwsem(hwlock_i2c_hw1);
}
EXPORT_SYMBOL(d2153_put_i2c_hwsem);

/*
 * d2153_force_release_hwsem() - force to release hw semaphore
 * @hwsem_id: Hardware semaphore ID
		0x01: AP Realtime side
 *		0x40: AP System side
 *		0x93: Baseband side
 * @hwsem: Hardware semaphore address
 *	        HPB_SEM_PMIC_PHYS
 *		HPB_SEM_SMGP1SRC_PHYS
 * return: void
 */
static void d2153_force_release_hwsem(u8 hwsem_id, unsigned int hwsem)
{
	void __iomem *ptr;
	u32 value = 0;
	unsigned long expire = msecs_to_jiffies(5) + jiffies;

	/*Check input hwsem_id*/
	switch (hwsem_id) {
	case RT_CPU_SIDE:
	case SYS_CPU_SIDE:
	case BB_CPU_SIDE:
		break;
	default:
		return;
	}

	ptr = ioremap(hwsem, EOS2_HWSEM_LEN);
	if (ptr == NULL) {
		dlg_err("%s: can not release hwsem 0x%x ioremap failed",
			 __func__, hwsem);
		return;
	}
	value = ioread32(ptr);
	iounmap(ptr);

	dlg_info("%s: ID (0x%x) is using HW semaphore\n", \
			__func__, value >> EOS2_HWSEM_ID_SHIFT);

	if ((value >> EOS2_HWSEM_ID_SHIFT) != hwsem_id)
		return;

	/*enable master access*/
	ptr = ioremap(HPB_SEM_MPACCTL_PHYS, EOS2_HWSEM_LEN);
	if (ptr == NULL) {
		dlg_err("%s: can not release hwsem 0x%x ioremap failed",
			 __func__, HPB_SEM_MPACCTL_PHYS);
		return;
	}
	for (;;) {

		/* Try to enable master access */
		iowrite32(EOS2_HWSEM_MASTER_ON, ptr);
		value = ioread32(ptr);
		if (value == EOS2_HWSEM_MASTER_ON) {
			iounmap(ptr);
			break;
		}

		/*
		 * Cannot enable master access, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			iounmap(ptr);
			return;
		}

		/*
		 * Wait 50 nanosecond for another round
		 */
		ndelay(50);
	}

	/*Force clear HW sem*/
	expire = msecs_to_jiffies(5) + jiffies;

	ptr = ioremap(hwsem, EOS2_HWSEM_LEN);
	if (ptr == NULL) {
		dlg_err("%s: can not release hwsem 0x%x ioremap failed",
			 __func__, hwsem);
		return;
	}
	for (;;) {
		/* Try to force clear HW sem */
		iowrite32(0, ptr);
		value = ioread32(ptr);
		if (value == 0x0) {
			dlg_err("%s: Force to release HW sem from ID 0x%x) successful\n",
				__func__, hwsem_id);
			break;
		}

		/*
		 * Cannot force clear HW sem, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			dlg_err(
				"%s: Fail to release HW sem from ID (0x%x)\n",
					__func__, hwsem_id);
			break;
		}

		/*
		 * Wait 50 nanosecond for another round
		 */
		ndelay(50);
	}
	iounmap(ptr);

	/*Disable master access*/
	expire = msecs_to_jiffies(5) + jiffies;
	ptr = ioremap(HPB_SEM_MPACCTL_PHYS, EOS2_HWSEM_LEN);
	if (ptr == NULL) {
		dlg_err("%s: can not release hwsem 0x%x ioremap failed",
			 __func__, HPB_SEM_MPACCTL_PHYS);
		return;
	}
	for (;;) {
		/* Try to disable master access */
		iowrite32(EOS2_HWSEM_MASTER_OFF, ptr);
		value = ioread32(ptr);
		if (value == EOS2_HWSEM_MASTER_OFF) {
			iounmap(ptr);
			break;
		}

		/*
		 * Cannot disable master access, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			iounmap(ptr);
			return;
		}

		/*
		 * Wait 50 nanosecond for another round
		 */
		ndelay(50);
	}
}

/*
 * d2153_force_release_swsem() - force to release sw semaphore
 * @swsem_id: Software semaphore ID
		0x01: AP Realtime side
 *		0x40: AP System side
 *		0x93: Baseband side
 * return: void
 */
static void d2153_force_release_swsem(struct hwspinlock *hwlock, u8 swsem_id)
{
	u32 lock_id;
	unsigned long expire = msecs_to_jiffies(10) + jiffies;

	/*Check input swsem_id*/
	switch (swsem_id) {
	case RT_CPU_SIDE:
	case SYS_CPU_SIDE:
	case BB_CPU_SIDE:
		break;
	default:
		return;
	}

	/* Check which CPU (Real time or Baseband or System) is using SW sem*/
	lock_id = hwspin_get_lock_id_nospin(hwlock);

	dlg_info("%s: ID (0x%x) is using SW semaphore\n", \
				__func__, lock_id);

	if (lock_id != swsem_id)
		return;

	for (;;) {
		/* Try to force to unlock SW sem*/
		d2153_put_hwsem(hwlock);
		lock_id = hwspin_get_lock_id_nospin(hwlock);
		if (lock_id == 0) {
			dlg_err(
		"%s: Forcing to release SW sem from ID (0x%x) is successful\n",
				__func__, swsem_id);
			break;
		}

		/*
		 * Cannot force to unlock SW sem, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			dlg_err(
				"%s: Fail to release HW sem from ID (0x%x)\n",
					__func__, swsem_id);
			return;
		}

		/*
		 * Wait 100 nanosecond for another round
		 */
		ndelay(100);
	}

}

/*
 * d2153_handle_modem_reset:
 * return: void
 */
void d2153_handle_modem_reset(void)
{
	atomic_set(&modem_reset_handing, 1);
	wake_up_interruptible(&d2153_modem_reset_event);
}
EXPORT_SYMBOL(d2153_handle_modem_reset);

/*
 * d2153_modem_reset: start thread to handle modem reset
 * @ptr:
 * return: 0
 */
static int d2153_modem_thread(void *ptr)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(d2153_modem_reset_event,
					atomic_read(&modem_reset_handing));

		d2153_force_release_hwsem(BB_CPU_SIDE, HPB_SEM_PMIC_PHYS);
		if (hwlock_adc)
			d2153_force_release_swsem(hwlock_adc, BB_CPU_SIDE);
		if (hwlock_i2c_hw0)
			d2153_force_release_swsem(hwlock_i2c_hw0, BB_CPU_SIDE);

		d2153_force_release_hwsem(BB_CPU_SIDE, HPB_SEM_SMGP1SRC_PHYS);
		if (hwlock_i2c_hw1)
			d2153_force_release_swsem(hwlock_i2c_hw1, BB_CPU_SIDE);

		atomic_set(&modem_reset_handing, 0);
	}
	return 0;
}

int d2153_hw_sem_reset_init(void)
{
	int ret = 0;

	/* Init thread to handle modem reset */
	init_waitqueue_head(&d2153_modem_reset_event);
	d2153_modem_reset_thread = kthread_run(d2153_modem_thread,
					NULL, "d2153_modem_reset_thread");
	if (NULL == d2153_modem_reset_thread) {
		dlg_err("%s:%d d2153_modem_reset_thread failed\n",\
				__func__, __LINE__);
		return -ENOMEM;
	}

	/* Initialize the hw semaphore */
	if (hwlock_adc == NULL)
		ret |= d2153_init_hwsem(SMGP000);
	if (hwlock_i2c_hw0 == NULL)
		ret |= d2153_init_hwsem(SMGP002);
	if (hwlock_i2c_hw1 == NULL)
		ret |= d2153_init_hwsem(SMGP102);

	pr_info("%s. End...\n", __func__);
	return ret;
}

int d2153_hw_sem_reset_deinit(void)
{
	int ret = 0;

	if (hwlock_adc) {
		ret |= hwspin_lock_free(hwlock_adc);
		hwlock_adc = NULL;
	}
	if (hwlock_i2c_hw0) {
		ret |= hwspin_lock_free(hwlock_i2c_hw0);
		hwlock_i2c_hw0 = NULL;
	}
	if (hwlock_i2c_hw1) {
		ret |= hwspin_lock_free(hwlock_i2c_hw1);
		hwlock_i2c_hw1 = NULL;
	}
	return ret;
}
