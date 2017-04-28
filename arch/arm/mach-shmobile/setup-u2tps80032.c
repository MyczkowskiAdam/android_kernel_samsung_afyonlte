#ifdef CONFIG_PMIC_INTERFACE
#include <linux/pmic/pmic.h>
#include <linux/pmic/pmic-tps80032.h>


#include <linux/regulator/tps80031-regulator.h>
#include <linux/mfd/tps80031.h>
#include <linux/io.h>
#include <mach/r8a7373.h>
#include <mach/irqs.h>
#include <mach/setup-u2tps80032.h>


static struct regulator_consumer_supply tps80031_ldo5_supply[] = {
	REGULATOR_SUPPLY("vdd_touch", NULL),
};

#define TPS_PDATA_INIT(_id, _minmv, _maxmv, _supply_reg, _always_on,	\
	_boot_on, _apply_uv, _init_uV, _init_enable, _init_apply,	\
	_flags, _delay)						\
	static struct tps80031_regulator_platform_data pdata_##_id = {	\
		.regulator = {						\
			.constraints = {				\
				.min_uV = (_minmv)*1000,		\
				.max_uV = (_maxmv)*1000,		\
				.valid_modes_mask = (REGULATOR_MODE_NORMAL |  \
						REGULATOR_MODE_STANDBY),      \
				.valid_ops_mask = (REGULATOR_CHANGE_MODE |    \
						REGULATOR_CHANGE_STATUS |     \
						REGULATOR_CHANGE_VOLTAGE),    \
				.always_on = _always_on,		\
				.boot_on = _boot_on,			\
				.apply_uV = _apply_uv,			\
			},						\
			.num_consumer_supplies =			\
				ARRAY_SIZE(tps80031_##_id##_supply),	\
			.consumer_supplies = tps80031_##_id##_supply,	\
			.supply_regulator = _supply_reg,		\
		},							\
		.init_uV =  _init_uV * 1000,				\
		.init_enable = _init_enable,				\
		.init_apply = _init_apply,				\
		.flags = _flags,					\
		.delay_us = _delay,					\
	}

TPS_PDATA_INIT(ldo5, 1000, 3300, 0, 0, 0, 0, 2700, 0, 1, 0, 0);

static struct tps80031_rtc_platform_data rtc_data = {
	.irq = ENT_TPS80031_IRQ_BASE + TPS80031_INT_RTC_ALARM,
	.time = {
		.tm_year = 2012,
		.tm_mon = 0,
		.tm_mday = 1,
		.tm_hour = 1,
		.tm_min = 2,
		.tm_sec = 3,
	},
};

#define TPS_REG(_id, _data)				\
	{						\
		.id	 = TPS80031_ID_##_id,		\
		.name   = "tps80031-regulator",		\
		.platform_data  = &pdata_##_data,	\
	}

#define TPS_RTC()				\
	{					\
		.id	= 0,			\
		.name	= "rtc_tps80032",	\
		.platform_data = &rtc_data,	\
	}

static struct tps80032_subdev_info tps80032_devs[] = {
	TPS_RTC(),
	TPS_REG(LDO5, ldo5),
};

static u8 tps80032_get_portcr_value(u32 addr)
{
	return __raw_readb(addr);
}

static void tps80032_set_portcr_value(u8 value, u32 addr)
{
	__raw_writeb(value, addr);
}

struct tps80032_platform_data tps_platform = {
	.pin_gpio	= {GPIO_PORT0, GPIO_PORT28,
				GPIO_PORT35, GPIO_PORT141,
				GPIO_PORT202},
	.pin_gpio_fn	= {GPIO_PORT0, GPIO_PORT28,
				GPIO_PORT35, GPIO_PORT141,
				GPIO_PORT202},
	.portcr		= {PORTCR0, PORTCR28,
					PORTCR35, PORTCR141,
					PORTCR202},
	.get_portcr_value = tps80032_get_portcr_value,
	.set_portcr_value = tps80032_set_portcr_value,
	.num_subdevs	= ARRAY_SIZE(tps80032_devs),
	.subdevs	= tps80032_devs,
	.irq_base	= ENT_TPS80032_IRQ_BASE,
};

#endif
