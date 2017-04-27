/*
 * ~/arch/arm/mach-shmobile/dev-u2evm-renesas-wifi.c
 */
/*
 * Copyright (C) 2011 Renesas Mobile Corporation.
 * Copyright (C) 2011 Renesas Design Vietnam Co., Ltd
 * Copyright (C) 2011 Google, Inc.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/wlan_plat.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <asm/io.h>
#include <mach/dev-wifi.h>

static unsigned char u2_mac_addr[IFHWADDRLEN] = { 0,0x90,0x4c,0,0,0 };

static int __init u2_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN*3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;

	WIFI_DEBUG("%s: Enter\n", __func__);

	if (strlen(str) >= sizeof(macstr))
		return 0;
	strcpy(macstr, str);

	while ((token = strsep(&macptr, ":")) != NULL) {
		unsigned long val;
		int res;

		if (i >= IFHWADDRLEN)
			break;
		res = strict_strtoul(token, 0x10, &val);
		if (res < 0)
			return 0;
		u2_mac_addr[i++] = (u8)val;
	}

	return 1;
}

__setup("androidboot.macaddr=", u2_mac_addr_setup);


static int u2_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	WIFI_DEBUG("%s: Enter\n", __func__);

	if ((u2_mac_addr[4] == 0) && (u2_mac_addr[5] == 0)) {
		srandom32((uint)jiffies);
		rand_mac = random32();
		u2_mac_addr[3] = (unsigned char)rand_mac;
		u2_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		u2_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, u2_mac_addr, IFHWADDRLEN);

	WIFI_DEBUG("%s: Done: MAC address = %s\n", __func__, buf);

	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ	4
typedef struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
} cntry_locales_custom_t;

static cntry_locales_custom_t u2_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XY", 4},  /* universal */
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},  /* input ISO "GB" to : EU regrev 05 */
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
	{"KR", "XY", 3},
	{"AU", "XY", 3},
	{"CN", "XY", 3},  /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3}
};

static void *u2_wifi_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(u2_wifi_translate_custom_table);
	int i;

	WIFI_DEBUG("%s: Enter\n", __func__);

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode, u2_wifi_translate_custom_table[i].iso_abbrev) == 0)
			return &u2_wifi_translate_custom_table[i];
	return &u2_wifi_translate_custom_table[0];
}

static int u2_wifi_power(int on)
{
	static int state;

	WIFI_DEBUG("%s: %s\n", __func__, (on ? "on" : "off"));

	if (state == on)
		return 0;

	/* Turn on/off WLAN chipset */
	if (on) {
		/* enable WL_REG_ON */
		gpio_set_value(GPIO_WLAN_REG_ON, 1);

	} else {
		/* disable WL_REG_ON */
		gpio_set_value(GPIO_WLAN_REG_ON, 0);
	}

	state = on;

	return 0;
}

static struct resource bcmdhd_res[] = {
	{
	.name = "bcmdhd_wlan_irq",
	.start = irqpin2irq(42),
	.end = irqpin2irq(42),
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	}
};

static struct wifi_platform_data u2_wifi_control = {
	.set_power      = u2_wifi_power,
	.get_mac_addr	= u2_wifi_get_mac_addr,
	.get_country_code = u2_wifi_get_country_code,
};

static struct platform_device u2_wifi_device = {
        .name           = "bcmdhd_wlan",
        .id             = 1,
        .dev            = {
                .platform_data = &u2_wifi_control,
        },
        .num_resources = ARRAY_SIZE(bcmdhd_res),
        .resource = bcmdhd_res,
};

static int u2_gpio_init(void)
{
	int ret;

	WIFI_DEBUG("%s: Enter\n", __func__);

	/* WLAN IRQ */
	ret = gpio_request(GPIO_WLAN_OOB_IRQ, NULL);
	if (ret) {
		WIFI_ERROR("%s: Fail to request IRQ GPIO\n", __func__);
		return -EIO;
	}

	gpio_direction_input(GPIO_WLAN_OOB_IRQ);
	gpio_set_debounce(GPIO_WLAN_OOB_IRQ, 1000); /* Set debounce to 1msec */

	__raw_writeb(0xA0, WLAN_OOB_IRQ_CR); /* Config WLAN IRQ with PD */

	/* WLAN REG ON */
	ret = gpio_request(GPIO_WLAN_REG_ON, NULL);
	if (ret) {
		WIFI_ERROR("%s: Fail to request EN GPIO\n", __func__);
		return -EIO;
	}
	gpio_direction_output(GPIO_WLAN_REG_ON, 0);

	WIFI_DEBUG("%s: Done\n", __func__);

	return 0;
}

static void u2_gpio_free(void)
{
	WIFI_DEBUG("%s: Enter\n", __func__);

	/* WLAN IRQ */
	gpio_free(GPIO_WLAN_OOB_IRQ);

	/* WLAN REG ON */
	gpio_free(GPIO_WLAN_REG_ON);

	WIFI_DEBUG("%s: Done\n", __func__);
}

int __init u2_wifi_init(void)
{
	int ret;

	WIFI_DEBUG("%s: start\n", __func__);

	ret = u2_gpio_init();
	if (ret) {
		WIFI_ERROR("%s: Initialization fail - exiting\n", __func__);
		return ret;
	}

	ret = platform_device_register(&u2_wifi_device);
	if (ret) {
		WIFI_ERROR("%s: Initialization fail - exiting\n", __func__);
		return ret;
	}

	WIFI_DEBUG("%s: Done\n", __func__);

	return 0;
}

static void __exit u2_wifi_exit(void) {
	u2_gpio_free();
}

module_init(u2_wifi_init);
module_exit(u2_wifi_exit);
