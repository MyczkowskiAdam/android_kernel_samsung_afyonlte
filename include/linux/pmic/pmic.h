/*
 * include/linux/pmic/pmic.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef _LINUX_PMIC_H
#define _LINUX_PMIC_H

#include <linux/device.h>	/* for struct device */
#include <linux/platform_device.h>
#include <linux/module.h>

/* Macro for PMIC Production Test enable */
#define PMIC_PT_TEST_ENABLE

enum {
	E_POWER_ALL = 0,		/* All power resources */
	E_POWER_VCORE,			/* Power resource SMPS1 */
	E_POWER_VIO2,			/* Power resource SMPS2 */
	E_POWER_VIO1,			/* Power resource SMPS3 */
	E_POWER_VANA1_RF,		/* Power resource SMPS4 */
	E_POWER_VCORE_RF,		/* Power resource SMPS5 */
	E_POWER_VIO_SD,			/* Power resource LDO1 */
	E_POWER_VDIG_RF,		/* Power resource LDO2 */
	E_POWER_VDDR,			/* Power resource LDO3 */
	E_POWER_VMIPI,			/* Power resource LDO4 */
	E_POWER_VANA_MM,		/* Power resource LDO5 */
	E_POWER_VMMC,			/* Power resource LDO6 */
	E_POWER_VUSIM1,			/* Power resource LDO7 */
};

enum {
	E_SMPS_VOLTAGE_0_0000V = 0, /* 0.0000V */
	E_SMPS_VOLTAGE_0_6000V,     /* 0.6000V */
	E_SMPS_VOLTAGE_0_6125V,     /* 0.6125V */
	E_SMPS_VOLTAGE_0_6250V,     /* 0.6250V */
	E_SMPS_VOLTAGE_0_6375V,     /* 0.6375V */
	E_SMPS_VOLTAGE_0_6500V,     /* 0.6500V */
	E_SMPS_VOLTAGE_0_6625V,     /* 0.6625V */
	E_SMPS_VOLTAGE_0_6750V,     /* 0.6750V */
	E_SMPS_VOLTAGE_0_6875V,     /* 0.6875V */
	E_SMPS_VOLTAGE_0_7000V,     /* 0.7000V */
	E_SMPS_VOLTAGE_0_7125V,     /* 0.7125V */
	E_SMPS_VOLTAGE_0_7250V,     /* 0.7250V */
	E_SMPS_VOLTAGE_0_7375V,     /* 0.7375V */
	E_SMPS_VOLTAGE_0_7500V,     /* 0.7500V */
	E_SMPS_VOLTAGE_0_7625V,     /* 0.7625V */
	E_SMPS_VOLTAGE_0_7750V,     /* 0.7750V */
	E_SMPS_VOLTAGE_0_7875V,     /* 0.7875V */
	E_SMPS_VOLTAGE_0_8000V,     /* 0.8000V */
	E_SMPS_VOLTAGE_0_8125V,     /* 0.8125V */
	E_SMPS_VOLTAGE_0_8250V,     /* 0.8250V */
	E_SMPS_VOLTAGE_0_8375V,     /* 0.8375V */
	E_SMPS_VOLTAGE_0_8500V,     /* 0.8500V */
	E_SMPS_VOLTAGE_0_8625V,     /* 0.8625V */
	E_SMPS_VOLTAGE_0_8750V,     /* 0.8750V */
	E_SMPS_VOLTAGE_0_8875V,     /* 0.8875V */
	E_SMPS_VOLTAGE_0_9000V,     /* 0.9000V */
	E_SMPS_VOLTAGE_0_9125V,     /* 0.9125V */
	E_SMPS_VOLTAGE_0_9250V,     /* 0.9250V */
	E_SMPS_VOLTAGE_0_9375V,     /* 0.9375V */
	E_SMPS_VOLTAGE_0_9500V,     /* 0.9500V */
	E_SMPS_VOLTAGE_0_9625V,     /* 0.9625V */
	E_SMPS_VOLTAGE_0_9750V,     /* 0.9750V */
	E_SMPS_VOLTAGE_0_9875V,     /* 0.9875V */
	E_SMPS_VOLTAGE_1_0000V,     /* 1.0000V */
	E_SMPS_VOLTAGE_1_0125V,     /* 1.0125V */
	E_SMPS_VOLTAGE_1_0250V,     /* 1.0250V */
	E_SMPS_VOLTAGE_1_0375V,     /* 1.0375V */
	E_SMPS_VOLTAGE_1_0500V,     /* 1.0500V */
	E_SMPS_VOLTAGE_1_0625V,     /* 1.0625V */
	E_SMPS_VOLTAGE_1_0750V,     /* 1.0750V */
	E_SMPS_VOLTAGE_1_0875V,     /* 1.0875V */
	E_SMPS_VOLTAGE_1_1000V,     /* 1.1000V */
	E_SMPS_VOLTAGE_1_1125V,     /* 1.1125V */
	E_SMPS_VOLTAGE_1_1250V,     /* 1.1250V */
	E_SMPS_VOLTAGE_1_1375V,     /* 1.1375V */
	E_SMPS_VOLTAGE_1_1500V,     /* 1.1500V */
	E_SMPS_VOLTAGE_1_1625V,     /* 1.1625V */
	E_SMPS_VOLTAGE_1_1750V,     /* 1.1750V */
	E_SMPS_VOLTAGE_1_1875V,     /* 1.1875V */
	E_SMPS_VOLTAGE_1_2000V,     /* 1.2000V */
	E_SMPS_VOLTAGE_1_2125V,     /* 1.2125V */
	E_SMPS_VOLTAGE_1_2250V,     /* 1.2250V */
	E_SMPS_VOLTAGE_1_2375V,     /* 1.2375V */
	E_SMPS_VOLTAGE_1_2500V,     /* 1.2500V */
	E_SMPS_VOLTAGE_1_2625V,     /* 1.2625V */
	E_SMPS_VOLTAGE_1_2750V,     /* 1.2750V */
	E_SMPS_VOLTAGE_1_2875V,     /* 1.2875V */
	E_SMPS_VOLTAGE_1_3000V,     /* 1.3000V */
	E_SMPS_VOLTAGE_1_3500V,     /* 1.3500V [Not supported] */
	E_SMPS_VOLTAGE_1_5000V,     /* 1.5000V [Not supported] */
	E_SMPS_VOLTAGE_1_8000V,     /* 1.8000V [Not supported] */
	E_SMPS_VOLTAGE_1_9000V,     /* 1.9000V [Not supported] */
	E_SMPS_VOLTAGE_2_1000V,     /* 2.1000V [Not supported] */

	E_LDO_VOLTAGE_1_0000V,      /* 1.0000V */
	E_LDO_VOLTAGE_1_1000V,      /* 1.1000V */
	E_LDO_VOLTAGE_1_2000V,      /* 1.2000V */
	E_LDO_VOLTAGE_1_3000V,      /* 1.3000V */
	E_LDO_VOLTAGE_1_4000V,      /* 1.4000V */
	E_LDO_VOLTAGE_1_5000V,      /* 1.5000V */
	E_LDO_VOLTAGE_1_6000V,      /* 1.6000V */
	E_LDO_VOLTAGE_1_7000V,      /* 1.7000V */
	E_LDO_VOLTAGE_1_8000V,      /* 1.8000V */
	E_LDO_VOLTAGE_1_9000V,      /* 1.9000V */
	E_LDO_VOLTAGE_2_0000V,      /* 2.0000V */
	E_LDO_VOLTAGE_2_1000V,      /* 2.1000V */
	E_LDO_VOLTAGE_2_2000V,      /* 2.2000V */
	E_LDO_VOLTAGE_2_3000V,      /* 2.3000V */
	E_LDO_VOLTAGE_2_4000V,      /* 2.4000V */
	E_LDO_VOLTAGE_2_5000V,      /* 2.5000V */
	E_LDO_VOLTAGE_2_6000V,      /* 2.6000V */
	E_LDO_VOLTAGE_2_7000V,      /* 2.7000V */
	E_LDO_VOLTAGE_2_8000V,      /* 2.8000V */
	E_LDO_VOLTAGE_2_9000V,      /* 2.9000V */
	E_LDO_VOLTAGE_3_0000V,      /* 3.0000V */
	E_LDO_VOLTAGE_3_1000V,      /* 3.1000V */
	E_LDO_VOLTAGE_3_2000V,      /* 3.2000V */
	E_LDO_VOLTAGE_3_3000V,      /* 3.3000V */
	E_LDO_VOLTAGE_2_7500V,      /* 2.7500V [Not supported] */
};

enum {
	E_POWER_OFF      = 0x00,
	E_POWER_ON       = 0x01,		/*E_POWER_ON  must be 0x01*/
	E_POWER_RESERVED = 0x02,		/* don't care */
	E_POWER_SLEEP    = 0x04,		/*E_POWER_SLEEP  must be 0x04*/
};

enum {
	E_PMODE_OFF = 0,	/* Disable */
	E_SMPS_PMODE_AUTO,	/* Auto Mode (PWM/PFM) */
	E_SMPS_PMODE_FORCE,	/* Force PWM Mode */
	E_LDO_PMODE_AMS,	/* AMS Mode (Sleep/Active) */
	E_LDO_PMODE_ACTIVE,	/* Active Mode */
};

enum {
	E_DEVICE_NONE                 = 0x1,	/* Not Connect */
	E_DEVICE_DEDICATEDCHARGER     = 0x2,	/* Dedicated Charger */
	E_DEVICE_CHARGINGDOWNSTREAM   = 0x4,	/* Charging Downstream */
	E_DEVICE_HOSTPC               = 0x8,	/* Standard Downstream */
	E_DEVICE_CHARGER5WIRETYPE2    = 0x10,	/* Charger 5wire/Type2 */
	E_DEVICE_CHARGER5WIRETYPE1    = 0x20,	/* Charger 5wire/Type1 */
	E_DEVICE_ACA_ADEVICERID_A     = 0x40,
				/* ACA A-device(RID_A)/Standard Downstream */
	E_DEVICE_PHONEPOWEREDACCESARY = 0x80,
				/* Phone Powered Accesary/HEADSET_Stereo */
	E_DEVICE_ACA_ADEVICERID_B     = 0x100,	/* ACA A-device(RID_B) */
	E_DEVICE_ACA_ADEVICERID_C     = 0x200,	/* ACA A-device(RID_C) */
	E_DEVICE_STEREO               = 0x400,	/* Stereo(+MIC) */
	E_DEVICE_STEREOANDCHARGER     = 0x800,	/* Stereo(+CHARGE) */
	E_DEVICE_MONORAL              = 0x1000,	/* Monoral(+MIC) */
	E_DEVICE_MONORALANDCHARGER    = 0x2000,	/* Monoral(+CHARGE) */
	E_DEVICE_BUTTON_ON            = 0x4000,	/* Control Button Pressed */
	E_DEVICE_USBOTG_HOST          = 0x8000,	/* USB OTG Host */
	E_DEVICE_RESERVE1             = 0x10000,/* Reserved1 */
	E_DEVICE_UNKNOWN              = 0x40000,/* Unidentified device */
};

enum {
	E_SLAVE_DVS        = 0x12,	/* DVS-I2C */
	E_SLAVE_RTC_POWER  = 0x48,	/* CTL-I2C for RTC and power */
	E_SLAVE_BATTERY    = 0x49,	/* CTL-I2C for battery */
	E_SLAVE_JTAG       = 0x4A,	/* CTL-I2C for JTAG */
};

enum {
	E_BATTERY_AC_ONLINE,
	E_BATTERY_USB_ONLINE,
	E_BATTERY_STATUS,
	E_BATTERY_HEALTH,
	E_BATTERY_PRESENT,
	E_BATTERY_CAPACITY,
	E_BATTERY_CAPACITY_LEVEL,
	E_BATTERY_TECHNOLOGY,
	E_BATTERY_TEMPERATURE,
	E_BATTERY_HPA_TEMPERATURE,
	E_BATTERY_VOLTAGE_NOW,
	E_BATTERY_STOP,
	E_BATTERY_TERMINATE,
};


enum {
	E_AC_STATUS_CHANGED      = 0x1,  /* AC power supply has been changed */
	E_USB_STATUS_CHANGED     = 0x2,  /* USB power supply has been changed */
	E_BATTERY_STATUS_CHANGED = 0x4,  /* Battery state has been changed */
};

enum {
	E_IFMODE_USB1 = 0x2,  /* USB Common1 */
};

#ifdef PMIC_PT_TEST_ENABLE
enum {
	E_BATT_PROP_ONLINE		= 0x1,
	E_BATT_PROP_STATUS		= 0x2,
	E_BATT_PROP_HEALTH		= 0x4,
	E_BATT_PROP_PRESENT		= 0x8,
	E_BATT_PROP_TECHNOLOGY		= 0x10,
	E_BATT_PROP_CAPACITY		= 0x20,
	E_BATT_PROP_VOLTAGE		= 0x40,
	E_BATT_PROP_TEMP		= 0x80,
};

struct pmic_time {
	int tm_year;
	int tm_mon;
	int tm_mday;
	int tm_hour;
	int tm_min;
	int tm_sec;
};
#endif

/*
 * struct pmic_device_ops
 *
 * For these PMIC methods the device parameter is the physical device
 * on whatever bus holds the hardware (I2C, Platform, SPI, etc), which
 * was passed to pmic_device_register().
 */
struct pmic_device_ops {
	int (*set_power_on)(struct device *dev, int resource);
	int (*set_power_off)(struct device *dev, int resource);
	int (*set_voltage)(struct device *dev, int resource, int voltage);
	int (*set_power_mode)(struct device *dev, int resource,
					int pstate, int pmode);
	void (*force_power_off)(struct device *dev, int resource);
	int (*get_ext_device)(struct device *dev);
	int (*read_register)(struct device *dev, int slave, u8 addr);
	int (*set_current_limit)(struct device *dev,
					int chrg_state, int chrg_type);
	int (*clk32k_enable)(u8 clk_res, u8 state);
	int (*read)(struct device *dev, u8 addr);
	int (*reads)(struct device *dev, u8 addr, int len, u8 *val);
	int (*write)(struct device *dev, u8 addr, u8 val);
	int (*writes)(struct device *dev, u8 addr, int len, u8 *val);
	int (*get_temp_status)(struct device *dev);
#ifdef PMIC_PT_TEST_ENABLE
	int (*get_batt_status)(struct device *dev, int property);
#endif
};

/*
 * struct pmic_wait_queue
 * @unmask:	unmask bit - A device driver can select an external device which
 *                    wants to detect the connection.For example, You should
 *                    write a code as follows if you want to detect Stereo(+MIC)
 *                    or Monoral(+MIC) device connection.
 *
 *                    queue->unmask = E_DEVICE_STEREO | E_DEVICE_MONORAL;
 *
 * @list:	list_head for doubly-linked list
 * @work:	entry of workqueue
 * @work_queue:	pointer to the workqueue
 */
struct pmic_wait_queue {
	unsigned int unmask;
	struct list_head list;
	struct work_struct work;
	struct workqueue_struct *work_queue;
};

/*
 * struct usb_otg_pmic_device_ops
 *
 * For these USB OTG PMIC methods the device parameter is the physical device
 * on whatever bus holds the hardware (I2C, Platform, SPI, etc), which
 * was passed to usb_otg_pmic_device_register().
 */
struct usb_otg_pmic_device_ops {
	int (*set_vbus) (struct device *dev, int enable);
};


/*
 * struct pmic_battery_ops
 *
 * For these PMIC battery methods the device parameter is the physical device
 * on whatever bus holds the hardware (I2C, Platform, SPI, etc), which
 * was passed to pmic_battery_device_register().
 */
struct pmic_battery_ops {
	int (*get_usb_online)(struct device *dev);
	int (*get_bat_status)(struct device *dev);
	int (*get_bat_health)(struct device *dev);
	int (*get_bat_present)(struct device *dev);
	int (*get_bat_technology)(struct device *dev);
	int (*get_bat_capacity)(struct device *dev);
	int (*get_bat_capacity_level)(struct device *dev);
	int (*get_bat_temperature)(struct device *dev);
	int (*get_hpa_temperature)(struct device *dev);
	int (*get_bat_voltage)(struct device *dev);
	int (*get_bat_time_to_empty)(struct device *dev);
	int (*get_bat_time_to_full)(struct device *dev);
	int (*stop_charging)(struct device *dev, int stop);
};

/*
 * struct battery_correct_ops
 *
 * For these PMIC battery correct methods the device parameter is
 * the physical device
 * on whatever bus holds the hardware (I2C, Platform, SPI, etc), which
 * was passed to pmic_battery_register_correct_func().
 */
struct battery_correct_ops {
	int (*correct_capacity_func)(int cap);
	int (*correct_temp_func)(int temp);
	int (*correct_status_func)(int status);
	int (*correct_voltage_func)(int vol);
	int (*correct_time_to_empty_func)(int tte);
	int (*correct_time_to_full_func)(int ttf);
};

extern int pmic_device_register(struct device *dev,
		struct pmic_device_ops *ops);
extern int pmic_device_unregister(struct device *dev);
extern int pmic_battery_device_register(struct device *dev,
		struct pmic_battery_ops *ops);
extern int pmic_battery_device_unregister(struct device *dev);
extern void pmic_add_wait_queue(struct pmic_wait_queue *queue);
extern void pmic_ext_device_changed(int device);
extern int pmic_get_ext_device(void);
extern void pmic_power_supply_changed(int status);
extern int pmic_get_temp_status(void);
extern int pmic_set_power_on(int resource);
extern int pmic_set_power_off(int resource);
extern void pmic_force_power_off(int resource);
extern int pmic_set_voltage(int resource, int voltage);
extern int pmic_set_power_mode(int resource, int pstate, int pmode);
extern int pmic_battery_register_correct_func(struct battery_correct_ops *ops);
extern void pmic_battery_unregister_correct_func(void);
extern int pmic_set_vbus(int enable);
extern int usb_otg_pmic_device_register(struct device *dev,
		struct usb_otg_pmic_device_ops *ops);
extern int usb_otg_pmic_device_unregister(struct device *dev);
extern int pmic_read_register(int slave, u8 addr);
extern int pmic_set_current_limit(int chrg_state, int chrg_type);
extern int pmic_clk32k_enable(u8 clk_res, u8 state);
extern int pmic_read(struct device *dev, u8 addr, uint8_t *val);
extern int pmic_reads(struct device *dev, u8 addr, int len, u8* val);
extern int pmic_write(struct device *dev, u8 addr, u8 val);
extern int pmic_writes(struct device *dev, u8 addr, int len, u8* val);
extern void tps80032_handle_modem_reset(void);
#ifdef PMIC_PT_TEST_ENABLE
int pmic_read_battery_status(int property);
#endif

#endif
