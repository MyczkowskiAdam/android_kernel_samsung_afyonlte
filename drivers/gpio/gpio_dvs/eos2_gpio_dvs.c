/*
 * Android R&D Group 3.
 *
 * drivers/gpio/gpio_dvs/eos2_gpio_dvs.c - EOS2 GPIO info. from MP523X(EOS2)
 * 
 * Copyright (C) 2013, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

/********************* Fixed Code Area !***************************/
#include <mach/gpio.h>
#include <linux/secgpio_dvs.h>
#include <linux/platform_device.h>
/****************************************************************/

/********************* Fixed Code Area !***************************/
#define GET_RESULT_GPIO(a, b, c)	\
	(((a)<<10 & 0xFC00) |((b)<<4 & 0x03F0) | ((c) & 0x000F))

#define GET_GPIO_IO(value)	\
	(unsigned char)((0xFC00 & (value)) >> 10)
#define GET_GPIO_PUPD(value)	\
	(unsigned char)((0x03F0 & (value)) >> 4)
#define GET_GPIO_LH(value)	\
	(unsigned char)(0x000F & (value))
/****************************************************************/

static int available_gpios_eos2[] = {
		
	/* Hardware manual Table 9-1 (GPIO) */
	GPIO_PORT0, GPIO_PORT1, GPIO_PORT2, GPIO_PORT3, GPIO_PORT4,
	GPIO_PORT5, GPIO_PORT6, GPIO_PORT7, GPIO_PORT8, GPIO_PORT9,
	GPIO_PORT10, GPIO_PORT11, GPIO_PORT12, GPIO_PORT13, GPIO_PORT14,
	GPIO_PORT15, GPIO_PORT16, GPIO_PORT17, GPIO_PORT18, GPIO_PORT19,
	GPIO_PORT20, GPIO_PORT21, GPIO_PORT22, GPIO_PORT23, GPIO_PORT24,
	GPIO_PORT25, GPIO_PORT26, GPIO_PORT27, GPIO_PORT28, GPIO_PORT29,
	GPIO_PORT30, GPIO_PORT31, GPIO_PORT32, GPIO_PORT33, GPIO_PORT34,
	GPIO_PORT35, GPIO_PORT36, GPIO_PORT37, GPIO_PORT38, GPIO_PORT39,
	GPIO_PORT40, GPIO_PORT41, GPIO_PORT42, GPIO_PORT43, GPIO_PORT44,
	GPIO_PORT45, GPIO_PORT46, GPIO_PORT47, GPIO_PORT48,

	/* 49 .. 63 are not available */
	GPIO_PORT64,
	GPIO_PORT65, GPIO_PORT66, GPIO_PORT67, GPIO_PORT68, GPIO_PORT69,
	GPIO_PORT70, GPIO_PORT71, GPIO_PORT72, GPIO_PORT73, GPIO_PORT74,
	GPIO_PORT75, GPIO_PORT76, GPIO_PORT77, GPIO_PORT78, GPIO_PORT79,
	GPIO_PORT80, GPIO_PORT81, GPIO_PORT82, GPIO_PORT83, GPIO_PORT84,
	GPIO_PORT85, GPIO_PORT86, GPIO_PORT87, GPIO_PORT88, GPIO_PORT89,
	GPIO_PORT90, GPIO_PORT91,

	/* 92 .. 95 are not available */
	GPIO_PORT96, GPIO_PORT97, GPIO_PORT98, GPIO_PORT99,
	GPIO_PORT100, GPIO_PORT101, GPIO_PORT102, GPIO_PORT103, GPIO_PORT104,
	GPIO_PORT105, GPIO_PORT106, GPIO_PORT107, GPIO_PORT108, GPIO_PORT109,
	GPIO_PORT110,

	/* 111 .. 127 are not available */
	GPIO_PORT128, GPIO_PORT129,
	GPIO_PORT130, GPIO_PORT131, /*132*/ GPIO_PORT133, GPIO_PORT134,
	GPIO_PORT135, GPIO_PORT136, GPIO_PORT137, GPIO_PORT138, GPIO_PORT139,
	GPIO_PORT140, GPIO_PORT141, GPIO_PORT142,

	/* 143 .. 197 are not available */
	GPIO_PORT198, GPIO_PORT199,
	GPIO_PORT200, GPIO_PORT201, GPIO_PORT202, GPIO_PORT203, GPIO_PORT204,
	GPIO_PORT205, GPIO_PORT206, GPIO_PORT207, GPIO_PORT208, GPIO_PORT209,
	GPIO_PORT210, GPIO_PORT211, GPIO_PORT212, GPIO_PORT213, GPIO_PORT214,
	GPIO_PORT215, GPIO_PORT216, GPIO_PORT217, GPIO_PORT218, GPIO_PORT219,

	/* 220 .. 223 are not available */
	GPIO_PORT224,
	GPIO_PORT225, GPIO_PORT226, GPIO_PORT227, GPIO_PORT228, GPIO_PORT229,
	GPIO_PORT230, GPIO_PORT231, GPIO_PORT232, GPIO_PORT233, GPIO_PORT234,
	GPIO_PORT235, GPIO_PORT236, GPIO_PORT237, GPIO_PORT238, GPIO_PORT239,
	GPIO_PORT240, GPIO_PORT241, GPIO_PORT242, GPIO_PORT243, GPIO_PORT244,
	GPIO_PORT245, GPIO_PORT246, GPIO_PORT247, GPIO_PORT248, GPIO_PORT249,
	GPIO_PORT250, GPIO_PORT251, GPIO_PORT252, GPIO_PORT253, GPIO_PORT254,
	GPIO_PORT255, GPIO_PORT256, GPIO_PORT257, GPIO_PORT258, GPIO_PORT259,
	GPIO_PORT260, GPIO_PORT261, GPIO_PORT262, GPIO_PORT263, GPIO_PORT264,
	GPIO_PORT265, GPIO_PORT266, GPIO_PORT267, GPIO_PORT268, GPIO_PORT269,
	GPIO_PORT270, GPIO_PORT271, GPIO_PORT272, GPIO_PORT273, GPIO_PORT274,
	GPIO_PORT275, GPIO_PORT276, GPIO_PORT277,

	/* 278 .. 287 are not available */
	GPIO_PORT288, GPIO_PORT289,
	GPIO_PORT290, GPIO_PORT291, GPIO_PORT292, GPIO_PORT293, GPIO_PORT294,
	GPIO_PORT295, GPIO_PORT296, GPIO_PORT297, GPIO_PORT298, GPIO_PORT299,
	GPIO_PORT300, GPIO_PORT301, GPIO_PORT302, GPIO_PORT303, GPIO_PORT304,
	GPIO_PORT305, GPIO_PORT306, GPIO_PORT307, GPIO_PORT308, GPIO_PORT309,
	GPIO_PORT310, GPIO_PORT311, GPIO_PORT312,

	/* 313 .. 319 are not available */
	GPIO_PORT320, GPIO_PORT321, GPIO_PORT322, GPIO_PORT323, GPIO_PORT324,
	GPIO_PORT325, GPIO_PORT326, GPIO_PORT327
};

#define EOS_GPIO_DATA_ADDR	(GPIO_BASE + 0x4000)

#define EOS_GPIO_DATA_ADDR_OFFSET(gpion)  ( (gpion >= 128) ? ( (gpion >= 192) ? (0x2000 - 0x18) : (0x1000 - 0x10) ) : 0x0000 )
#define EOS_GET_GPIO_REG_VALUE(gpion)	(__raw_readl(EOS_GPIO_DATA_ADDR + EOS_GPIO_DATA_ADDR_OFFSET(gpion) +((gpion/32)*4) ))
#define EOS_GET_GPIO_VALUE(gpion)	((EOS_GET_GPIO_REG_VALUE(gpion) & (1 << (gpion%32))) >> (gpion%32))

#define EOS_GPIO_FUNC_MASK	0x07
#define EOS_GPIO_DIR_MASK	0x30
#define EOS_GPIO_PULL_MASK	0xC0

#define EOS_GPIO_COUNT	(ARRAY_SIZE(available_gpios_eos2))

/****************************************************************/
/* Define value in accordance with
	the specification of each BB vendor. */
#define AP_GPIO_COUNT EOS_GPIO_COUNT
/****************************************************************/

/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static uint16_t checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result_t gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};

#ifdef SECGPIO_SLEEP_DEBUGGING
static struct sleepdebug_gpiotable sleepdebug_table;
#endif
/****************************************************************/

unsigned char get_gpio_pull_value(unsigned int pulldata)
{
	if (pulldata == 2) /* 10b Pull down */
		return GDVS_PUPD_PD;
	else if (pulldata == 3) /* 11b Pull Up */
		return GDVS_PUPD_PU;
	else /* Pull Off */
		return GDVS_PUPD_NP;
}

unsigned char get_gpio_dir_value(unsigned int dirdata)
{
	if (dirdata == 2) /* 10b  INPUT*/
		return GDVS_IO_IN;
	else if (dirdata == 1) /* 01b OUTPUT */
		return GDVS_IO_OUT;
	else if (dirdata == 0) /* 00b HI_Z */
		return GDVS_IO_HI_Z;
	else /* NONE */
		return GDVS_IO_ERR;
}

unsigned char get_gpio_value(unsigned int data)
{
	if (data == 0) /* 00b LOW */
		return GDVS_HL_L;
	else /* 01b HIGH */
		return GDVS_HL_H; /*  */
}

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void check_gpio_status(unsigned char phonestate)
{
	unsigned int i = 0, gpion = 0;
	unsigned char temp_io = 0, temp_pdpu = 0, temp_lh = 0;

	for(i = 0 ; i < AP_GPIO_COUNT ; i++)
	{
		gpion = available_gpios_eos2[i];
		
		if ((__raw_readb(GPIO_PORTCR(gpion)) & EOS_GPIO_FUNC_MASK) == 0x000)
			temp_io = get_gpio_dir_value((__raw_readb(GPIO_PORTCR(gpion)) & EOS_GPIO_DIR_MASK) >> 4);
		else
			temp_io = GDVS_IO_FUNC;
		
		temp_pdpu = get_gpio_pull_value((__raw_readb(GPIO_PORTCR(gpion)) & EOS_GPIO_PULL_MASK) >> 6);

		temp_lh = get_gpio_value(EOS_GET_GPIO_VALUE(gpion));
	
		checkgpiomap_result[phonestate][i] = GET_RESULT_GPIO(temp_io,temp_pdpu,temp_lh);
	}
	
	return;
}

#ifdef SECGPIO_SLEEP_DEBUGGING
/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
void setgpio_for_sleepdebug(int gpionum, uint16_t  io_pupd_lh)
{

}
/****************************************************************/

/****************************************************************/
/* Define this function in accordance with the specification of each BB vendor */
static void undo_sleepgpio(void)
{
	int i;
	int gpio_num;

	pr_info("[GPIO_DVS][%s] ++\n", __func__);

	for (i = 0; i < sleepdebug_table.gpio_count; i++) {
		gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;
		/* 
		 * << Caution >> 
		 * If it's necessary, 
		 * change the following function to another appropriate one 
		 * or delete it 
		 */
		setgpio_for_sleepdebug(gpio_num, gpiomap_result.sleep[gpio_num]);
	}

	pr_info("[GPIO_DVS][%s] --\n", __func__);
	return;
}
/****************************************************************/
#endif

/********************* Fixed Code Area !***************************/
#ifdef SECGPIO_SLEEP_DEBUGGING
static void set_sleepgpio(void)
{
	int i;
	int gpio_num;
	uint16_t set_data;

	pr_info("[GPIO_DVS][%s] ++, cnt=%d\n",
		__func__, sleepdebug_table.gpio_count);

	for (i = 0; i < sleepdebug_table.gpio_count; i++) {
		pr_info("[GPIO_DVS][%d] gpio_num(%d), io(%d), pupd(%d), lh(%d)\n",
			i, sleepdebug_table.gpioinfo[i].gpio_num,
			sleepdebug_table.gpioinfo[i].io,
			sleepdebug_table.gpioinfo[i].pupd,
			sleepdebug_table.gpioinfo[i].lh);

		gpio_num = sleepdebug_table.gpioinfo[i].gpio_num;

		// to prevent a human error caused by "don't care" value 
		if( sleepdebug_table.gpioinfo[i].io == GDVS_IO_IN)		/* IN */
			sleepdebug_table.gpioinfo[i].lh =
				GET_GPIO_LH(gpiomap_result.sleep[gpio_num]);
		else if( sleepdebug_table.gpioinfo[i].io == GDVS_IO_OUT)		/* OUT */
			sleepdebug_table.gpioinfo[i].pupd =
				GET_GPIO_PUPD(gpiomap_result.sleep[gpio_num]);

		set_data = GET_RESULT_GPIO(
			sleepdebug_table.gpioinfo[i].io,
			sleepdebug_table.gpioinfo[i].pupd,
			sleepdebug_table.gpioinfo[i].lh);

		setgpio_for_sleepdebug(gpio_num, set_data);
	}

	pr_info("[GPIO_DVS][%s] --\n", __func__);
	return;
}
#endif

static struct gpio_dvs_t gpio_dvs = {
	.result = &gpiomap_result,
	.count = AP_GPIO_COUNT,
	.check_init = false,
	.check_sleep = false,
	.check_gpio_status = check_gpio_status,
#ifdef SECGPIO_SLEEP_DEBUGGING
	.sdebugtable = &sleepdebug_table,
	.set_sleepgpio = set_sleepgpio,
	.undo_sleepgpio = undo_sleepgpio,
#endif
};

static struct platform_device secgpio_dvs_device = {
	.name	= "secgpio_dvs",
	.id		= -1,
	.dev.platform_data = &gpio_dvs,
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	return platform_add_devices(
		secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
/****************************************************************/

