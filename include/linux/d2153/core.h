/*
 * core.h  --  Core Driver for Dialog Semiconductor D2153 PMIC
 *
 * Copyright 2011 Dialog Semiconductor Ltd
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __D2153_LEOPARD_CORE_H_
#define __D2153_LEOPARD_CORE_H_

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/d2153/pmic.h>
#include <linux/d2153/rtc.h>
#include <linux/d2153/hwmon.h>
#include <linux/d2153/audio.h>
#include <linux/power_supply.h>
#include <linux/d2153/d2153_battery.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif /*CONFIG_HAS_EARLYSUSPEND*/

/*
 * Register values.
 */  

#define I2C								1

#ifdef CONFIG_SND_SOC_D2153_AAD_MODULE
#define CONFIG_SND_SOC_D2153_AAD
#endif

#define D2153_I2C						"d2153"

/* Module specific error codes */
#define INVALID_REGISTER				2
#define INVALID_READ					3
#define INVALID_PAGE					4

/* Total number of registers in D2153 */
#define D2153_MAX_REGISTER_CNT			0x0100

#define D2153_PMIC_I2C_ADDR				(0x92 >> 1)   // 0x49
#define D2153_CODEC_I2C_ADDR			(0x30 >> 1)   // 0x18
#define D2153_AAD_I2C_ADDR				(0x32 >> 1)   // 0x19

#define D2153_IOCTL_READ_REG  			0xc0025083
#define D2153_IOCTL_WRITE_REG 			0x40025084

#define D2153_AA_Silicon    0x00
#define D2153_AB_Silicon    0x10
#define D2153_AC_Silicon    0x20

typedef struct {
	unsigned long reg;
	unsigned short val;
} pmu_reg;

/*
 * D2153 Number of Interrupts
 */
enum D2153_IRQ {
	// EVENT_A register IRQ
	D2153_IRQ_EVF = 0,
	D2153_IRQ_EADCIN1,
	D2153_IRQ_ETBAT2,
	D2153_IRQ_EVDD_LOW,
	D2153_IRQ_EVDD_MON,
	D2153_IRQ_EALARM,
	D2153_IRQ_ESEQRDY,
	D2153_IRQ_ETICK,

	// EVENT_B register IRQ
	D2153_IRQ_ENONKEY_LO,
	D2153_IRQ_ENONKEY_HI,
	D2153_IRQ_ENONKEY_HOLDON,
	D2153_IRQ_ENONKEY_HOLDOFF,
	D2153_IRQ_ETBAT1,
	D2153_IRQ_EADCEOM,

	// EVENT_C register IRQ
	D2153_IRQ_ETA,
	D2153_IRQ_ENJIGON,
	D2153_IRQ_EACCDET,
	D2153_IRQ_EJACKDET,

	D2153_NUM_IRQ
};


#define D2153_MCTL_MODE_INIT(_reg_id, _dsm_mode, _default_pm_mode) \
	[_reg_id] = { \
		.reg_id = _reg_id, \
		.dsm_opmode = _dsm_mode, \
		.default_pm_mode = _default_pm_mode, \
	}

// for DEBUGGING and Troubleshooting
#if 1	//defined(DEBUG)
#define dlg_crit(fmt, ...) printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define dlg_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define dlg_warn(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define dlg_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#else
#define dlg_crit(fmt, ...) 	do { } while(0);
#define dlg_err(fmt, ...)	do { } while(0);
#define dlg_warn(fmt, ...)	do { } while(0);
#define dlg_info(fmt, ...)	do { } while(0);
#endif

typedef u32 (*pmu_platform_callback)(int event, int param);

struct d2153_irq {
	irq_handler_t handler;
	void *data;
};

struct d2153_onkey {
	struct platform_device *pdev;
	struct input_dev *input;
};

//
struct d2153_regl_init_data {
	int regl_id;
	struct regulator_init_data   *initdata;
};


struct d2153_regl_map {
	u8 reg_id;
	u8 dsm_opmode;
	u8 default_pm_mode;
};


/**
 * Data to be supplied by the platform to initialise the D2153.
 *
 * @init: Function called during driver initialisation.  Should be
 *        used by the platform to configure GPIO functions and similar.
 * @irq_high: Set if D2153 IRQ is active high.
 * @irq_base: Base IRQ for genirq (not currently used).
 */

struct temp2adc_map {
	int temp;
	int adc;
};

struct d2153_hwmon_platform_data {
	u32	battery_capacity;
	u32	vf_lower;
	u32	vf_upper;
	int battery_technology;
	struct temp2adc_map *bcmpmu_temp_map;
	int bcmpmu_temp_map_len;
};
//AFO struct i2c_slave_platform_data { };

struct d2153_battery_platform_data {
	u32	battery_capacity;
	u32	vf_lower;
	u32	vf_upper;
	int battery_technology;
};

struct d2153_platform_data {
	//AFO struct i2c_slave_platform_data i2c_pdata;
	int i2c_adapter_id;

	int 	(*init)(struct d2153 *d2153);
	int 	(*irq_init)(struct d2153 *d2153);
	int		irq_mode;	/* Clear interrupt by read/write(0/1) */
	int		irq_base;	/* IRQ base number of D2153 */
	struct d2153_regl_init_data	*regulator_data;
	//struct  regulator_consumer_supply *regulator_data;
	struct d2153_hwmon_platform_data *hwmon_pdata;
	struct d2153_battery_platform_data *pbat_platform;

	struct d2153_regl_map regl_map[D2153_NUMBER_OF_REGULATORS];

	struct d2153_audio audio;
};

struct d2153 {
	struct device *dev;

	struct i2c_client *pmic_i2c_client;
	struct i2c_client *codec_i2c_client;
	struct mutex i2c_mutex;

	int (*read_dev)(struct d2153 * const d2153, char reg, int size, void *dest);
	int (*write_dev)(struct d2153 * const d2153, char reg, int size, u8 *src);

	u8 *reg_cache;
	u16 vbat_init_adc[3];
	u16 average_vbat_init_adc;

	/* Interrupt handling */
    struct work_struct irq_work;
    struct task_struct *irq_task;
	struct mutex irq_mutex; /* IRQ table mutex */
	struct d2153_irq irq[D2153_NUM_IRQ];
	int chip_irq;

	struct d2153_pmic pmic;
	struct d2153_rtc rtc;
	struct d2153_onkey onkey;
	struct d2153_hwmon hwmon;
	struct d2153_battery batt;

	struct d2153_platform_data *pdata;
	struct mutex d2153_io_mutex;
	struct delayed_work     vdd_fault_work;
};

/*
 * d2153 Core device initialisation and exit.
 */
int 	d2153_device_init(struct d2153 *d2153, int irq, struct d2153_platform_data *pdata);
void 	d2153_device_exit(struct d2153 *d2153);

/*
 * d2153 device IO
 */
int 	d2153_clear_bits(struct d2153 * const d2153, u8 const reg, u8 const mask);
int 	d2153_set_bits(struct d2153* const d2153, u8 const reg, u8 const mask);
int     d2153_reg_read(struct d2153 * const d2153, u8 const reg, u8 *dest);
int 	d2153_reg_write(struct d2153 * const d2153, u8 const reg, u8 const val);
int 	d2153_block_read(struct d2153 * const d2153, u8 const start_reg, u8 const regs, u8 * const dest);
int 	d2153_block_write(struct d2153 * const d2153, u8 const start_reg, u8 const regs, u8 * const src);

/*
 * d2153 internal interrupts
 */
int 	d2153_register_irq(struct d2153 *d2153, int irq, irq_handler_t handler, 
				            unsigned long flags, const char *name, void *data);
int 	d2153_free_irq(struct d2153 *d2153, int irq);
int 	d2153_mask_irq(struct d2153 *d2153, int irq);
int 	d2153_unmask_irq(struct d2153 *d2153, int irq);
int 	d2153_irq_init(struct d2153 *d2153, int irq, struct d2153_platform_data *pdata);
int 	d2153_irq_exit(struct d2153 *d2153);

void	d2153_set_irq_disable(void);
void	d2153_set_irq_enable(void);

#if defined(CONFIG_MACH_RHEA_SS_IVORY) || defined(CONFIG_MACH_RHEA_SS_NEVIS)
/* DLG IOCTL interface */
extern int d2153_ioctl_regulator(struct d2153 *d2153, unsigned int cmd, unsigned long arg);
#endif /* CONFIG_MACH_RHEA_SS_IVORY */

/* DLG new prototype */
void 	d2153_system_poweroff(void);
void 	d2153_set_mctl_enable(void);
void d2153_clk32k_enable(int onoff);
extern struct d2153_platform_data d2153_pdata;

/* HW Sem*/
int  d2153_hw_sem_reset_init(void);
int  d2153_hw_sem_reset_deinit(void);
int  d2153_get_i2c_hwsem(void);
int  d2153_get_adc_hwsem(void);
void d2153_put_i2c_hwsem(void);
void d2153_put_adc_hwsem(void);

#endif /* __D2153_LEOPARD_CORE_H_ */
