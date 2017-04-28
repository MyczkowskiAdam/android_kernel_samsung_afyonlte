/*
 * arch/arm/mach-shmobile/setup-u2camera.c
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <mach/r8a7373.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/pmic/pmic-ncp6914.h>
#include <mach/common.h>
#include <media/sh_mobile_csi2.h>
#include <media/sh_mobile_rcu.h>
#include <media/soc_camera.h>
#include <media/soc_camera_platform.h>
#ifdef CONFIG_MFD_D2153
#include <linux/d2153/core.h>
#include <linux/d2153/pmic.h>
#include <linux/d2153/d2153_battery.h>
#endif
#include <mach/board.h>
#include <mach/setup-u2camera.h>
#include <mach/setup-u2csi2.h>
#include <mach/setup-u2rcu.h>
#include <mach/setup-u2ion.h>
extern int flash_check; 

#if defined(CONFIG_MACH_GARDALTE) || \
	defined(CONFIG_MACH_LOGANLTE) || \
	defined(CONFIG_MACH_AFYONLTE) || \
	defined(CONFIG_MACH_HEATLTE)
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX) && \
	defined(CONFIG_SOC_CAMERA_SR030PC50) /* Select by board Rev */
struct i2c_board_info i2c_cameras[] = {
	{
		I2C_BOARD_INFO("S5K4ECGX", 0x56),
	},
	{
		I2C_BOARD_INFO("SR030PC50", 0x30), /* TODO::HYCHO (0x61>>1) */
	},
};

struct soc_camera_link camera_links[] = {
	{
		.bus_id			= 0,
		.board_info		= &i2c_cameras[0],
		.i2c_adapter_id	= 1,
		.module_name	= "S5K4ECGX",
		.power			= S5K4ECGX_power,
	},
	{
		.bus_id			= 1,
		.board_info		= &i2c_cameras[1],
		.i2c_adapter_id	= 1,
		.module_name	= "SR030PC50",
		.power			= SR030PC50_power,
	},
};
#else	/* Select by board Rev */
struct i2c_board_info i2c_cameras[] = {
	/* Rear Camera */
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX)
	{
		I2C_BOARD_INFO("S5K4ECGX", 0x56),
	},
#endif
	/* Front Camera */
#if defined(CONFIG_SOC_CAMERA_SR030PC50)
	{
		I2C_BOARD_INFO("SR030PC50", 0x30), /* TODO::HYCHO (0x61>>1) */
	},
#endif
};

struct soc_camera_link camera_links[] = {
	/* Rear Camera */
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX)
	{
		.bus_id			= 0,
		.board_info		= &i2c_cameras[0],
		.i2c_adapter_id	= 1,
		.module_name	= "S5K4ECGX",
		.power			= S5K4ECGX_power,
	},
#endif
	/* Front Camera */
#if defined(CONFIG_SOC_CAMERA_SR030PC50)
	{
		.bus_id			= 1,
		.board_info		= &i2c_cameras[1],
		.i2c_adapter_id	= 1,
		.module_name	= "SR030PC50",
		.power			= SR030PC50_power,
	},
#endif
};
#endif	/* Select by board Rev */
#endif	/* Gardalte, Logan */

#if defined(CONFIG_MACH_LT02LTE) /* LT02LTE */
#if defined(CONFIG_SOC_CAMERA_SR352) && \
	defined(CONFIG_SOC_CAMERA_SR130PC20)	/* Select by board Rev */
static struct i2c_board_info i2c_cameras[] = {
	{
		I2C_BOARD_INFO("SR352", 0x20),
	},
	{
		I2C_BOARD_INFO("SR130PC20", 0x28), /* TODO::HYCHO (0x41>>1) */
	},
};
static struct soc_camera_link camera_links[] = {
	{
		.bus_id			= 0,
		.board_info		= &i2c_cameras[0],
		.i2c_adapter_id		= 1,
		.module_name		= "SR352",
		.power			= SR352_power,
	},
	{
		.bus_id			= 1,
		.board_info		= &i2c_cameras[1],
		.i2c_adapter_id	= 1,
		.module_name		= "SR130PC20",
		.power			= SR130PC20_power,
	},
};
#else	/* Select by board Rev */
struct i2c_board_info i2c_cameras[] = {
	/* Rear Camera */
#if defined(CONFIG_SOC_CAMERA_SR352)
	{
		I2C_BOARD_INFO("SR352", 0x20),
	},
#endif
	/* Front Camera */
#if defined(CONFIG_SOC_CAMERA_SR130PC20)
	{
		I2C_BOARD_INFO("SR130PC20", 0x28), /* TODO::HYCHO (0x41>>1) */
	},
#endif
};

struct soc_camera_link camera_links[] = {
	/* Rear Camera */
#if defined(CONFIG_SOC_CAMERA_SR352)
	{
		.bus_id			= 0,
		.board_info		= &i2c_cameras[0],
		.i2c_adapter_id		= 1,
		.module_name		= "SR352",
		.power			= SR352_power,
	},
#endif
	/* Front Camera */
#if defined(CONFIG_SOC_CAMERA_SR130PC20)
	{
		.bus_id			= 1,
		.board_info		= &i2c_cameras[1],
		.i2c_adapter_id	= 1,
		.module_name	= "SR130PC20",
		.power			= SR130PC20_power,
	},
#endif
};
#endif	/* Select by board Rev */
#endif	/* LT02LTE */

struct platform_device camera_devices[] = {
	{
		.name	= "soc-camera-pdrv",
		.id		= 0,
		.dev	= {
			.platform_data = &camera_links[0],
		},
	},
	{
		.name	= "soc-camera-pdrv",
		.id	=	1,
		.dev	= {
			.platform_data = &camera_links[1],
		},
	},
};

int camera_init(void)
{
	struct clk *vclk1_clk;
	struct clk *pll1_div2_clk;
	int iRet;

	gpio_request(GPIO_PORT3, NULL);
	gpio_direction_output(GPIO_PORT3, 0);	/* CAM_PWR_EN */
	gpio_request(GPIO_PORT20, NULL);
	gpio_direction_output(GPIO_PORT20, 0);	/* CAM0_RST_N */
	/* Gardalte, Logan, lt02lte */
		gpio_request(GPIO_PORT45, NULL);
		gpio_direction_output(GPIO_PORT45, 0);	/* CAM0_STBY */

	pll1_div2_clk = clk_get(NULL, "pll1_div2_clk");
	if (IS_ERR(pll1_div2_clk))
		printk(KERN_ERR "clk_get(pll1_div2_clk) failed\n");

	vclk1_clk = clk_get(NULL, "vclk1_clk");
	if (IS_ERR(vclk1_clk))
		printk(KERN_ERR "clk_get(vclk1_clk) failed\n");

	iRet = clk_set_parent(vclk1_clk, pll1_div2_clk);
	if (0 != iRet) {
		printk(KERN_ERR
		"clk_set_parent(vclk1_clk) failed (ret=%d)\n", iRet);
	}

	clk_put(vclk1_clk);
	clk_put(pll1_div2_clk);

	/* Camera ES version convert */
	camera_links[0].priv = &csi20_info;
	camera_links[1].priv = &csi21_info;
	printk(KERN_ALERT "Camera ISP ES version switch (ES2)\n");

#if defined(CONFIG_MACH_GARDALTE) || \
	defined(CONFIG_MACH_LOGANLTE) || \
	defined(CONFIG_MACH_AFYONLTE) || \
	defined(CONFIG_MACH_HEATLTE)
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX) && \
	defined(CONFIG_SOC_CAMERA_SR030PC50) /* Select by board Rev */
	csi20_info.clients[0].lanes = 0x3;
#else	/* Select by board Rev */
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX)
	csi20_info.clients[0].lanes = 0x3;
#endif
#endif	/* Select by board Rev */
#endif	/* Gardalte, Logan */

#if defined(CONFIG_MACH_LT02LTE) /* lt02lte */
#if defined(CONFIG_SOC_CAMERA_SR200PC20M) && \
	defined(CONFIG_SOC_CAMERA_SR130PC20) /* Select by board Rev */
	csi20_info.clients[0].lanes = 0x1;
	camera_devices[0].dev.platform_data = &camera_links_rev7[0];
	camera_devices[1].dev.platform_data = &camera_links_rev7[1];
	camera_links_rev7[0].priv = &csi20_info;
	camera_links_rev7[1].priv = &csi21_info;
#else	/* Select by board Rev */
#if defined(CONFIG_SOC_CAMERA_SR200PC20M)
	csi20_info.clients[0].lanes = 0x1;
#endif
	camera_devices[0].dev.platform_data = &camera_links[0];
	camera_devices[1].dev.platform_data = &camera_links[1];
	camera_links[0].priv = &csi20_info;
	camera_links[1].priv = &csi21_info;
#endif	/* Select by board Rev */

#if defined(CONFIG_SOC_CAMERA_SR352) && \
	defined(CONFIG_SOC_CAMERA_SR130PC20) /* Select by board Rev */
	csi20_info.clients[0].lanes = 0x1;
	camera_devices[0].dev.platform_data = &camera_links[0];
	camera_devices[1].dev.platform_data = &camera_links[1];
	camera_links[0].priv = &csi20_info;
	camera_links[1].priv = &csi21_info;
#else	/* Select by board Rev */
#if defined(CONFIG_SOC_CAMERA_SR352)
	csi20_info.clients[0].lanes = 0x1;
#endif
	camera_devices[0].dev.platform_data = &camera_links[0];
	camera_devices[1].dev.platform_data = &camera_links[1];
	camera_links[0].priv = &csi20_info;
	camera_links[1].priv = &csi21_info;
#endif	/* Select by board Rev */
#endif	/* lr02lte */

	return 0;
}

#define CAM_FLASH_ENSET     (GPIO_PORT99)
#define CAM_FLASH_FLEN      (GPIO_PORT100)

/* CAM0 Power function */
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX)
int S5K4ECGX_power(struct device *dev, int power_on)
{
	struct clk *vclk1_clk, *vclk2_clk;
	int iRet;
#if defined(CONFIG_MFD_D2153)
	struct regulator *regulator;
#endif
	dev_dbg(dev, "%s(): power_on=%d\n", __func__, power_on);

	vclk1_clk = clk_get(NULL, "vclk1_clk");
	if (IS_ERR(vclk1_clk)) {
		dev_err(dev, "clk_get(vclk1_clk) failed\n");
		return -1;
	}

	vclk2_clk = clk_get(NULL, "vclk2_clk");
	if (IS_ERR(vclk2_clk)) {
		dev_err(dev, "clk_get(vclk2_clk) failed\n");
		return -1;
	}

	if (power_on) {
		printk(KERN_ALERT "%s PowerON\n", __func__);
		sh_csi2_power(dev, power_on);
		gpio_set_value(GPIO_PORT3, 0); /* CAM_PWR_EN Low */
		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */
		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */

#if defined(CONFIG_MFD_D2153)
		/* CAM_AVDD_2V8  On */
		regulator = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator))
			return -1;
		regulator_enable(regulator);
		regulator_put(regulator);

		mdelay(2);

		/* CAM_VDDIO_1V8 On */
		regulator = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator))
			return -1;
		regulator_enable(regulator);
		regulator_put(regulator);
#endif

		mdelay(2);

		gpio_set_value(GPIO_PORT91, 1); /* CAM1_STBY */
		mdelay(2);

		iRet = clk_set_rate(vclk1_clk,
			clk_round_rate(vclk1_clk, 24000000));
		if (0 != iRet) {
			dev_err(dev,
				"clk_set_rate(vclk1_clk) failed (ret=%d)\n",
				iRet);
		}

		iRet = clk_enable(vclk1_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk1_clk) failed (ret=%d)\n",
				iRet);
		}
		mdelay(3);

		gpio_set_value(GPIO_PORT16, 1); /* CAM1_RST_N */
		mdelay(2);

		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */
		mdelay(2);

		/* CAM_CORE_1V2  On */
		gpio_set_value(GPIO_PORT3, 1);
		mdelay(1);

		gpio_set_value(GPIO_PORT45, 1); /* CAM0_STBY */
		mdelay(1);

		gpio_set_value(GPIO_PORT20, 1); /* CAM0_RST_N Hi */
		udelay(70);
		/* 1ms */

		/* 5M_AF_2V8 On */
#if defined(CONFIG_MFD_D2153)
		regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator))
			return -1;
		regulator_enable(regulator);
		regulator_put(regulator);
#endif

		printk(KERN_ALERT "%s PowerON fin\n", __func__);
	} else {
		printk(KERN_ALERT "%s PowerOFF\n", __func__);

		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		mdelay(1);

		clk_disable(vclk1_clk);

		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */
		mdelay(1);

		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		mdelay(1);

#if defined(CONFIG_MFD_D2153)
		/* CAM_CORE_1V2  Off */
		gpio_set_value(GPIO_PORT3, 0);
		mdelay(1);

		/* CAM_VDDIO_1V8 Off */
		regulator = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
		mdelay(1);

		/* CAM_AVDD_2V8  Off */
		regulator = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
		mdelay(1);

		/* 5M_AF_2V8 Off */
		regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
#endif
		if (!flash_check) {
                gpio_set_value(CAM_FLASH_ENSET, 0);
                gpio_set_value(CAM_FLASH_FLEN, 0);
	   	}
		sh_csi2_power(dev, power_on);
		printk(KERN_ALERT "%s PowerOFF fin\n", __func__);
	}

	clk_put(vclk1_clk);
	clk_put(vclk2_clk);

	return 0;
}
#endif
/*
#define CAM_FLASH_ENSET     (GPIO_PORT99)
#define CAM_FLASH_FLEN      (GPIO_PORT100)
*/
#if defined(CONFIG_FLASH_MIC2871) 
static void MIC2871_write(char addr, char data)
{
	int i;
	/* send address */
	printk(KERN_ALERT "%s addr(%d) data(%d)\n", __func__, addr, data);
	for (i = 0; i < (addr + 1); i++) {
		gpio_set_value(CAM_FLASH_ENSET, 0);
		udelay(1);
		gpio_set_value(CAM_FLASH_ENSET, 1);
		udelay(1);
	}
	/* wait T lat */
	udelay(97);
	/* send data */
	for (i = 0; i < (data + 1); i++) {
		gpio_set_value(CAM_FLASH_ENSET, 0);
		udelay(1);
		gpio_set_value(CAM_FLASH_ENSET, 1);
		udelay(1);
	}
/* THis only needs to be 405us */
	/* wait T end */
	udelay(405);
}

int main_cam_led(int light, int mode)
{
	unsigned long flags;
	spinlock_t lock;
	spin_lock_init(&lock);

	gpio_request(CAM_FLASH_ENSET, "camacq");
	gpio_request(CAM_FLASH_FLEN, "camacq");

	switch (light) {
	case SH_RCU_LED_ON:

		spin_lock_irqsave(&lock, flags);
		gpio_set_value(CAM_FLASH_ENSET, 1);

		/* write "Disabled"(0) to LB_TH(4) */
		MIC2871_write(4, 0);

		if (mode == SH_RCU_LED_MODE_PRE) {
			/* write 56%(21) to TEN/TCUR(2) */
			MIC2871_write(2, 21);
		} else {
			MIC2871_write(5, 1);
/* Register value 7 is the default for regiser 3, so no need to do this */
			/* MIC2871_write(3, 7); */
#if 0	/* Old case */
/* IF you use the FEN pin,then there is no need to program this register
						(FCUR default is 100%) */
			/* write 100%(0) to FEN/FCUR(1) */
			MIC2871_write(1, 0);

			/* Noneed to do this for torch mode */
		/* enable */
		gpio_set_value(CAM_FLASH_FLEN, 1);
#else	/* Following is hte new case using registers only */
		/* write 100%(0) to FEN/FCUR(1) */
		MIC2871_write(1, 16);
#endif		
		}

		spin_unlock_irqrestore(&lock, flags);
		break;
	case SH_RCU_LED_OFF:

		/* initailize falsh IC */
		gpio_set_value(CAM_FLASH_FLEN, 0);
		gpio_set_value(CAM_FLASH_ENSET, 0);
/* For SWI this only needs to be 400us */
		/* mdelay(1); */
		udelay(500);
		break;
	default:
		printk(KERN_ALERT "%s:not case %d", __func__, light);
		return -1;
		break;
	}
	gpio_free(CAM_FLASH_ENSET);
	gpio_free(CAM_FLASH_FLEN);

	return 0;
}
#endif

#if defined(CONFIG_FLASH_RT8547)
#define LED_MODE_MASK	    0x10

static unsigned char reg_value[4] = //for RT8547 flash
{
	0x00,
	0x12,
	0x02,
	0x0f,
};
#define RT8547_ADDR 	0x99
#define LONG_DELAY	9
#define SHORT_DELAY	4
#define START_DELAY 	10
#define STOP_DELAY	1500

static inline int RT8547_flash_send_bit(unsigned char bit)
{
	if (bit > 0)
	{
		gpio_set_value(CAM_FLASH_ENSET, 0);
		udelay(SHORT_DELAY);
		gpio_set_value(CAM_FLASH_ENSET, 1);
		udelay(LONG_DELAY);
	}
	else
	{
		gpio_set_value(CAM_FLASH_ENSET, 0);
		udelay(LONG_DELAY);
		gpio_set_value(CAM_FLASH_ENSET, 1);
		udelay(SHORT_DELAY);
	}
	return 0;
}


static inline int RT8547_flash_send_byte(unsigned char byte)
{
	int i;
	//send order is high bit to low bit
	for (i=7; i>=0; i--)
		RT8547_flash_send_bit(byte&(0x1<<i));
	return 0;
}

static inline int RT8547_flash_send_special_byte(unsigned char byte)
{
	int i;
	//only send three bit for register address
	for (i=2; i>=0; i--)
		RT8547_flash_send_bit(byte&(0x1<<i));
	return 0;
}


static inline int RT8547_flash_start_xfer(void)
{
	gpio_set_value(CAM_FLASH_ENSET, 1);
	udelay(START_DELAY);
	return 0;
}

static inline int RT8547_flash_stop_xfer(void)
{
	//redundant 1 bit as the stop condition
	RT8547_flash_send_bit(1);
	return 0;
}

static void RT8547_write(int reg,unsigned char data)
{
#if 0
	int i;
	/* send address */
	printk(KERN_ALERT "%s addr(%d) data(%d)\n", __func__, addr, data);
	for (i = 0; i < (addr + 1); i++) {
		gpio_set_value(CAM_FLASH_ENSET, 0);
		udelay(1);
		gpio_set_value(CAM_FLASH_ENSET, 1);
		udelay(1);
	}
	/* wait T lat */
	udelay(97);
	/* send data */
	for (i = 0; i < (data + 1); i++) {
		gpio_set_value(CAM_FLASH_ENSET, 0);
		udelay(1);
		gpio_set_value(CAM_FLASH_ENSET, 1);
		udelay(1);
	}
/* THis only needs to be 405us */
	/* wait T end */
	udelay(405);
#endif

unsigned long flags;
unsigned char xfer_data[3]; // 0: adddr, 1: reg, 2: reg data
spinlock_t lock;
spin_lock_init(&lock);
xfer_data[0] = RT8547_ADDR;
xfer_data[1] = (unsigned char)reg;
xfer_data[2] = (unsigned char)data;
//CAM_INFO_PRINTK( "rt8547-> 0: 0x%02x, 1: 0x%02x, 2: 0x%02x\n", xfer_data[0], xfer_data[1], xfer_data[2]));
spin_lock_irqsave(&lock, flags);

RT8547_flash_start_xfer();	
//send order is high bit to low bit
RT8547_flash_send_byte(xfer_data[0]);
RT8547_flash_send_special_byte(xfer_data[1]);
RT8547_flash_send_byte(xfer_data[2]);

RT8547_flash_stop_xfer();

spin_unlock_irqrestore(&lock, flags);

//write back to reg array
reg_value[reg-1] = data;



}

int main_cam_led(int light, int mode)
{
	int reg;
	unsigned char reg_val1;
	unsigned long flags;
	spinlock_t lock;
	spin_lock_init(&lock);

	gpio_request(CAM_FLASH_ENSET, "camacq");
	gpio_request(CAM_FLASH_FLEN, "camacq");

	switch (light) {
	case SH_RCU_LED_ON:

		spin_lock_irqsave(&lock, flags);
		gpio_set_value(CAM_FLASH_ENSET, 0);
		gpio_set_value(CAM_FLASH_FLEN, 0);



		if (mode == SH_RCU_LED_MODE_PRE) {
						// set torch current & set torch mode
									//set Low Vin Protection 
			reg = 0x01;
			reg_val1 = reg_value[reg-1];
			RT8547_write(reg, reg_val1);
			
			reg = 0x03;
			reg_val1 = (reg_value[reg-1] | LED_MODE_MASK);
			RT8547_write(reg, reg_val1);
		} 
		else {
			// set strobe current
						//set Low Vin Protection 
			reg = 0x01;
			reg_val1 = reg_value[reg-1];
			RT8547_write(reg, reg_val1);
			
			reg = 0x02;
			reg_val1 = reg_value[reg-1];
			RT8547_write(reg, reg_val1);
			// set strobe mode
			reg = 0x03;
			reg_val1 = (reg_value[reg-1] & ~LED_MODE_MASK);
			RT8547_write(reg, reg_val1);
			// set strobe timing to maximum time 0.5s
				//	udelay(SHORT_DELAY);
			reg = 0x04;
			reg_val1 = reg_value[reg-1];
			RT8547_write(reg, reg_val1);
		}
		gpio_set_value(CAM_FLASH_ENSET, 1);
		gpio_set_value(CAM_FLASH_FLEN, 1);
		spin_unlock_irqrestore(&lock, flags);
		break;
	case SH_RCU_LED_OFF:

		/* initailize falsh IC */
		gpio_set_value(CAM_FLASH_FLEN, 0);
		gpio_set_value(CAM_FLASH_ENSET, 0);
/* For SWI this only needs to be 400us */
		/* mdelay(1); */
		udelay(500);
		break;
	default:
		printk(KERN_ALERT "%s:not case %d", __func__, light);
		return -1;
		break;
	}
	gpio_free(CAM_FLASH_ENSET);
	gpio_free(CAM_FLASH_FLEN);

	return 0;
}
#endif



/* CAM1 Power function */
#if defined(CONFIG_SOC_CAMERA_SR030PC50)
int SR030PC50_power(struct device *dev, int power_on)
{
	struct clk *vclk1_clk, *vclk2_clk;
	int iRet;
#if defined(CONFIG_MFD_D2153)
	struct regulator *regulator;
#endif

	vclk1_clk = clk_get(NULL, "vclk1_clk");
	if (IS_ERR(vclk1_clk)) {
		dev_err(dev, "clk_get(vclk1_clk) failed\n");
		return -1;
	}

	vclk2_clk = clk_get(NULL, "vclk2_clk");
	if (IS_ERR(vclk2_clk)) {
		dev_err(dev, "clk_get(vclk2_clk) failed\n");
		return -1;
	}

	if (power_on) {
		printk(KERN_ALERT "%s PowerON\n", __func__);

		sh_csi2_power(dev, power_on);
		gpio_set_value(GPIO_PORT3, 0); /* CAM_PWR_EN */
		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		gpio_set_value(GPIO_PORT91, 0); /* TODO::HYCHO CAM1_CEN */
		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */

		mdelay(10);
		/* 10ms */

#if defined(CONFIG_MFD_D2153)
		/* CAM_AVDD_2V8  On */
		regulator = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator))
			return -1;
		regulator_enable(regulator);
		regulator_put(regulator);
		mdelay(1);

		/* CAM_VDDIO_1V8 On */
		regulator = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator))
			return -1;
		regulator_enable(regulator);
		regulator_put(regulator);
		mdelay(1);
#endif

		/* CAM_CORE_1V2  On */
		gpio_set_value(GPIO_PORT3, 1);
		mdelay(2);

		/* CAM_CORE_1V2  Off */
		gpio_set_value(GPIO_PORT3, 0);
		mdelay(4);

		/* MCLK Sub-Camera */
		iRet = clk_set_rate(vclk2_clk,
			clk_round_rate(vclk2_clk, 24000000));
		if (0 != iRet) {
			dev_err(dev,
				"clk_set_rate(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}

		iRet = clk_enable(vclk2_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}
		mdelay(10);

		gpio_set_value(GPIO_PORT91, 1); /* CAM1_STBY */
		mdelay(4);

		gpio_set_value(GPIO_PORT16, 1); /* CAM1_RST_N */
		mdelay(100);

		printk(KERN_ALERT "%s PowerON fin\n", __func__);
	} else {
		printk(KERN_ALERT "%s PowerOFF\n", __func__);

		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		mdelay(1);

		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */
		mdelay(2);

		clk_disable(vclk2_clk);
		mdelay(1);

#if defined(CONFIG_MFD_D2153)
		/* CAM_VDDIO_1V8 Off */
		regulator = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
		mdelay(1);

		/* CAM_AVDD_2V8  Off */
		/* cam_sensor_a2.8 */
		regulator = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
		mdelay(1);

		/* CAM_CORE_1V2  Off */
		gpio_set_value(GPIO_PORT3, 0);
		mdelay(1);
#endif
		sh_csi2_power(dev, power_on);
		printk(KERN_ALERT "%s PowerOFF fin\n", __func__);

	}

	clk_put(vclk1_clk);
	clk_put(vclk2_clk);

	return 0;
}

#endif

#if defined(CONFIG_SOC_CAMERA_SR200PC20M)
int SR200PC20M_power(struct device *dev, int power_on)
{
	struct clk *vclk1_clk, *vclk2_clk;
	int iRet;
#if defined(CONFIG_MFD_D2153)
	struct regulator *regulator_io;
	struct regulator *regulator_a;
	struct regulator *regulator_core;
#endif
	dev_dbg(dev, "%s(): power_on=%d\n", __func__, power_on);

	vclk1_clk = clk_get(NULL, "vclk1_clk");
	if (IS_ERR(vclk1_clk)) {
		dev_err(dev, "clk_get(vclk1_clk) failed\n");
		return -1;
	}

	vclk2_clk = clk_get(NULL, "vclk2_clk");
	if (IS_ERR(vclk2_clk)) {
		dev_err(dev, "clk_get(vclk2_clk) failed\n");
		return -1;
	}

	if (power_on) {
		printk(KERN_ALERT "%s PowerON\n", __func__);
		sh_csi2_power(dev, power_on);
		/* gpio_set_value(GPIO_PORT3, 0); */ /* CAM_PWR_EN Low */
		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */
		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */
		mdelay(10);
		/* 10ms */

#if defined(CONFIG_MFD_D2153)
#if 0
		/* CAM_CORE_1V2  On */
		gpio_set_value(GPIO_PORT3, 1);
		mdelay(1);
#endif

		/* CAM_VDDIO_1V8 Get */
		regulator_io = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator_io)) {
			dev_err(dev,
				"regulator_get(cam_sensor_io) failed\n");
			return -1;
		}
		regulator_set_voltage(regulator_io, 1800000, 1800000);
		/* CAM_AVDD_2V8  Get */
		regulator_a = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator_a)) {
			dev_err(dev,
				"regulator_get(cam_sensor_a) failed\n");
			return -1;
		}
		regulator_set_voltage(regulator_a, 2800000, 2800000);
		/* VT_DVDD_1V5   Get */
		regulator_core = regulator_get(NULL, "cam_core");
		if (IS_ERR(regulator_core)) {
			dev_err(dev,
				"regulator_get(cam_core) failed\n");
			return -1;
		}
		regulator_set_voltage(regulator_core, 1800000, 1800000);

		dev_err(dev, "regulator_enable s\n");
		/* CAM_VDDIO_1V8 enable */
		regulator_enable(regulator_io);
		/* CAM_AVDD_2V8  enable */
		regulator_enable(regulator_a);
		/* VT_DVDD_1V5   enable */
		regulator_enable(regulator_core);
		dev_err(dev, "regulator_enable e\n");

		/* Regulator free */
		regulator_put(regulator_io);
		regulator_put(regulator_a);
		regulator_put(regulator_core);
#endif

		gpio_set_value(GPIO_PORT91, 1); /* CAM1_STBY */
		udelay(50);

		/* MCLK Sub-Camera */
		iRet = clk_set_rate(vclk2_clk,
			clk_round_rate(vclk2_clk, 24000000));
		if (0 != iRet) {
			dev_err(dev,
				"clk_set_rate(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}

		iRet = clk_enable(vclk2_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}
		mdelay(10);

		gpio_set_value(GPIO_PORT16, 1); /* CAM1_RST_N */
		mdelay(150);
		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */
		clk_disable(vclk2_clk);

		mdelay(10);

		iRet = clk_set_rate(vclk1_clk,
			clk_round_rate(vclk1_clk, 24000000));
		if (0 != iRet) {
			dev_err(dev,
				"clk_set_rate(vclk1_clk) failed (ret=%d)\n",
				iRet);
		}

		iRet = clk_enable(vclk1_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk1_clk) failed (ret=%d)\n",
				iRet);
		}

		gpio_set_value(GPIO_PORT45, 1); /* CAM0_STBY */
		mdelay(30);

		gpio_set_value(GPIO_PORT20, 1); /* CAM0_RST_N Hi */
		mdelay(660);

		/* 5M_AF_2V8 On */
#if defined(CONFIG_MFD_D2153)
#if 0
		regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator))
			return -1;
		regulator_enable(regulator);
		regulator_put(regulator);
#endif
#endif
		mdelay(20);

		printk(KERN_ALERT "%s PowerON fin\n", __func__);
	} else {
		printk(KERN_ALERT "%s PowerOFF\n", __func__);

		mdelay(660);

		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		mdelay(10);

		clk_disable(vclk1_clk);

		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */
		mdelay(1);

		iRet = clk_enable(vclk2_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}
		mdelay(1);

		gpio_set_value(GPIO_PORT91, 1); /* CAM1_STBY */
		mdelay(1);
		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		mdelay(1);
		clk_disable(vclk2_clk);
		mdelay(1);
		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */

#if defined(CONFIG_MFD_D2153)
		/* VT_DVDD_1V5   Off */
		regulator_core = regulator_get(NULL, "cam_core");
		if (IS_ERR(regulator_core)) {
			dev_err(dev,
				"regulator_get(cam_core) failed\n");
			return -1;
		}

		/* CAM_AVDD_2V8  Off */
		regulator_a = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator_a)) {
			dev_err(dev,
				"regulator_get(cam_sensor_a) failed\n");
			return -1;
		}

		/* CAM_VDDIO_1V8 Off */
		regulator_io = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator_io)) {
			dev_err(dev,
				"regulator_get(cam_sensor_io) failed\n");
			return -1;
		}

		dev_err(dev, "regulator_disable s\n");
		/* VT_DVDD_1V5   disable */
		regulator_disable(regulator_core);
		/* CAM_AVDD_2V8  disable */
		regulator_disable(regulator_a);
		/* CAM_VDDIO_1V8 disable */
		regulator_disable(regulator_io);
		dev_err(dev, "regulator_disable e\n");

		/* Regulator free */
		regulator_put(regulator_core);
		regulator_put(regulator_a);
		regulator_put(regulator_io);

#if 0
		gpio_set_value(GPIO_PORT3, 0);
		mdelay(1);

		/* 5M_AF_2V8 Off */
		regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
#endif
#endif
		sh_csi2_power(dev, power_on);
		printk(KERN_ALERT "%s PowerOFF fin\n", __func__);
	}

	clk_put(vclk1_clk);
	clk_put(vclk2_clk);

	return 0;
}
#endif

#if defined(CONFIG_SOC_CAMERA_SR352)
int SR352_power(struct device *dev, int power_on)
{
	struct clk *vclk1_clk;
	int iRet;
	unsigned int u2_board_rev;
#if defined(CONFIG_MFD_D2153)
	struct regulator *regulator_io = NULL;
	struct regulator *regulator_a = NULL;
	struct regulator *regulator_af = NULL;
	struct regulator *regulator_vt = NULL;
#endif
	dev_dbg(dev, "%s(): power_on=%d\n", __func__, power_on);
	u2_board_rev = u2_get_board_rev();
	vclk1_clk = clk_get(NULL, "vclk1_clk");
	if (IS_ERR(vclk1_clk)) {
		dev_err(dev, "clk_get(vclk1_clk) failed\n");
		return -1;
	}

	if (power_on) {
		printk(KERN_ALERT "%s PowerON\n", __func__);
		sh_csi2_power(dev, power_on);
		/* gpio_set_value(GPIO_PORT3, 0); *//* CAM_PWR_EN Low */
		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		gpio_set_value(GPIO_PORT91, 0); /* CAM1_STBY */
		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */
		mdelay(10);
		/* 10ms */

#if defined(CONFIG_MFD_D2153)
#if 0
		/* CAM_CORE_1V2  On */
		gpio_set_value(GPIO_PORT3, 1);
		mdelay(1);
#endif

		/* CAM_VDDIO_1V8 Get */
		regulator_io = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator_io)) {
			dev_err(dev,
				"regulator_get(cam_sensor_io) failed\n");
			return -1;
		}
		regulator_set_voltage(regulator_io, 1800000, 1800000);
		/* CAM_AVDD_2V8  Get */
		regulator_a = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator_a)) {
			dev_err(dev,
				"regulator_get(cam_sensor_a) failed\n");
			return -1;
		}
		regulator_set_voltage(regulator_a, 2800000, 2800000);
		if (BOARD_REV_0_0 == u2_board_rev) {
			/* CAM_CORE_1.8V Get */
			regulator_vt = regulator_get(NULL, "vt_cam");
			if (IS_ERR(regulator_vt)) {
				dev_err(dev,
					"regulator_get(vt_cam) failed\n");
				return -1;
			}
			regulator_set_voltage(regulator_vt, 1200000, 1200000);
		} else {
		/* CAM_CORE_1_1V8 (VT)	 Get */
			regulator_vt = regulator_get(NULL, "vt_cam");
			if (IS_ERR(regulator_vt)) {
		   dev_err(dev,
					"regulator_get(vt_cam) failed\n");
		   return -1;
		}
			regulator_set_voltage(regulator_vt, 1800000, 1800000);
			/* CAM_af_1V2 (MAIN)  Get */
			regulator_af = regulator_get(NULL, "cam_af");
			if (IS_ERR(regulator_af)) {
			dev_err(dev,
					"regulator_get(cam_af) failed\n");
			return -1;
		}
			regulator_set_voltage(regulator_af, 1200000, 1200000);
		}
		dev_err(dev, "regulator_enable s\n");
		/* CAM_VDDIO_1V8 enable */
		regulator_enable(regulator_io);
		/* CAM_AVDD_2V8  enable */
		regulator_enable(regulator_a);
		if (BOARD_REV_0_0 == u2_board_rev) {
		/* CAM_CORE_1_1V8 (VT)	enable */
			regulator_enable(regulator_vt);
		} else {
		/* CAM_CORE_1_1V8 (VT)	enable */
			regulator_enable(regulator_vt);
		/* CAM_CORE_0_1V2 (MAIN)  enable */
			regulator_enable(regulator_af);
		}
		dev_err(dev, "regulator_enable e\n");

		/* Regulator free */
		regulator_put(regulator_io);
		regulator_put(regulator_a);
		if (BOARD_REV_0_0 == u2_board_rev) {
			regulator_put(regulator_vt);
		} else {
			regulator_put(regulator_vt);
			regulator_put(regulator_af);
		}
#endif

		mdelay(5);

		iRet = clk_set_rate(vclk1_clk,
			clk_round_rate(vclk1_clk, 24000000));
		if (0 != iRet) {
			dev_err(dev,
				"clk_set_rate(vclk1_clk) failed (ret=%d)\n",
				iRet);
		}

		iRet = clk_enable(vclk1_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk1_clk) failed (ret=%d)\n",
				iRet);
		}

		gpio_set_value(GPIO_PORT45, 1); /* CAM0_STBY */
		mdelay(20);

		gpio_set_value(GPIO_PORT20, 1); /* CAM0_RST_N Hi */

		mdelay(10);

		printk(KERN_ALERT "%s PowerON fin\n", __func__);
	} else {
		printk(KERN_ALERT "%s PowerOFF\n", __func__);

		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		mdelay(10);

		clk_disable(vclk1_clk);
		mdelay(1);

		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */
		mdelay(1);

#if defined(CONFIG_MFD_D2153)
		if (BOARD_REV_0_0 == u2_board_rev) {
			/* CAM_CORE_1.8V Get */
			regulator_vt = regulator_get(NULL, "vt_cam");
			if (IS_ERR(regulator_vt)) {
				dev_err(dev,
					"regulator_get(vt_cam) failed\n");
				return -1;
			}
		} else {
		/* CAM_CORE_0_1V2 (MAIN)  off */
			regulator_af = regulator_get(NULL, "cam_af");
			if (IS_ERR(regulator_af)) {
			dev_err(dev,
					"regulator_get(cam_af) failed\n");
			return -1;
		}

		/* CAM_CORE_1_1V8 (VT)	off */
			regulator_vt = regulator_get(NULL, "vt_cam");
			if (IS_ERR(regulator_vt)) {
			dev_err(dev,
					"regulator_get(vt_cam) failed\n");
			return -1;
		}
		}

		/* CAM_AVDD_2V8  Off */
		regulator_a = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator_a)) {
			dev_err(dev,
				"regulator_get(cam_sensor_a) failed\n");
			return -1;
		}

		/* CAM_VDDIO_1V8 Off */
		regulator_io = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator_io)) {
			dev_err(dev,
				"regulator_get(cam_sensor_io) failed\n");
			return -1;
		}

		dev_err(dev, "regulator_disable s\n");
		if (BOARD_REV_0_0 == u2_board_rev) {
			/* CAM_CORE_1_1V8 (VT) disable */
			regulator_disable(regulator_vt);
		} else {
		/* CAM_CORE_0_1V2 (MAIN) disable */
			regulator_disable(regulator_af);
		/* CAM_CORE_1_1V8 (VT) disable */
			regulator_disable(regulator_vt);
		}
		/* CAM_AVDD_2V8  disable */
		regulator_disable(regulator_a);
		/* CAM_VDDIO_1V8 disable */
		regulator_disable(regulator_io);
		dev_err(dev, "regulator_disable e\n");

		/* Regulator free */
		if (BOARD_REV_0_0 == u2_board_rev) {
			regulator_put(regulator_vt);
		} else {
			regulator_put(regulator_af);
			regulator_put(regulator_vt);
		}
		regulator_put(regulator_a);
		regulator_put(regulator_io);

#if 0
		gpio_set_value(GPIO_PORT3, 0);
		mdelay(1);

		/* 5M_AF_2V8 Off */
		regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator))
			return -1;
		regulator_disable(regulator);
		regulator_put(regulator);
#endif
#endif
		sh_csi2_power(dev, power_on);
		printk(KERN_ALERT "%s PowerOFF fin\n", __func__);
	}

	clk_put(vclk1_clk);

	return 0;
}
#endif

#if defined(CONFIG_SOC_CAMERA_SR130PC20)
int SR130PC20_power(struct device *dev, int power_on)
{
	struct clk *vclk2_clk;
	int iRet;
	struct regulator *regulator_io;
	struct regulator *regulator_a;
	struct regulator *regulator_af;
	struct regulator *regulator_vt;

	vclk2_clk = clk_get(NULL, "vclk2_clk");
	if (IS_ERR(vclk2_clk)) {
		dev_err(dev, "clk_get(vclk2_clk) failed\n");
		return -1;
	}

	if (power_on) {
		printk(KERN_ALERT "%s PowerON\n", __func__);

	/*
	[SXGA ON] CAM_A2.8V(LDO12) / CAM_IO_1.8V(LDO17) / CAM_CORE_1.8V(LDO14)
	VDD:I(1.8v) -> VDD:A(2.8v) -> VDD:D(1.8v)
	CAM ENB/RESET Low -> SXGA ENB Low(OFF) -> CAM Power High ->
	SXGA_ENB HIGH(ON) -> MCLK 24MHz On -> Delay 10ms -> SXGA RESET High ->
	Delay 100 cycle -> SXGA_ENB LOW(OFF) -> Main CAM Hi-Z Sequence ->
	SXGA_ENB_HIGH(ON) -> Delay 30ms -> SXGA RESET Toggle -> SXGA INIT CODE
	*/

		sh_csi2_power(dev, power_on);
		gpio_set_value(GPIO_PORT3, 0); /* CAM_PWR_EN */
		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		gpio_set_value(GPIO_PORT91, 0); /* CAM1_ENB */
		gpio_set_value(GPIO_PORT20, 0); /* CAM0_RST_N */
		gpio_set_value(GPIO_PORT45, 0); /* CAM0_STBY */

		mdelay(10);

		/* CAM_VDDIO_1V8 On */
		regulator_io = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator_io))
			return -1;
		regulator_set_voltage(regulator_io, 1800000, 1800000);
		
		regulator_enable(regulator_io);
		regulator_put(regulator_io);
		mdelay(1);

		/* CAM_AVDD_2V8  On */
		regulator_a = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator_a))
			return -1;
		regulator_set_voltage(regulator_a, 2800000, 2800000);
		
		regulator_enable(regulator_a);
		regulator_put(regulator_a);
		mdelay(1);

		/* CAM_CORE_1_1V8 (VT)	 On */
		regulator_vt = regulator_get(NULL, "vt_cam");
		if (IS_ERR(regulator_vt))
			return -1;
		if (!(BOARD_REV_0_0 == u2_get_board_rev()))
			regulator_set_voltage(regulator_vt, 1800000, 1800000);

		regulator_enable(regulator_vt);
		regulator_put(regulator_vt);
		mdelay(1);

		/* CAM_CORE_0_1V2 (MAIN)  On */
		regulator_af = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator_af)) {
			dev_err(dev,
				"regulator_get(cam_af) failed\n");
			return -1;
		}
		regulator_set_voltage(regulator_af, 1200000, 1200000);
		regulator_enable(regulator_af);
		regulator_put(regulator_af);

		mdelay(10);

		/* MCLK Sub-Camera */
		iRet = clk_set_rate(vclk2_clk,
			clk_round_rate(vclk2_clk, 24000000));
		if (0 != iRet) {
			dev_err(dev,
				"clk_set_rate(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}

		iRet = clk_enable(vclk2_clk);
		if (0 != iRet) {
			dev_err(dev, "clk_enable(vclk2_clk) failed (ret=%d)\n",
				iRet);
		}
		gpio_set_value(GPIO_PORT91, 1); /* CAM1_ENB */
		mdelay(50);

		gpio_set_value(GPIO_PORT16, 1); /* CAM1_RST_N */
		mdelay(10);

		printk(KERN_ALERT "%s PowerON fin\n", __func__);
	} else {
		printk(KERN_ALERT "%s PowerOFF\n", __func__);

	/*
	[Main CAM OFF]
	Main CAM Power OFF Sequence -> Sleep command->SXGA RESET LOW(OFF) ->
	MCLK OFF -> SXGA_STDBY LOW(OFF) -> VDD:C Low(OFF) ->
	VDD:A Low(OFF) -> VDD:I Low(OFF)
	*/

	/* Main CAM Power-off sequence */

		gpio_set_value(GPIO_PORT16, 0); /* CAM1_RST_N */
		mdelay(1);

		clk_disable(vclk2_clk);

		gpio_set_value(GPIO_PORT91, 0); /* CAM1_ENB */
		mdelay(1);

		/* CAM_af_1V2 (MAIN)  Off */
		regulator_af = regulator_get(NULL, "cam_af");
		if (IS_ERR(regulator_af))
			return -1;
		regulator_disable(regulator_af);
		regulator_put(regulator_af);
		mdelay(1);

		/* CAM_CORE_1_1V8 (VT)	 Off */
		regulator_vt = regulator_get(NULL, "vt_cam");
		if (IS_ERR(regulator_vt))
			return -1;
		regulator_disable(regulator_vt);
		regulator_put(regulator_vt);
		mdelay(1);

		/* CAM_AVDD_2V8  Off */
		regulator_a = regulator_get(NULL, "cam_sensor_a");
		if (IS_ERR(regulator_a))
			return -1;
		regulator_disable(regulator_a);
		regulator_put(regulator_a);
		mdelay(1);

		/* CAM_VDDIO_1V8  Off */
		/* cam_sensor_a2.8 */
		regulator_io = regulator_get(NULL, "cam_sensor_io");
		if (IS_ERR(regulator_io))
			return -1;
		regulator_disable(regulator_io);
		regulator_put(regulator_io);
		mdelay(1);

		sh_csi2_power(dev, power_on);
		printk(KERN_ALERT "%s PowerOFF fin\n", __func__);

	}

	clk_put(vclk2_clk);

	return 0;
}
#endif
