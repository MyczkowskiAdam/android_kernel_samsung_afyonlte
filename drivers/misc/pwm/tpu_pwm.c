/*
 * /drivers/misc/pwm/tpu_pwm.c
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
#include	<linux/slab.h>
#include	<linux/module.h>
#include	<linux/delay.h>
#include	<linux/wakelock.h>
#include	<linux/pm_runtime.h>
#include	<linux/platform_device.h>
#include	<linux/err.h>
#include	<linux/clk.h>
#include	<linux/io.h>
#include	<linux/ioport.h>
#include	<linux/gpio.h>
#include	<linux/fs.h>

#include	<mach/r8a7373.h>
#include	<linux/tpu_pwm.h>
#include	<linux/tpu_pwm_board.h>

/* Macro */
#define CLEAR_BY_TGRB_COMPARE_MATCH 0x0040
#define COUNT_AT_RISING_EDGE 0x0000
#define BUF_OPERATION_B 0x0060
#define BUF_OPERATION_A 0x0050
#define PWM_MODE 0x0002
#define OUT_1_COMPARE_MATCH_WITH_TPUn_TGRA_MODE 0x0002
#define DUTY_CYCLE_MAX 65535
#define DUTY_CYCLE_MIN 0

#define	TPUn_TSTR(handle) (*(((struct tpu_device *)(handle))->base_add))

#define	TPUn_CHBASE(handle) \
	(&(TPUn_TSTR(handle)[(0x08 + (0x20 * (((struct tpu_device *)(handle))->channel)))]))

#define	TPUn_TCR(handle)	(&(TPUn_CHBASE(handle)[0]))
#define	TPUn_TMDR(handle)	(&(TPUn_CHBASE(handle)[2]))
#define	TPUn_TIOR(handle)	(&(TPUn_CHBASE(handle)[4]))
#define	TPUn_TGRA(handle)	(&(TPUn_CHBASE(handle)[12]))
#define	TPUn_TGRB(handle)	(&(TPUn_CHBASE(handle)[14]))
#define	TPUn_TGRC(handle)	(&(TPUn_CHBASE(handle)[16]))
#define	TPUn_TGRD(handle)	(&(TPUn_CHBASE(handle)[18]))

#define UN_SUSPEND 0
#define SUSPEND    1

#ifdef DEBUG_PWM
	#include	<asm/uaccess.h>
	#include	<linux/miscdevice.h>
#endif


/* Enum and structure */
enum tpu_requests {
	TPU_OPEN,
	TPU_CLOSE,
	TPU_ENABLE,
	TPU_SUSPEND,
	TPU_RESUME,
	TPU_MAX
};

enum tpu_device_state {
	TPU_DEVICE_DISABLE,
	TPU_DEVICE_ENABLE
};

struct tpu_device_info {
	const char *tpu_name;
	const enum tpu_modules module;
	const enum tpu_channels channel;
	enum tpu_device_state dev_state;
};

struct tpu_device {
	enum tpu_modules module;
	enum tpu_channels channel;
	int prescaler; /* This value is set to TPUn_TCR.TPSC[2:0] */
	int duty; /* This value is set to TPUn_TGRA.(If PWM is active then this value is set to TPUn_TGRC.) */
	int cycle;
	enum tpu_pwm_state *start_stop;
	u16 **base_add;
	enum tpu_device_state *dev_state;
	struct clk **clock;
};

struct tpu_platdevice {
	struct platform_device *pdev;
	struct clk *clock;
	u16 *base_add;
	enum tpu_pwm_state start_stop[TPU_CHANNEL_MAX];
	enum tpu_pwm_state open_status[TPU_CHANNEL_MAX];
};

struct tpu_completion {
	int result;
	struct semaphore respond_comp;
};

struct tpu_work_arg {
	struct work_struct work;
	enum tpu_requests request;
	struct tpu_completion *respond;
	void *data;
};

struct tpu_enable_arg {
	void *handle;
	enum tpu_pwm_state start_stop;
	int duty;
	int cycle;
};

struct tpu_open_arg {
	const char *tpu_name;
	void **handle;
	int prescaler;
};

struct tpu_close_arg {
	void *handle;
};

/* Function prototype */
static int add_to_workqueue(struct tpu_work_arg *work_arg);
static void comp_tpu_open(struct work_struct *work);
static int handle_tpu_open(void *param);
static void comp_tpu_enable(struct work_struct *work);
static int handle_tpu_enable(void *param);
static void comp_tpu_close(struct work_struct *work);
static int handle_tpu_close(void *param);
static void handle_tpu_suspend(struct work_struct *work);
static void handle_tpu_resume(struct work_struct *work);

static int __init tpu_init(void);
static void __exit tpu_exit(void);
static int tpu_suspend(struct platform_device *pdev, pm_message_t state);
static int tpu_resume(struct platform_device *pdev);
static int tpu_probe(struct platform_device *pdev);
static int __devexit tpu_remove(struct platform_device *pdev);

#ifdef	DEBUG_PWM
static void pwm_create_debug_if(void);
static void pwm_delete_debug_if(void);
#endif	/* DEBUG_PWM */

/* Global variable */
static struct workqueue_struct *tpu_work_queue;
static struct wake_lock tpu_wakelock;
static int tpu_suspend_state;
static int tpu_open_counter;

static struct tpu_platdevice tpu_platform_device[TPU_MODULE_MAX] = {
	[TPU_MODULE_0] = {
		.pdev        = NULL,
		.clock       = NULL,
		.base_add    = NULL,
		.start_stop  = {
			[TPU_CHANNEL_0]    = TPU_PWM_STOP,
			[TPU_CHANNEL_1]    = TPU_PWM_STOP,
			[TPU_CHANNEL_2]    = TPU_PWM_STOP,
			[TPU_CHANNEL_3]    = TPU_PWM_STOP,
		},
		.open_status = {
			[TPU_CHANNEL_0]    = TPU_PWM_STOP,
			[TPU_CHANNEL_1]    = TPU_PWM_STOP,
			[TPU_CHANNEL_2]    = TPU_PWM_STOP,
			[TPU_CHANNEL_3]    = TPU_PWM_STOP,
		},
	},
};

static struct platform_driver tpu_platform_driver = {
	.probe   = tpu_probe,
	.remove  = __devexit_p(tpu_remove),
	.suspend = tpu_suspend,
	.resume  = tpu_resume,
	.driver    = {
		.name  = "tpu-renesas-sh_mobile",
		.owner = THIS_MODULE,
	},
};

static struct tpu_device_info device_info[] = {
	{
		.tpu_name  = "TPU0TO0",
		.module    = TPU_MODULE_0,
		.channel   = TPU_CHANNEL_0,
		.dev_state = TPU_DEVICE_DISABLE,
	},
	{
		.tpu_name  = "TPU0TO3",
		.module    = TPU_MODULE_0,
		.channel   = TPU_CHANNEL_3,
		.dev_state = TPU_DEVICE_DISABLE,
	},
	{
		.tpu_name  = NULL,
		.module    = TPU_MODULE_0,
		.channel   = TPU_CHANNEL_0,
		.dev_state = TPU_DEVICE_DISABLE,
	},
};

/*
* add_to_workqueue: Top-half processing of work-queue(This function will select corresponding function then add to work queue).
* @work_arg: Pointer to struct of tpu_work_arg
* return:
*        0 if success, otherwise return negative value
*/
static int add_to_workqueue(struct tpu_work_arg *work_arg)
{
	struct tpu_completion comp;

	/* Init semaphore */
	sema_init(&comp.respond_comp, 0);
	work_arg->respond = &comp;

	switch (work_arg->request) {
	case TPU_OPEN:
		INIT_WORK(&work_arg->work, comp_tpu_open);
		break;
	case TPU_ENABLE:
		INIT_WORK(&work_arg->work, comp_tpu_enable);
		break;
	case TPU_CLOSE:
		INIT_WORK(&work_arg->work, comp_tpu_close);
		break;
	case TPU_RESUME:
		INIT_WORK(&work_arg->work, handle_tpu_resume);
		break;
	case TPU_SUSPEND:
		INIT_WORK(&work_arg->work, handle_tpu_suspend);
		break;
	default:
		return -EINVAL;
	}

	queue_work(tpu_work_queue, &work_arg->work);

	/* waiting the complete processing of workqueue */
	down(&comp.respond_comp);

	return work_arg->respond->result;
}

/*
 * tpu_pwm_open: open the TPU driver
 * @channel: TPU channel name
 * @prescaler: Clock divider
 * @handle: Pointer to pointer of the close handler
 * return:
 *     0: successful
 *     -EINVAL: invalid argument
*/
extern int tpu_pwm_open(const char *channel, int prescaler, void **handle)
{
	int	ret = 0;
	struct tpu_open_arg open;
	struct tpu_work_arg work_arg;

	open.tpu_name = channel;
	open.handle = handle;
	open.prescaler = prescaler;

	work_arg.request = TPU_OPEN;
	work_arg.data = &open;

	/* check the argument */
	if (!channel || (0 > prescaler) || (4 < prescaler)) {
		printk(KERN_ERR "[PWM ERR - tpu_pwm_open] invalid argument\n");
		return -EINVAL;
	}

	ret = add_to_workqueue(&work_arg);
	return ret;
}
EXPORT_SYMBOL(tpu_pwm_open);

/*
* comp_tpu_open: bottom-half processing of work-queue to open TPU channel.
* @work: Pointer to struct work_struct.
* return:
*        None
*/
static void comp_tpu_open(struct work_struct *work)
{
	int ret = 0;

	struct tpu_work_arg *work_arg;

	/* get the containing data structure tpu_work_arg from its member work */
	work_arg = container_of(work, struct tpu_work_arg, work);

	/* check state condition to allow openning TPU */
	if (tpu_suspend_state) {
		ret = -EBUSY;
	} else {
		ret = handle_tpu_open(work_arg->data);
	}

	work_arg->respond->result = ret;
	/* complete processing of Workqueue */
	up(&work_arg->respond->respond_comp);
}

/*
 * handle_tpu_open: Handler of openning TPU driver
 * @param: Argument of tpu_pwm_open passed to its handler
 * return:
 *     0: successful
 *     -ENODEV: TPU device not exist
 *     -EBUSY: TPU device is busy
 *     -ENOMEM: Lack of memory for allocation
*/
static int handle_tpu_open(void *param)
{
	int ret = -ENODEV;

	struct tpu_open_arg *open = (struct tpu_open_arg *)param;
	struct tpu_device *handle;
	enum tpu_modules module;
	enum tpu_channels channel;
	/* char  clkName[35]; //Note: temp comment because issue of clk_get() */
	int i;
	enum tpu_device_state *dev_state = NULL;

	struct tpu_platdevice *tpu_pdev;
	struct platform_device *pdev;

	for (i = 0; device_info[i].tpu_name; i++) {
		if (!strcmp(open->tpu_name, device_info[i].tpu_name)) {
			dev_state = &device_info[i].dev_state;
			if (TPU_DEVICE_ENABLE == *dev_state) {
				printk(KERN_ERR "[PWM ERR - handle_tpu_open] device is openning\n");
				return -EBUSY;
			}

			*dev_state = TPU_DEVICE_ENABLE;
			module = device_info[i].module;
			channel = device_info[i].channel;
			ret = 0;
			break;
		}
	}

	if (ret) {
		printk(KERN_ERR "[PWM ERR - handle_tpu_open] no TPU device name: %s\n", open->tpu_name);
		return ret;
	}

	handle = (struct tpu_device *)kmalloc(sizeof(struct tpu_device), GFP_KERNEL);
	if (!handle) {
		printk(KERN_ERR "[PWM ERR - handle_tpu_open] no memory to be allocated to TPU handler\n");
		return -ENOMEM;
	}

	/* Retrieve the TPU device information registered into the platform */
	tpu_pdev = (struct tpu_platdevice *)platform_get_drvdata(tpu_platform_device[module].pdev);
	pdev = tpu_pdev->pdev;

	handle->module = module;
	handle->channel = channel;
	handle->prescaler = open->prescaler;
	handle->duty = 0;
	handle->cycle = 1;
	handle->start_stop = &tpu_pdev->start_stop[channel];
	handle->base_add = &tpu_pdev->base_add;
	handle->clock = &tpu_pdev->clock;
	handle->dev_state = dev_state;

	if (!(*(handle->base_add))) {
		wake_lock(&tpu_wakelock);
		/* enable a3sp */
		ret = pm_runtime_get_sync(&pdev->dev);
		if (ret) {
			kfree(handle);
			printk(KERN_ERR "[PWM ERR - handle_tpu_open] get PM sync unsuccessfully\n");
			return ret;
		}
		/* enable cp-clock and mstp */
		/* sprintf(clkName, "tpu-renesas-sh_mobile.%d", (int)module); //Note: temp comment because issue of clk_get */
		/* *(handle->clock) = clk_get(NULL, clkName); //Note: temp comment because issue of clk_get */

		*(handle->clock) = clk_get(&pdev->dev, NULL);

		if (IS_ERR(*(handle->clock))) {
			printk(KERN_ERR "[PWM ERR - handle_tpu_open] clk_get unsuccessfully\n");
			kfree(handle);
			return -EINVAL;
		}

		ret = clk_enable(*(handle->clock));
		if (ret) {
			kfree(handle);
			printk(KERN_ERR "[PWM ERR handle_tpu_open] enable clock unsuccessfully\n");
			return ret;
		}

		/* map I/O memory */
		if (!request_mem_region(pdev->resource->start,
								resource_size(pdev->resource),
								pdev->resource->name)) {
			kfree(handle);
			printk(KERN_ERR "[PWM ERR handle_tpu_open] the mapped IO memory is in use\n");
			return -EBUSY;
		}

		*(handle->base_add) = (u16 *)ioremap_nocache(pdev->resource->start, resource_size(pdev->resource));
	}

	tpu_pdev->open_status[channel] = TPU_PWM_START;
	tpu_open_counter++;
	*(open->handle) = handle;

	return ret;
}


/*
* tpu_pwm_close : Close TPU channel.
* @handle : Handle representing for TPU channel (point to struct of TPU device).
* return:
*        0 if success, otherwise return negative value
*/
extern int tpu_pwm_close(void *handle)
{
	int ret = 0;
	struct tpu_close_arg close;
	struct tpu_work_arg work_arg;

	if (unlikely(NULL == handle)) {
		printk(KERN_ERR "[PWM ERR - tpu_pwm_close] handle is NULL\n");
		return -EINVAL;
	}

	close.handle = handle;

	/* package to work struct argument */
	work_arg.request = TPU_CLOSE;
	work_arg.data = &close;

	/* pass struct work argument to workqueue */
	ret = add_to_workqueue(&work_arg);
	return ret;
}
EXPORT_SYMBOL(tpu_pwm_close);



/*
* comp_tpu_close: bottom-half processing of work-queue to close TPU channel.
* @work: Pointer to struct work_struct.
* return:
*        None
*/
static void comp_tpu_close(struct work_struct *work)
{
	int ret = 0;
	struct tpu_work_arg *work_arg = NULL;

	if (!tpu_suspend_state) { /* not in suspend state */
		/* get parameter which is passed to workqueue */
		work_arg = container_of(work, struct tpu_work_arg, work);
		ret = handle_tpu_close(work_arg->data);
	} else { /* in suspend state */
		ret = -EBUSY;
	}
	if (NULL != work_arg) {
		work_arg->respond->result = ret;
 	/* complete processing of Workqueue */
		up(&work_arg->respond->respond_comp);
	}
}

/*
 * handle_tpu_close: Handler of closing TPU driver
 * @param: Argument of tpu_pwm_close passed to its handler
 * return:
 *     0: successful
 *     Other code: error existence
*/
static int handle_tpu_close(void *param)
{
	int ret = 0;
	int flag_unmap = 1;
	int i = 0;

	struct tpu_platdevice *tpu_pdev;
	struct platform_device *pdev;
	struct tpu_device *handle;

	handle = (struct tpu_device *)((struct tpu_close_arg *)param)->handle;

	/* Retrieve the TPU device information registered into the platform */
	tpu_pdev = (struct tpu_platdevice *)platform_get_drvdata(tpu_platform_device[handle->module].pdev);
	pdev = tpu_pdev->pdev;

	*(handle->dev_state) = TPU_DEVICE_DISABLE;
	*(handle->start_stop) = TPU_PWM_STOP;
	tpu_pdev->open_status[handle->channel] = TPU_PWM_STOP;

	for (i = 0; i < TPU_CHANNEL_MAX; i++) {
		if (TPU_PWM_START == tpu_pdev->open_status[i]) {
			flag_unmap = 0;
			break;
		}
	}

	if (flag_unmap) {
		/* unmap I/O memory */
		iounmap(*(handle->base_add));
		release_mem_region(pdev->resource->start, resource_size(pdev->resource));
		*(handle->base_add) = NULL;
		/* disable cp-clock and mstp. */
		if (!*(handle->clock)) {
			printk(KERN_ERR "[PWM ERR - handle_tpu_close] warning! clock is NULL\n");
		} else {
			clk_disable(*(handle->clock));
			clk_put(*(handle->clock));
			*(handle->clock) = NULL;
		}
		/* disable a3sp */
		ret = pm_runtime_put_sync(&pdev->dev);
		if (ret) {
			printk(KERN_ERR "[PWM ERR - handle_tpu_close] put PM sync unsuccessfully\n");
		}
		wake_unlock(&tpu_wakelock);

	}
	tpu_open_counter--;

	kfree(handle);
	return ret;
}

/*
* tpu_pwm_enable: Setting duty cycle, enable/disable to TPU channel.
* @handle : Handle representing for TPU channel (point to struct of TPU device).
* @state    : TPU_PWM_STOP : Stop PWM signal
*            : TPU_PWM_START : Start PWM signal
* @duty : Duty cycle
* @cycle : Periodic cycle
* return:
*        0 if success, otherwise return negative value
*/
extern int tpu_pwm_enable(void *handle, enum tpu_pwm_state state, int duty, int cycle)
{
	int ret = 0;
	struct tpu_enable_arg enable;
	struct tpu_work_arg work_arg;

	/* Check argument */
	if (unlikely(NULL == handle)) {
		printk(KERN_ERR "[PWM ERR - tpu_pwm_enable] handle is NULL\n");
		return -EINVAL;
	}
	if (unlikely(duty > DUTY_CYCLE_MAX || duty < DUTY_CYCLE_MIN || cycle < DUTY_CYCLE_MIN || cycle > DUTY_CYCLE_MAX)) {
		printk(KERN_ERR "[PWM ERR - tpu_pwm_enable] invalid argument\n");
		return -EINVAL;
	}
	/* Set enable argument */
	enable.handle = handle;
	enable.start_stop = state;
	enable.duty = duty;
	enable.cycle = cycle;

	/* package to work struct argument */
	work_arg.request = TPU_ENABLE;
	work_arg.data = &enable;

	/* pass struct work argument to workqueue */
	ret = add_to_workqueue(&work_arg);
	return ret;
}
EXPORT_SYMBOL(tpu_pwm_enable);

/*
* comp_tpu_enable: bottom-half processing of work-queue to enable TPU channel.
* @work: Pointer to struct work_struct.
* return:
*        None
*/
static void comp_tpu_enable(struct work_struct *work)
{
	int ret = 0;
	struct tpu_work_arg *work_arg = NULL;

	if (!tpu_suspend_state) { /* not in suspend state */
		/* get parameter which is passed to workqueue */
		work_arg = container_of(work, struct tpu_work_arg, work);
		if (work_arg != NULL)
			ret = handle_tpu_enable(work_arg->data);

	} else { /* in suspend state */
		ret = -EBUSY;
	}

	if (work_arg != NULL) {
		work_arg->respond->result = ret;
		/* complete processing of Workqueue */
		up(&work_arg->respond->respond_comp);
	}
}

/*
* handle_tpu_enable: Setting duty cycle, enable/disable to TPU channel.
* @param : Including a handler representing for TPU channel, and the PWM signal setting for its Start/Stop, Periodic cycle and Duty cycle.
* return:
*        0 if success, otherwise return negative value
*/
static int handle_tpu_enable(void *param)
{
	int ret = 0;
	u16 value;
	struct tpu_enable_arg *enable;
	struct tpu_device *handle;
	struct platform_device *pdev;
	struct port_info *pinfo;

	enable = (struct tpu_enable_arg *)param;
	handle = (struct tpu_device *)(enable->handle);

	/* Get pwm port function information */
	pdev = ((struct tpu_platdevice *)(platform_get_drvdata(tpu_platform_device[handle->module].pdev)))->pdev;
	pinfo = ((struct port_info *)pdev->dev.platform_data) + handle->channel;

	/* need to stop TPU channel */
	if (TPU_PWM_STOP == enable->start_stop) {
		if (enable->start_stop != *(handle->start_stop)) {/* the state of device is starting -> disable the counter*/
			value = __raw_readw(TPUn_TSTR(handle));
			value &= ~(1 << (handle->channel));
			__raw_writew(value, TPUn_TSTR(handle));
			/* free GPIO port function */
			if (pinfo->func_name != NULL) {
				gpio_free(pinfo->port_func);
			}
		}
		return ret;
	}

	/* need to update duty and cycle value */
	if (TPU_PWM_STOP != *(handle->start_stop)) {
		/* write Duty and Cycle value to TPUn_TGRC and TPUn_TGRD */
		handle->duty = enable->duty;
		handle->cycle = enable->cycle;

		__raw_writew(enable->duty, TPUn_TGRC(handle));
		__raw_writew(enable->cycle, TPUn_TGRD(handle));
		return ret;
	}

	/* Update state of device */
	*(handle->start_stop) = enable->start_stop;

	/*[1] Select the counter clock with bits TPSC[2:0] in TPUn_TCR.
	* At the same time, select the input clock edge with bits CKEG[1:0] in TPUn_TCR.
	* [2] Use bits CCLR[2:0] in TPUn_TCR to select the TPUn_TGRB
	* to be used as the TPUn_TCNT clearing source.
	*/
	__raw_writew(((CLEAR_BY_TGRB_COMPARE_MATCH | COUNT_AT_RISING_EDGE) | (handle->prescaler)), TPUn_TCR(handle));

	/* [3] Use TPUn_TIOR to select the initial value and output value */
	__raw_writew(OUT_1_COMPARE_MATCH_WITH_TPUn_TGRA_MODE, TPUn_TIOR(handle)); /* Output 1 on compare match with TPUn_TGRA */

	/* [4] Set the cycle in TPUn_TGRB and set the duty cycle in TPUn_TGRA. */
	handle->duty = enable->duty;
	handle->cycle = enable->cycle;

	__raw_writew(enable->duty, TPUn_TGRA(handle));/* Seting the duty cycle */
	__raw_writew(enable->duty, TPUn_TGRC(handle)); /* setting buffer operation C */
	__raw_writew(enable->cycle, TPUn_TGRB(handle)); /* Setting the cycle */
	__raw_writew(enable->cycle, TPUn_TGRD(handle)); /* setting buffer operation D */

	/* [5] Select the PWM mode with bits MD[3:0] in TPUn_TMDR */
	__raw_writew(((BUF_OPERATION_B | BUF_OPERATION_A) | PWM_MODE), TPUn_TMDR(handle));

	/* [6] Set external pin function by pin function controller(PFC) */
	if (pinfo->func_name != NULL) {
		ret = gpio_request(pinfo->port_func, pinfo->func_name);
		if (ret) {
			printk(KERN_ERR "[PWM ERR - gpio_request] request gpio is failed\n");
			return ret;
		}
	}

	/* [7] Set the CST bit in TPUn_TSTR to 1 to start the count operation */
	value = __raw_readw(TPUn_TSTR(handle));
	value |= (1 << (handle->channel)); /* Select channel */
	__raw_writew(value, TPUn_TSTR(handle)); /* Counter start */

	return ret;
}


/*
* tpu_init : Register TPU driver and create work queue.
* return   :
*         0               : Successful
*         -ENOMEM         : Can not create work queue
*         -EBUSY          : TPU driver is already registered
*         other error code: Can not register TPU driver
*/
static int __init tpu_init(void)
{
	int ret = 0;
	tpu_work_queue = create_singlethread_workqueue("tpu_workqueue");
	if (NULL == tpu_work_queue) {
		printk(KERN_ERR "[PWM ERR - tpu_init] create workqueue error\n");
		return -ENOMEM;
	}
	/* Init wakelock(prevent suspend) */
	wake_lock_init(&tpu_wakelock, WAKE_LOCK_SUSPEND, "tpu-wakelock");
	tpu_suspend_state = UN_SUSPEND;

	/* Register platform driver */
	ret = platform_driver_register(&tpu_platform_driver);
	if (ret) {
		printk(KERN_ERR "[PWM ERR - tpu_init] can't register TPU driver\n");
	}

	/* init static variable */
	tpu_open_counter = 0;
#ifdef	DEBUG_PWM
		pwm_create_debug_if();
#endif /* End DEBUG_PWM */

	return ret;
}

/*
* tpu_exit : Unregister TPU driver and destroy work queue.
* return   : None
*/
static void __exit tpu_exit(void)
{
#ifdef	DEBUG_PWM
	pwm_delete_debug_if();
#endif	/* DEBUG_PWM */
	/* Unregister platform driver */
	platform_driver_unregister(&tpu_platform_driver);
	destroy_workqueue(tpu_work_queue);
}
/* GPIO Settings */
static void tpu_gpio_setting(struct port_info *pinfo, int suspend_mode)
{
	int i;
	int port;
	struct portn_gpio_setting_tpu *gpio_prev, *gpio_current;
	if (pinfo == NULL || pinfo->port_count == 0)
		return ;

	for (i = 0; i < pinfo->port_count; i++)	{
		port = pinfo->tpu_gpio_setting_info[i].port ;
		if (suspend_mode == 1) {
			gpio_prev  = &pinfo->tpu_gpio_setting_info[i].active;
			gpio_current = \
				&pinfo->tpu_gpio_setting_info[i].inactive;
		} else {
			gpio_prev = &pinfo->tpu_gpio_setting_info[i].inactive;
			gpio_current  = &pinfo->tpu_gpio_setting_info[i].active;
		}

		if (pinfo->tpu_gpio_setting_info[i].flag == 1) {
			gpio_free(gpio_prev->port_fn);

		/* Set Pull up/down/off */
			switch (gpio_current->direction) {

			case PORTn_CR_DIRECTION_NOT_SET:
				break;

			case PORTn_CR_DIRECTION_NONE:
				gpio_request(port, NULL);
				gpio_direction_input(port);
				gpio_direction_none_port(port);
				if (gpio_current->port_fn != port)
					gpio_free(port);
				break;

			case PORTn_CR_DIRECTION_OUTPUT:
				gpio_request(port, NULL);
				gpio_direction_output(port,
					gpio_current->output_level);
				if (gpio_current->port_fn != port)
					gpio_free(port);
				break;

			case PORTn_CR_DIRECTION_INPUT:
				gpio_request(port, NULL);
				gpio_direction_input(port);
				if (gpio_current->port_fn != port)
					gpio_free(port);
				break;

			default:
				break;
			}

			switch (gpio_current->pull) {

			case PORTn_CR_PULL_NOT_SET:
				break;

			case PORTn_CR_PULL_OFF:
				gpio_pull_off_port(port);
				break;

			case PORTn_CR_PULL_DOWN:
				gpio_pull_down_port(port);
				break;

			case PORTn_CR_PULL_UP:
				gpio_pull_up_port(port);
				break;
			}

			if (gpio_current->port_fn != port)
				gpio_request(gpio_current->port_fn, NULL);
		}
	}
	return;
}

/*
 * tpu_suspend: Indicate that driver goes to low power state
 * @pdev: TPU device
 * @state: Power management message
 * return:
 *       0: successful
 *       -EBUSY: TPU device is opening
 */
static int tpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct tpu_work_arg work_arg;
	struct port_info *pinfo;
	work_arg.request = TPU_SUSPEND;
	ret = add_to_workqueue(&work_arg);
	pinfo = (struct port_info *)pdev->dev.platform_data;
	tpu_gpio_setting(pinfo, 1);
	return ret;
}

/*
* handle_tpu_suspend: bottom-half processing of work-queue to suspend.
* @work: Pointer to struct work_struct.
* return:
*        None
*/
static void handle_tpu_suspend(struct work_struct *work)
{
	int ret = 0;

	struct tpu_work_arg *work_arg;
	/* get the containing data structure tpu_work_arg from its member work */
	work_arg = container_of(work, struct tpu_work_arg, work);

	if (tpu_open_counter) {
		ret = -EBUSY;
	} else {
		tpu_suspend_state = SUSPEND;
	}

	work_arg->respond->result = ret;
	/* complete processing of Workqueue */
	up(&work_arg->respond->respond_comp);
}

/*
 * tpu_resume: Indicate that driver goes to normal state
 * @pdev: TPU device
 * return:
 *       0: successful
 */
static int tpu_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct tpu_work_arg work_arg;
	struct port_info *pinfo;
	work_arg.request = TPU_RESUME;
	ret = add_to_workqueue(&work_arg);
	pinfo = (struct port_info *)pdev->dev.platform_data;
	tpu_gpio_setting(pinfo, 0);
	gpio_free(pinfo->port_func);
	return ret;
}

/*
* handle_tpu_resume: bottom-half processing of work-queue to resume.
* @work: Pointer to struct work_struct.
* return:
*        None
*/
static void handle_tpu_resume(struct work_struct *work)
{
	int ret = 0;
	struct tpu_work_arg *work_arg;

	/* get the containing data structure tpu_work_arg from its member work */
	work_arg = container_of(work, struct tpu_work_arg, work);
	tpu_suspend_state = UN_SUSPEND;
	work_arg->respond->result = ret;
	/* complete processing of Workqueue */
	up(&work_arg->respond->respond_comp);
}

/*
 * tpu_probe: Initialize the device when device is registered by the platform
 * @pdev: device which is initialized
 * return:
 *        0: successful
 */
static int tpu_probe(struct platform_device *pdev)
{
	tpu_platform_device[pdev->id].pdev = pdev;
	platform_set_drvdata(pdev, &tpu_platform_device[pdev->id]);
	pm_runtime_enable(&pdev->dev);
	return 0;
}

/*
 * tpu_remove: This function is called when the device is removed from the system
 * @pdev: device which is removed
 * return:
 *       0: successful
 */
static int __devexit tpu_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}

#ifdef DEBUG_PWM
static const char *channel_03 = "TPU0TO3";
static const char *channel_00 = "TPU0TO0";
static const char *invalidChannel = "InvalName";

static void *handle_tpu_03;
static void *handle_tpu_00;
static void *invalid_handle;

static ssize_t pwm_test_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	int number_request = 0;
	int ret;
	sscanf(buf, "%d", &number_request);

	switch (number_request) {
	case 1:
		ret = tpu_pwm_open(channel_03, 1, &handle_tpu_03);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_open] ret = %d\n", ret);
		}
		break;
	case 2:
		ret = tpu_pwm_enable(handle_tpu_03, TPU_PWM_START, 0, 10800);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 3:
		ret = tpu_pwm_enable(handle_tpu_03, TPU_PWM_STOP, 0, 0);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 4:
		ret = tpu_pwm_close(handle_tpu_03);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_close] ret = %d\n", ret);
		}
		handle_tpu_03 = NULL;
		break;
	case 5:
		ret = tpu_pwm_enable(handle_tpu_03, TPU_PWM_START, 2700, 10800);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 6:
		ret = tpu_pwm_enable(handle_tpu_03, TPU_PWM_START, 10800, 10800);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 7:
		ret = tpu_pwm_open(channel_00, 1, &handle_tpu_00);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_open] ret = %d\n", ret);
		}
		break;
	case 8:
		ret = tpu_pwm_enable(handle_tpu_00, TPU_PWM_START, 200, 10800);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 9:
		ret = tpu_pwm_enable(handle_tpu_00, TPU_PWM_START, 500, 9100);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 10:
		ret = tpu_pwm_enable(handle_tpu_00, TPU_PWM_STOP, 200, 10800);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_enable] ret = %d\n", ret);
		}
		break;
	case 11:
		ret = tpu_pwm_close(handle_tpu_00);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_close] ret = %d\n", ret);
		}
		handle_tpu_00 = NULL;
		break;
	case 12:
		ret = tpu_pwm_open(invalidChannel, 1, &invalid_handle);
		if (ret) {
			printk(KERN_ERR "[DBG PWM ERR - tpu_pwm_open] ret = %d\n", ret);
		}
	break;
	default:
		printk(KERN_ERR "[DBG PWM ERR] Request not correct\n");
	}

	return count;
}

static struct file_operations pwm_test_fops = {
	.owner	= THIS_MODULE,
	.write	= pwm_test_write,
};

static struct miscdevice pwm_test_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "pwm_test",
	.fops	= &pwm_test_fops,
};

static void	pwm_create_debug_if(void)
{
	misc_register(&pwm_test_miscdev);
}
static void	pwm_delete_debug_if(void)
{
	misc_deregister(&pwm_test_miscdev);
}
#endif /* DEBUG_PWM */


MODULE_AUTHOR("Renesas");
MODULE_DESCRIPTION("driver that control pwm waveform in tpu.");
MODULE_LICENSE("GPL v2");

module_init(tpu_init);
module_exit(tpu_exit);
