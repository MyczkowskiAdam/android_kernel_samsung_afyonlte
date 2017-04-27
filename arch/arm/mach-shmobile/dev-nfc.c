#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>
#include <linux/nfc/pn547.h>

#ifdef CONFIG_PMIC_INTERFACE
#include <linux/pmic/pmic-tps80032.h>
#endif

#include <mach/r8a7373.h>
#include <mach/irqs.h>

#if defined(CONFIG_BCMI2CNFC) || defined(CONFIG_PN547_NFC)
#define NFC_IRQ_GPIO		(GPIO_PORT13)  //PATCH CPN (GPIO_PORT105)   //NFC_IRQ
#define NFC_EN_GPIO			(GPIO_PORT12)  //PATCH CPN (25)             //NFC_EN
#define NFC_I2C_SDA_GPIO	(274)          //NFC_SDA
#define NFC_I2C_SCL_GPIO	(273)          //NFC_SCL
#define NFC_FIRM_GPIO		(GPIO_PORT101) //PATCH CPN (134)
#define NFC_CLK_REQ_GPIO	(43)           //NFC_CLK_REQ
#define NFC_WAKE_GPIO		(88)           //NFC_WAKE
#define NFC_I2C_BUS_ID		(7)            //PATCH CPN I2C_GPIO (3)
#endif

#if defined(CONFIG_BCMI2CNFC)
static int bcmi2cnfc_gpio_setup(void *);
static int bcmi2cnfc_gpio_clear(void *);
static struct bcmi2cnfc_i2c_platform_data bcmi2cnfc_pdata = {
	.irq_gpio = BCMBT_NFC_IRQ_GPIO,
	.en_gpio = BCMBT_NFC_EN_GPIO,
	.wake_gpio = BCMBT_NFC_WAKE_GPIO,
	.init = bcmi2cnfc_gpio_setup,
	.reset = bcmi2cnfc_gpio_clear,
	.i2c_pdata	= {ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_400K),},
};


static int bcmi2cnfc_gpio_setup(void *this)
{
	struct bcmi2cnfc_i2c_platform_data *p;
	p = (struct bcmi2cnfc_i2c_platform_data *) this;
	if (!p)
		return -1;
	pr_info("bcmi2cnfc_gpio_setup nfc en %d, wake %d, irq %d\n",
		p->en_gpio, p->wake_gpio, p->irq_gpio);

	gpio_request(p->irq_gpio, "nfc_irq");
	gpio_direction_input(p->irq_gpio);

	gpio_request(p->en_gpio, "nfc_en");
	gpio_direction_output(p->en_gpio, 1);

	gpio_request(p->wake_gpio, "nfc_wake");
	gpio_direction_output(p->wake_gpio, 0);

	return 0;
}
static int bcmi2cnfc_gpio_clear(void *this)
{
	struct bcmi2cnfc_i2c_platform_data *p;
	p = (struct bcmi2cnfc_i2c_platform_data *) this;
	if (!p)
		return -1;

	pr_info("bcmi2cnfc_gpio_clear nfc en %d, wake %d, irq %d\n",
		p->en_gpio, p->wake_gpio, p->irq_gpio);

	gpio_direction_output(p->en_gpio, 0);
	gpio_direction_output(p->wake_gpio, 1);
	gpio_free(p->en_gpio);
	gpio_free(p->wake_gpio);
	gpio_free(p->irq_gpio);

	return 0;
}

static struct i2c_board_info __initdata bcmi2cnfc[] = {
	{
	 I2C_BOARD_INFO("bcmi2cnfc", 0x1FA),
	 .flags = I2C_CLIENT_TEN,
	 .platform_data = (void *)&bcmi2cnfc_pdata,
	 .irq = gpio_to_irq(BCMBT_NFC_IRQ_GPIO),
	 },

};
static struct i2c_gpio_platform_data rhea_nfc_i2c_gpio_platdata = {
        .sda_pin                = BCMBT_NFC_I2C_SDA_GPIO,
        .scl_pin                = BCMBT_NFC_I2C_SCL_GPIO,
        .udelay                 = 1,
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device rhea_nfc_i2c_gpio_device = {
        .name                   = "i2c-gpio",
        .id                     = 0x5,
        .dev.platform_data      = &rhea_nfc_i2c_gpio_platdata,
 };
#endif

#ifdef CONFIG_PN547_NFC
static struct pn547_i2c_platform_data PN547_pdata = {
 	.irq_gpio 	= NFC_IRQ_GPIO,
 	.ven_gpio = NFC_EN_GPIO,
 	.firm_gpio = NFC_FIRM_GPIO,
};

struct i2c_board_info PN547_info[] __initdata = {
{
	I2C_BOARD_INFO("pn547", 0x2b),
	.irq = irqpin2irq(NFC_IRQ_GPIO),
	.platform_data = &PN547_pdata,
 	},
};

int pn547_info_size(void) {
	return ARRAY_SIZE(PN547_info);
}

void pn547_device_i2c_register(void) {
	pr_info("pn547_device_i2c_register\n");
	i2c_register_board_info(NFC_I2C_BUS_ID, PN547_info, ARRAY_SIZE(PN547_info));
}
#endif /* CONFIG_PN547_NFC	*/
