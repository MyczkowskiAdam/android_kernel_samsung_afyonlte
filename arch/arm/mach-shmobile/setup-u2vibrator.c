#include <mach/r8a7373.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#ifdef CONFIG_VIBRATOR_ISA1000A
#include <linux/isa1000a_haptic.h>

/******************************************************/
/*
 * ISA1000 HAPTIC MOTOR Driver IC.
 * MOTOR Resonance frequency: 205HZ.
 * Input PWM Frequency: 205 * 128 = 26240 HZ.
 * PWM_period_ns = 1000000000/26240 = 38109.
 * PWM Enable GPIO number = 189.
*/
/******************************************************/

#define GPIO_MOTOR_EN	GPIO_PORT226

static int isa1000_enable(bool en)
{
	return gpio_direction_output(GPIO_MOTOR_EN, en);
}

static struct platform_isa1000_vibrator_data isa1000_vibrator_data = {
	.gpio_en	= isa1000_enable,
	.pwm_name	= "TPU0TO0",
	.pwm_duty	= 542,
	.pwm_period_ns	= 580, /*0x244, VIB_CYC*/
	.polarity	= 0,
	.regulator_id	= "vibldo_uc",
};

static struct platform_device isa1000_device = {
	.name     = "isa1000-vibrator",
	.id       = -1,
	.dev      =	{
		.platform_data = &isa1000_vibrator_data,
	},
};

static void isa1000_gpio_init(void)
{
	gpio_request(GPIO_MOTOR_EN, "MOTOR_EN");
	gpio_direction_output(GPIO_MOTOR_EN, 0);
	gpio_export(GPIO_MOTOR_EN, 0);
}

static void __init isa1000_vibrator_init(void)
{
	isa1000_gpio_init();
	platform_device_register(&isa1000_device);
}
#endif

#ifdef CONFIG_VIBRATOR_SS
struct platform_device ss_vibrator_device = {
		.name = "vibrator",
		.id = 0,
		.dev =
		{
			.platform_data="vdd_motor_pmic",
		},
};
#endif

void vibrator_init(unsigned int u2_board_rev)
{
#if defined(CONFIG_VIBRATOR_ISA1000A)
		isa1000_vibrator_init();
#endif

#ifdef CONFIG_VIBRATOR_SS
	platform_device_register(&ss_vibrator_device);
#endif
}
