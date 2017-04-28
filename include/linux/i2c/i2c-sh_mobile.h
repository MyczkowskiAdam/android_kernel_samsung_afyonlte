#ifndef __I2C_SH_MOBILE_H__
#define __I2C_SH_MOBILE_H__

#include <linux/platform_device.h>

#define I2C_SDA_NODELAY			0
#define MAX_SDA_DELAY			31
#define MIN_SDA_DELAY			0
#define UNMASK_ICTC_BITS_0TO2		0x07
#define UNMASK_DATA_DELAY_3TO7		0xF8
#define I2C_SDA_163NS_DELAY        	17
struct i2c_sh_mobile_port_info {
	unsigned int port_num;
	unsigned int port_func;
};

struct i2c_sh_mobile_platform_data {
	unsigned long	bus_speed;
	bool		pin_multi;
	unsigned char bus_data_delay;
	struct i2c_sh_mobile_port_info	scl_info;
	struct i2c_sh_mobile_port_info	sda_info;
	unsigned int clks_per_count;
};

#endif /* __I2C_SH_MOBILE_H__ */
