/*
 * arch/arm/mach-shmobile/powerdomain.c
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corporation
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
 */

/*
 * About terminology in this file:
 * The word "power domain" is used to point "power area(s)" in SYSC module.
 * It is inherited from history.
 */

#ifndef POWER_DOMAIN_H
#define POWER_DOMAIN_H

#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <mach/pm.h>
#include <mach/common.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/atomic.h>
#include <linux/export.h>

#endif /*POWER_DOMAIN_H*/

/* #define __DEBUG_PDC */
/* #define __DEBUG_PDWAIT */

/******************************************************************************
 * SYSC accessor
 ******************************************************************************/
#define C4_POWER_DOWN_SEL_ALL		0x0000001F
#define PSTR_POLLING_INTERVAL_US	10
#define PSTR_POLLING_COUNT_MAX		50

#define HIGH_THRESHOLD	(1000000)
#define DELAYED_TIME_IN_US	(200*1000)
#define CPU0_ID				0
struct workqueue_struct *pdwait_wq;
static DECLARE_DEFERRED_WORK(pdwait_work, NULL);
static atomic_t	pdwait_judge_count;
static bool a2ri_status_old ;
static bool a2ri_status_new;
static bool suspend_state;
static void __iomem *sbsc_sdpdcr0a_reg;
static DEFINE_SPINLOCK(pdwait_lock);

/******************************************************************************
 * Declaration
 ******************************************************************************/

struct drv_pd_mapping_table {
		char *name;			/* Driver name  */
		unsigned int area;	/* Power area id*/
};

struct pdi_mapping_table {
	char *name;			/* Driver name  */
	struct power_domain_info pdi;
};

static DEFINE_MUTEX(power_status_mutex);
static u64 a3sp_power_down_count;
static unsigned int default_c4_pdsel;
#ifdef CONFIG_PM_RUNTIME_A4RM
static int power_a4rm_mask;
#endif
static int chip_rev = ES_REV_2_0;

static DEFINE_SPINLOCK(pdc_lock);
#ifdef CONFIG_PM_DEBUG
static int pdc_enable = 1;
static int old_pdc_enable = 1;
static int power_areas_status;
#endif


static struct drv_pd_mapping_table tbl2[] = {
	/* MFIS		 */	{ "mfis.0",			ID_C4	},
	/* SGX544MP1 */	{ "pvrsrvkm",			ID_A3SG },
	/* SY-DMA0	*/	{ "sh-dma-engine.0",	ID_A3SP },
	/* CC4.2 0	*/	{ "sep_sec_driver.0",	ID_A3SP },
	/* MMCIF.0	*/	{ "renesas_mmcif.0",		ID_A3SP },
	/* MSIOF0	*/	{ "spi_sh_msiof.0",	ID_A3SP },
	/* MSIOF1	*/	{ "spi_sh_msiof.1",	ID_A3SP },
	/* MSIOF2	*/	{ "spi_sh_msiof.2",	ID_A3SP },
	/* MSIOF3	*/	{ "spi_sh_msiof.3",	ID_A3SP },
	/* MSIOF4	*/	{ "spi_sh_msiof.4",	ID_A3SP },
	/* USB		*/	{ "r8a66597_hcd.0",	ID_A3SP },
	/* USB		*/	{ "r8a66597_udc.0",	ID_A3SP },
	/* USB		*/	{ "usb_mass_storage",	ID_A3SP },
	/* USB		*/	{ "android_usb",		ID_A3SP },
	/* USB_OTG	*/	{ "tusb1211_driver.0",	ID_A3SP },
	/* SCIFA0	*/	{ "sh-sci.0",			ID_C4	},
	/* SCIFA1	*/	{ "sh-sci.1",			ID_A3SP },
	/* SCIFA2	*/	{ "sh-sci.2",			ID_A3SP },
	/* SCIFA3	*/	{ "sh-sci.3",			ID_A3SP },
	/* SCIFB0	*/	{ "sh-sci.4",			ID_A3SP },
	/* SCIFB1	*/	{ "sh-sci.5",			ID_A3SP },
	/* SCIFB2	*/	{ "sh-sci.6",			ID_A3SP },
	/* SCIFB3	*/	{ "sh-sci.7",			ID_C4	},
	/* I2C0		*/	{ "i2c-sh_mobile.0",	ID_A3SP },
	/* I2C1		*/	{ "i2c-sh_mobile.1",	ID_A3SP },
	/* I2C2		*/	{ "i2c-sh_mobile.2",	ID_A3SP },
	/* I2C3		*/	{ "i2c-sh_mobile.3",	ID_A3SP },
	/* I2C0H	*/	{ "i2c-sh_mobile.4",	ID_A3SP },
	/* I2C1H	*/	{ "i2c-sh_mobile.5",	ID_A3SP },
    /* I2CM		*/	{ "i2c-sh7730.8",		ID_A3SP },
	/* I2C2H	*/	{ "i2c-sh_mobile.6",	ID_A3SP },
	/* I2C3H	*/	{ "i2c-sh_mobile.7",	ID_A3SP },
	/* SDHI0	*/	{ "renesas_sdhi.0",	ID_A3SP },
	/* SDHI1	*/	{ "renesas_sdhi.1",	ID_A3SP },
	/* SDHI2	*/	{ "renesas_sdhi.2",	ID_A3SP },
	/* TPU		*/	{ "tpu-renesas-sh_mobile.0",	ID_A3SP },
	/* HSI		*/	{ "sh_hsi.0",			ID_A3SP },
	/* MFI		*/	{ "av-domain",		ID_A3R	},
	/* FSI2/ALSA	*/	{ "snd-soc-fsi",	ID_A4MP },
	/* SPUV/VOCODER	*/	{ "vcd",			ID_A4MP },
	/* PCM2PWM	*/	{ "pcm2pwm-renesas-sh_mobile.1", ID_A4MP },
	/* SHX(rtapi) */{ "meram-domain",	ID_A4RM },
	/* AudioPT	*/	{ "snd-soc-audio-test",	ID_A4MP },

#if 0
	/* The following device is used for test purpose only */
	/*	C4 dummy device	*/		{ "dummy_test_c4.0",	 ID_C4},
	/*	A3SG dummy device	*/	{ "dummy_test_a3sg.0", ID_A3SG},
	/*	A3SP dummy device	*/	{ "dummy_test_a3sp.0", ID_A3SP},
	/*	A3R dummy device	*/	{ "dummy_test_a3r.0", ID_A3R},
	/*	A4RM dummy device	*/	{ "dummy_test_a4rm.0", ID_A4RM},
	/*	A4MP dummy device	*/	{ "dummy_test_a4mp.0", ID_A4MP},
#endif

};

#define pdi_init_val {{NULL, NULL, NULL}, 0}
static struct pdi_mapping_table pdi_tb2[] = {
	/* MFIS		*/ { "mfis.0", pdi_init_val},
	/* SGX544MP1*/ { "pvrsrvkm", pdi_init_val},
	/* SY-DMA0	*/ { "sh-dma-engine.0", pdi_init_val},
	/* CC4.2 0	*/ { "sep_sec_driver.0", pdi_init_val},
	/* MMCIF.0	*/ { "renesas_mmcif.0", pdi_init_val},
	/* MSIOF0	*/ { "spi_sh_msiof.0", pdi_init_val},
	/* MSIOF1	*/ { "spi_sh_msiof.1", pdi_init_val},
	/* MSIOF2	*/ { "spi_sh_msiof.2", pdi_init_val},
	/* MSIOF3	*/ { "spi_sh_msiof.3", pdi_init_val},
	/* MSIOF4	*/ { "spi_sh_msiof.4", pdi_init_val},
	/* USB		*/ { "r8a66597_hcd.0", pdi_init_val},
	/* USB		*/ { "r8a66597_udc.0", pdi_init_val},
	/* USB		*/ { "usb_mass_storage", pdi_init_val},
	/* USB		*/ { "android_usb", pdi_init_val},
	/* USB_OTG	*/ { "tusb1211_driver.0", pdi_init_val},
	/* SCIFA0	*/ { "sh-sci.0", pdi_init_val},
	/* SCIFA1	*/ { "sh-sci.1", pdi_init_val},
	/* SCIFA2	*/ { "sh-sci.2", pdi_init_val},
	/* SCIFA3	*/ { "sh-sci.3", pdi_init_val},
	/* SCIFB0	*/ { "sh-sci.4", pdi_init_val},
	/* SCIFB1	*/ { "sh-sci.5", pdi_init_val},
	/* SCIFB2	*/ { "sh-sci.6", pdi_init_val},
	/* SCIFB3	*/ { "sh-sci.7", pdi_init_val},
	/* I2C0		*/ { "i2c-sh_mobile.0", pdi_init_val},
	/* I2C1		*/ { "i2c-sh_mobile.1", pdi_init_val},
	/* I2C2		*/ { "i2c-sh_mobile.2", pdi_init_val},
	/* I2C3		*/ { "i2c-sh_mobile.3", pdi_init_val},
	/* I2C0H	*/ { "i2c-sh_mobile.4", pdi_init_val},
	/* I2C1H	*/ { "i2c-sh_mobile.5", pdi_init_val},
    /* I2CM		*/ { "i2c-sh7730.8", pdi_init_val},
	/* I2C2H	*/ { "i2c-sh_mobile.6", pdi_init_val},
	/* I2C3H	*/ { "i2c-sh_mobile.7", pdi_init_val},
	/* SDHI0	*/ { "renesas_sdhi.0", pdi_init_val},
	/* SDHI1	*/ { "renesas_sdhi.1", pdi_init_val},
	/* SDHI2	*/ { "renesas_sdhi.2", pdi_init_val},
	/* TPU		*/ { "tpu-renesas-sh_mobile.0", pdi_init_val},
	/* HSI		*/ { "sh_hsi.0", pdi_init_val},
	/* MFI		*/ { "av-domain", pdi_init_val},
	/* FSI2/ALSA*/ { "snd-soc-fsi", pdi_init_val},
	/* SPUV/VOCODER*/ { "vcd", pdi_init_val},
	/* PCM2PWM	*/ { "pcm2pwm-renesas-sh_mobile.1", pdi_init_val},
	/* SHX(rtapi) */ { "meram-domain", pdi_init_val},
	/* AudioPT */ { "snd-soc-audio-test", pdi_init_val },


#if 0
	/* The following device is used for test purpose only */
	/*	C4 dummy device */ { "dummy_test_c4.0", pdi_init_val},
	/*	A3SG dummy device */ { "dummy_test_a3sg.0", pdi_init_val},
	/*	A3SP dummy device */ { "dummy_test_a3sp.0", pdi_init_val},
	/*	A3R dummy device */ { "dummy_test_a3r.0", pdi_init_val},
	/*	A4RM dummy device */ { "dummy_test_a4rm.0", pdi_init_val},
	/*	A4MP dummy device */ { "dummy_test_a4mp.0", pdi_init_val},
#endif

};

struct power_domain_info *get_pdi(const char *name)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(pdi_tb2); i++) {
		if (0 == strcmp(name, pdi_tb2[i].name))
			return &(pdi_tb2[i].pdi);
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(get_pdi);

#define POWER_DOMAIN_DEVICE(_pd_dev, _pwr_id, _parent_dev) \
	static struct platform_device _pd_dev = { \
		.name = "power-domain", \
		.id = _pwr_id, \
		.dev.parent = _parent_dev, \
		.dev.power.pdi = NULL, \
	}

/* Define power domain(area) devices (pointer) */
POWER_DOMAIN_DEVICE(a4rm_device, ID_A4RM, NULL);
POWER_DOMAIN_DEVICE(a4mp_device, ID_A4MP, NULL);
/* POWER_DOMAIN_DEVICE(a3r_device,  ID_A3R, &a4rm_device.dev); */
POWER_DOMAIN_DEVICE(a3r_device,  ID_A3R, NULL);
POWER_DOMAIN_DEVICE(a3sg_device, ID_A3SG, NULL);
POWER_DOMAIN_DEVICE(a3sp_device, ID_A3SP, NULL);

/* Function declaration */
static inline void sort_mapping_table(
	struct drv_pd_mapping_table *drv_pd_mp_tbl, int arr_size);
static int get_power_area_index(struct drv_pd_mapping_table *drv_pd_mp_tbl,
					int arr_size, const char *drv_name);
static void power_status_set(unsigned int area, bool on);
static int power_domain_driver_suspend(struct device *dev);
static int power_domain_driver_resume(struct device *dev);
static int power_domain_driver_runtime_suspend(struct device *dev);
static int power_domain_driver_runtime_resume(struct device *dev);
static int power_domain_driver_probe(struct device *dev);
static int power_domain_driver_remove(struct device *dev);
static unsigned int c4_power_down_sel(void);
static int c4_power_driver_runtime_resume(struct device *dev);
static int c4_power_driver_runtime_suspend(struct device *dev);
static int c4_power_domain_driver_probe(struct device *dev);
static int power_domain_driver_init(void);
static void set_c4_power_down_sel(unsigned int);
static bool is_power_status_on(unsigned int);
#ifdef __DEBUG_PDC
static void power_areas_info(void);
#endif /* __DEBUG_PDC */

/******************************************************************************
 * Common
 *****************************************************************************/
#ifdef __DEBUG_PDC
static void power_areas_info(void)
{
	u32 reg_val;
	reg_val = __raw_readl(PSTR);
	printk(KERN_INFO "[PDC] PSTR(0x%08x) = 0x%08x\n",
			PSTR, reg_val);
	printk(KERN_INFO "[PDC] A3SG = %s\n",
			(POWER_A3SG & reg_val) ? "ON" : "OFF");
	printk(KERN_INFO "[PDC] A3SP = %s\n",
			(POWER_A3SP & reg_val) ? "ON" : "OFF");
	printk(KERN_INFO "[PDC] A3R  = %s\n",
			(POWER_A3R & reg_val) ? "ON" : "OFF");
	printk(KERN_INFO "[PDC] A4RM = %s\n",
			(POWER_A4RM & reg_val) ? "ON" : "OFF");
	printk(KERN_INFO "[PDC] A4MP = %s\n",
			(POWER_A4MP & reg_val) ? "ON" : "OFF");
}
#endif /* __DEBUG_PDC */

 /*
 * sort_mapping_table:sort mapping table(of user drivers and power domain(area))
 * It is based on order of driver name
 * @drv_pd_mp_tbl: mapping table of drivers and power domains(areas).
 *				It is also output after sorting
 * @arr_size: size of mapping table (drv_pd_mp_tbl array)
 * This is selection sort.
 */
inline void sort_mapping_table(struct drv_pd_mapping_table *drv_pd_mp_tbl,
						int arr_size)
{
	int i;
	int j;
	int largest_pos;
	struct drv_pd_mapping_table temp;

	for (i = arr_size - 1; i > 0; i--) {
		largest_pos = i;
		for (j = 0; j < i; j++) {
			if (strcmp(drv_pd_mp_tbl[j].name,
				drv_pd_mp_tbl[largest_pos].name) > 0) {
					largest_pos = j;
			}
		}
		/*Swap largest value for i*/
		if (largest_pos != i) {
			temp.name = drv_pd_mp_tbl[largest_pos].name;
			temp.area = drv_pd_mp_tbl[largest_pos].area;

			drv_pd_mp_tbl[largest_pos].name = drv_pd_mp_tbl[i].name;
			drv_pd_mp_tbl[largest_pos].area = drv_pd_mp_tbl[i].area;

			drv_pd_mp_tbl[i].name = temp.name;
			drv_pd_mp_tbl[i].area = temp.area;
		}
	}
}

/*
 * get_power_area_index: search driver name in mapping table of drivers
 * and power domains(areas) (binary search)
 * @drv_pd_mp_tbl: mapping table of drivers and power domains(areas)
 * @arr_size: size of mapping table (drv_pd_mp_tbl array)
 * @drv_name: name of driver that need to be searched
 * return:
 *	> 0: index of an element in mapping table that contain driver name
 *	-1 : there is not driver name in mapping table
 */
int get_power_area_index(struct drv_pd_mapping_table *drv_pd_mp_tbl,
			int arr_size, const char *drv_name)
{
	int lower_boundary = 0;	/* lower boundary index */
	int upper_boundary = arr_size - 1;	/* upper boundary index */
	int mid;
	int temp_comp;

	do {
		mid = (lower_boundary + upper_boundary)/2;
		temp_comp = strcmp(drv_name, drv_pd_mp_tbl[mid].name);
		if (0 == temp_comp)
			return mid;
		else if (temp_comp < 0)
			upper_boundary = mid - 1;
		else
			lower_boundary = mid + 1;
	} while (lower_boundary <= upper_boundary);

	return -1;
}

/*
 * is_power_status_on: check a certain power area is on or off
 * @area: power area that need to check. Its values compose of {POWER_A2SL,...,
 *		POWER_D4}. If other than above values, it will raise panic.
 * return:
 *		true: power area is being turned on
 *		false: power area is being turned off
 */
static bool is_power_status_on(unsigned int area)
{
	if (0 != (area & ~POWER_ALL))
		panic("power status invalid argument: 0%08x", area);
	return ((__raw_readl(PSTR) & area) == area);
}

/*
 * __to_pdi: get the power domain information of a certain device
 * @dev: certain device
 * return: address of power.pdi
 */
struct power_domain_info *__to_pdi(const struct device *dev)
{
	return dev ? dev->power.pdi : NULL;
}

/*
 * power_status_set: turn on or turn off a power area
 * @area: power area that need to set(turn on/off). Its values compose of
 * {POWER_A2SL, ..., POWER_D4}. If other than above values, it will raise panic.
 * @on: power status need to be set
 *		true: turn on power area
 *		false: turn off power area
 */
static void power_status_set(unsigned int area, bool on)
{
	int i = 0;
	void __iomem *reg = (on ? SWUCR : SPDCR);
	int reg_val = 0;

	if (0 != (area & ~POWER_ALL))
		panic("power status invalid argument: 0%08x", area);


	if (!is_power_status_on(area) == !on)
		return;

	/* Dummy read register (SWUCR, SPDCR) to wait all bits is 0 */
	reg_val = __raw_readl(reg);
	while (0 != reg_val)
		reg_val = __raw_readl(reg);

	__raw_writel(area, reg);

	for (i = 0; i < PSTR_POLLING_COUNT_MAX; i++) {
		udelay(PSTR_POLLING_INTERVAL_US);
		if (!is_power_status_on(area) == !on) {
			return;
		} else if (false == on) { /* Set SPDCR each time repeat */
			reg_val = __raw_readl(reg);
			while (0 != reg_val)
				reg_val = __raw_readl(reg);

			__raw_writel(area, reg);
		}
	}

	panic("power status error (area:0x%08x on:%d PSTR:0x%08x)",
					area, on, __raw_readl(PSTR));

}


/******************************************************************************
 * Power domain driver
 *****************************************************************************/
/* Common power domain(area) driver for supported areas (other C4 than area) */

/*
 * power_domain_driver_suspend: implement for ->suspend_noirq()
 * callback function of power domain(area) driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int power_domain_driver_suspend(struct device *dev)
{
	if (0 == suspend_state) {
		cancel_delayed_work_sync(&pdwait_work);
		suspend_state = 1;
	}
	return 0;
}

 /* Common power domain(area) driver for supported areas (other C4 than area) */

/*
 * power_domain_driver_resume: implement for ->resume_noirq()
 * callback function of power domain(area) driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int power_domain_driver_resume(struct device *dev)
{
	int delay = 0;
	int r = pm_runtime_resume(dev);

	if (0 < r)
		(void)pr_notice("%s resume notice: %d\n", dev_name(dev), r);

	delay = usecs_to_jiffies(DELAYED_TIME_IN_US);
	if (1 == suspend_state) {
		queue_delayed_work_on(CPU0_ID, pdwait_wq, &pdwait_work, delay);
		suspend_state = 0;
	}
	return 0;
}

/*
 * power_domain_driver_runtime_suspend: implement for ->runtime_suspend()
 * callback function of power domain(area) driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int power_domain_driver_runtime_suspend(struct device *dev)
{
	unsigned int area = 1 << (to_platform_device(dev)->id);
	unsigned int mask = 0;

#ifndef CONFIG_PM_RUNTIME_A3SG
	mask |= POWER_A3SG;
#endif

#ifndef CONFIG_PM_RUNTIME_A3SP
	mask |= POWER_A3SP;
#endif

#ifndef CONFIG_PM_RUNTIME_A3R
	mask |= POWER_A3R;
#endif

#ifndef CONFIG_PM_RUNTIME_A4RM
	mask |= POWER_A4RM;
#else
	if (POWER_A4RM == area) {
		mask |= power_a4rm_mask;
		power_a4rm_mask = 0;
	}
#endif

#ifndef CONFIG_PM_RUNTIME_A4MP
	mask |= POWER_A4MP;
#endif

	if (0 != (area & mask)) {
		pm_runtime_get_noresume(dev);
		return 0;
	}

	spin_lock(&pdc_lock);
#ifdef CONFIG_PM_DEBUG
	power_areas_status &= (~area);
	if (0 == pdc_enable) {
#ifdef __DEBUG_PDC
		power_areas_info();
#endif /* __DEBUG_PDC */
		spin_unlock(&pdc_lock);
		return 0;
	}
#endif

#ifdef CONFIG_PM_HAS_SECURE
	if (POWER_A3SP == area) {
#if 1
		(void)sec_hal_pm_a3sp_state_request(false);
#else
		int ret = sec_hal_pm_a3sp_state_request(false);
		printk(KERN_INFO "[PDC] sec_hal_pm_a3sp_state_request = %x\n",
				ret);
			/* return 0 */
#endif
		a3sp_power_down_count++;
	} else {
		power_status_set(area, false);
	}
#else
	power_status_set(area, false);

	if (POWER_A3SP == area)
		a3sp_power_down_count++;
#endif /* CONFIG_PM_HAS_SECURE */

#ifdef __DEBUG_PDC
	power_areas_info();
#endif /* __DEBUG_PDC */

	spin_unlock(&pdc_lock);

	if (POWER_A3SG == area) {
		atomic_dec(&pdwait_judge_count);
#ifdef __DEBUG_PDWAIT
		printk(KERN_INFO "[PDC] A3SG = OFF\n");
		printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
				atomic_read(&pdwait_judge_count));
#endif
		pdwait_judge();
	}

	if (POWER_A3R == area) {
		pm_runtime_put_noidle(&a4rm_device.dev);
		pm_runtime_suspend(&a4rm_device.dev);
	}

	return 0;
}

/*
 * power_domain_driver_runtime_resume: implement for ->runtime_resume()
 * callback function of power domain(area) driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int power_domain_driver_runtime_resume(struct device *dev)
{
	unsigned int area = 1 << (to_platform_device(dev)->id);
	if (POWER_A3R == area)
		pm_runtime_get_sync(&a4rm_device.dev);

	spin_lock(&pdc_lock);
#ifdef CONFIG_PM_DEBUG
	power_areas_status |= area;
	if (0 == pdc_enable) {
#ifdef __DEBUG_PDC
		power_areas_info();
#endif /* __DEBUG_PDC */
		spin_unlock(&pdc_lock);
		return 0;
	}
#endif

#ifdef CONFIG_PM_HAS_SECURE
	if (POWER_A3SP == area) {
#if 1
		(void)sec_hal_pm_a3sp_state_request(true);
#else
		int ret = sec_hal_pm_a3sp_state_request(true);
		printk(KERN_INFO "[PDC] sec_hal_pm_a3sp_state_request = %x\n",
				ret);
			/* return 0 */
#endif
	} else {
		power_status_set(area, true);
	}
#else
	power_status_set(area, true);
#endif /* CONFIG_PM_HAS_SECURE */

#ifdef __DEBUG_PDC
	power_areas_info();
#endif /* __DEBUG_PDC */

	spin_unlock(&pdc_lock);

	if (POWER_A3SG == area) {
		atomic_inc(&pdwait_judge_count);
#ifdef __DEBUG_PDWAIT
		printk(KERN_INFO "[PDC] A3SG = ON\n");
		printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
				atomic_read(&pdwait_judge_count));
#endif
		pdwait_judge();
	}

	return 0;
}

/*
 * power_domain_driver_probe: implement for ->probe() callback function
 * of power domain(area) driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int power_domain_driver_probe(struct device *dev)
{
	if (false != is_power_status_on(1 << to_platform_device(dev)->id))
		(void)pm_runtime_set_active(dev);

	pm_runtime_enable(dev);
	pm_runtime_irq_safe(dev);
	return 0;
}

/*
 * power_domain_driver_remove: implement for ->remove() callback function
 * of power domain(area) driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int power_domain_driver_remove(struct device *dev)
{
	pm_runtime_disable(dev);
	return 0;
}

/*
 * power_domain_driver_pm_ops: pm field of power domain(area) driver
 */

static const struct dev_pm_ops power_domain_driver_pm_ops = {
	.suspend_noirq		= &power_domain_driver_suspend,
	.resume_noirq		= &power_domain_driver_resume,
	.runtime_suspend	= &power_domain_driver_runtime_suspend,
	.runtime_resume		= &power_domain_driver_runtime_resume
};


/*
 * power_domain: pm pwr_domain field of power domain(area) devices
 */
/*
static struct dev_power_domain power_domain = {
	.ops.resume_noirq		= &power_domain_driver_resume,
	.ops.runtime_suspend	= &power_domain_driver_runtime_suspend,
	.ops.runtime_resume		= &power_domain_driver_runtime_resume
};
 */

/*
 * pwr_driver: power domain(area) driver
 */
static struct platform_driver pwr_driver = {
	.driver = {
		.name = "power-domain",
		.pm = &power_domain_driver_pm_ops,
		.probe = &power_domain_driver_probe,
		.remove = &power_domain_driver_remove
	}
};

/*****************************************************************************/
/* C4 area driver */
/*
 * c4_power_down_sel: get value of C4PD[4:0] bits
 * (Power down selection bits) in PDNSEL register
 */
static unsigned int c4_power_down_sel(void)
{
	return __raw_readl(PDNSEL);
}

/*
 * set_c4_power_down_sel: set new value into C4PD[4:0] bits
 * (Power down selection bits) in PDNSEL register
 * @condition: value to set
 */
static void set_c4_power_down_sel(unsigned int condition)
{
	if (0 != (condition & ~C4_POWER_DOWN_SEL_ALL))
		panic("C4 power down condition invalid argument: 0%08x",
				condition);
	__raw_writel(condition, PDNSEL);
}

/*
 * c4_power_driver_runtime_suspend: implement for ->runtime_resume()
 * callback function of C4 area driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int c4_power_driver_runtime_resume(struct device *dev)
{
	set_c4_power_down_sel(0);
	return 0;
}

/*
 * c4_power_driver_runtime_suspend: implement for ->runtime_suspend()
 * callback function of C4 area driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int c4_power_driver_runtime_suspend(struct device *dev)
{
	set_c4_power_down_sel(default_c4_pdsel);
	return 0;
}

/*
 * c4_power_domain_driver_probe: implement for ->probe() callback function
 *							of C4 area driver
 * @dev: device of power domain(area)
 * return: always return 0 (because it is template of callback function)
 */
static int c4_power_domain_driver_probe(struct device *dev)
{
	default_c4_pdsel = c4_power_down_sel();
	(void)c4_power_driver_runtime_suspend(dev);
	pm_runtime_enable(dev);
	pm_runtime_irq_safe(dev);
	return 0;
}


static const struct dev_pm_ops c4_power_driver_pm_ops = {
	.runtime_suspend = &c4_power_driver_runtime_suspend,
	.runtime_resume = &c4_power_driver_runtime_resume
};


/*
 * c4_power_driver: C4 area driver
 */
static struct platform_driver c4_power_driver = {
	.driver = {
		.name	= "power-domain-C4",
		.pm		= &c4_power_driver_pm_ops,
		.probe	= &c4_power_domain_driver_probe,
		.remove	= &power_domain_driver_remove
	}
};

/*
 * c4_power_domain: pm pwr_domain field of C4 power domain(area) device
 */
 /*
static struct dev_power_domain c4_power_domain = {
	.ops.runtime_suspend = &c4_power_driver_runtime_suspend,
	.ops.runtime_resume = &c4_power_driver_runtime_resume
};
 */

/* Define power domain(area) devices (pointer) for C4 */
static struct platform_device c4_device = {
	.name = "power-domain-C4",
	.id = ID_C4,
};


/*************************************************************************/
static struct platform_device *pd[] = {
	NULL,	/* appropriate with bit 0 in SWUCR, SPDCR, PSTR reg */
	NULL,	/* bit 1 */
	NULL,	/* bit 2 */
	NULL,	/* bit 3 */
	NULL,	/* bit 4 */
	NULL,	/* bit 5 */
	NULL,	/* bit 6 */
	NULL,	/* bit 7 */
	&a4mp_device,	/* bit 8 (ID_A4MP)*/
	NULL,	/* bit 9 */
	NULL,	/* bit 10 */
	NULL,	/* bit 11 */
	&a4rm_device,	/* bit 12 (ID_A4RM)*/
	&a3r_device,	/* bit 13 (ID_A3R)*/
	NULL,	/* bit 14 */
	NULL,	/* bit 15 */
	&c4_device,		/* bit 16 (ID_C4)*/
	&a3sp_device,	/* bit 17 (ID_A3SP)*/
	&a3sg_device	/* bit 18 (ID_A3SG)*/
};

void pdwait_judge()
{

	u32 reg_val = 0;
	u32 mask = 0x00FFFFFF;
	u32 pdwait = 0x08000000;
	unsigned long flags;

	int current_pdwait_flag = 0;
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[PDC] pdwait_judge()\n");
#endif

	if (NULL != sbsc_sdpdcr0a_reg) {
		spin_lock_irqsave(&pdwait_lock, flags);

		current_pdwait_flag = atomic_read(&pdwait_flag);
		reg_val = __raw_readl(sbsc_sdpdcr0a_reg);
		if ((atomic_read(&pdwait_judge_count) > 0)
				|| (current_pdwait_flag > 0))
			pdwait = 0xFF000000;

		if (pdwait != (reg_val & ~mask))
			__raw_writel(pdwait | (reg_val & mask), \
						sbsc_sdpdcr0a_reg);

		spin_unlock_irqrestore(&pdwait_lock, flags);
	}
#ifdef __DEBUG_PDWAIT
	printk(KERN_INFO "[pdc] pdwait_flag = %d\n", current_pdwait_flag);

	printk(KERN_INFO "[PDC] SDPDCR0A current pdwait = 0x%08x\n", \
			reg_val);
	if (pdwait != (reg_val & ~mask))
		printk(KERN_INFO "[PDC] new pdwait = 0x%08x\n", pdwait);
	printk(KERN_INFO "\n");
#endif
}
EXPORT_SYMBOL_GPL(pdwait_judge);

int pdc_cpufreq_transition(struct notifier_block *block, unsigned long state,
								void *data)
{
	if (state == CPUFREQ_POSTCHANGE) {
		struct cpufreq_freqs *freqs = data;
		if (freqs && (freqs->cpu == CPU0_ID)) {
			if ((freqs->new > HIGH_THRESHOLD)
					&& (freqs->old < HIGH_THRESHOLD)) {
				/* frequency change
				upper HIGH_THRESHOLD = 1 GHz */
				atomic_inc(&pdwait_judge_count);
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[PDC] Z clock goes > 1 GHz\n");
printk(KERN_INFO "[PDC] freqs->old = %d\n", freqs->old);
printk(KERN_INFO "[PDC] freqs->new = %d\n", freqs->new);
printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
		atomic_read(&pdwait_judge_count));
#endif
			} else if ((freqs->new < HIGH_THRESHOLD)
					&& (freqs->old > HIGH_THRESHOLD)) {
				/* frequency change
				under HIGH_THRESHOLD = 1 GHz */
				atomic_dec(&pdwait_judge_count);
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[PDC] Z clock goes < 1 GHz\n");
printk(KERN_INFO "[PDC] freqs->old = %d\n", freqs->old);
printk(KERN_INFO "[PDC] freqs->new = %d\n", freqs->new);
printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
		atomic_read(&pdwait_judge_count));
#endif
			}

			pdwait_judge();
		}
	}

	return NOTIFY_OK;
}

static void pdwait_work_fnc(struct work_struct *work)
{
	int delay = usecs_to_jiffies(DELAYED_TIME_IN_US);
	u32 pstr_val;
#ifdef __DEBUG_PDWAIT
printk(KERN_INFO "[PDC] workqueue function\n");
#endif

	a2ri_status_old = a2ri_status_new;
	pstr_val = __raw_readl(PSTR);

	a2ri_status_new = ((pstr_val & POWER_A2RI) == POWER_A2RI);
	if (a2ri_status_new && (a2ri_status_new != a2ri_status_old)) {
		/* A2RI change OFF -> ON */
		atomic_inc(&pdwait_judge_count);
#ifdef __DEBUG_PDWAIT
		printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
				atomic_read(&pdwait_judge_count));
		printk(KERN_INFO "[PDC] A2RI change from %s -> %s\n",
			a2ri_status_old ? "ON" : "OFF",
			a2ri_status_new ? "ON" : "OFF");
#endif
	} else if (!a2ri_status_new && (a2ri_status_new != a2ri_status_old)) {
		/* A2RI change ON -> OFF */
		atomic_dec(&pdwait_judge_count);
#ifdef __DEBUG_PDWAIT
		printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
				atomic_read(&pdwait_judge_count));
		printk(KERN_INFO "[PDC] A2RI change from %s -> %s\n",
				a2ri_status_old ? "ON" : "OFF",
				a2ri_status_new ? "ON" : "OFF");
#endif
	}

#ifdef __DEBUG_PDWAIT
	printk(KERN_INFO "[PDC] pdwait_judge_count = %d\n",
			atomic_read(&pdwait_judge_count));
#endif
	pdwait_judge();
	queue_delayed_work_on(CPU0_ID, pdwait_wq, &pdwait_work, delay);
}

static struct notifier_block pdc_cpufreq_nb = {
	.notifier_call = pdc_cpufreq_transition,
};

/*
 * power_domain_driver_init: initial function of power domain(area) driver
 * return:	0 all device is registered successfully
 * # 0 there is at least one device is registered unsuccessfully
 */
static int __init power_domain_driver_init(void)
{
	int ret = 0;
	int i;
	int j;
	int delay = 0;

#ifdef CONFIG_PM_DEBUG
	pdc_enable = 1;
	old_pdc_enable = 1;
	power_areas_status = POWER_A3R | POWER_A4RM | POWER_A4MP
					| POWER_A3SP | POWER_A3SG;
#endif

	chip_rev = shmobile_chip_rev();

	sbsc_sdpdcr0a_reg = NULL;

#ifdef CONFIG_PM_RUNTIME_A4RM
power_a4rm_mask = POWER_A4RM;
#endif

	ret = cpufreq_register_notifier(&pdc_cpufreq_nb,
			CPUFREQ_TRANSITION_NOTIFIER);
	if (0 != ret)
		return ret;

	/* Z clock > 1GHz, A3SG, A2RI, A2RV ON at boot time */
	atomic_set(&pdwait_judge_count, 3);
	a2ri_status_old = 1;
	a2ri_status_new = 1;
	suspend_state	= 0;
	sbsc_sdpdcr0a_reg = ioremap(SBSC_SDPDCR0A_PHYS, PAGE_SIZE);
	if (NULL == sbsc_sdpdcr0a_reg)
		printk(KERN_INFO "[PDC] error can't map SBSC_SDPDCR0A\n");

	pdwait_wq = alloc_ordered_workqueue("pdwait_wq", 0);
	INIT_DELAYED_WORK(&pdwait_work, pdwait_work_fnc);
	delay = usecs_to_jiffies(DELAYED_TIME_IN_US);
	queue_delayed_work_on(CPU0_ID, pdwait_wq, &pdwait_work, delay);

	ret = platform_driver_register(&pwr_driver);
	if (0 != ret)
		return ret;

	ret = platform_driver_register(&c4_power_driver);
	if (0 != ret) {
		platform_driver_unregister(&pwr_driver);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(pd); i++) {
		if (NULL == pd[i])
			continue;

		ret = platform_device_register(pd[i]);
		if (0 != ret) {
			platform_driver_unregister(&c4_power_driver);
			platform_driver_unregister(&pwr_driver);
			for (j = i - 1; j > 0; j--) {
				if (NULL != pd[j])
					platform_device_unregister(pd[j]);
			}
		return ret;
		}
	}

	sort_mapping_table(tbl2, ARRAY_SIZE(tbl2));

	return 0;
}

core_initcall(power_domain_driver_init);

/******************************************************************************
 * APIs
 *****************************************************************************/
/*
 * power_domain_devices: get power domain(area) device that supply power
 *						for driver (drv_name)
 * @drv_name: driver name that need to get power domain(area) device
 * @dev: Output value. Pointer points to power domain(area) device(s)
 * that supply power for driver.
 * @dev_cnt: Output value. Number of power domain(area) device.
 */

int power_domain_devices(const char *drv_name,
				struct device **dev, size_t *dev_cnt)
{
	int n;
	int index; /* index of element in mapping table (client_names)
						that contain drv_name */

	if ((NULL == drv_name) || (NULL == dev_cnt) || (NULL == dev))
		return -EINVAL;

	*dev_cnt = 0;

	index = get_power_area_index(tbl2,ARRAY_SIZE(tbl2), drv_name);

	if (0 <= index) {
		*(dev++) = &(pd[tbl2[index].area]->dev);
		(*dev_cnt)++;

		/* Check for upper successive elements
		of 1st mapped element */
		for (n = index + 1; n < ARRAY_SIZE(tbl2); n++) {
			if (0 != strcmp(tbl2[n].name, drv_name)) {
				break;
			} else {
				*(dev++) = &(pd[tbl2[n].area]->dev);
				(*dev_cnt)++;
			}
		}
		/* Check for lower successive elements
		of 1st mapped element */
		for (n = index - 1; n >= 0; n--) {
		if (0 != strcmp(tbl2[n].name, drv_name)) {
				break;
			} else {
				*(dev++) = &(pd[tbl2[n].area]->dev);
				(*dev_cnt)++;
			}
		}

	return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(power_domain_devices);

/*
 * power_down_count: get number of times that A3SP area is turned off
 * @powerdomain: This is reserved argument. It must be always
 * set by 0 or POWER_A3SP if otherwise it raises panic.
 */
u64 power_down_count(unsigned int powerdomain)
{
	if (0 != powerdomain && POWER_A3SP != powerdomain)
		panic("power_down_count() unsupported domain: %u", powerdomain);

	return a3sp_power_down_count;
}
EXPORT_SYMBOL(power_down_count);

/*
 * for_each_power_device: perform a Runtime-PM helper function
 * for driver device
 * @name: name of driver that need to run Runtime-PM helper function
 * @iterator: Runtime-PM helper function
 */
void for_each_power_device(const struct device *dev,
		int (*iterator)(struct device *))
{
	struct power_domain_info *pdi = __to_pdi(dev);
	unsigned int i;

	for (i = 0; i < pdi->cnt; i++)
		(void)iterator(pdi->devs[i]);
}

/*
 * power_domains_get_sync: request resume power area(s)
 * that supply power for driver
 * @name: driver name that raises request
 * Should be called by Runtime PM framework
 */
void power_domains_get_sync(const struct device *dev)
{
#ifdef __DEBUG_PDC
printk(KERN_INFO "[PDC] %s: %s\n", __func__, dev_name(dev));
#endif /* __DEBUG_PDC */
	for_each_power_device(dev, pm_runtime_get_sync);
	if (0 == strcmp(dev_name(dev), "pvrsrvkm")) {
		int r = sgx_cpufreq(CPUFREQ_SGXON);
		if (0 != r)
			panic("DVFS SGX on error: %d", r);
	}
}

/*
 * power_domains_put_noidle: request suspend power area(s)
 * that supply power for driver
 * @name: driver name that raises request
 * Should be called by Runtime PM framework
 */
void power_domains_put_noidle(const struct device *dev)
{
#ifdef __DEBUG_PDC
	printk(KERN_INFO "[PDC] %s: %s\n", __func__, dev_name(dev));
#endif /* __DEBUG_PDC */
	if (0 == strcmp(dev_name(dev), "pvrsrvkm")) {
		int r = sgx_cpufreq(CPUFREQ_SGXOFF);
		if (0 != r)
			panic("DVFS SGX off error: %d", r);
	}
	for_each_power_device(dev,
		(int (*)(struct device *))pm_runtime_put_noidle);
}
EXPORT_SYMBOL(power_domains_put_noidle);

void power_domain_set_status(unsigned int area, bool on)
{
	spin_lock(&pdc_lock);
	power_status_set(area, on);
	spin_unlock(&pdc_lock);
}
EXPORT_SYMBOL(power_domain_set_status);

#ifdef CONFIG_PM_DEBUG
int control_pdc(int is_enable)
{
	spin_lock(&pdc_lock);
	old_pdc_enable = pdc_enable;
	if ((1 == old_pdc_enable) && (0 == is_enable)) {
		if (0 == (power_areas_status & POWER_A4RM))
			power_status_set(POWER_A4RM, true);
		if (0 == (power_areas_status & POWER_A3R))
			power_status_set(POWER_A3R, true);
		if (0 == (power_areas_status & POWER_A4MP))
			power_status_set(POWER_A4MP, true);
		if (0 == (power_areas_status & POWER_A3SP)) {
#ifdef CONFIG_PM_HAS_SECURE
			(void)sec_hal_pm_a3sp_state_request(true);
#else
			power_status_set(POWER_A3SP, true);
#endif /* CONFIG_PM_HAS_SECURE */
		}
		if (0 == (power_areas_status & POWER_A3SG))
			power_status_set(POWER_A3SG, true);
	}

	if ((0 == old_pdc_enable) && (1 == is_enable)) {
		if (0 == (power_areas_status & POWER_A3R))
			power_status_set(POWER_A3R, false);
		if (0 == (power_areas_status & POWER_A4RM))
			power_status_set(POWER_A4RM, false);
		if (0 == (power_areas_status & POWER_A4MP))
			power_status_set(POWER_A4MP, false);
		if (0 == (power_areas_status & POWER_A3SP)) {
#ifdef CONFIG_PM_HAS_SECURE
			(void)sec_hal_pm_a3sp_state_request(false);
#else
			power_status_set(POWER_A3SP, false);
#endif /* CONFIG_PM_HAS_SECURE */
		}
		if (0 == (power_areas_status & POWER_A3SG))
			power_status_set(POWER_A3SG, false);
	}
	pdc_enable = is_enable;

#ifdef __DEBUG_PDC
	power_areas_info();
#endif /* __DEBUG_PDC */

	spin_unlock(&pdc_lock);
	return 0;
}
EXPORT_SYMBOL(control_pdc);

int is_pdc_enable(void)
{
	return pdc_enable;
}
EXPORT_SYMBOL(is_pdc_enable);
#endif
