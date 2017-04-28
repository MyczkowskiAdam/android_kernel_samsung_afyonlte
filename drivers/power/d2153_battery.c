/*
 * Battery driver for Dialog D2153
 *   
 * Copyright(c) 2013 Dialog Semiconductor Ltd.
 *  
 * Author: Dialog Semiconductor Ltd. D. Chen, A Austin, E Jeong
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#ifdef CONFIG_D2153_HW_TIMER
#include <linux/irq.h>
#include <mach/irqs.h>
#include <mach/r8a7373.h>
#endif

#include <linux/io.h>
#include <mach/common.h>
#include <linux/jiffies.h>
#include <linux/spa_power.h>

#include "linux/err.h"

#include <linux/d2153/core.h>
#include <linux/d2153/d2153_battery.h>
#include <linux/d2153/d2153_reg.h>


static const char __initdata d2153_battery_banner[] = \
    "D2153 Battery, (c) 2013 Dialog Semiconductor Ltd.\n";

/***************************************************************************
 Pre-definition
***************************************************************************/
#define FALSE								(0)
#define TRUE								(1)

#define ADC_RES_MASK_LSB					(0x0F)
#define ADC_RES_MASK_MSB					(0xF0)

#if USED_BATTERY_CAPACITY == BAT_CAPACITY_1800MA
#define ADC_VAL_100_PERCENT            		3645   // About 4280mV 
#define MAX_FULL_CHARGED_ADC				3768   // About 4340mV
#define ORIGN_CV_START_ADC					3686   // About 4300mV
#define ORIGN_FULL_CHARGED_ADC				3780   // About 4345mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4310   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4347   // About EOC 50mA

#define FIRST_VOLTAGE_DROP_ADC  			121

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				880
#define CHARGE_OFFSET_KRNL_L				5

#define CHARGE_ADC_KRNL_H					1700
#define CHARGE_ADC_KRNL_L					2050
#define CHARGE_ADC_KRNL_F					2060

#define CHARGE_OFFSET_KRNL					77
#define CHARGE_OFFSET_KRNL2					50

#define LAST_CHARGING_WEIGHT      			380   // 900
#define VBAT_3_4_VOLTAGE_ADC				1605

#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA
#define ADC_VAL_100_PERCENT            		3604   // 3645(4280mV) -> 3604(4260mV)
#define MAX_FULL_CHARGED_ADC				3788   // About 4350mV
#define ORIGN_CV_START_ADC					3686   // About 4300mV
#define ORIGN_FULL_CHARGED_ADC				3780   // About 4345mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4260   // 1st EOC : about 180mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4327   // 2nd EOC : about 80mA
#define D2153_BAT_RECHG_FULL_LVL			4330   // Recharge EOC : about 80mA

#define FIRST_VOLTAGE_DROP_ADC  			145

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				270     // 360
#define CHARGE_OFFSET_KRNL_L				101     // 160

#define CHARGE_OFFSET_KRNL_H3				92
#define CHARGE_OFFSET_KRNL_L3				10

#define CHARGE_OFFSET_KRNL_F4				108

#define CHARGE_ADC_KRNL_H					1100    // 1150
#define CHARGE_ADC_KRNL_L					2609    // 2304
#define CHARGE_ADC_KRNL_F					2610    // 2305

#define CHARGE_ADC_KRNL_F2					3462    // 3300

#define CHARGE_ADC_KRNL_H3					3463    // 3301
#define CHARGE_ADC_KRNL_L3					3479    // 3474
#define CHARGE_ADC_KRNL_F3					3480    // 3475

#define CHARGE_OFFSET_KRNL					108
#define CHARGE_OFFSET_KRNL2					86      // 104

#define LAST_CHARGING_WEIGHT      			198
#define VBAT_3_4_VOLTAGE_ADC				1605

#else

#define ADC_VAL_100_PERCENT            		3445
#define MAX_FULL_CHARGED_ADC				3470
#define ORIGN_CV_START_ADC					3320
#define ORIGN_FULL_CHARGED_ADC				3480   // About 4345mV

#define D2153_BAT_CHG_FRST_FULL_LVL         4160   // About EOC 160mA
#define D2153_BAT_CHG_BACK_FULL_LVL         4185   // About EOC 60mA

#define FIRST_VOLTAGE_DROP_ADC  			165

#define FIRST_VOLTAGE_DROP_LL_ADC			340
#define FIRST_VOLTAGE_DROP_L_ADC			60
#define FIRST_VOLTAGE_DROP_RL_ADC			25

#define CHARGE_OFFSET_KRNL_H				880
#define CHARGE_OFFSET_KRNL_L				5

#define CHARGE_ADC_KRNL_H					1700
#define CHARGE_ADC_KRNL_L					2050
#define CHARGE_ADC_KRNL_F					2060

#define CHARGE_OFFSET_KRNL					77
#define CHARGE_OFFSET_KRNL2					50

#define LAST_CHARGING_WEIGHT      			450   // 900
#define VBAT_3_4_VOLTAGE_ADC				1605

#endif /* USED_BATTERY_CAPACITY == BAT_CAPACITY_????MA */

#define FULL_CAPACITY						1000

#define NORM_NUM                			10000
#define NORM_CHG_NUM						100000

#define DISCHARGE_SLEEP_OFFSET              55    // 45
#define LAST_VOL_UP_PERCENT                 75

/* Static Function Prototype */
/* static void d2153_external_event_handler(int category, int event); */
static int  d2153_read_adc_in_auto(struct d2153_battery *pbat, adc_channel channel);
static int  d2153_read_adc_in_manual(struct d2153_battery *pbat, adc_channel channel);

static struct d2153_battery *gbat = NULL;
#ifdef CONFIG_D2153_HW_TIMER
static struct timeval suspend_time = {0, 0};
static struct timeval resume_time = {0, 0};
static long time_diff = 0;
#endif

static u8  is_called_by_ticker = 0;

extern struct spa_power_data spa_power_pdata;

/* This array is for setting ADC_CONT register about each channel.*/
static struct adc_cont_in_auto adc_cont_inven[D2153_ADC_CHANNEL_MAX - 1] = {
	/* VBAT_S channel */
	[D2153_ADC_VOLTAGE] = {
		.adc_preset_val = 0,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK | D2153_ADC_MODE_MASK 
							| D2153_AUTO_VBAT_EN_MASK),
		.adc_msb_res = D2153_VDD_RES_VBAT_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO1_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	/* TEMP_1 channel */
	[D2153_ADC_TEMPERATURE_1] = {
		.adc_preset_val = D2153_TEMP1_ISRC_EN_MASK,	 
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK | D2153_ADC_MODE_MASK),
		.adc_msb_res = D2153_TBAT1_RES_TEMP1_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO1_REG,
		.adc_lsb_mask = ADC_RES_MASK_MSB,
	},
	/*	TEMP_2 channel */
	[D2153_ADC_TEMPERATURE_2] = {
		.adc_preset_val =  D2153_TEMP2_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK | D2153_ADC_MODE_MASK),
		.adc_msb_res = D2153_TBAT2_RES_TEMP2_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO3_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	/* VF channel */
	[D2153_ADC_VF] = {
		.adc_preset_val = D2153_AD4_ISRC_ENVF_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK | D2153_ADC_MODE_MASK 
							| D2153_AUTO_VF_EN_MASK),
		.adc_msb_res = D2153_ADCIN4_RES_VF_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO2_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	/* AIN channel */
	[D2153_ADC_AIN] = {
		.adc_preset_val = D2153_AD5_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK | D2153_ADC_MODE_MASK
							| D2153_AUTO_AIN_EN_MASK),
		.adc_msb_res = D2153_ADCIN5_RES_AIN_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO2_REG,
		.adc_lsb_mask = ADC_RES_MASK_MSB
	},
};

/* LUT for NCP15XW223 thermistor with 10uA current source selected */
static struct adc2temp_lookuptbl adc2temp_lut = {
	// Case of NCP03XH223 for TEMP1/TEMP2
	.adc  = {  // ADC-12 input value
		2144,      1691,      1341,      1072,     865,      793,      703,
		577,       480,       400,       334,      285,      239,      199,
		179,       168,       143,       124,      106,      98,       93,
		88, 
	},
	.temp = {	// temperature (degree K)
		C2K(-200), C2K(-150), C2K(-100), C2K(-50), C2K(0),	 C2K(20),  C2K(50),
		C2K(100),  C2K(150),  C2K(200),  C2K(250), C2K(300), C2K(350), C2K(400),
		C2K(430),  C2K(450),  C2K(500),  C2K(550), C2K(600), C2K(630), C2K(650),
		C2K(670),
	},
};


static struct adc2temp_lookuptbl adc2temp_lut_for_adcin = {
	// Case of NCP03XH223 for ADC-IN
	.adc  = {  // ADC-12 input value
		3967,      3229,      2643,      2176,     1801,     1498,     1344,
		1251,      1166,      1051,      948,      885,      828,      749,
		679,       636,       597,       543,      495,      465,      438,
		401, 
	},
	.temp = {	// temperature (degree K)
		C2K(50),  C2K(100), C2K(150), C2K(200), C2K(250), C2K(300), C2K(330),
		C2K(350), C2K(370), C2K(400), C2K(430), C2K(450), C2K(470), C2K(500),
		C2K(530), C2K(550), C2K(570), C2K(600), C2K(630), C2K(650), C2K(670),
		C2K(700),
	},
};



static u16 temp_lut_length = (u16)sizeof(adc2temp_lut.adc)/sizeof(u16);
static u16 adcin_lut_length = (u16)sizeof(adc2temp_lut_for_adcin.adc)/sizeof(u16); 

static struct adc2vbat_lookuptbl adc2vbat_lut = {
	.adc	 = {1843, 1946, 2148, 2253, 2458, 2662, 2867, 2683, 3072, 3482,}, // ADC-12 input value
	.offset  = {   0,	 0,    0,	 0,    0,	 0,    0,	 0,    0,    0,}, // charging mode ADC offset
	.vbat	 = {3400, 3450, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200,}, // VBAT (mV)
};

#if USED_BATTERY_CAPACITY == BAT_CAPACITY_1800MA
#if defined(CONFIG_MACH_LOGANLTE)
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1906, 2056, 2213, 2310, 2396, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)          //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100
static u16 adc_weight_section_discharge[]       = {14100, 9100,  7585, 2418, 1235,  375,  101,   98,  142,  356,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_rlt[]   = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lt[]    = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lmt[]   = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_llt[]   = {19100, 9400, 10985, 1985,  480,  337,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};

//Charging Weight(Room/Low/low low)             //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90, 100
static u16 adc_weight_section_charge[]          = { 9700,  734,   585,  453,  321,  248,   95,   93,  105,  223,  219,  265,  281,  288, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		= { 9700,  734,   488,  385,  225,  225,   95,   93,  105,  218,  216,  259,  281,  303, LAST_CHARGING_WEIGHT};

static u16 fg_reset_drop_offset[] = {122, 58};

#elif defined(CONFIG_MACH_CS02)

static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1906, 2155, 2310, 2338, 2367, 2563, 2627, 2688, 2762, 2920, 3069, 3249, 3458, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
// This is for 0.1C
static u16 adc_weight_section_discharge_1c[]     = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_rlt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_lt_1c[]  = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_lmt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};
static u16 adc_weight_section_discharge_llt_1c[] = {110, 160, 230, 280, 240, 101,  17,  13,  18,  33,  34,  68,  95, 125, 195};

// This is for 0.2C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
                                                 //{ 40,  80, 150, 150, 120,  59,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_2c[]     = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_rlt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_lt_2c[]  = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_lmt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};
static u16 adc_weight_section_discharge_llt_2c[] = {140, 180, 190, 150, 120, 105,  18,  16,  21,  39,  36,  58,  73,  80, 125};

//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_charge[]           = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]       = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]        = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]       = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]       = {2950, 355, 265,  66,  45,  93,  32,  33,  42,  92,  97, 110, 120,  90, LAST_CHARGING_WEIGHT};

static struct diff_tbl dischg_diff_tbl = {
   //  0,  2-1,  4-3,  6-5,  9-7,19-10,29-20,39-30,49-40
	{173,  173,  143,    8,  15,   54,   40,   37,   50},  // 0.1C
	{314,  314,  314,  145, 145,  177,  209,  221,  216},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {172-50, 98-40};
#endif

#elif USED_BATTERY_CAPACITY == BAT_CAPACITY_2100MA

static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1891, 2170, 2314, 2395, 2438, 2573, 2623, 2666, 2746, 2898, 3057, 3246, 3466, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low)           //     0,    1,     3,    5,    7,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100
static u16 adc_weight_section_discharge[]        = {14100, 4800,  3385,  965,  393,  425,  471,  485,  638,  683,  793,  885, 1002, 1328, 2495};
static u16 adc_weight_section_discharge_rlt[]    = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lt[]     = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_lmt[]    = {14100, 9100,  8985, 1585,  525,  352,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};
static u16 adc_weight_section_discharge_llt[]    = {19100, 9400, 10985, 1985,  480,  337,  132,  119,  165,  375,  355,  514,  769, 1228, 2495};

//These table are for 0.1C load currents.
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_discharge_1c[]     = {155, 195, 103,  42,  21,  19,   8,   8,  13,  21,  29,  38,  51,  50, 285};
static u16 adc_weight_section_discharge_rlt_1c[] = {155, 195, 103,  42,  21,  19,   8,   8,  13,  21,  29,  38,  51,  50, 285};
static u16 adc_weight_section_discharge_lt_1c[]  = {155, 195, 103,  42,  21,  19,   8,   8,  13,  21,  29,  38,  51,  50, 285};
static u16 adc_weight_section_discharge_lmt_1c[] = {155, 195, 103,  42,  21,  19,   8,   8,  13,  21,  29,  38,  51,  50, 285};
static u16 adc_weight_section_discharge_llt_1c[] = {155, 195, 103,  42,  21,  19,   8,   8,  13,  21,  29,  38,  51,  50, 285};

//These table are for 0.2C load currents.
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_discharge_2c[]     = {198, 249, 155,  69,  30,  29,  11,  10,  18,  32,  33,  43,  58,  53, 285};
static u16 adc_weight_section_discharge_rlt_2c[] = {198, 249, 155,  69,  30,  29,  11,  10,  18,  32,  33,  43,  58,  53, 285};
static u16 adc_weight_section_discharge_lt_2c[]  = {198, 249, 155,  69,  30,  29,  11,  10,  18,  32,  33,  43,  58,  53, 285};
static u16 adc_weight_section_discharge_lmt_2c[] = {198, 249, 155,  69,  30,  29,  11,  10,  18,  32,  33,  43,  58,  53, 285};
static u16 adc_weight_section_discharge_llt_2c[] = {198, 249, 155,  69,  30,  29,  11,  10,  18,  32,  33,  43,  58,  53, 285};

//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
static u16 adc_weight_section_charge[]           = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 119, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 119, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 119, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 119, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		 = {2950, 954, 468, 325, 102,  94,  33,  30,  52, 101, 112, 132, 141, 119, LAST_CHARGING_WEIGHT};

static struct diff_tbl dischg_diff_tbl = {
   //  0,  1-2,  3-4,  5-6,  7-9,10-19,20-29,30-39,40-49,50-59,60-69,70-79,80-89,90-99, 100
	{355,  277,  267,  264,  180,  117,  110,  117,  115,  115,   95,   79,   69,   30,  35},  // 0.1C
	{710,  706,  513,  409,  350,  299,  262,  282,  277,  291,  266,  222,  217,  185, 164},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {166, 86};

#else
// For 2100mAh battery
static struct adc2soc_lookuptbl adc2soc_lut = {
	.adc_ht  = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ high temp
	.adc_rt  = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ room temp
	.adc_rlt = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lt  = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low temp(0)
	.adc_lmt = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low mid temp(-10)
	.adc_llt = {1843, 1911, 2206, 2375, 2395, 2415, 2567, 2623, 2670, 2755, 2921, 3069, 3263, 3490, ADC_VAL_100_PERCENT,}, // ADC input @ low low temp(-20)
	.soc	 = {   0,	10,   30,	50,   70,  100,  200,  300,  400,  500,  600,  700,  800,  900, 1000,}, // SoC in %
};

//Discharging Weight(Room/Low/low low) for 0.1C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_discharge_1c[]   = { 50,  95,  75,  15,  15,  25,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_1c[]     = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_rlt_1c[] = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_lt_1c[]  = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_lmt_1c[] = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};
static u16 adc_weight_section_discharge_llt_1c[] = { 49,  40,  15,  10,  14,  23,  10,  10,  17,  28,  32,  54,  75, 100, 295};

//Discharging Weight(Room/Low/low low) for 0.2C
//Discharging Weight(Room/Low/low low)           //   0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_discharge_2c[]   = { 50, 245, 210,  24,  18,  40,  14,  12,  23,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_2c[]     = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_rlt_2c[] = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_lt_2c[]  = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_lmt_2c[] = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};
static u16 adc_weight_section_discharge_llt_2c[] = { 50, 215, 201,  24,  19,  42,  15,  13,  24,  38,  37,  54,  75,  75, 265};

//Charging Weight(Room/Low/low low)              //    0,   1,   3,   5,   7,  10,  20,  30,  40,  50,  60,  70,  80,  90, 100
//static u16 adc_weight_section_charge[]         = {2800, 642, 620,  63,  53, 108,  40,  33,  58, 113, 107, 133, 142,  63, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge[]           = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_rlt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_lmt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};
static u16 adc_weight_section_charge_llt[]		 = {2800, 642, 620,  63,  53, 106,  37,  31,  54, 108, 102, 130, 147,  70, LAST_CHARGING_WEIGHT};

static struct diff_tbl dischg_diff_tbl = {
   //  0,  1-2,  3-4,  5-6,  7-9,10-19,20-29,30-39,40-49,50-59,60-69,70-79,80-89,90-99, 100
	{400,  360,  230,  120,  120,  130,  110,  110,  110,  115,  100,   75,   60,   30,  20},  // 0.1C
	{635,  465,  270,  245,  220,  245,  230,  225,  217,  245,  245,  213,  178,  153, 250},  // 0.2C
};

static u16 fg_reset_drop_offset[] = {172, 98};

#endif /* USED_BATTERY_CAPACITY == BAT_CAPACITY_????MA */


static u16 adc2soc_lut_length = (u16)sizeof(adc2soc_lut.soc)/sizeof(u16);
static u16 adc2vbat_lut_length = (u16)sizeof(adc2vbat_lut.offset)/sizeof(u16);


#ifdef CONFIG_D2153_DEBUG_FEATURE
unsigned int d2153_attr_idx=0;

static ssize_t d2153_battery_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf);

enum {
	D2153_PROP_SOC_LUT = 0,
	D2153_PROP_DISCHG_WEIGHT,
	D2153_PROP_CHG_WEIGHT,
	D2153_PROP_BAT_CAPACITY,
	/* If you want to add a property, 
	   then please add before "D2153_PROP_ALL" attributes */
	D2153_PROP_ALL,
};

#define D2153_PROP_MAX 			(D2153_PROP_ALL + 1)

static struct device_attribute d2153_battery_attrs[]=
{
	__ATTR(display_soc_lut, 0644, d2153_battery_attrs_show, NULL),
	__ATTR(display_dischg_weight, 0644, d2153_battery_attrs_show, NULL),
	__ATTR(display_chg_weight, 0644, d2153_battery_attrs_show, NULL),
	__ATTR(display_capacity, 0644, d2153_battery_attrs_show, NULL),
	/* If you want to add a property, 
	   then please add before "D2153_PROP_ALL" attributes */
	__ATTR(display_all_information, 0644, d2153_battery_attrs_show, NULL),
};


static ssize_t d2153_battery_attrs_show(struct device *pdev, 
										struct device_attribute *attr, 
										char *buf)
{
	u8 i = 0, length = 0;
	ssize_t count = 0;
	unsigned int view_all = 0;
	const ptrdiff_t off = attr - d2153_battery_attrs;

	switch(off) {
		case D2153_PROP_ALL:
			view_all=1;

		case D2153_PROP_SOC_LUT:
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"\n## SOC Look up table ...\n");
			// High temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc2soc_lut.adc_ht  = {");
			for(i = 0; i < adc2soc_lut_length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%4d,", adc2soc_lut.adc_ht[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Room temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc2soc_lut.adc_rt  = {");
			for(i = 0; i < adc2soc_lut_length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%4d,", adc2soc_lut.adc_rt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Room-low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc2soc_lut.adc_rlt = {");
			for(i = 0; i < adc2soc_lut_length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%4d,", adc2soc_lut.adc_rlt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc2soc_lut.adc_lt  = {");
			for(i = 0; i < adc2soc_lut_length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%4d,", adc2soc_lut.adc_lt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Low-mid temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc2soc_lut.adc_lmt = {");
			for(i = 0; i < adc2soc_lut_length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%4d,", adc2soc_lut.adc_lmt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Low-low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc2soc_lut.adc_llt = {");
			for(i = 0; i < adc2soc_lut_length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%4d,", adc2soc_lut.adc_llt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			if (!view_all) break;
 
		case D2153_PROP_DISCHG_WEIGHT:
			length = (u8)sizeof(adc_weight_section_discharge)/sizeof(u16);
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"\n## Discharging weight table ...\n");

			// Discharge weight at high and room temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge     = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at room-low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_rlt = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_rlt[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_lt  = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_lt[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-mid temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_lmt = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_lmt[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_llt = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_llt[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

#ifdef CONFIG_D2153_MULTI_WEIGHT
			// Discharge weight at high and room temperature. for 0.1c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_1c     = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_1c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at room-low temperature. for 0.1c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_rlt_1c = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_rlt_1c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low temperature. for 0.1c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_lt_1c  = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_lt_1c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-mid temperature. for 0.1c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_lmt_1c = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_lmt_1c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-low temperature. for 0.1c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_llt_1c = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_llt_1c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");


			// Discharge weight at high and room temperature. for 0.2c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_2c     = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_2c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at room-low temperature. for 0.2
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_rlt_2c = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_rlt_2c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low temperature. for 0.2c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_lt_2c  = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_lt_2c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-mid temperature. for 0.2c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_lmt_2c = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_lmt_2c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-low temperature. for 0.2c
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_discharge_llt_2c = {");
			for(i = 0; i < length; i++)
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_discharge_llt_2c[i]);
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");
#endif /* CONFIG_D2153_MULTI_WEIGHT */

			if (!view_all) break;

		case D2153_PROP_CHG_WEIGHT:
			length = (u8)sizeof(adc_weight_section_charge)/sizeof(u16);
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"\n## Charging weight table ...\n");

			// Discharge weight at high and room temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_charge     = {");
			for(i = 0; i < length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_charge[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at room-low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_charge_rlt = {");
			for(i = 0; i < length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_charge_rlt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_charge_lt  = {");
			for(i = 0; i < length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_charge_lt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-mid temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_charge_lmt = {");
			for(i = 0; i < length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_charge_lmt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			// Discharge weight at low-low temperature
			count += scnprintf(buf + count, PAGE_SIZE - count, 
									"adc_weight_section_charge_llt = {");
			for(i = 0; i < length; i++) {
				count += scnprintf(buf + count, PAGE_SIZE - count, 
									"%5d,", adc_weight_section_charge_llt[i]);
			}
			count += scnprintf(buf + count, PAGE_SIZE - count, "}\n");

			if (!view_all) break;

 		case D2153_PROP_BAT_CAPACITY:
			count += scnprintf(buf + count, PAGE_SIZE - count, 
								"\n## Battery Capacity is  %d mAh\n",
								USED_BATTERY_CAPACITY);
			if (!view_all) break;

		default:
			break;
	}

	return count;
}

#endif /* CONFIG_D2153_DEBUG_FEATURE */


/*
 * Name : chk_lut
 *
 */
static int chk_lut (u16* x, u16* y, u16 v, u16 l) {
	int i;
	//u32 ret;
	int ret;

	if (v < x[0])
		ret = y[0];
	else if (v >= x[l-1])
		ret = y[l-1]; 
	else {          
		for (i = 1; i < l; i++) {          
			if (v < x[i])               
				break;      
		}       
		ret = y[i-1];       
		ret = ret + ((v-x[i-1])*(y[i]-y[i-1]))/(x[i]-x[i-1]);   
	}   
	//return (u16) ret;
	return ret;
}

/* 
 * Name : chk_lut_temp
 * return : The return value is Kelvin degree
 */
static int chk_lut_temp (u16* x, u16* y, u16 v, u16 l) {
	int i, ret;

	if (v >= x[0])
		ret = y[0];
	else if (v < x[l-1])
		ret = y[l-1]; 
	else {			
		for (i=1; i < l; i++) { 		 
			if (v > x[i])				
				break;		
		}		
		ret = y[i-1];		
		ret = ret + ((v-x[i-1])*(y[i]-y[i-1]))/(x[i]-x[i-1]);	
	}
	
	return ret;
}


/* 
 * Name : adc_to_soc_with_temp_compensat
 *
 */
u32 adc_to_soc_with_temp_compensat(u16 adc, u16 temp) {	
	int sh, sl;

	if(temp < BAT_LOW_LOW_TEMPERATURE)
		temp = BAT_LOW_LOW_TEMPERATURE;
	else if(temp > BAT_HIGH_TEMPERATURE)
		temp = BAT_HIGH_TEMPERATURE;

	if((temp <= BAT_HIGH_TEMPERATURE) && (temp > BAT_ROOM_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_ht, adc2soc_lut.soc, adc, adc2soc_lut_length);    
		sl = chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, adc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_TEMPERATURE)*(sh - sl)
								/ (BAT_HIGH_TEMPERATURE - BAT_ROOM_TEMPERATURE);
	} else if((temp <= BAT_ROOM_TEMPERATURE) && (temp > BAT_ROOM_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_rlt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sh = sl + (temp - BAT_ROOM_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
	} else if((temp <= BAT_ROOM_LOW_TEMPERATURE) && (temp > BAT_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_rlt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_lt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sh = sl + (temp - BAT_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
	} else if((temp <= BAT_LOW_TEMPERATURE) && (temp > BAT_LOW_MID_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.adc_lt, adc2soc_lut.soc, adc, adc2soc_lut_length); 	   
		sl = chk_lut(adc2soc_lut.adc_lmt, adc2soc_lut.soc, adc, adc2soc_lut_length);		
		sh = sl + (temp - BAT_LOW_MID_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);	
	} else {		
		sh = chk_lut(adc2soc_lut.adc_lmt, adc2soc_lut.soc,	adc, adc2soc_lut_length);		 
		sl = chk_lut(adc2soc_lut.adc_llt, adc2soc_lut.soc, adc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE); 
	}

	return sh;
}


/*
 * Name : soc_to_adc_with_temp_compensat
 *
 */
u32 soc_to_adc_with_temp_compensat(u16 soc, u16 temp) {
	int sh, sl;

	if(temp < BAT_LOW_LOW_TEMPERATURE)
		temp = BAT_LOW_LOW_TEMPERATURE;
	else if(temp > BAT_HIGH_TEMPERATURE)
		temp = BAT_HIGH_TEMPERATURE;

	pr_info("%s. Parameter. SOC = %d. temp = %d\n", __func__, soc, temp);

	if((temp <= BAT_HIGH_TEMPERATURE) && (temp > BAT_ROOM_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_ht, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_TEMPERATURE)*(sh - sl)
								/ (BAT_HIGH_TEMPERATURE - BAT_ROOM_TEMPERATURE);
	} else if((temp <= BAT_ROOM_TEMPERATURE) && (temp > BAT_ROOM_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rlt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_ROOM_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
	} else if((temp <= BAT_ROOM_LOW_TEMPERATURE) && (temp > BAT_LOW_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_rlt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
	} else if((temp <= BAT_LOW_TEMPERATURE) && (temp > BAT_LOW_MID_TEMPERATURE)) {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lt, soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lmt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_MID_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
	} else {
		sh = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_lmt,	soc, adc2soc_lut_length);
		sl = chk_lut(adc2soc_lut.soc, adc2soc_lut.adc_llt, soc, adc2soc_lut_length);
		sh = sl + (temp - BAT_LOW_LOW_TEMPERATURE)*(sh - sl)
								/ (BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE);
	}

	return sh;
}


/* 
 * Name : adc_to_degree
 *
 */
int adc_to_degree_k(u16 adc, u8 ch) {
	if(ch == D2153_ADC_AIN) {
		return (chk_lut_temp(adc2temp_lut_for_adcin.adc, 
								adc2temp_lut_for_adcin.temp, 
								adc, adcin_lut_length));
	} else {
    	return (chk_lut_temp(adc2temp_lut.adc, 
								adc2temp_lut.temp, adc, temp_lut_length));
	}
}

int degree_k2c(u16 k) {
	return (K2C(k));
}

/* 
 * Name : get_adc_offset
 *
 */
int get_adc_offset(u16 adc) {	
    return (chk_lut(adc2vbat_lut.adc, adc2vbat_lut.offset, adc, adc2vbat_lut_length));
}

/* 
 * Name : adc_to_vbat
 *
 */
u16 adc_to_vbat(u16 adc, u8 is_charging) {    
	u16 a = adc;

	if(is_charging)
		a = adc - get_adc_offset(adc); // deduct charging offset
	// return (chk_lut(adc2vbat_lut.adc, adc2vbat_lut.vbat, a, adc2vbat_lut_length));
	return (2500 + ((a * 2000) >> 12));
}

/* 
 * Name : adc_to_soc
 * get SOC (@ room temperature) according ADC input
 */
int adc_to_soc(u16 adc, u8 is_charging) { 

	u16 a = adc;

	if(is_charging)
		a = adc - get_adc_offset(adc); // deduct charging offset
	return (chk_lut(adc2soc_lut.adc_rt, adc2soc_lut.soc, a, adc2soc_lut_length));
}


/* 
 * Name : do_interpolation
 */
int do_interpolation(int x0, int x1, int y0, int y1, int x)
{
	int y = 0;
	
	if(!(x1 - x0 )) {
		pr_err("%s. Divied by Zero. Plz check x1(%d), x0(%d) value \n",
				__func__, x1, x0);
		return 0;
	}

	y = y0 + (x - x0)*(y1 - y0)/(x1 - x0);
	pr_info("%s. Interpolated y_value is = %d\n", __func__, y);

	return y;
}


#define D2153_MAX_SOC_CHANGE_OFFSET		20

/* 
 * Name : d2153_get_soc
 */
static int d2153_get_soc(struct d2153_battery *pbat)
{
	int soc;
	struct d2153_battery_data *pbat_data = NULL;

	if((pbat == NULL) || (!pbat->battery_data.volt_adc_init_done)) {
		pr_err("%s. Invalid parameter. \n", __func__);
		return -EINVAL;
	}

	pr_debug("%s. Getting SOC\n", __func__);

	pbat_data = &pbat->battery_data;

	if(pbat_data->soc)
		pbat_data->prev_soc = pbat_data->soc;

	soc = adc_to_soc_with_temp_compensat(pbat_data->average_volt_adc, 
										C2K(pbat_data->average_temperature));

	if(soc > FULL_CAPACITY) {
		soc = FULL_CAPACITY;
		if(pbat_data->virtual_battery_full == 1) {
			pbat_data->virtual_battery_full = 0;
			pbat_data->soc = FULL_CAPACITY;
		}
	}

	pr_debug("%s. 0. SOC = %d\n", __func__, soc);

	/* Don't allow soc goes up when battery is dicharged.
	 and also don't allow soc goes down when battey is charged. */
	if(pbat_data->is_charging != TRUE 
		&& (soc > pbat_data->prev_soc && pbat_data->prev_soc )) {
		soc = pbat_data->prev_soc;
	}
	else if(pbat_data->is_charging
		&& (soc < pbat_data->prev_soc) && pbat_data->prev_soc) {
		soc = pbat_data->prev_soc;

	}

	pbat_data->soc = soc;

	// Write SOC and average VBAT ADC to GP Register
	d2153_reg_write(pbat->pd2153, D2153_GP_ID_2_REG, (0xFF & soc));
	d2153_reg_write(pbat->pd2153, D2153_GP_ID_3_REG, (0x0F & (soc>>8)));
	d2153_reg_write(pbat->pd2153, D2153_GP_ID_4_REG,
							(0xFF & pbat_data->average_volt_adc));
	d2153_reg_write(pbat->pd2153, D2153_GP_ID_5_REG,
							(0xF & (pbat_data->average_volt_adc>>8)));

	return soc;
}


/* 
 * Name : d2153_get_weight_from_lookup
 */
static u16 d2153_get_weight_from_lookup(u16 tempk, 
										u16 average_adc, 
										u8 is_charging,
										int diff)
{
	u8 i = 0;
	u16 *plut = NULL;
	int weight = 0;
#ifdef CONFIG_D2153_MULTI_WEIGHT
	int diff_offset, weight_1c, weight_2c = 0;
#endif

	// Sanity check.
	if (tempk < BAT_LOW_LOW_TEMPERATURE)		
		tempk = BAT_LOW_LOW_TEMPERATURE;
	else if (tempk > BAT_HIGH_TEMPERATURE)
		tempk = BAT_HIGH_TEMPERATURE;

	// Get the SOC look-up table
	if(tempk >= BAT_HIGH_TEMPERATURE) {
		plut = &adc2soc_lut.adc_ht[0];
	} else if(tempk < BAT_HIGH_TEMPERATURE && tempk >= BAT_ROOM_TEMPERATURE) {
		plut = &adc2soc_lut.adc_rt[0];
	} else if(tempk < BAT_ROOM_TEMPERATURE && tempk >= BAT_ROOM_LOW_TEMPERATURE) {
		plut = &adc2soc_lut.adc_rlt[0];
	} else if (tempk < BAT_ROOM_LOW_TEMPERATURE && tempk >= BAT_LOW_TEMPERATURE) {
		plut = &adc2soc_lut.adc_lt[0];
	} else if(tempk < BAT_LOW_TEMPERATURE && tempk >= BAT_LOW_MID_TEMPERATURE) {
		plut = &adc2soc_lut.adc_lmt[0];
	} else 
		plut = &adc2soc_lut.adc_llt[0];

	for(i = adc2soc_lut_length - 1; i; i--) {
		if(plut[i] <= average_adc)
			break;
	}

#ifdef CONFIG_D2153_MULTI_WEIGHT
	// 0.1C, 0.2C
	if(i <= (MULTI_WEIGHT_SIZE-1) && is_charging == 0) {
		if(dischg_diff_tbl.c1_diff[i] > diff)
			diff_offset = dischg_diff_tbl.c1_diff[i];
		else if(dischg_diff_tbl.c2_diff[i] < diff)
			diff_offset = dischg_diff_tbl.c2_diff[i];
		else
			diff_offset = diff;
		pr_debug("%s. diff = %d, diff_offset = %d\n", __func__, diff, diff_offset);
	} else {
		diff_offset = 0;
		pr_debug("%s. diff = %d\n", __func__, diff);
	}
#endif /* CONFIG_D2153_MULTI_WEIGHT */


	if((tempk <= BAT_HIGH_TEMPERATURE) && (tempk > BAT_ROOM_TEMPERATURE)) {  
		if(is_charging) {
			if(average_adc < plut[0]) {
				// under 1% -> fast charging
				weight = adc_weight_section_charge[0];
			} else
				weight = adc_weight_section_charge[i];
		} else {
#ifdef CONFIG_D2153_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight = adc_weight_section_discharge_1c[i] -
							((diff_offset - dischg_diff_tbl.c1_diff[i])
							 * (adc_weight_section_discharge_1c[i] -
							 adc_weight_section_discharge_2c[i]) /
							 (dischg_diff_tbl.c2_diff[i] -
							 dischg_diff_tbl.c1_diff[i]));
			} else 
#endif /* CONFIG_D2153_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge[i];
			}
		}
	} else if((tempk <= BAT_ROOM_TEMPERATURE) && (tempk > BAT_ROOM_LOW_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;
		
			weight=adc_weight_section_charge_rlt[i];
			weight = weight + ((tempk-BAT_ROOM_LOW_TEMPERATURE)
				*(adc_weight_section_charge[i]-adc_weight_section_charge_rlt[i]))
							/(BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE);
		} else {
#ifdef CONFIG_D2153_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);

			} else 
#endif /* CONFIG_D2153_MULTI_WEIGHT */
			{
				weight=adc_weight_section_discharge_rlt[i];
				weight = weight + ((tempk-BAT_ROOM_LOW_TEMPERATURE)
								*(adc_weight_section_discharge[i]
									- adc_weight_section_discharge_rlt[i]))
								/(BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 
			}
		}
	} else if((tempk <= BAT_ROOM_LOW_TEMPERATURE) && (tempk > BAT_LOW_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;
		
			weight = adc_weight_section_charge_lt[i];
			weight = weight + ((tempk-BAT_LOW_TEMPERATURE)
				*(adc_weight_section_charge_rlt[i]-adc_weight_section_charge_lt[i]))
								/(BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE);
		} else {
#ifdef CONFIG_D2153_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);

			} else 
#endif /* CONFIG_D2153_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_lt[i];
				weight = weight + ((tempk-BAT_LOW_TEMPERATURE)
								*(adc_weight_section_discharge_rlt[i]
									- adc_weight_section_discharge_lt[i]))
								/(BAT_ROOM_LOW_TEMPERATURE-BAT_LOW_TEMPERATURE); 
			}
		}
	} else if((tempk <= BAT_LOW_TEMPERATURE) && (tempk > BAT_LOW_MID_TEMPERATURE)) {
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;
		
			weight = adc_weight_section_charge_lmt[i];
			weight = weight + ((tempk-BAT_LOW_MID_TEMPERATURE)
				*(adc_weight_section_charge_lt[i]-adc_weight_section_charge_lmt[i]))
								/(BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE);
		} else {
#ifdef CONFIG_D2153_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);
			} else 
#endif /* CONFIG_D2153_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_lmt[i];
				weight = weight + ((tempk-BAT_LOW_MID_TEMPERATURE)
								* (adc_weight_section_discharge_lt[i]
									- adc_weight_section_discharge_lmt[i]))
								/ (BAT_LOW_TEMPERATURE-BAT_LOW_MID_TEMPERATURE); 
			}
		}
	} else {		
		if(is_charging) {
			if(average_adc < plut[0]) i = 0;

			weight = adc_weight_section_charge_llt[i];
			weight = weight + ((tempk-BAT_LOW_LOW_TEMPERATURE)
				*(adc_weight_section_charge_lmt[i]-adc_weight_section_charge_llt[i]))
								/(BAT_LOW_MID_TEMPERATURE-BAT_LOW_LOW_TEMPERATURE); 
		} else {
#ifdef CONFIG_D2153_MULTI_WEIGHT
			if(i <= (MULTI_WEIGHT_SIZE-1)) {
				weight_1c = adc_weight_section_discharge_rlt_1c[i];
				weight_1c = weight_1c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_1c[i] 
							- adc_weight_section_discharge_rlt_1c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight_2c = adc_weight_section_discharge_rlt_2c[i];
				weight_2c = weight_2c + ((tempk - BAT_ROOM_LOW_TEMPERATURE)
							* (adc_weight_section_discharge_2c[i] 
							- adc_weight_section_discharge_rlt_2c[i]))
							/ (BAT_ROOM_TEMPERATURE-BAT_ROOM_LOW_TEMPERATURE); 

				weight = weight_1c - ((diff_offset - dischg_diff_tbl.c1_diff[i])
										* (weight_1c - weight_2c)
										/ (dischg_diff_tbl.c2_diff[i] -
											dischg_diff_tbl.c1_diff[i]));

				pr_debug("%s(line.%d). weight = %d, weight_1c = %d, weight_2c = %d\n",
							__func__, __LINE__, weight, weight_1c, weight_2c);
			} else 
#endif /* CONFIG_D2153_MULTI_WEIGHT */
			{
				weight = adc_weight_section_discharge_llt[i];
				weight = weight + ((tempk-BAT_LOW_LOW_TEMPERATURE)
								* (adc_weight_section_discharge_lmt[i]
									- adc_weight_section_discharge_llt[i]))
								/ (BAT_LOW_MID_TEMPERATURE
									-BAT_LOW_LOW_TEMPERATURE); 
			}
		}
	}

	// Prevent exception case about minus value
	if(weight < 0)
		weight = 0;

	return weight;	

}


/* 
 * Name : d2153_set_adc_mode
 */
static int d2153_set_adc_mode(struct d2153_battery *pbat, adc_mode type)
{
	if(unlikely(!pbat)) {
		pr_err("%s. Invalid parameter.\n", __func__);
		return -EINVAL;
	}

	if(pbat->adc_mode != type)
	{
		if(type == D2153_ADC_IN_AUTO) {
			pbat->d2153_read_adc = d2153_read_adc_in_auto;
			pbat->adc_mode = D2153_ADC_IN_AUTO;
		}
		else if(type == D2153_ADC_IN_MANUAL) {
			pbat->d2153_read_adc = d2153_read_adc_in_manual;
			pbat->adc_mode = D2153_ADC_IN_MANUAL;
		}
	}
	else {
		pr_debug("%s: ADC mode is same before was set \n", __func__);
	}

	return 0;
}

extern atomic_t timeout_wa;
#define D2153_ADC_I2C_TIMEOUT_SET   200
#define D2153_ADC_I2C_TIMEOUT_RESET 0

/* 
 * Name : d2153_read_adc_in_auto
 * Desc : Read ADC raw data for each channel.
 * Param : 
 *    - d2153 : 
 *    - channel : voltage, temperature 1, temperature 2, VF and TJUNC* Name : d2153_set_end_of_charge
 */
static int d2153_read_adc_in_auto(struct d2153_battery *pbat, adc_channel channel)
{
	u8 msb_res, lsb_res;
	int ret = 0;
	struct d2153_battery_data *pbat_data = NULL;
	struct d2153 *d2153 = NULL;

	if(pbat == NULL) {
		pr_err("%s. Invalid argument\n", __func__);
		return -EINVAL;
	}
	pbat_data = &pbat->battery_data;
	d2153 = pbat->pd2153;

	// The valid channel is from ADC_VOLTAGE to ADC_AIN in auto mode.
	if(channel >= D2153_ADC_CHANNEL_MAX - 1) {
		pr_err("%s. Invalid channel(%d) in auto mode\n", __func__, channel);
		return -EINVAL;
	}
	
	mutex_lock(&pbat->meoc_lock);

	ret = d2153_get_adc_hwsem();
	if (ret < 0) {
		mutex_unlock(&pbat->meoc_lock);
		pr_err("%s:lock is already taken.\n", __func__);
		return -EBUSY;
	}

	pbat_data->adc_res[channel].is_adc_eoc = FALSE;
	pbat_data->adc_res[channel].read_adc = 0;

	// Set ADC_CONT register to select a channel.
	if(adc_cont_inven[channel].adc_preset_val) {
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_SET);
		if(channel == D2153_ADC_AIN) {
			ret = d2153_reg_write(d2153, D2153_ADC_CONT2_REG, 
										adc_cont_inven[channel].adc_preset_val);
		} else {
			ret = d2153_reg_write(d2153, D2153_ADC_CONT_REG, 
										adc_cont_inven[channel].adc_preset_val);
		}
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
		if(ret < 0)
			goto out;
		msleep(1);
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_SET);
		ret = d2153_set_bits(d2153, D2153_ADC_CONT_REG, adc_cont_inven[channel].adc_cont_val);
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
		if(ret < 0)
			goto out;
	} else {
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_SET);
		ret = d2153_reg_write(d2153, D2153_ADC_CONT_REG, adc_cont_inven[channel].adc_cont_val);
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
		if(ret < 0)
			goto out;
	}
	msleep(3);

	// Read result register for requested adc channel
	atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_SET);
	ret = d2153_reg_read(d2153, adc_cont_inven[channel].adc_msb_res, &msb_res);
	atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
	if(ret < 0)
		goto out;
	atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_SET);
	ret = d2153_reg_read(d2153, adc_cont_inven[channel].adc_lsb_res, &lsb_res);
	atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
	if(ret <  0)
		goto out;
	lsb_res &= adc_cont_inven[channel].adc_lsb_mask;
	atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_SET);
	if((ret = d2153_reg_write(d2153, D2153_ADC_CONT_REG, 0x00)) < 0){
		atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
		goto out;
	}
	atomic_set(&timeout_wa, D2153_ADC_I2C_TIMEOUT_RESET);
	// Make ADC result
	pbat_data->adc_res[channel].is_adc_eoc = TRUE;
	pbat_data->adc_res[channel].read_adc =
		((msb_res << 4) | (lsb_res >> 
			(adc_cont_inven[channel].adc_lsb_mask == ADC_RES_MASK_MSB ? 4 : 0)));

out:
	d2153_put_adc_hwsem();
	mutex_unlock(&pbat->meoc_lock);

	return ret;
}


/* 
 * Name : d2153_read_adc_in_manual
 */
static int d2153_read_adc_in_manual(struct d2153_battery *pbat, adc_channel channel)
{
	u8 mux_sel, flag = FALSE;
	int ret, retries = D2153_MANUAL_READ_RETRIES;
	struct d2153_battery_data *pbat_data = &pbat->battery_data;
	struct d2153 *d2153 = pbat->pd2153;
	
	mutex_lock(&pbat->meoc_lock);

	ret = d2153_get_adc_hwsem();
	if (ret < 0) {
			mutex_unlock(&pbat->meoc_lock);
			pr_err("%s:lock is already taken.\n", __func__);
			return -EBUSY;
	}
	
	pbat_data->adc_res[channel].is_adc_eoc = FALSE;
	pbat_data->adc_res[channel].read_adc = 0;

	switch(channel) {
		case D2153_ADC_VOLTAGE:
			mux_sel = D2153_MUXSEL_VBAT;
			break;
		case D2153_ADC_TEMPERATURE_1:
			mux_sel = D2153_MUXSEL_TEMP1;
			break;
		case D2153_ADC_TEMPERATURE_2:
			mux_sel = D2153_MUXSEL_TEMP2;
			break;
		case D2153_ADC_VF:
			mux_sel = D2153_MUXSEL_VF;
			break;
		case D2153_ADC_TJUNC:
			mux_sel = D2153_MUXSEL_TJUNC;
			break;
		default :
			pr_err("%s. Invalid channel(%d) \n", __func__, channel);
			ret = -EINVAL;
			goto out;
	}

	mux_sel |= D2153_MAN_CONV_MASK;
	if((ret = d2153_reg_write(d2153, D2153_ADC_MAN_REG, mux_sel)) < 0)
		goto out;

	do {
		schedule_timeout_interruptible(msecs_to_jiffies(1));
		flag = pbat_data->adc_res[channel].is_adc_eoc;
	} while(retries-- && (flag == FALSE));

out:
	d2153_put_adc_hwsem();
	mutex_unlock(&pbat->meoc_lock);
	
	if(flag == FALSE) {
		pr_warn("%s. Failed manual ADC conversion. channel(%d)\n", __func__, channel);
		ret = -EIO;
	}
	
	return ret;    
}


#define LCD_DIM_ADC			2168

extern int get_cable_type(void);

/* 
 * Name : d2153_reset_sw_fuelgauge
 */
static int d2153_reset_sw_fuelgauge(struct d2153_battery *pbat)
{
	u8 i, j = 0;
	int read_adc = 0;
	u32 average_adc, sum_read_adc = 0;
	struct d2153_battery_data *pbatt_data = NULL;

	if(pbat == NULL) {
		pr_err("%s. Invalid argument\n", __func__);
		return -EINVAL;
	}
	pbatt_data = &pbat->battery_data;
	
	pr_info("++++++ Reset Software Fuelgauge +++++++++++\n");
	pbatt_data->volt_adc_init_done = FALSE;

	/* Initialize ADC buffer */
	memset(pbatt_data->voltage_adc, 0x0, ARRAY_SIZE(pbatt_data->voltage_adc));
	pbatt_data->sum_voltage_adc = 0;
	pbatt_data->soc = 0;
	pbatt_data->prev_soc = 0;

	/* Read VBAT_S ADC */
	for(i = 8, j = 0; i; i--) {
		read_adc = pbat->d2153_read_adc(pbat, D2153_ADC_VOLTAGE);
		if(pbatt_data->adc_res[D2153_ADC_VOLTAGE].is_adc_eoc) {
			read_adc = pbatt_data->adc_res[D2153_ADC_VOLTAGE].read_adc;
			//pr_info("%s. Read ADC %d : %d\n", __func__, i, read_adc);
			if(read_adc > 0) {
				sum_read_adc += read_adc;
				j++;
			}
		}
		msleep(10);
	}
	average_adc = read_adc = sum_read_adc / j;
	pr_info("%s. average = %d, j = %d \n", __func__, average_adc, j);

	/* To be compensated a read ADC */
	average_adc += fg_reset_drop_offset[0];
	if(average_adc > MAX_FULL_CHARGED_ADC) {
		average_adc = MAX_FULL_CHARGED_ADC;
	}
	pr_info("%s. average ADC = %d. voltage = %d mV\n", 
				__func__, average_adc, 
				adc_to_vbat(average_adc, pbatt_data->is_charging));
 
	pbatt_data->current_volt_adc = average_adc;

	pbatt_data->origin_volt_adc = read_adc;
	pbatt_data->average_volt_adc = average_adc;
	pbatt_data->current_voltage = adc_to_vbat(pbatt_data->current_volt_adc,
										 pbatt_data->is_charging);
	pbatt_data->average_voltage = adc_to_vbat(pbatt_data->average_volt_adc,
										 pbatt_data->is_charging);
	pbat->battery_data.volt_adc_init_done = TRUE;

	pr_info("%s. Average. ADC = %d, Voltage =  %d\n", 
			__func__, pbatt_data->average_volt_adc, pbatt_data->average_voltage);

	return 0;
}


#ifdef CONFIG_SEC_CHARGING_FEATURE
extern struct spa_power_data spa_power_pdata;
extern int spa_event_handler(int evt, void *data);
#endif

#ifdef CONFIG_CHARGER_SMB328A
int smb328a_check_charging_status(void);
#endif


/* 
 * Name : d2153_read_voltage
 */
static int d2153_read_voltage(struct d2153_battery *pbat,struct power_supply *ps)
{
	int new_vol_adc = 0, base_weight, new_vol_orign = 0;
	int offset_with_new = 0;
	int ret = 0;
	int num_multi=0;
	struct d2153_battery_data *pbat_data = &pbat->battery_data;
	int orign_offset = 0;
	u8  ta_status;
	int charging_status;
	static u8 is_first_check = 0;
#ifdef CONFIG_D2153_EOC_CTRL
	struct power_supply *ps_bat = NULL;
	union power_supply_propval value;

	ps_bat = power_supply_get_by_name("battery");
	if(ps_bat == NULL) {
		pr_err("%s. Failed a battery supply instance\n", __func__);
		return -EINVAL;
	}
	ps_bat->get_property(ps_bat, POWER_SUPPLY_PROP_STATUS, &value);
#endif /* CONFIG_D2153_EOC_CTRL */

#ifdef CONFIG_CHARGER_SMB328A
	if(is_first_check == 0) {
		ret = d2153_reg_read(pbat->pd2153, D2153_STATUS_C_REG, &ta_status);
		charging_status = (int)(ta_status & D2153_GPI_3_TA_MASK);
		is_first_check = 1;
	} else {
	charging_status = smb328a_check_charging_status();
	}
#else
	ret = d2153_reg_read(pbat->pd2153, D2153_STATUS_C_REG, &ta_status);
	charging_status = (int)(ta_status & D2153_GPI_3_TA_MASK);
#endif /* CONFIG_CHARGER_SMB328A */
 
	if(charging_status) {
		pbat_data->is_charging = 1;
#ifdef CONFIG_D2153_EOC_CTRL
		if(pbat_data->charger_ctrl_status == D2153_BAT_CHG_MAX)
			pbat_data->charger_ctrl_status = D2153_BAT_CHG_START;
#endif /* CONFIG_D2153_EOC_CTRL */
	} else {
		pbat_data->is_charging = 0;
#ifdef CONFIG_D2153_EOC_CTRL
		if(value.intval != POWER_SUPPLY_STATUS_FULL)
			pbat_data->charger_ctrl_status = D2153_BAT_CHG_MAX;
#endif /* CONFIG_D2153_EOC_CTRL */
	}

	pr_info("%s. is_charging = %d\n", __func__, pbat_data->is_charging);

	// Read voltage ADC
	ret = pbat->d2153_read_adc(pbat, D2153_ADC_VOLTAGE);
	if(ret < 0)
		return ret;

	
	if(pbat_data->adc_res[D2153_ADC_VOLTAGE].is_adc_eoc) {

		new_vol_orign = new_vol_adc = pbat_data->adc_res[D2153_ADC_VOLTAGE].read_adc;

		
		if(pbat->battery_data.volt_adc_init_done) {

			if(pbat_data->is_charging) {

				orign_offset = new_vol_adc - pbat_data->average_volt_adc;
				base_weight = d2153_get_weight_from_lookup(
											C2K(pbat_data->average_temperature),
											pbat_data->average_volt_adc,
											pbat_data->is_charging,
											orign_offset);
				offset_with_new = orign_offset * base_weight;
				
				pbat_data->sum_total_adc += offset_with_new;
				num_multi = pbat_data->sum_total_adc / NORM_CHG_NUM;
				if(num_multi) {
					pbat_data->average_volt_adc += num_multi;
					pbat_data->sum_total_adc = pbat_data->sum_total_adc 
												% NORM_CHG_NUM;
				} else {
					new_vol_adc = pbat_data->average_volt_adc;
				}

				pbat_data->current_volt_adc = new_vol_adc;
			} else {

				orign_offset = pbat_data->average_volt_adc - new_vol_adc;
				base_weight = d2153_get_weight_from_lookup(
											C2K(pbat_data->average_temperature),
											pbat_data->average_volt_adc,
											pbat_data->is_charging,
											orign_offset);
				pr_info("%s. orign_offset = %d, base_weight = %d\n", __func__, orign_offset, base_weight);

				if (is_called_by_ticker == 1) {
					base_weight = (base_weight * 235) / 10;
					is_called_by_ticker = 0;
					pr_info("%s. called_by_ticker. calculated base_weight = %d\n", __func__, base_weight);
				}
				if(time_diff > 24) {
					base_weight = (base_weight * (int)time_diff * 49) / 1000;
					time_diff = 0;
					pr_info("%s. time_diff > 24. calculated base_weight = %d\n", __func__, base_weight);
				}

				offset_with_new = orign_offset * base_weight;

				if(pbat_data->reset_total_adc) {
					pbat_data->sum_total_adc = pbat_data->sum_total_adc / 10;
					pbat_data->reset_total_adc = 0;
					pr_debug("%s. reset_total_adc. true to false \n", __func__);
				}
				pbat_data->sum_total_adc += offset_with_new;

				num_multi = pbat_data->sum_total_adc / NORM_NUM;
				if(num_multi) {
					pbat_data->average_volt_adc -= num_multi;
					pbat_data->sum_total_adc = pbat_data->sum_total_adc 
																% NORM_NUM;
				} else {
					new_vol_adc = pbat_data->average_volt_adc;
				}
		
			}
		}else {
			// Before initialization
			u8 i = 0;
			u8 res_msb, res_lsb, res_msb_adc, res_lsb_adc = 0;
			u32 capacity = 0, vbat_adc = 0;
			int convert_vbat_adc, X1, X0;
			int Y1, Y0 = FIRST_VOLTAGE_DROP_ADC;
			int X = C2K(pbat_data->average_temperature);

			// If there is SOC data in the register
			// the SOC(capacity of battery) will be used as initial SOC
			ret = d2153_reg_read(pbat->pd2153, D2153_GP_ID_2_REG, &res_lsb);
			ret = d2153_reg_read(pbat->pd2153, D2153_GP_ID_3_REG, &res_msb);
			capacity = (((res_msb & 0x0F) << 8) | (res_lsb & 0xFF));

			ret = d2153_reg_read(pbat->pd2153, D2153_GP_ID_4_REG, &res_lsb_adc);
			ret = d2153_reg_read(pbat->pd2153, D2153_GP_ID_5_REG, &res_msb_adc);
			vbat_adc = (((res_msb_adc & 0x0F) << 8) | (res_lsb_adc & 0xFF));

			pr_info("%s. capacity = %d, vbat_adc = %d \n", __func__,
															capacity,
															vbat_adc);

			if(capacity) {
				convert_vbat_adc = vbat_adc;
				pr_info("!#!#!# Boot by NORMAL Power off !#!#!#\n");
			} else {
				pr_info("!#!#!# Boot by Battery insert !#!#!#\n");

				pbat->pd2153->average_vbat_init_adc =
								(pbat->pd2153->vbat_init_adc[0] +
								 pbat->pd2153->vbat_init_adc[1] +
								 pbat->pd2153->vbat_init_adc[2]) / 3;

				vbat_adc = pbat->pd2153->average_vbat_init_adc;

				if(pbat_data->is_charging) {
					pr_info("%s. Charging case : average_vbat_init_adc = %d\n", __func__, vbat_adc);
					if(vbat_adc < CHARGE_ADC_KRNL_F) {
						// In this case, vbat_adc is bigger than OCV
						// So, subtract a interpolated value 
						// from initial average value(vbat_adc)
						u16 temp_adc = 0;

						if(vbat_adc < CHARGE_ADC_KRNL_H)
							vbat_adc = CHARGE_ADC_KRNL_H;

						X0 = CHARGE_ADC_KRNL_H;    X1 = CHARGE_ADC_KRNL_L;
						Y0 = CHARGE_OFFSET_KRNL_H; Y1 = CHARGE_OFFSET_KRNL_L;
						temp_adc = do_interpolation(X0, X1, Y0, Y1, vbat_adc);
						pr_debug("%s. Charging case 1 : temp_adc = %d\n", __func__, temp_adc);
						convert_vbat_adc = vbat_adc - temp_adc;
					} else if(vbat_adc < CHARGE_ADC_KRNL_F2) {
						pr_debug("%s. Charging case 2. offset = %d\n", __func__, CHARGE_OFFSET_KRNL);
						convert_vbat_adc = vbat_adc - CHARGE_OFFSET_KRNL;
					} else if(vbat_adc < CHARGE_ADC_KRNL_F3 
								&& (smb328a_check_charging_status() == 1)) {
						u16 temp_adc = 0;

						X0 = CHARGE_ADC_KRNL_H3;    X1 = CHARGE_ADC_KRNL_L3;
						Y0 = CHARGE_OFFSET_KRNL_H3; Y1 = CHARGE_OFFSET_KRNL_L3;
						temp_adc = do_interpolation(X0, X1, Y0, Y1, vbat_adc);
						pr_debug("%s. Charging case 3 : temp_adc = %d\n", __func__, temp_adc);
						convert_vbat_adc = vbat_adc - temp_adc;
					} else {
						pr_debug("%s. Charging case 4. offset = %d\n", __func__, CHARGE_OFFSET_KRNL2);
						convert_vbat_adc = new_vol_orign + CHARGE_OFFSET_KRNL2;
					}
				} else {
					vbat_adc = new_vol_orign;
					pr_debug("[L%d] %s discharging new_vol_adc = %d\n",
						__LINE__, __func__, vbat_adc);

					Y0 = FIRST_VOLTAGE_DROP_ADC;
					if(C2K(pbat_data->average_temperature)
												<= BAT_LOW_LOW_TEMPERATURE) {
						convert_vbat_adc = vbat_adc 
											+ (Y0 + FIRST_VOLTAGE_DROP_LL_ADC);
					} else if(C2K(pbat_data->average_temperature)
												>= BAT_ROOM_TEMPERATURE) {
						convert_vbat_adc = vbat_adc + Y0;
					} else {
						if(C2K(pbat_data->average_temperature)
												<= BAT_LOW_MID_TEMPERATURE) {
							Y1 = Y0 + FIRST_VOLTAGE_DROP_ADC;
							Y0 = Y0 + FIRST_VOLTAGE_DROP_LL_ADC;
							X0 = BAT_LOW_LOW_TEMPERATURE;
							X1 = BAT_LOW_MID_TEMPERATURE;
						} else if(C2K(pbat_data->average_temperature)
												<= BAT_LOW_TEMPERATURE) {
							Y1 = Y0 + FIRST_VOLTAGE_DROP_L_ADC;	
							Y0 = Y0 + FIRST_VOLTAGE_DROP_ADC;
							X0 = BAT_LOW_MID_TEMPERATURE;
							X1 = BAT_LOW_TEMPERATURE;
						} else {
							Y1 = Y0 + FIRST_VOLTAGE_DROP_RL_ADC;
							Y0 = Y0 + FIRST_VOLTAGE_DROP_L_ADC;
							X0 = BAT_LOW_TEMPERATURE;
							X1 = BAT_ROOM_LOW_TEMPERATURE;
						}
						convert_vbat_adc = vbat_adc + Y0
											+ ((X - X0) * (Y1 - Y0)) / (X1 - X0);
					}
				}
			}
			new_vol_adc = convert_vbat_adc;

			if(new_vol_adc > MAX_FULL_CHARGED_ADC) {
				new_vol_adc = MAX_FULL_CHARGED_ADC;
				pr_debug("%s. Set new_vol_adc to max. ADC value\n", __func__);
			}

			for(i = AVG_SIZE; i ; i--) {
				pbat_data->voltage_adc[i-1] = new_vol_adc;
				pbat_data->sum_voltage_adc += new_vol_adc;
			}

			pbat_data->current_volt_adc = new_vol_adc;
			pbat_data->volt_adc_init_done = TRUE;
			pbat_data->average_volt_adc = new_vol_adc;
		}	

		pbat_data->origin_volt_adc = new_vol_orign;
		pbat_data->current_voltage = adc_to_vbat(pbat_data->current_volt_adc,
													pbat_data->is_charging);
		pbat_data->average_voltage = adc_to_vbat(pbat_data->average_volt_adc,
													pbat_data->is_charging);
	}
	else {
		pr_err("%s. Voltage ADC read failure \n", __func__);
		ret = -EIO;
	}

	return ret;
}


/*
 * Name : d2153_read_temperature
 */
static int d2153_read_temperature(struct d2153_battery *pbat)
{
	u8 ch = 0;
	u16 new_temp_adc = 0;
	int ret = 0;
	struct d2153_battery_data *pbat_data = &pbat->battery_data;

	/* Read temperature ADC
	 * Channel : D2153_ADC_TEMPERATURE_1 -> TEMP_BOARD
	 * Channel : D2153_ADC_TEMPERATURE_2 -> TEMP_RF
	 */

	// To read a temperature ADC of BOARD
	ch = D2153_ADC_TEMPERATURE_1;
	ret = pbat->d2153_read_adc(pbat, ch);
	if(pbat_data->adc_res[ch].is_adc_eoc) {
		new_temp_adc = pbat_data->adc_res[ch].read_adc;

		pbat_data->current_temp_adc = new_temp_adc;

		if(pbat_data->temp_adc_init_done) {
			pbat_data->sum_temperature_adc += new_temp_adc;
			pbat_data->sum_temperature_adc -= 
						pbat_data->temperature_adc[pbat_data->temperature_idx];
			pbat_data->temperature_adc[pbat_data->temperature_idx] = new_temp_adc;
		} else {
			u8 i;

			for(i = 0; i < AVG_SIZE; i++) {
				pbat_data->temperature_adc[i] = new_temp_adc;
				pbat_data->sum_temperature_adc += new_temp_adc;
			}
			pbat_data->temp_adc_init_done = TRUE;
		}

		pbat_data->average_temp_adc =
								pbat_data->sum_temperature_adc >> AVG_SHIFT;
		pbat_data->temperature_idx = (pbat_data->temperature_idx+1) % AVG_SIZE;
		pbat_data->average_temperature = 
					degree_k2c(adc_to_degree_k(pbat_data->average_temp_adc, ch));
		pbat_data->current_temperature = 
								degree_k2c(adc_to_degree_k(new_temp_adc, ch));

	}
	else {
		pr_err("%s. Temperature ADC read failed \n", __func__);
		ret = -EIO;
	}

	return ret;
}


/* 
 * Name : d2153_get_rf_temperature
 */
int d2153_get_rf_temperature(void)
{
	u8 i, j, channel;
	int sum_temp_adc, ret = 0;
	struct d2153_battery *pbat = gbat;
	struct d2153_battery_data *pbat_data = &gbat->battery_data;

	if(pbat == NULL || pbat_data == NULL) {
		pr_err("%s. battery_data is NULL\n", __func__);
		return -EINVAL;
	}

	/* To read a temperature2 ADC */
	sum_temp_adc = 0;
	channel = D2153_ADC_TEMPERATURE_2;
	for(i = 10, j = 0; i; i--) {
		ret = pbat->d2153_read_adc(pbat, channel);
		if(ret == 0) {
			sum_temp_adc += pbat_data->adc_res[channel].read_adc;
			if(++j == 3)
				break;
		} else
			msleep(20);
	}
	if (j) {
		pbat_data->current_rf_temp_adc = (sum_temp_adc / j);
		pbat_data->current_rf_temperature =
		   degree_k2c(adc_to_degree_k(pbat_data->current_rf_temp_adc, channel));
#ifdef CONFIG_D2153_BATTERY_DEBUG
		pr_info("%s. RF_TEMP_ADC = %d, RF_TEMPERATURE = %3d.%d\n",
				__func__,
				pbat_data->current_rf_temp_adc,
				(pbat_data->current_rf_temperature/10),
				(pbat_data->current_rf_temperature%10));
#endif
		return pbat_data->current_rf_temperature;
	} else {
		pr_err("%s:ERROR in reading RF temperature.\n", __func__);
		return -EIO;
	}
 }
EXPORT_SYMBOL(d2153_get_rf_temperature);


#define DEGREE_20				(20)
#define IS_N_DIFF(a, b, deg)	((a - b > deg) || (b - a > deg))

/* 
 * Name : d2153_get_ap_temperature
 */
int d2153_get_ap_temperature(int opt)
{
	u8 i, j, channel;
	int read_adc, sum_temp_adc, ret = 0;
	struct d2153_battery *pbat = NULL;
	struct d2153_battery_data *pbat_data = NULL;

	if(gbat == NULL) {
		pr_err("%s. battery_data is NULL\n", __func__);
		return -EINVAL;
	}
	pbat = gbat;
	pbat_data = &gbat->battery_data;

	/* To read a AP_TEMP ADC */
	j = sum_temp_adc = 0;
	channel = D2153_ADC_AIN;
	for(i = 10, j = 0; i; i--) {
		ret = pbat->d2153_read_adc(pbat, channel);
		read_adc = pbat_data->adc_res[channel].read_adc;
		if(ret == 0 && (read_adc != 0)
			 && pbat_data->adc_res[channel].is_adc_eoc) {
			 pr_info("pbat_data->adc_res[D2153_ADC_AIN].read_adc = %d\n", read_adc);
			sum_temp_adc += read_adc;
			if(++j == 3)
				break;
		} else
			msleep(20);
	}
	if (j) {
		pbat_data->current_ap_temp_adc = (sum_temp_adc / j);
		pbat_data->current_ap_temperature =
		   degree_k2c(adc_to_degree_k(pbat_data->current_ap_temp_adc, channel));

		pr_debug("%s. j = %d, AP_TEMP_ADC = %d, AP_TEMPERATURE = %3d.%d, Prev_TEMP_ADC = %d, Prev_Temperature = %d\n",
					__func__, j,
					pbat_data->current_ap_temp_adc,
					(pbat_data->current_ap_temperature/10),
					(pbat_data->current_ap_temperature%10),
					 pbat_data->prev_ap_temp_adc,
					 pbat_data->prev_ap_temperature);

		if(pbat_data->is_ap_temp_jumped == TRUE) {
			pbat_data->prev_ap_temp_adc = pbat_data->current_ap_temp_adc;
			pbat_data->prev_ap_temperature = pbat_data->current_ap_temperature;
			pbat_data->is_ap_temp_jumped = FALSE;
		} else {
			if(pbat_data->prev_ap_temperature != -EINVAL
				&& IS_N_DIFF(pbat_data->prev_ap_temperature, 
							pbat_data->current_ap_temperature, DEGREE_20)) {
				pbat_data->current_ap_temp_adc = pbat_data->prev_ap_temp_adc;
				pbat_data->current_ap_temperature = pbat_data->prev_ap_temperature;
				pbat_data->is_ap_temp_jumped = TRUE;
			} else {
				pbat_data->prev_ap_temp_adc = pbat_data->current_ap_temp_adc;
				pbat_data->prev_ap_temperature = pbat_data->current_ap_temperature;
			}
		}
	
		if (opt == D2153_OPT_ADC)
			return pbat_data->current_ap_temp_adc;
		else
			return pbat_data->current_ap_temperature;
	} else {
		pr_err("%s:ERROR in reading AP temperature.\n", __func__);
		return -EIO;
	}
}
EXPORT_SYMBOL(d2153_get_ap_temperature);

/* 
 * Name : d2153_battery_read_status
 */
int d2153_battery_read_status(int type)
{
	int val = 0;
	struct d2153_battery *pbat = NULL;

	if (gbat == NULL) {
		dlg_err("%s. driver data is NULL\n", __func__);
		return -EINVAL;
	}

	pbat = gbat;

	switch(type){
		case D2153_BATTERY_SOC:
			val = d2153_get_soc(pbat);
			//val = (val+5)/10;
			val = (val)/10;
			break;

		case D2153_BATTERY_CUR_VOLTAGE:
			val = pbat->battery_data.current_voltage;
			break;

		case D2153_BATTERY_AVG_VOLTAGE:
			val = pbat->battery_data.average_voltage;
			break;

		case D2153_BATTERY_VOLTAGE_NOW :
		{
			u8 ch = D2153_ADC_VOLTAGE;

			val = pbat->d2153_read_adc(pbat, ch);
			if(val < 0)
				return val;
			if(pbat->battery_data.adc_res[ch].is_adc_eoc) {
				val = adc_to_vbat(pbat->battery_data.adc_res[ch].read_adc, 0);
#ifdef CONFIG_D2153_BATTERY_DEBUG
				printk("%s: read adc to bat value = %d\n",__func__, val);
#endif
			} else {
				val = -EINVAL;
			}
			break;
		}

		case D2153_BATTERY_TEMP_HPA:
			val = d2153_get_rf_temperature();
			break;

		case D2153_BATTERY_TEMP_ADC:
			val = pbat->battery_data.average_temp_adc;
			break;

		case D2153_BATTERY_SLEEP_MONITOR:
			is_called_by_ticker = 1;
#ifdef CONFIG_D2153_HW_TIMER
			do_gettimeofday(&suspend_time);
#endif
			wake_lock_timeout(&pbat->battery_data.sleep_monitor_wakeup,
									D2153_SLEEP_MONITOR_WAKELOCK_TIME);
			cancel_delayed_work_sync(&pbat->monitor_temp_work);
			cancel_delayed_work_sync(&pbat->monitor_volt_work);
			schedule_delayed_work(&pbat->monitor_temp_work, 0);
			schedule_delayed_work(&pbat->monitor_volt_work, 0);
			break;
	}

	return val;
}
EXPORT_SYMBOL(d2153_battery_read_status);


/*
 * Name : d2153_battery_set_status
 */
int d2153_battery_set_status(int type, int status)
{
	int val = 0;
	struct d2153_battery *pbat = NULL;

	if(gbat == NULL) {
		dlg_err("%s. driver data is NULL\n", __func__);
		return -EINVAL;
	}

	pbat = gbat;

	switch(type){
		case D2153_STATUS_CHARGING :
			/* Discharging = 0, Charging = 1 */
			pbat->battery_data.is_charging = status;
			break;
		case D2153_RESET_SW_FG :
			/* Reset SW fuel gauge */
			cancel_delayed_work_sync(&pbat->monitor_volt_work);	
			val = d2153_reset_sw_fuelgauge(pbat);
			schedule_delayed_work(&gbat->monitor_volt_work, 0);
			break;
#ifdef CONFIG_D2153_EOC_CTRL
		case D2153_STATUS_EOC_CTRL:
			pbat->battery_data.charger_ctrl_status = status;
			break;
#endif
		default :
			return -EINVAL;
	}

	return val;
}
EXPORT_SYMBOL(d2153_battery_set_status);


static void d2153_monitor_voltage_work(struct work_struct *work)
{
	int ret=0;
	struct d2153_battery *pbat = container_of(work, struct d2153_battery, monitor_volt_work.work);
	struct d2153_battery_data *pbat_data = NULL;
	struct power_supply *ps;

	if(pbat == NULL) {
		pr_err("%s. Invalid driver data\n", __func__);
		goto err_adc_read;
	}
	pbat_data = &pbat->battery_data;
	
#ifdef CONFIG_SEC_CHARGING_FEATURE
	ps = power_supply_get_by_name(spa_power_pdata.charger_name);
#else
	ps = NULL;
#endif
	if(ps == NULL){
		pr_info("spa is not registered yet !!!");
		schedule_delayed_work(&pbat->monitor_volt_work, D2153_VOLTAGE_MONITOR_START);
		return;
	}

	ret = d2153_read_voltage(pbat,ps);
	if(ret < 0)
	{
		pr_err("%s. Read voltage ADC failure\n", __func__);
		goto err_adc_read;
	}

	if(pbat_data->is_charging == 0) {
		schedule_delayed_work(&pbat->monitor_volt_work, D2153_VOLTAGE_MONITOR_NORMAL);
	}
	else {
#ifdef CONFIG_D2153_EOC_CTRL
		if(pbat_data->volt_adc_init_done && pbat_data->is_charging) {
			struct power_supply *ps;
			union power_supply_propval value;

			ps = power_supply_get_by_name("battery");
			if(ps == NULL) {
				pr_err("%s. Failed a battery supply instance\n", __func__);
				goto err_adc_read;
			}

			ps->get_property(ps, POWER_SUPPLY_PROP_STATUS, &value);
			pr_info("%s. Battery PROP_STATUS = %d\n", __func__, value.intval);

			if( value.intval == POWER_SUPPLY_STATUS_FULL) {
				if(((pbat_data->charger_ctrl_status == D2153_BAT_RECHG_FULL)
					|| (pbat_data->charger_ctrl_status == D2153_BAT_CHG_BACKCHG_FULL))
					&& (pbat_data->average_voltage >= D2153_BAT_RECHG_FULL_LVL)) {
					spa_event_handler(SPA_EVT_EOC, 0);
					pbat_data->charger_ctrl_status = D2153_BAT_RECHG_FULL;
					pr_info("%s. Recharging Done.(3) 2nd full > discharge > Recharge\n", __func__);
				} else if((pbat_data->charger_ctrl_status == D2153_BAT_CHG_FRST_FULL)
							&& (pbat_data->average_voltage >= D2153_BAT_CHG_BACK_FULL_LVL)) {
					pbat_data->charger_ctrl_status = D2153_BAT_CHG_BACKCHG_FULL;
					spa_event_handler(SPA_EVT_EOC, 0);
					pr_info("%s. Recharging Done.(4) 1st full > 2nd full\n", __func__);
				}
			} else if(value.intval != POWER_SUPPLY_STATUS_DISCHARGING){
				// Will stop charging when a voltage approach to first full charge level.
				if((pbat_data->charger_ctrl_status < D2153_BAT_CHG_FRST_FULL)
					&& (pbat_data->average_voltage >= D2153_BAT_CHG_FRST_FULL_LVL)) {
					spa_event_handler(SPA_EVT_EOC, 0);
					pbat_data->charger_ctrl_status = D2153_BAT_CHG_FRST_FULL;
					pr_info("%s. First charge done.(1)\n", __func__);
				} else if((pbat_data->charger_ctrl_status < D2153_BAT_CHG_FRST_FULL)
					&& (pbat_data->average_voltage >= D2153_BAT_CHG_BACK_FULL_LVL)) {
					spa_event_handler(SPA_EVT_EOC, 0);
					spa_event_handler(SPA_EVT_EOC, 0);
					pbat_data->charger_ctrl_status = D2153_BAT_CHG_BACKCHG_FULL;
					pr_info("%s. Fully charged.(2)(Back-charging done)\n", __func__);
				}
			}
		}
#endif
		schedule_delayed_work(&pbat->monitor_volt_work, D2153_VOLTAGE_MONITOR_FAST);
	}

#if 1 // def CONFIG_D2153_BATTERY_DEBUG
	pr_info("# SOC = %3d.%d %%, ADC(read) = %4d, ADC(avg) = %4d, Voltage(avg) = %4d mV, ADC(VF) = %4d\n",
				(pbat->battery_data.soc/10),
				(pbat->battery_data.soc%10),
				pbat->battery_data.origin_volt_adc,
				pbat->battery_data.average_volt_adc,
				pbat->battery_data.average_voltage,
				pbat->battery_data.vf_adc);

#endif
#ifdef CONFIG_D2153_HW_TIMER
	if(is_called_by_ticker ==1) {
		is_called_by_ticker=0;
		wake_unlock(&gbat->battery_data.sleep_monitor_wakeup);
	}
#endif
	return;

err_adc_read:
	schedule_delayed_work(&pbat->monitor_volt_work, D2153_VOLTAGE_MONITOR_START);
	return;
}


static void d2153_monitor_temperature_work(struct work_struct *work)
{
	struct d2153_battery *pbat = container_of(work, struct d2153_battery, monitor_temp_work.work);
	int ret;

	ret = d2153_read_temperature(pbat);
	if(ret < 0) {
		pr_err("%s. Failed to read_temperature\n", __func__);
		schedule_delayed_work(&pbat->monitor_temp_work, D2153_TEMPERATURE_MONITOR_FAST);
		return;
	}

	if(pbat->battery_data.temp_adc_init_done) {
		schedule_delayed_work(&pbat->monitor_temp_work, D2153_TEMPERATURE_MONITOR_NORMAL);
	}
	else {
		schedule_delayed_work(&pbat->monitor_temp_work, D2153_TEMPERATURE_MONITOR_FAST);
	}

#ifdef CONFIG_D2153_BATTERY_DEBUG
	pr_info("# TEMP_BOARD(ADC) = %4d, Board Temperauter(Celsius) = %3d.%d\n",
				pbat->battery_data.average_temp_adc,
				(pbat->battery_data.average_temperature/10),
				(pbat->battery_data.average_temperature%10));
#endif
	return ;
}


/* 
 * Name : d2153_battery_start
 */
void d2153_battery_start(void)
{
	schedule_delayed_work(&gbat->monitor_volt_work, 0);
}
EXPORT_SYMBOL_GPL(d2153_battery_start);


/* 
 * Name : d2153_battery_data_init
 */
static void d2153_battery_data_init(struct d2153_battery *pbat)
{
	struct d2153_battery_data *pbat_data = &pbat->battery_data;

	if(unlikely(!pbat_data)) {
		pr_err("%s. Invalid platform data\n", __func__);
		return;
	}

	pbat->adc_mode = D2153_ADC_MODE_MAX;

	pbat_data->sum_total_adc = 0;
	pbat_data->vdd_hwmon_level = 0;
	pbat_data->volt_adc_init_done = FALSE;
	pbat_data->temp_adc_init_done = FALSE;
	pbat_data->battery_present = TRUE;
	pbat_data->is_charging = D2153_BATTERY_STATUS_MAX;
	pbat_data->is_ap_temp_jumped = FALSE;
	pbat_data->prev_ap_temperature = -EINVAL;
#ifdef CONFIG_D2153_EOC_CTRL
	pbat_data->charger_ctrl_status = D2153_BAT_CHG_MAX;
#endif
	wake_lock_init(&pbat_data->sleep_monitor_wakeup, WAKE_LOCK_SUSPEND, "sleep_monitor");

	return;
}

#ifdef CONFIG_D2153_HW_TIMER
//#define CMSTR17				IO_ADDRESS(0xE6130700U)
//#define CMCSR17				IO_ADDRESS(0xE6130710U)
//#define CMCNT17				IO_ADDRESS(0xE6130714U)
//#define CMCOR17				IO_ADDRESS(0xE6130718U)
#define CMT17_SPI			100U

#define ICD_ISR0 0xF0001080
#define ICD_IPTR0 0xf0001800

static DEFINE_SPINLOCK(cmt_lock);
#define CONFIG_D2153_BAT_CMT_OVF 60*5

#define CMT_OVF		((256*CONFIG_D2153_BAT_CMT_OVF) - 2)

static inline u32 dec2hex(u32 dec)
{
	return dec;
}


/*
 * rmu2_cmt_start: start CMT
 * input: none
 * output: none
 * return: none
 */
static void d2153_battery_cmt_start(void)
{
	unsigned long flags, wrflg, i = 0;

	printk(KERN_INFO "START < %s >\n", __func__);
	dlg_info("< %s >CMCLKE=%08x\n", __func__, __raw_readl(CMCLKE));
	dlg_info("< %s >CMSTR17=%08x\n", __func__, __raw_readl(CMSTR17));
	dlg_info("< %s >CMCSR17=%08x\n", __func__, __raw_readl(CMCSR17));
	dlg_info("< %s >CMCNT17=%08x\n", __func__, __raw_readl(CMCNT17));
	dlg_info("< %s >CMCOR17=%08x\n", __func__, __raw_readl(CMCOR17));

	spin_lock_irqsave(&cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) | (1<<7), CMCLKE);
	spin_unlock_irqrestore(&cmt_lock, flags);

	mdelay(8);

	__raw_writel(0, CMSTR17);
	__raw_writel(0U, CMCNT17);
	__raw_writel(0x000000a6U, CMCSR17);	/* Int enable */
	__raw_writel(dec2hex(CMT_OVF), CMCOR17);

	do {
		wrflg = ((__raw_readl(CMCSR17) >> 13) & 0x1);
		i++;
	} while (wrflg != 0x00 && i < 0xffffffff);

	__raw_writel(1, CMSTR17);

	dlg_info("< %s >CMCLKE=%08x\n", __func__, __raw_readl(CMCLKE));
	dlg_info("< %s >CMSTR17=%08x\n", __func__, __raw_readl(CMSTR17));
	dlg_info("< %s >CMCSR17=%08x\n", __func__, __raw_readl(CMCSR17));
	dlg_info("< %s >CMCNT17=%08x\n", __func__, __raw_readl(CMCNT17));
	dlg_info("< %s >CMCOR17=%08x\n", __func__, __raw_readl(CMCOR17));
}

/*
 * rmu2_cmt_stop: stop CMT
 * input: none
 * output: none
 * return: none
 */
void d2153_battery_cmt_stop(void)
{
	unsigned long flags, wrflg, i = 0;

	printk(KERN_INFO "START < %s >\n", __func__);
	__raw_readl(CMCSR17);
	__raw_writel(0x00000186U, CMCSR17);	/* Int disable */
	__raw_writel(0U, CMCNT17);
	__raw_writel(0, CMSTR17);

	do {
		wrflg = ((__raw_readl(CMCSR17) >> 13) & 0x1);
		i++;
	} while (wrflg != 0x00 && i < 0xffffffff);

	mdelay(12);
	spin_lock_irqsave(&cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) & ~(1<<7), CMCLKE);
	spin_unlock_irqrestore(&cmt_lock, flags);
}

#if 0
/*
 * rmu2_cmt_clear: CMT counter clear
 * input: none
 * output: none
 * return: none
 */
static void d2153_battery_cmt_clear(void)
{

	int wrflg, i = 0;
	printk(KERN_INFO "START < %s >\n", __func__);
	__raw_writel(0, CMSTR17);	/* Stop counting */

	__raw_writel(0U, CMCNT17);	/* Clear the count value */

	do {
		wrflg = ((__raw_readl(CMCSR17) >> 13) & 0x1);
		i++;
	} while (wrflg != 0x00 && i < 0xffffffff);
	__raw_writel(1, CMSTR17);	/* Enable counting again */
}
#endif

/*
 * rmu2_cmt_irq: IRQ handler for CMT
 * input:
 *		@irq: interrupt number
 *		@dev_id: device ID
 * output: none
 * return:
 *		IRQ_HANDLED: irq handled
 */
static irqreturn_t d2153_battery_cmt_irq(int irq, void *dev_id)
{
	unsigned int reg_val = __raw_readl(CMCSR17);

	reg_val &= ~0x0000c000U;
	__raw_writel(reg_val, CMCSR17);

	printk(KERN_ERR "d2153_battery_cmt_irq!!!!!!!!!!!!!!!!!!..");

	d2153_battery_read_status(D2153_BATTERY_SLEEP_MONITOR);

	return IRQ_HANDLED;
}

/*
 * rmu2_cmt_init_irq: IRQ initialization handler for CMT
 * input: none
 * output: none
 * return: none
 */
static void d2153_battery_cmt_init_irq(void)
{
	int ret;
	unsigned int irq;

	pr_info(" < %s >\n", __func__);

	irq = gic_spi(CMT17_SPI);
	set_irq_flags(irq, IRQF_VALID);
	ret = request_threaded_irq(irq, NULL, d2153_battery_cmt_irq, IRQF_ONESHOT,
				"CMT17_RWDT0", (void *)irq);
	if (0 > ret) {
		pr_err("%s:%d request_irq failed err=%d\n",
				__func__, __LINE__, ret);
		free_irq(irq, (void *)irq);
		return;
	}

	enable_irq_wake(irq);
}
#endif /* CONFIG_D2153_HW_TIMER */

/* 
 * Name : d2153_battery_probe
 */
static __devinit int d2153_battery_probe(struct platform_device *pdev)
{
	struct d2153 *d2153 = platform_get_drvdata(pdev);
	struct d2153_battery *pbat = &d2153->batt;
	int i;
	int ret = 0;

	pr_info("Start %s\n", __func__);

	if(unlikely(!pbat)) {
		pr_err("%s. Invalid platform data\n", __func__);
		return -EINVAL;
	}

	gbat = pbat;
	pbat->pd2153 = d2153;

	// Initialize a resource locking
	mutex_init(&pbat->lock);
	mutex_init(&pbat->api_lock);
	mutex_init(&pbat->meoc_lock);

	// Store a driver data structure to platform.
	platform_set_drvdata(pdev, pbat);

	d2153_battery_data_init(pbat);
	d2153_set_adc_mode(pbat, D2153_ADC_IN_AUTO);
	// Disable 50uA current source in Manual ctrl register
	d2153_reg_write(d2153, D2153_ADC_MAN_REG, 0x00);

	INIT_DELAYED_WORK(&pbat->monitor_volt_work, d2153_monitor_voltage_work);
	INIT_DELAYED_WORK(&pbat->monitor_temp_work, d2153_monitor_temperature_work);

	// Start schedule of dealyed work for temperature.
	schedule_delayed_work(&pbat->monitor_temp_work, 0);

	device_init_wakeup(&pdev->dev, 1);


#ifdef CONFIG_D2153_DEBUG_FEATURE
	for (i = 0; i < D2153_PROP_MAX ; i++) {
		ret = device_create_file(&pdev->dev, &d2153_battery_attrs[i]);
		if (ret) {
			printk(KERN_ERR "Failed to create battery sysfs entries\n");
			return ret;
		}
	}
#endif

#ifdef CONFIG_D2153_HW_TIMER
	d2153_battery_cmt_init_irq();
#endif
	
	pr_info("%s. End...\n", __func__);
	return ret;
}


/*
 * Name : d2153_battery_suspend
 */
static int d2153_battery_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct d2153_battery *pbat = platform_get_drvdata(pdev);
	struct d2153 *d2153 = pbat->pd2153;

	pr_info("%s. Enter\n", __func__);

	if(unlikely(!d2153)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&pbat->monitor_temp_work);
	cancel_delayed_work_sync(&pbat->monitor_volt_work);


#ifdef CONFIG_D2153_HW_TIMER
	do_gettimeofday(&suspend_time);
	pr_info("##### %s. suspend_time = %ld\n", __func__, suspend_time.tv_sec);

	d2153_battery_cmt_start();
#endif /* CONFIG_D2153_HW_TIMER */
	pr_info("%s. Leave\n", __func__);

	return 0;
}


/*
 * Name : d2153_battery_resume
 */
static int d2153_battery_resume(struct platform_device *pdev)
{
	struct d2153_battery *pbat = platform_get_drvdata(pdev);
	struct d2153 *d2153 = pbat->pd2153;
#ifdef CONFIG_D2153_HW_TIMER
	u8 do_sampling = 0;
	unsigned long monitor_work_start = 0;
#endif

	pr_info("%s. Enter\n", __func__);

	if(unlikely(!d2153)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_D2153_HW_TIMER
	d2153_battery_cmt_stop();
#endif
	// Start schedule of dealyed work for monitoring voltage and temperature.
	if(!is_called_by_ticker) {
#ifdef CONFIG_D2153_HW_TIMER
		do_gettimeofday(&resume_time);

		pr_info("##### suspend_time = %ld, resume_time = %ld\n",
					suspend_time.tv_sec, resume_time.tv_sec);
		if((resume_time.tv_sec - suspend_time.tv_sec) > 10) {
			time_diff = (long)(resume_time.tv_sec - suspend_time.tv_sec);
			memset(&suspend_time, 0, sizeof(struct timeval));
			do_sampling = 1;
			pr_info("###### Sampling voltage & temperature ADC\n");
		}

		if(do_sampling) {
			monitor_work_start = 0;

			wake_lock_timeout(&pbat->battery_data.sleep_monitor_wakeup,
										D2153_SLEEP_MONITOR_WAKELOCK_TIME);
		}
		else {
			monitor_work_start = 1 * HZ;
		}
		schedule_delayed_work(&pbat->monitor_temp_work, monitor_work_start);
		schedule_delayed_work(&pbat->monitor_volt_work, monitor_work_start);
#else
		schedule_delayed_work(&pbat->monitor_temp_work, 0);
		schedule_delayed_work(&pbat->monitor_volt_work, 0);
#endif
	}

	pr_info("%s. Leave\n", __func__);

	return 0;
}


/*
 * Name : d2153_battery_remove
 */
static __devexit int d2153_battery_remove(struct platform_device *pdev)
{
	struct d2153_battery *pbat = platform_get_drvdata(pdev);
	struct d2153 *d2153 = pbat->pd2153;
	u8 i;

	pr_info("%s.Start \n", __func__);

	if(unlikely(!d2153)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&pbat->monitor_volt_work);
	cancel_delayed_work_sync(&pbat->monitor_temp_work);

	// Free IRQ
#ifdef D2153_REG_EOM_IRQ
	d2153_free_irq(d2153, D2153_IRQ_EADCEOM);
#endif /* D2153_REG_EOM_IRQ */
#ifdef D2153_REG_VDD_MON_IRQ
	d2153_free_irq(d2153, D2153_IRQ_EVDD_MON);
#endif /* D2153_REG_VDD_MON_IRQ */
#ifdef D2153_REG_VDD_LOW_IRQ
	d2153_free_irq(d2153, D2153_IRQ_EVDD_LOW);
#endif /* D2153_REG_VDD_LOW_IRQ */
#ifdef D2153_REG_TBAT2_IRQ
	d2153_free_irq(d2153, D2153_IRQ_ETBAT2);
#endif /* D2153_REG_TBAT2_IRQ */

	d2153_put_adc_hwsem();

#ifdef CONFIG_D2153_DEBUG_FEATURE
	for (i = 0; i < D2153_PROP_MAX ; i++) {
		device_remove_file(&pdev->dev, &d2153_battery_attrs[i]);
	}
#endif

	return 0;
}

/*
 * Name : d2153_battery_shutdown
 */
static void d2153_battery_shutdown(struct platform_device *pdev)
{
	struct d2153_battery *pbat = platform_get_drvdata(pdev);
	struct d2153 *d2153 = pbat->pd2153;
	u8 i;

	pr_info("%s. Start \n", __func__);

	if(unlikely(!d2153)) {
		pr_err("%s. Invalid parameter\n", __func__);
		return;
	}

	cancel_delayed_work_sync(&pbat->monitor_volt_work);
	cancel_delayed_work_sync(&pbat->monitor_temp_work);

	// Free IRQ
#ifdef D2153_REG_EOM_IRQ
	d2153_free_irq(d2153, D2153_IRQ_EADCEOM);
#endif /* D2153_REG_EOM_IRQ */
#ifdef D2153_REG_VDD_MON_IRQ
	d2153_free_irq(d2153, D2153_IRQ_EVDD_MON);
#endif /* D2153_REG_VDD_MON_IRQ */
#ifdef D2153_REG_VDD_LOW_IRQ
	d2153_free_irq(d2153, D2153_IRQ_EVDD_LOW);
#endif /* D2153_REG_VDD_LOW_IRQ */
#ifdef D2153_REG_TBAT2_IRQ
	d2153_free_irq(d2153, D2153_IRQ_ETBAT2);
#endif /* D2153_REG_TBAT2_IRQ */

	d2153_put_adc_hwsem();

#ifdef CONFIG_D2153_DEBUG_FEATURE
	for (i = 0; i < D2153_PROP_MAX ; i++) {
		device_remove_file(&pdev->dev, &d2153_battery_attrs[i]);
	}
#endif

	pr_info("%s. End \n", __func__);

	return;
}


static struct platform_driver d2153_battery_driver = {
	.probe    = d2153_battery_probe,
	.suspend  = d2153_battery_suspend,
	.resume   = d2153_battery_resume,
	.remove   = d2153_battery_remove,
	.shutdown = d2153_battery_shutdown,
	.driver   = {
		.name  = "d2153-battery",
		.owner = THIS_MODULE,
    },
};

static int __init d2153_battery_init(void)
{
	printk(d2153_battery_banner);
	return platform_driver_register(&d2153_battery_driver);
}
subsys_initcall(d2153_battery_init);

static void __exit d2153_battery_exit(void)
{
	flush_scheduled_work();
	platform_driver_unregister(&d2153_battery_driver);
}
module_exit(d2153_battery_exit);

MODULE_AUTHOR("Dialog Semiconductor Ltd. < eric.jeong@diasemi.com >");
MODULE_DESCRIPTION("Battery driver for the Dialog D2153 PMIC");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Power supply : d2153-battery");
