/*
 * /drivers/misc/vibrator/vibrator.c
 *
 * Copyright (C) 2011-2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <mach/r8a7373.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/tpu_pwm.h>
#include <linux/pcm2pwm.h>
#include <linux/vibrator.h>
#include "../../../drivers/staging/android/timed_output.h"

/****************************  MACRO   ************************************/
#define TPU_CHANNEL		"TPU0TO0"
#define CLK_SOURCE_CP		0 /* CP clock Common Peripheral*/

#if defined(VIB_DCMOTOR) /* In case use Linear Motor */
#define VIB_OVERDRV_TIME 40 /* Over-drive duration of DC motor (ms) */
#else /* !defined(VIB_DCMOTOR) */
#define VIB_OVERDRV_TIME 20 /* Over-drive duration of Linear motor (ms) */
#endif /* defined(VIB_DCMOTOR) */

#define VIB_DUTY_OVERDRV_ON	0 /* Duty cycle when turning on DC motor */
#define VIB_DUTY_VIBRATE	0x0100 /* Duty cycle when vibrating */
#define VIB_CYCLE		0x0400 /* Value of TGRB to set the cycle */

#define SAMPLE_NUM		133
#define SAMPLE_SIZE		(SAMPLE_NUM * sizeof(unsigned short))
#define VIB_CNT_LINEAR		1024
#define VIB_CNT_OFF		1600
#define VIB_CNT_75		4
#define VIB_CNT_ON		1
#define USER_TURN_OFF		0
#define USER_TURN_ON		1
#define HRTIMER_REQUEST		2
#define HIGH			1
#define LOW			0

#define VIB_LOG(msg, ...) \
	printk(KERN_ERR "%s(%d):" msg, __func__, __LINE__, ## __VA_ARGS__)
/*************************** FUNCTION PROTOTYPE  ****************************/
static int __init init_vibrator(void);
static void __exit exit_vibrator(void);
static int __devinit vibrator_probe(struct platform_device *pdev);
static int __devexit vibrator_remove(struct platform_device *pdev);



static void vibrator_enable(struct timed_output_dev *sdev, int timeout);
static int vib_get_time(struct timed_output_dev *sdev);
static enum hrtimer_restart vib_timer_function(struct hrtimer *timer);
static int vibrate_control_thread(void *arg);
static struct vibrator_port_info *pinfo;

struct vibrator_single_request {
	struct list_head node;
	int object_request;
	int request_state;
	int timeout;
};

static struct platform_driver vibrator_platform_driver = {
	.probe   = vibrator_probe,
	.remove  = __devexit_p(vibrator_remove),
	.driver    = {
		.name  = "vibrator-renesas-sh_mobile",
		.owner = THIS_MODULE,
	},
};


enum vibration_state {
	NO_VIBRATE,	/* Status when there is no operation of DC motor */
	TURN_ON,	/* Status when DC motor begins the operation */
	VIBRATE,	/* Status when DC motor is in stable operation */
	TURN_OFF	/* Status when DC motor finishes the operation */
};

/*********************** GLOBAL VARIABLE  ************************************/
static struct hrtimer vib_timer;
static struct timed_output_dev vib_device; /* Vibrator device object */
static int vib_state = NO_VIBRATE;
static spinlock_t vibrator_spinlock;
static int vib_duration; /* Vibrate duration in millisecond */
static LIST_HEAD(vibrator_list_head);
wait_queue_head_t vib_queue;

#if defined(VIB_DCMOTOR) /* In case use DC Motor */
/* DC Motor*/
static const unsigned short const_pcm[SAMPLE_NUM] = {3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
				3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
#else /* Linear Motor */
struct work_struct vibrator_work_on_pcm2pwm;
struct work_struct vibrator_work_off_pcm2pwm;
/* sine wave PCM data (Linear Motor) */
static const unsigned short wave_pcm[SAMPLE_NUM] = {0x0200, 0x020C, 0x0218,
			0x0223, 0x022F, 0x023B, 0x0247, 0x0252, 0x025E, 0x0269,
			0x0273, 0x027E, 0x0288, 0x0292, 0x029C, 0x02A5, 0x02AE,
			0x02B7, 0x02BF, 0x02C7, 0x02CE, 0x02D5, 0x02DB, 0x02E1,
			0x02E7, 0x02EC, 0x02F0, 0x02F4, 0x02F7, 0x02FA, 0x02FC,
			0x02FE, 0x02FF, 0x02FF, 0x02FF, 0x02FF, 0x02FE, 0x02FC,
			0x02FA, 0x02F7, 0x02F3, 0x02EF, 0x02EB, 0x02E6, 0x02E1,
			0x02DB, 0x02D4, 0x02CD, 0x02C6, 0x02BE, 0x02B6, 0x02AD,
			0x02A4, 0x029B, 0x0291, 0x0287, 0x027C, 0x0272, 0x0267,
			0x025C, 0x0250, 0x0245, 0x0239, 0x022D, 0x0222, 0x0216,
			0x020A, 0x01FE, 0x01F2, 0x01E6, 0x01DA, 0x01CE, 0x01C2,
			0x01B6, 0x01AB, 0x01A0, 0x0195, 0x018A, 0x017F, 0x0175,
			0x016B, 0x0161, 0x0158, 0x014F, 0x0147, 0x013F, 0x0137,
			0x0130, 0x0129, 0x0123, 0x011D, 0x0117, 0x0113, 0x010E,
			0x010B, 0x0107, 0x0105, 0x0103, 0x0101, 0x0100, 0x0100,
			0x0100, 0x0100, 0x0102, 0x0103, 0x0106, 0x0109, 0x010C,
			0x0110, 0x0115, 0x011A, 0x011F, 0x0125, 0x012C, 0x0133,
			0x013A, 0x0142, 0x014B, 0x0153, 0x015D, 0x0166, 0x0170,
			0x017A, 0x0184, 0x018F, 0x019A, 0x01A5, 0x01B0, 0x01BC,
			0x01C8, 0x01D3, 0x01DF, 0x01EB};
#endif /* defined(VIB_DCMOTOR) */


/*********************** FUNCTION IMPLEMENT   ********************************/
/*
* vib_get_time: Get the current number of milliseconds remaining on the timer
* @sdev		: Pointer to structure timed_output_dev
* return	: the current number of milliseconds remaining on the timer
* Note		: This function use for TPU and PCM2PWM driver
*/
static int vib_get_time(struct timed_output_dev *sdev)
{
	if (hrtimer_active(&vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib_timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;
}

/*
* vib_timer_function: Timer expiry callback function
* @timer:
* return:
*	HRTIMER_NORESTART : Timer is not restarted (actual value: 0)
*	HRTIMER_RESTART : Timer must be restarted (actual value: 1)
*/
static enum hrtimer_restart vib_timer_function(struct hrtimer *timer)
{
	struct vibrator_single_request *request;
	request = (struct vibrator_single_request *)
		kzalloc(sizeof(struct vibrator_single_request), GFP_KERNEL);
	if (request == NULL) {
		VIB_LOG("failed to allocate memory\n");
		return HRTIMER_NORESTART;
	}

	/* identify this request by HRTIMER */
	request->object_request = HRTIMER_REQUEST;

	if (TURN_ON == vib_state) {
		/* request change to VIBRATE state */
		request->request_state = VIBRATE;
	} else if (VIBRATE == vib_state) {
		/* request change to TURN_OFF state */
		request->request_state = TURN_OFF;
	} else if (TURN_OFF == vib_state) {
		/* request change to NO_VIBRATE state */
		request->request_state = NO_VIBRATE;
	} else {
		/* vib_state in another state, not in above state */
		kfree(request);
		return HRTIMER_NORESTART;
	}

	list_add_tail(&request->node, &vibrator_list_head);
	wake_up_interruptible(&vib_queue);
	return HRTIMER_NORESTART;
}

/*
* vibrator_enable: Handle request Turn off vibration,
*			or turn on vibration for a specified duration
* @sdev		: Pointer to structure timed_output_dev
* @timeout	: The duration of vibration
* return	: the current number of milliseconds remaining on the timer
*/
static void vibrator_enable(struct timed_output_dev *sdev, int timeout)
{
	struct vibrator_single_request *request;
	struct vibrator_single_request *request_off;
	unsigned long flags;

	if (timeout < 0) {
		VIB_LOG("timeout value is negative = %d\n", timeout);
		return;
	}

	request = (struct vibrator_single_request *)
		kzalloc(sizeof(struct vibrator_single_request), GFP_KERNEL);
	if (NULL == request) {
		VIB_LOG("failed to allocate memory\n");
		return;
	}

	if (!timeout) {
		request->object_request = USER_TURN_OFF;
	} else if (timeout > 0) {
		request->object_request = USER_TURN_ON;
		/* manual add request turn-off before turn-on */
		request_off = (struct vibrator_single_request *)
			kzalloc(sizeof(struct vibrator_single_request),
					GFP_KERNEL);
		if (NULL == request_off) {
			kfree(request);
			VIB_LOG("failed to allocate memory\n");
			return;
		}
		request_off->object_request = USER_TURN_OFF;
		request_off->timeout = 0;
		spin_lock_irqsave(&vibrator_spinlock, flags);
		list_add_tail(&request_off->node, &vibrator_list_head);
		spin_unlock_irqrestore(&vibrator_spinlock, flags);
	}
	request->timeout = timeout;
	spin_lock_irqsave(&vibrator_spinlock, flags);
	list_add_tail(&request->node, &vibrator_list_head);
	wake_up_interruptible(&vib_queue);
	spin_unlock_irqrestore(&vibrator_spinlock, flags);
}

/*
* init_vibrator: init vibrator driver
* return:
	0: if no error
	other value if error
*/
static int __devinit vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct task_struct *vib_thread;
	
	pinfo = pdev->dev.platform_data ;
	/* Initializing vibrator device object */
	vib_device.name = "vibrator";
	vib_device.enable = vibrator_enable;
	vib_device.get_time = vib_get_time;
#if !defined(VIB_PCM2PWM) /* use TPU driver */
	ret = gpio_request(pinfo->tpu_port, NULL);
	if (ret) {
		VIB_LOG("request pinfo->tpu_port error\n");
		return ret;
	}

	ret = gpio_direction_output(pinfo->tpu_port, LOW);
	if (ret) {
		VIB_LOG("set direction pinfo->tpu_port error\n");
		return ret;
	}
#else /* Use PCM2PWM driver */
	ret = gpio_request(pinfo->pcm2pwm_port, NULL);
	if (ret) {
		VIB_LOG("request pinfo->pcm2pwm_port error\n");
		return ret;
	}

	ret = gpio_direction_output(pinfo->pcm2pwm_port, LOW);
	if (ret) {
		VIB_LOG("set direction pinfo->pcm2pwm_port error\n");
		return ret;
	}
#endif /* !defined(VIB_PCM2PWM) */

	ret = gpio_request(pinfo->vibrator_port, NULL);
	if (ret) {
		VIB_LOG("request pinfo->vibrator_port error\n");
		return ret;
	}

	ret = gpio_direction_output(pinfo->vibrator_port, LOW);
	if (ret) {
		VIB_LOG("set direction pinfo->vibrator_port error\n");
		return ret;
	}

	/* Register vibrator to timed_output device */
	ret = timed_output_dev_register(&vib_device);
	if (ret) {
		VIB_LOG("register timed_output_dev failded\n");
		return ret;
	}

	/* init spinlock */
	spin_lock_init(&vibrator_spinlock);

	/* init hrtimer */
	hrtimer_init(&vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	/* Specify timer expiry callback function */
	vib_timer.function = vib_timer_function;
	vib_state = NO_VIBRATE;

	/* init static variable */
	vib_duration = 0;

	/* init thread handle vibrator request */
	init_waitqueue_head(&vib_queue);

	vib_thread = kthread_create(vibrate_control_thread, NULL, "vib_thread");
	if (IS_ERR(vib_thread)) {
		VIB_LOG("can not create vib_thread\n");
		return -ENOMEM;
	}
	wake_up_process(vib_thread);
	return ret;
}

/*=============================================================================
*				DC MOTOR
=============================================================================*/
#if defined(VIB_DCMOTOR)
#if !defined(VIB_PCM2PWM) /* Case using TPU */
/*
* vibrate_control_thread: thread to turn on/off tpu
* @arg :
* note		: This function use for TPU driver with DC Motor
*/
static int vibrate_control_thread(void *arg)
{
	struct vibrator_single_request *request;
	static void *tpu_handle;
	static int on_duration;
	static int remain_time;
	static int off_duration;
	static int flag_vib;
	int ret = 0;
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(vib_queue,
					!list_empty(&vibrator_list_head));
		if (ret < 0 || kthread_should_stop()) {
			VIB_LOG("Stop vibrate_control_thread\n");
			break;
		}

		spin_lock(&vibrator_spinlock);
		/* get first node in list */
		if (!list_empty(&vibrator_list_head)) {
			request = list_first_entry(&vibrator_list_head,
					struct vibrator_single_request, node);
			list_del(&request->node);
		} else {
			VIB_LOG("vibrator_list_head is empty\n");
			break;
		}
		spin_unlock(&vibrator_spinlock);

		if (HRTIMER_REQUEST == request->object_request) {
			/* processing hrtimer request */
			if ((TURN_ON == vib_state)
				&& (VIBRATE == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				if (0 == vib_duration) {
					/* turn_off immediately */
					ret = tpu_pwm_enable(tpu_handle,
							TPU_PWM_STOP, 0, 0);
					if (ret) {
						tpu_pwm_close(tpu_handle);
						tpu_handle = NULL;
						kfree(request);
						break;
					}
					ret = tpu_pwm_close(tpu_handle);
					if (ret) {
						VIB_LOG("tpu_pwm_close = %d\n",
								ret);
						tpu_handle = NULL;
						kfree(request);
						break;
					}
					tpu_handle = NULL;

					spin_lock(&vibrator_spinlock);
					vib_state = TURN_OFF;
					spin_unlock(&vibrator_spinlock);

					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				} else {
					/* set tpu to duty cycle is 75% */
					ret = tpu_pwm_enable(tpu_handle,
							TPU_PWM_START,
							VIB_DUTY_VIBRATE,
							VIB_CYCLE);
					if (ret) {
						VIB_LOG("tpu_pwm_enable = %d\n",
								ret);
						tpu_pwm_close(tpu_handle);
						tpu_handle = NULL;
						kfree(request);
						break;
					}
					spin_lock(&vibrator_spinlock);
					vib_state = VIBRATE;
					spin_unlock(&vibrator_spinlock);
					flag_vib = 1;
					/* vibrate in timeout */
					hrtimer_start(&vib_timer,
						ktime_set(vib_duration / 1000,
						(vib_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			} else if ((VIBRATE == vib_state)
				&& (TURN_OFF == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				/* turn off and close tpu */
				ret = tpu_pwm_enable(tpu_handle,
							TPU_PWM_STOP, 0, 0);
				if (ret) {
					VIB_LOG("tpu_pwm_enable = %d\n", ret);
					tpu_pwm_close(tpu_handle);
					tpu_handle = NULL;
					kfree(request);
					break;
				}

				ret = tpu_pwm_close(tpu_handle);
				if (ret) {
					VIB_LOG("call tpu_pwm_close = %d\n",
								ret);
					tpu_handle = NULL;
					kfree(request);
					break;
				}
				tpu_handle = NULL;

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				off_duration = VIB_OVERDRV_TIME;
				/* start hrtimer for 40ms */
				hrtimer_start(&vib_timer,
					ktime_set(VIB_OVERDRV_TIME / 1000,
					(VIB_OVERDRV_TIME % 1000) * 1000000),
					HRTIMER_MODE_REL);


			} else if ((TURN_OFF == vib_state)
				&& (NO_VIBRATE == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				ret = gpio_direction_output(pinfo->vibrator_port, LOW);
				if (ret) {
					VIB_LOG("gpio_direction_output\n");
					kfree(request);
					break;
				}
				flag_vib = 0;
				spin_lock(&vibrator_spinlock);
				vib_state = NO_VIBRATE;
				spin_unlock(&vibrator_spinlock);
			}
		} else if (USER_TURN_ON == request->object_request) {
			/* User request to turn ON*/
			if (NO_VIBRATE == vib_state) {
				hrtimer_cancel(&vib_timer);

				ret = tpu_pwm_open(TPU_CHANNEL,
						CLK_SOURCE_CP, &tpu_handle);
				if (ret) {
					VIB_LOG("tpu_pwm_open = %d\n", ret);
					kfree(request);
					break;
				}

				ret = tpu_pwm_enable(tpu_handle, TPU_PWM_START,
						VIB_DUTY_OVERDRV_ON, VIB_CYCLE);
				if (ret) {
					VIB_LOG("tpu_pwm_enable = %d\n", ret);
					tpu_pwm_close(tpu_handle);
					kfree(request);
					tpu_handle = NULL;
					break;
				}

				ret = gpio_direction_output(pinfo->vibrator_port, HIGH);
				if (ret) {
					VIB_LOG("gpio_direction_output\n");
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_ON;
				spin_unlock(&vibrator_spinlock);

				if (VIB_OVERDRV_TIME < request->timeout) {
					/* vibrate request duration > 40ms */
					vib_duration = request->timeout - VIB_OVERDRV_TIME;
					on_duration = VIB_OVERDRV_TIME;

					hrtimer_start(&vib_timer,
						ktime_set(VIB_OVERDRV_TIME / 1000,
						(VIB_OVERDRV_TIME % 1000) * 1000000),
						HRTIMER_MODE_REL);

				} else {
					/* 40 >= vibrate request duration */
					vib_duration = 0;
					on_duration = request->timeout;

					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			} else if (TURN_OFF == vib_state) {
				if (hrtimer_active(&vib_timer)) {
					ktime_t r = hrtimer_get_remaining(&vib_timer);
					struct timeval t = ktime_to_timeval(r);
					remain_time = (t.tv_sec * 1000)
							+ (t.tv_usec / 1000);
				} else {
					remain_time = 0;
				}
				hrtimer_cancel(&vib_timer);

				on_duration = off_duration - remain_time;

				ret = tpu_pwm_open(TPU_CHANNEL,
						CLK_SOURCE_CP, &tpu_handle);
				if (ret) {
					VIB_LOG("tpu_pwm_open = %d\n", ret);
					kfree(request);
					break;
				}

				ret = tpu_pwm_enable(tpu_handle, TPU_PWM_START,
						VIB_DUTY_OVERDRV_ON, VIB_CYCLE);
				if (ret) {
					VIB_LOG("tpu_pwm_enable = %d\n", ret);
					tpu_pwm_close(tpu_handle);
					kfree(request);
					tpu_handle = NULL;
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_ON;
				spin_unlock(&vibrator_spinlock);

				if (on_duration < request->timeout) {
					if (request->timeout > 40 || flag_vib == 1) {
						vib_duration = request->timeout
								- on_duration;

						hrtimer_start(&vib_timer,
							ktime_set(on_duration / 1000,
							(on_duration % 1000) * 1000000),
							HRTIMER_MODE_REL);
					} else {
						vib_duration = 0;
						on_duration = request->timeout;
						hrtimer_start(&vib_timer,
							ktime_set(on_duration / 1000,
							(on_duration % 1000) * 1000000),
							HRTIMER_MODE_REL);
					}
				} else {
					/* on_duration >= vibrate request duration */
					vib_duration = 0;
					on_duration = request->timeout;
					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}

			}
		} else if (USER_TURN_OFF == request->object_request) {
			/* User request to turn off */
			if (TURN_ON == vib_state) {
				if (hrtimer_active(&vib_timer)) {
					ktime_t r = hrtimer_get_remaining(&vib_timer);
					struct timeval t = ktime_to_timeval(r);
					remain_time = (t.tv_sec * 1000)
							+ t.tv_usec / 1000;
				} else {
					remain_time = 0;
				}
				hrtimer_cancel(&vib_timer);

				off_duration = on_duration - remain_time;

				ret = tpu_pwm_enable(tpu_handle,
							TPU_PWM_STOP, 0, 0);
				if (ret) {
					VIB_LOG("tpu_pwm_enable = %d\n", ret);
					tpu_pwm_close(tpu_handle);
					tpu_handle = NULL;
					kfree(request);
					break;
				}
				ret = tpu_pwm_close(tpu_handle);
				if (ret) {
					VIB_LOG("tpu_pwm_close = %d\n", ret);
					tpu_handle = NULL;
					kfree(request);
					break;
				}
				tpu_handle = NULL;

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				on_duration = 0;

				hrtimer_start(&vib_timer,
					ktime_set(off_duration / 1000,
					(off_duration % 1000) * 1000000),
					HRTIMER_MODE_REL);
			} else if (VIBRATE == vib_state) {
				/* turn off and close tpu */
				hrtimer_cancel(&vib_timer);
				ret = tpu_pwm_enable(tpu_handle,
							TPU_PWM_STOP, 0, 0);
				if (ret) {
					VIB_LOG("tpu_pwm_enable = %d\n", ret);
					tpu_pwm_close(tpu_handle);
					tpu_handle = NULL;
					kfree(request);
					break;
				}
				ret = tpu_pwm_close(tpu_handle);
				if (ret) {
					VIB_LOG("tpu_pwm_close = %d\n", ret);
					tpu_handle = NULL;
					kfree(request);
					break;
				}
				tpu_handle = NULL;

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				off_duration = VIB_OVERDRV_TIME;
				on_duration = 0;
				hrtimer_start(&vib_timer,
					ktime_set(VIB_OVERDRV_TIME / 1000,
					(VIB_OVERDRV_TIME % 1000) * 1000000),
					HRTIMER_MODE_REL);
			}
		}
		kfree(request);
	}

	ret = gpio_direction_output(pinfo->vibrator_port, LOW);
	if (ret) {
		VIB_LOG("gpio_direction_output\n");
	}
	VIB_LOG("vib_thread is STOPPED\n");
	return ret;
}
#else /* use PCM2PWM  defined(VIB_PCM2PWM)*/

/*
* vibrate_control_thread: thread to turn on/off tpu
* @arg :
* note		: This function use for PCM2PWM driver with DC Motor
*/
static int vibrate_control_thread(void *arg)
{
	struct vibrator_single_request *request;
	static int on_duration;
	static int remain_time;
	static int off_duration;
	static int flag_vib;
	int ret = 0;
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(vib_queue,
					!list_empty(&vibrator_list_head));
		if (ret < 0 || kthread_should_stop()) {
			VIB_LOG("Stop vibrate_control_thread\n");
			break;
		}

		spin_lock(&vibrator_spinlock);
		/* get first node in list */
		if (!list_empty(&vibrator_list_head)) {
			request = list_first_entry(&vibrator_list_head,
					struct vibrator_single_request, node);
			list_del(&request->node);
		} else {
			VIB_LOG("vibrator_list_head is empty\n");
			break;
		}
		spin_unlock(&vibrator_spinlock);

		if (HRTIMER_REQUEST == request->object_request) {
			/* processing hrtimer request */
			if ((TURN_ON == vib_state)
				&& (VIBRATE == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				if (0 == vib_duration) {
					/* turn_off immediately */
					ret = pcm2pwm_enable(STOP_PCM2PWM,
							const_pcm, SAMPLE_SIZE,
							VIB_CNT_OFF);
					if (ret) {
						ret = pcm2pwm_close();
						kfree(request);
						break;
					}
					ret = pcm2pwm_close();
					if (ret) {
						VIB_LOG("pcm2pwm_close = %d\n",
								ret);
						kfree(request);
						break;
					}

					spin_lock(&vibrator_spinlock);
					vib_state = TURN_OFF;
					spin_unlock(&vibrator_spinlock);

					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				} else {
					/* set pcm2pwm to duty cycle is 75% */
					ret = pcm2pwm_enable(START_PCM2PWM,
							const_pcm, SAMPLE_SIZE,
							VIB_CNT_75);
					if (ret) {
						VIB_LOG("pcm2pwm_enable = %d\n",
								ret);
						pcm2pwm_close();
						kfree(request);
						break;
					}
					spin_lock(&vibrator_spinlock);
					vib_state = VIBRATE;
					spin_unlock(&vibrator_spinlock);
					flag_vib = 1;
					/* vibrate in timeout */
					hrtimer_start(&vib_timer,
						ktime_set(vib_duration / 1000,
						(vib_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			} else if ((VIBRATE == vib_state)
				&& (TURN_OFF == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				/* turn off and close pcm2pwm */
				ret = pcm2pwm_enable(STOP_PCM2PWM,
							const_pcm, SAMPLE_SIZE,
							VIB_CNT_OFF);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}

				ret = pcm2pwm_close();
				if (ret) {
					VIB_LOG("pcm2pwm_close = %d\n", ret);
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				off_duration = VIB_OVERDRV_TIME;
				/* start hrtimer for 40ms */
				hrtimer_start(&vib_timer,
					ktime_set(VIB_OVERDRV_TIME / 1000,
					(VIB_OVERDRV_TIME % 1000) * 1000000),
					HRTIMER_MODE_REL);


			} else if ((TURN_OFF == vib_state)
				&& (NO_VIBRATE == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				ret = gpio_direction_output(pinfo->vibrator_port, LOW);
				if (ret) {
					VIB_LOG("gpio_direction_output\n");
					kfree(request);
					break;
				}
				flag_vib = 0;
				spin_lock(&vibrator_spinlock);
				vib_state = NO_VIBRATE;
				spin_unlock(&vibrator_spinlock);
			}
		} else if (USER_TURN_ON == request->object_request) {
			/* User request to turn ON*/
			if (NO_VIBRATE == vib_state) {
				hrtimer_cancel(&vib_timer);

				ret = pcm2pwm_open();
				if (ret) {
					VIB_LOG("pcm2pwm_open = %d\n", ret);
					kfree(request);
					break;
				}

				ret = pcm2pwm_enable(START_PCM2PWM, const_pcm,
							SAMPLE_SIZE, VIB_CNT_ON);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}

				ret = gpio_direction_output(pinfo->vibrator_port, HIGH);
				if (ret) {
					VIB_LOG("gpio_direction_output\n");
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_ON;
				spin_unlock(&vibrator_spinlock);

				if (VIB_OVERDRV_TIME < request->timeout) {
					/* vibrate request duration > 40ms */
					vib_duration = request->timeout - VIB_OVERDRV_TIME;
					on_duration = VIB_OVERDRV_TIME;

					hrtimer_start(&vib_timer,
						ktime_set(VIB_OVERDRV_TIME / 1000,
						(VIB_OVERDRV_TIME % 1000) * 1000000),
						HRTIMER_MODE_REL);

				} else {
					/* 40 >= vibrate request duration */
					vib_duration = 0;
					on_duration = request->timeout;

					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			} else if (TURN_OFF == vib_state) {
				if (hrtimer_active(&vib_timer)) {
					ktime_t r = hrtimer_get_remaining(&vib_timer);
					struct timeval t = ktime_to_timeval(r);
					remain_time = (t.tv_sec * 1000)
							+ (t.tv_usec / 1000);
				} else {
					remain_time = 0;
				}
				hrtimer_cancel(&vib_timer);

				on_duration = off_duration - remain_time;

				ret = pcm2pwm_open();
				if (ret) {
					VIB_LOG("pcm2pwm_open = %d\n", ret);
					kfree(request);
					break;
				}

				ret = pcm2pwm_enable(START_PCM2PWM, const_pcm,
							SAMPLE_SIZE, VIB_CNT_ON);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_ON;
				spin_unlock(&vibrator_spinlock);

				if (on_duration < request->timeout) {
					if (request->timeout > 40 || flag_vib == 1) {
						vib_duration = request->timeout
								- on_duration;

						hrtimer_start(&vib_timer,
							ktime_set(on_duration / 1000,
							(on_duration % 1000) * 1000000),
							HRTIMER_MODE_REL);
					} else {
						vib_duration = 0;
						on_duration = request->timeout;
						hrtimer_start(&vib_timer,
							ktime_set(on_duration / 1000,
							(on_duration % 1000) * 1000000),
							HRTIMER_MODE_REL);
					}
				} else {
					/* on_duration > vibrate request duration */
					vib_duration = 0;
					on_duration = request->timeout;
					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			}
		} else if (USER_TURN_OFF == request->object_request) {
			/* User request to turn off */
			if (TURN_ON == vib_state) {
				if (hrtimer_active(&vib_timer)) {
					ktime_t r = hrtimer_get_remaining(&vib_timer);
					struct timeval t = ktime_to_timeval(r);
					remain_time = (t.tv_sec * 1000)
							+ t.tv_usec / 1000;

				} else {
					remain_time = 0;
				}
				hrtimer_cancel(&vib_timer);

				off_duration = on_duration - remain_time;

				ret = pcm2pwm_enable(STOP_PCM2PWM, const_pcm,
							SAMPLE_SIZE, VIB_CNT_OFF);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}
				ret = pcm2pwm_close();
				if (ret) {
					VIB_LOG("pcm2pwm_close = %d\n", ret);
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				on_duration = 0;

				hrtimer_start(&vib_timer,
					ktime_set(off_duration / 1000,
					(off_duration % 1000) * 1000000),
					HRTIMER_MODE_REL);
			} else if (VIBRATE == vib_state) {
				/* turn off and close pcm2pwm */
				hrtimer_cancel(&vib_timer);
				ret = pcm2pwm_enable(STOP_PCM2PWM, const_pcm,
							SAMPLE_SIZE, VIB_CNT_OFF);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}
				ret = pcm2pwm_close();
				if (ret) {
					VIB_LOG("pcm2pwm_close = %d\n", ret);
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				off_duration = VIB_OVERDRV_TIME;
				on_duration = 0;
				hrtimer_start(&vib_timer,
					ktime_set(VIB_OVERDRV_TIME / 1000,
					(VIB_OVERDRV_TIME % 1000) * 1000000),
					HRTIMER_MODE_REL);
			}
		}
		kfree(request);
	}

	ret = gpio_direction_output(pinfo->vibrator_port, LOW);
	if (ret) {
		VIB_LOG("gpio_direction_output\n");
	}
	VIB_LOG("vib_thread is STOPPED\n");
	return ret;
}
#endif /* !defined(VIB_PCM2PWM) */

#else /* !defined(VIB_DCMOTOR) <<< use Linear motor */
/*=============================================================================
 *			PCM2PWM driver with LINEAR MOTOR
 *============================================================================*/
/*
* vibrate_control_thread: thread to turn on/off PCM2PWM
* @arg :
* note		: This function use for PCM2PWM driver with Linear Motor
*/
static int vibrate_control_thread(void *arg)
{
	struct vibrator_single_request *request;
	static int on_duration;
	static int remain_time;
	static int off_duration;
	static int flag_vib;
	int ret = 0;
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(vib_queue,
					!list_empty(&vibrator_list_head));
		if (ret < 0 || kthread_should_stop()) {
			VIB_LOG("Stop vibrate_control_thread\n");
			break;
		}

		spin_lock(&vibrator_spinlock);
		/* get first node in list */
		if (!list_empty(&vibrator_list_head)) {
			request = list_first_entry(&vibrator_list_head,
					struct vibrator_single_request, node);
			list_del(&request->node);
		} else {
			VIB_LOG("vibrator_list_head is empty\n");
			break;
		}
		spin_unlock(&vibrator_spinlock);

		if (HRTIMER_REQUEST == request->object_request) {
			/* processing hrtimer request */
			if ((TURN_ON == vib_state)
				&& (VIBRATE == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				if (0 == vib_duration) {
					/* turn_off immediately */
					ret = pcm2pwm_enable(STOP_PCM2PWM,
								wave_pcm,
								SAMPLE_SIZE,
								VIB_CNT_LINEAR);
					if (ret) {
						VIB_LOG("pcm2pwm_enable = %d\n",
								ret);
						ret = pcm2pwm_close();
						kfree(request);
						break;
					}
					ret = pcm2pwm_close();
					if (ret) {
						VIB_LOG("pcm2pwm_close = %d\n",
								ret);
						kfree(request);
						break;
					}

					spin_lock(&vibrator_spinlock);
					vib_state = TURN_OFF;
					spin_unlock(&vibrator_spinlock);

					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				} else {
					/* vibrate */
					spin_lock(&vibrator_spinlock);
					vib_state = VIBRATE;
					spin_unlock(&vibrator_spinlock);
					flag_vib = 1;
					/* vibrate in timeout */
					hrtimer_start(&vib_timer,
						ktime_set(vib_duration / 1000,
						(vib_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			} else if ((VIBRATE == vib_state)
				&& (TURN_OFF == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				/* turn off and close pcm2pwm */
				ret = pcm2pwm_enable(STOP_PCM2PWM, wave_pcm,
						SAMPLE_SIZE, VIB_CNT_LINEAR);
				if (ret) {
					VIB_LOG(" pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}

				ret = pcm2pwm_close();
				if (ret) {
					VIB_LOG("pcm2pwm_close = %d\n", ret);
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				off_duration = VIB_OVERDRV_TIME;
				/* start hrtimer for 40ms */
				hrtimer_start(&vib_timer,
					ktime_set(VIB_OVERDRV_TIME / 1000,
					(VIB_OVERDRV_TIME % 1000) * 1000000),
					HRTIMER_MODE_REL);


			} else if ((TURN_OFF == vib_state)
				&& (NO_VIBRATE == request->request_state)) {
				hrtimer_cancel(&vib_timer);
				ret = gpio_direction_output(pinfo->vibrator_port, LOW);
				if (ret) {
					VIB_LOG("gpio_direction_output\n");
					kfree(request);
					break;
				}
				flag_vib = 0;
				spin_lock(&vibrator_spinlock);
				vib_state = NO_VIBRATE;
				spin_unlock(&vibrator_spinlock);
			}
		} else if (USER_TURN_ON == request->object_request) {
			/* User request to turn ON*/
			if (NO_VIBRATE == vib_state) {
				hrtimer_cancel(&vib_timer);

				ret = pcm2pwm_open();
				if (ret) {
					VIB_LOG("pcm2pwm_open = %d\n", ret);
					kfree(request);
					break;
				}

				ret = pcm2pwm_enable(START_PCM2PWM, wave_pcm,
						SAMPLE_SIZE, VIB_CNT_LINEAR);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}

				ret = gpio_direction_output(pinfo->vibrator_port, HIGH);
				if (ret) {
					VIB_LOG("gpio_direction_output\n");
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_ON;
				spin_unlock(&vibrator_spinlock);

				if (VIB_OVERDRV_TIME < request->timeout) {
					/* vibrate request duration > 20ms */
					vib_duration = request->timeout - VIB_OVERDRV_TIME;
					on_duration = VIB_OVERDRV_TIME;

					hrtimer_start(&vib_timer,
						ktime_set(VIB_OVERDRV_TIME / 1000,
						(VIB_OVERDRV_TIME % 1000) * 1000000),
						HRTIMER_MODE_REL);

				} else {
					/* 20 >= vibrate request duration */
					vib_duration = 0;
					on_duration = request->timeout;

					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}
			} else if (TURN_OFF == vib_state) {
				if (hrtimer_active(&vib_timer)) {
					ktime_t r = hrtimer_get_remaining(&vib_timer);
					struct timeval t = ktime_to_timeval(r);
					remain_time = (t.tv_sec * 1000)
							+ (t.tv_usec / 1000);
				} else {
					remain_time = 0;
				}
				hrtimer_cancel(&vib_timer);

				on_duration = off_duration - remain_time;

				ret = pcm2pwm_open();
				if (ret) {
					VIB_LOG("pcm2pwm_open = %d\n", ret);
					kfree(request);
					break;
				}

				ret = pcm2pwm_enable(START_PCM2PWM, wave_pcm,
							SAMPLE_SIZE, VIB_CNT_ON);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_ON;
				spin_unlock(&vibrator_spinlock);

				if (on_duration < request->timeout) {
					if (request->timeout > 40 || flag_vib == 1) {
						vib_duration = request->timeout
								- on_duration;

						hrtimer_start(&vib_timer,
							ktime_set(on_duration / 1000,
							(on_duration % 1000) * 1000000),
							HRTIMER_MODE_REL);
					} else {
						vib_duration = 0;
						on_duration = request->timeout;
						hrtimer_start(&vib_timer,
							ktime_set(on_duration / 1000,
							(on_duration % 1000) * 1000000),
							HRTIMER_MODE_REL);
					}
				} else {
					/* on_duration > vibrate request duration */
					vib_duration = 0;
					on_duration = request->timeout;
					hrtimer_start(&vib_timer,
						ktime_set(on_duration / 1000,
						(on_duration % 1000) * 1000000),
						HRTIMER_MODE_REL);
				}

			}
		} else if (USER_TURN_OFF == request->object_request) {
			/* User request to turn off */
			if (TURN_ON == vib_state) {
				if (hrtimer_active(&vib_timer)) {
					ktime_t r = hrtimer_get_remaining(&vib_timer);
					struct timeval t = ktime_to_timeval(r);
					remain_time = (t.tv_sec * 1000)
							+ t.tv_usec / 1000;
				} else {
					remain_time = 0;
				}
				hrtimer_cancel(&vib_timer);

				off_duration = on_duration - remain_time;

				ret = pcm2pwm_enable(STOP_PCM2PWM, wave_pcm,
						SAMPLE_SIZE, VIB_CNT_LINEAR);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}
				ret = pcm2pwm_close();
				if (ret) {
					VIB_LOG("pcm2pwm_close = %d\n", ret);
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				on_duration = 0;

				hrtimer_start(&vib_timer,
					ktime_set(off_duration / 1000,
					(off_duration % 1000) * 1000000),
					HRTIMER_MODE_REL);
			} else if (VIBRATE == vib_state) {
				/* turn off and close pcm2pwm */
				hrtimer_cancel(&vib_timer);
				ret = pcm2pwm_enable(STOP_PCM2PWM, wave_pcm,
						SAMPLE_SIZE, VIB_CNT_LINEAR);
				if (ret) {
					VIB_LOG("pcm2pwm_enable = %d\n", ret);
					pcm2pwm_close();
					kfree(request);
					break;
				}
				ret = pcm2pwm_close();
				if (ret) {
					VIB_LOG("pcm2pwm_close = %d\n", ret);
					kfree(request);
					break;
				}

				spin_lock(&vibrator_spinlock);
				vib_state = TURN_OFF;
				spin_unlock(&vibrator_spinlock);

				off_duration = VIB_OVERDRV_TIME;
				on_duration = 0;
				hrtimer_start(&vib_timer,
					ktime_set(VIB_OVERDRV_TIME / 1000,
					(VIB_OVERDRV_TIME % 1000) * 1000000),
					HRTIMER_MODE_REL);
			}
		}
		kfree(request);
	}

	ret = gpio_direction_output(pinfo->vibrator_port, LOW);
	if (ret) {
		VIB_LOG("gpio_direction_output\n");
	}
	VIB_LOG("vib_thread is STOPPED\n");
	return ret;
}

#endif

static int __devexit vibrator_remove(struct platform_device *pdev)
{

	return 0;
}

static int __init init_vibrator(void)
{
	int ret = 0;


	/* Register platform driver */

	ret = platform_driver_register(&vibrator_platform_driver);
	if (ret) {
		printk(KERN_ERR "[PWM ERR - tpu_init] can't register TPU driver\n");
	}

	return ret;
}

/*
* tpu_exit : Unregister TPU driver and destroy work queue.
* return   : None
*/
static void __exit exit_vibrator(void)
{
	
	platform_driver_unregister(&vibrator_platform_driver);
}

//device_initcall_sync(init_vibrator);
module_init(init_vibrator);
module_exit(exit_vibrator);

MODULE_AUTHOR("Renesas Mobile");
MODULE_DESCRIPTION("timed output vibrator device");
MODULE_LICENSE("GPL v2");
