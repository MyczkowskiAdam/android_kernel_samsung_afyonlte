/*
* Haptic driver for ISA1000A
*
* Copyright (C) 2012 kc45.kim@samsung.com
*
* This program is free software. you can redistribute it and/or modify it
* under the terms of the GNU Public License version 2 as
* published by the Free Software Foundation
*
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/isa1000a_haptic.h>
#include <linux/pmic/pmic.h>
#include <linux/tpu_pwm.h>
#include <linux/tpu_pwm_board.h>

#include "../staging/android/timed_output.h"

#include <mach/common.h>

#define CLK_SOURCE_CP 0 /* CP clock Common Peripheral*/
#define VIB_CYCLE		0x0244 /* Value of TGRB to set the cycle */
#define VIB_DUTY_OVERDRV_ON 0
#define CP_CLK 13000000	// When using TPU0
unsigned int vib_duty = 99;//CONFIG_TPU1_PWM_DUTY_RATE;		// Full vibration 99%
unsigned int pre_nForce = 0;


typedef struct
{
    void *pwm_handle;
	struct timed_output_dev timed_dev;
	const char *vib_vcc;
	const char *pwm_name;
	unsigned int pwm_period;
	unsigned int pwm_duty;
	unsigned int pwm_polarity;
	int (*gpio_en) (bool);
	int	initialized;
}t_vib_desc;

static t_vib_desc vib_desc;


void vibtonz_en(bool en)
{
	t_vib_desc *vib_iter = &vib_desc;
    int ret;

	if(vib_iter->initialized == 0) return;

    printk("%s %s \n", __func__, (en?"enabled":"disabled"));

	if(en)
	{
        ret = tpu_pwm_open(vib_iter->pwm_name, CLK_SOURCE_CP, &(vib_iter->pwm_handle));
	if (ret)
	{
		pr_err("%s : Failed tpu open. %d\n", __func__, ret);
            return ;
	}

        ret = tpu_pwm_enable(vib_iter->pwm_handle, TPU_PWM_START,	VIB_DUTY_OVERDRV_ON, VIB_CYCLE);
        if (ret)
        {
            pr_err("%s : Failed tpu overdrv on. %d\n", __func__, ret);
			tpu_pwm_close(vib_iter->pwm_handle);
			vib_iter->pwm_handle = NULL;
			return ;
        }
		vib_iter->gpio_en(en);
	}
	else
	{
        ret = tpu_pwm_enable(vib_iter->pwm_handle, TPU_PWM_STOP,    0, 0);
        if (ret)
        {
            pr_err("%s : Failed tpu stop. %d\n", __func__, ret);
			tpu_pwm_close(vib_iter->pwm_handle);
			vib_iter->pwm_handle = NULL;
			return ;
        }

        ret = tpu_pwm_close(vib_iter->pwm_handle);
        if (ret)
        {
            pr_err("%s : Failed tpu close. %d\n", __func__, ret);
			vib_iter->pwm_handle = NULL;
			return ;
        }
        vib_iter->pwm_handle = NULL;

		vib_iter->gpio_en(en);
	}
}
EXPORT_SYMBOL(vibtonz_en);

void vibtonz_pwm(int nForce)
{
	t_vib_desc *vib_iter = &vib_desc;
	int ret;
	int pwm_period=0, pwm_duty = 0;

	if( vib_iter->initialized == 0) return;

	printk("%s : %d \n", __func__, nForce);

	pwm_period = vib_iter->pwm_period;
	pwm_duty = pwm_period/2 + ((pwm_period/2 - 2) *(-nForce)) /127;

	if(pwm_duty > vib_iter->pwm_duty)
	{
		pwm_duty = vib_iter->pwm_duty;
	}
	else if(pwm_period - pwm_duty > vib_iter->pwm_duty)
	{
		pwm_duty = pwm_period - vib_iter->pwm_duty;
	}

	ret = tpu_pwm_enable(vib_iter->pwm_handle, TPU_PWM_START,    pwm_duty, pwm_period);
	if (ret)
	{
		pr_err("%s : Failed tpu start. %d\n", __func__, ret);
		tpu_pwm_close(vib_iter->pwm_handle);
		vib_iter->pwm_handle = NULL;
		return ;
	}
}
EXPORT_SYMBOL(vibtonz_pwm);

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev, int timeout)
{
	printk(KERN_INFO "%s : Do nothing.\n", __func__);
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	printk(KERN_INFO "%s : Do nothing.\n", __func__);
    return 0;
}

static int isa1000a_haptic_probe(struct platform_device *pdev)
{
	struct platform_isa1000_vibrator_data *pdata = pdev->dev.platform_data;
	t_vib_desc *vib_iter;
	int ret;

	printk("%s\n", __func__);
	vib_iter = &vib_desc;

	vib_iter->gpio_en = pdata->gpio_en;
	vib_iter->pwm_name = (const char *)pdata->pwm_name;

	vib_iter->pwm_duty = pdata->pwm_duty;
	vib_iter->pwm_period = pdata->pwm_period_ns;
	vib_iter->pwm_polarity = pdata->polarity;


	platform_set_drvdata(pdev, vib_iter);

	vib_iter->initialized = 1;

	return 0;
}

static int __devexit isa1000a_haptic_remove(struct platform_device *pdev)
{
	t_vib_desc *vib_iter = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&vib_iter->timed_dev);
    tpu_pwm_close(vib_iter->pwm_handle);
	return 0;
}

static struct platform_driver isa1000a_haptic_driver = {
	.probe = isa1000a_haptic_probe,
	.remove = isa1000a_haptic_remove,
	.driver = {
		.name = "isa1000-vibrator",
		.owner = THIS_MODULE,
	},
};

static int __init isa1000a_haptic_init(void)
{
	return platform_driver_register(&isa1000a_haptic_driver);
}

static void __exit isa1000a_haptic_exit(void)
{
	platform_driver_unregister(&isa1000a_haptic_driver);
}

module_init(isa1000a_haptic_init);
module_exit(isa1000a_haptic_exit);

MODULE_DESCRIPTION("Samsung Vibrator driver");
MODULE_LICENSE("GPL");
