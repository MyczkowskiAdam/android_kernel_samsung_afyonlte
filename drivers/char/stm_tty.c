/* drivers/char/stm_tty.c
 *
 * Copyright (C) 2012 RMC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/io.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <asm/hardware/coresight.h>
#include <mach/r8a7373.h>
MODULE_DESCRIPTION("STM TTY Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

struct stm_port {
	/* pointer to the tty for this device */
	struct tty_struct	*tty;
	/* number of times this port has been opened */
	int			open_count;
	/* locks this structure */
	struct semaphore	sem;
};

#define STM_TCSR_TRACEID (0x40 << 16)
#define STM_TTY_MINORS 7
#define STM_CONSOLE 7

void __iomem *fun;
void __iomem *stm_ctrl;
void __iomem *stm_prt_reg;
static struct tty_driver *g_stm_tty_driver;
static struct stm_port *stm_table[STM_TTY_MINORS];
static uint32_t stm_channel_end;

static int is_debug_enabled(void)
{
	uint32_t val;

	val = __raw_readl(DBGREG1);
	if (((val >> 28) & 3) == 2)
		return 1; /* Debug is enabled */
	else
		return 0; /* Debug is disabled */

}

static int is_debug_disabled(void)
{
	return !(is_debug_enabled());
}

inline void __iomem *get_stm(uint32_t channel)
{
	void __iomem *stm = NULL;

	if ((channel > stm_channel_end) || !stm_prt_reg)
		return NULL;

	stm = stm_prt_reg + STM_PORT_SIZE*channel;
	return stm;
}

static int stm_write(const unsigned char *buf, int count, void __iomem *stm)
{
	int written = 0;
	int retval = -EINVAL;
	u32 *aligned;
	u16 *b_u16;
	u8 *b_u8;

	if (count >= 4) {
		/* align source buffer address */
		/* to move data 32 bits per 32 bits */
		while ((unsigned)buf & 3) {
			__raw_writeb(*buf, stm + STM_G_D);
			buf++;
			written++;
		};
		/* write 32 bits word */
		aligned = (u32 *)buf;
		while ((written+sizeof(u32)) < count) {
			__raw_writel(*aligned, stm + STM_G_D);
			aligned++;
			written += sizeof(u32);
		}
		/* Last unaligned bytes with timestamp on last write */
		switch (count-written) {
		case sizeof(u8):
			b_u8 = (u8 *)aligned;
			__raw_writeb(*b_u8, stm + STM_G_DMTS);
			break;
		case sizeof(u16):
			b_u16 = (u16 *)aligned;
			__raw_writew(*b_u16, stm + STM_G_DMTS);
			break;
		case (sizeof(u16)+sizeof(u8)):
			b_u16 = (u16 *)aligned;
			__raw_writew(*b_u16, stm + STM_G_D);
			b_u16++;
			b_u8 = (u8 *)b_u16;
			__raw_writeb(*b_u8, stm + STM_G_DMTS);
			break;
		case sizeof(u32):
			__raw_writel(*aligned, stm + STM_G_DMTS);
			break;
		default:
		/* it should not happen, however trace string is closed */
			__raw_writeb(0, stm + STM_G_DMTS);
			break;
		}
	} else {
		/* unaligned short message is written byte per byte */
		while (written+1 < count) {
			__raw_writeb(*buf, stm + STM_G_D);
			buf++;
			written++;
		}
		__raw_writeb(*buf, stm + STM_G_DMTS);
		buf++;
		written++;
	}
	retval = count;
	__raw_writel(__raw_readl(stm_ctrl + STM_SYNCR), stm_ctrl + STM_SYNCR);
	return retval;
}

void stm_console_write(struct console *co, const char *b, unsigned count)
{
	void __iomem *stm = get_stm(STM_CONSOLE);
	if (stm == NULL)
		return;

	if (count) {
		__raw_writeb(0x20, stm + STM_G_D);
		stm_write(b, count, stm);
	}
}

static struct tty_driver *stm_console_device(struct console *c, int *index)
{
	*index = 0;
	return g_stm_tty_driver;
}

static int __init stm_console_setup(struct console *co, char *options)
{
	if (co->index != 0)
		return -ENODEV;
	return 0;
}

static struct console stm_console = {
	.name = "STM",
	.write = stm_console_write,
	.device = stm_console_device,
	.setup = stm_console_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
};


#ifdef CONFIG_TTY_LL_STM
inline void stm_printk(const char *bptr, void __iomem* stm, const char stimulus)
{
	int rem_len;

	rem_len = strlen(bptr);
	__raw_writeb(0x20, stm + STM_G_D);

	/* Leading unaligned bytes */
	while ((unsigned)bptr & 3 && rem_len >= 1) {
		__raw_writeb(*(uint8_t *)bptr, stm + STM_G_D);
		bptr += 1;
		rem_len -= 1;
	};

	/* Aligned words */
	while (rem_len >= 4) {
		__raw_writel(*(uint32_t *)bptr, stm + STM_G_D);
		bptr += 4;
		rem_len -= 4;
	}

	/* Trailing unaligned bytes */
	while (rem_len > 0) {
		__raw_writeb(*(uint8_t *)bptr, stm + STM_G_D);
		bptr += 1;
		rem_len -= 1;
	}
	__raw_writeb(0x00, stm + stimulus);
}

void stm_printk_str(uint32_t channel, const char *bptr)
{
	void __iomem *stm = get_stm(channel);
	if (stm == NULL)
		return;

	stm_printk(bptr, stm, STM_G_DMTS);
}
EXPORT_SYMBOL(stm_printk_str);

void stm_printk_fmt_1x(uint32_t channel, const char *bptr, unsigned arg1)
{
	void __iomem *stm = get_stm(channel);
	if (stm == NULL)
		return;

	stm_printk(bptr, stm, STM_G_D);

	__raw_writel(arg1, stm + STM_G_DMTS);
}
EXPORT_SYMBOL(stm_printk_fmt_1x);

void stm_printk_fmt_2x(uint32_t channel, const char *bptr, unsigned arg1,
		unsigned arg2)
{
	void __iomem *stm = get_stm(channel);
	if (stm == NULL)
		return;

	stm_printk(bptr, stm, STM_G_D);
	__raw_writel(arg1, stm + STM_G_D);
	__raw_writel(arg2, stm + STM_G_DMTS);
}
EXPORT_SYMBOL(stm_printk_fmt_2x);
#endif




static int stm_tty_open(struct tty_struct *tty, struct file *filp)
{
	int index;
	struct stm_port *prt;

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;
	/* get the serial object associated with this tty pointer */
	index = tty->index;
	printk(KERN_EMERG "### stm_tty_open %d\n", index);
	prt = stm_table[index];
	if (prt == NULL) {
		/* first time accessing this device, let's create it */
		prt = devm_kzalloc(tty->dev, sizeof(*prt), GFP_KERNEL);
		if (!prt)
			return -ENOMEM;
		sema_init(&prt->sem, 1);
		prt->open_count = 0;
		stm_table[index] = prt;
	}
	if (!down_interruptible(&prt->sem)) {
		/* save our structure within the tty structure */
		tty->driver_data = prt;
		prt->tty = tty;
		++prt->open_count;
		up(&prt->sem);
	}
	return 0;
}

static void do_close(struct stm_port *prt)
{
	if (!down_interruptible(&prt->sem)) {
		if (prt->open_count)
			--prt->open_count;
		up(&prt->sem);
	}
}

static void stm_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct stm_port *prt = tty->driver_data;
	printk(KERN_EMERG "### stm_tty_close %d\n", tty->index);
	if (prt)
		do_close(prt);
}

static int stm_tty_write(struct tty_struct *tty,
				const unsigned char *buf,
				int count)
{
	int retval = -EINVAL;
	struct stm_port *prt = tty->driver_data;
	void __iomem *stm = get_stm(tty->index);
	if (stm == NULL)
		return -ENODEV;

	if (!prt)
		return -ENODEV;
	retval = down_interruptible(&prt->sem);
	if (retval)
		return retval;
	if (!prt->open_count)
		/* port was not opened */
		goto exit;
	if (!count) {
		/* no data */
		retval = count;
		goto exit;
	}

	retval = stm_write(buf, count, stm);
exit:
	up(&prt->sem);
	return retval;
}

static int stm_tty_write_room(struct tty_struct *tty)
{
	return 0x800;	/* Always 2 kbytes */
}

static const struct tty_operations stm_tty_ops = {
	.open = stm_tty_open,
	.close = stm_tty_close,
	.write = stm_tty_write,
	.write_room = stm_tty_write_room,
};

static void __iomem *remap(struct platform_device *pd, int id)
{
	struct resource *io;
	void __iomem *ret = NULL;

	io = platform_get_resource(pd, IORESOURCE_MEM, id);
	if (!io) {
		dev_err(&pd->dev, "IO memory resource error\n");
		return ret;
	}

	if (strcmp(io->name, "stm_ports") == 0)
		stm_channel_end = resource_size(io)/STM_PORT_SIZE;

	io = devm_request_mem_region(&pd->dev, io->start,
					resource_size(io), dev_name(&pd->dev));

	if (!io) {
		dev_err(&pd->dev, "IO memory region request failed\n");
		stm_channel_end = 0;
		return ret;
	}

	ret = devm_ioremap(&pd->dev, io->start, resource_size(io));
	if (!ret) {
		stm_channel_end = 0;
		dev_err(&pd->dev, "%s IO memory remap failed\n",
								io->name);
	}
	return ret;
}

static int __devinit stm_probe(struct platform_device *pd)
{
	unsigned i;
	unsigned int tscr;
	struct clk *clk;
	unsigned long rate;
	void __iomem *stm_prt_reg_tmp;

	clk = clk_get(NULL, "cp_clk");
	if (clk)
		rate = clk_get_rate(clk);
	else
		rate = 13000000;
	dev_info(&pd->dev, "clk %p %ld\n", clk, rate);

	stm_ctrl = remap(pd, 0);
	if (!stm_ctrl)
		return -ENXIO;

	fun = remap(pd, 2);
	if (!fun)
		return -ENXIO;

	stm_prt_reg_tmp = remap(pd, 1);
	if (!stm_prt_reg_tmp)
		return -ENXIO;

	/* Unlock trace funnel */
	__raw_writel(UNLOCK_MAGIC, fun + CSMR_LOCKACCESS);
	CSTF_PORT_DISABLE(fun, 0);
	CSTF_PRIO_SET(fun, 0, 0);
	/* Re-enable Funnel */
	CSTF_PORT_ENABLE(fun, 0);
	__raw_writel(0, fun + CSMR_LOCKACCESS);

	/* Unlock STM. STM is not relocked */
	/* to allow STP synchronization writes to STM_SYNCR */
	__raw_writel(UNLOCK_MAGIC, stm_ctrl + CSMR_LOCKACCESS);
	/* Disable STM for configuration */
	tscr = __raw_readl(stm_ctrl + STM_TCSR);
	__raw_writel(tscr & ~STM_TCSR_EN, stm_ctrl + STM_TCSR);
	/* wait 0 busy in STMTCSR */
	i = 0x10000;
	while (__raw_readl(stm_ctrl + STM_TCSR) & STM_TCSR_BUSY) {
		i--;
		if (i == 0) {
			dev_err(&pd->dev, "STM still busy\n");
			return -ENXIO;
		}
	};
	/* set timestamp clock */
	__raw_writel(rate, stm_ctrl + STM_TSFREQR);
	/* enable all 32 stimulus ports, 8 are used by APE and 2 by SPUV */
	__raw_writel(0xffffffff, stm_ctrl + STM_SPER);
	__raw_writel(0x400, stm_ctrl + STM_SYNCR);
	/* Control settings in STMTCSR */
	tscr = STM_TCSR_TSEN|STM_TCSR_SYNCEN|STM_TCSR_TRACEID;
	__raw_writel(tscr, stm_ctrl + STM_TCSR);
	/* Re-enable STM */
	tscr = __raw_readl(stm_ctrl + STM_TCSR);
	__raw_writel(tscr | STM_TCSR_EN, stm_ctrl + STM_TCSR);

	for (i = 0; i < STM_TTY_MINORS; ++i) {
		struct device   *tty_dev;
		tty_dev = tty_register_device(g_stm_tty_driver, i, &pd->dev);
		if (IS_ERR(tty_dev))
			pr_warning("%s: no classdev for port %d, err %ld\n",
						__func__, i, PTR_ERR(tty_dev));
	}
	stm_prt_reg = stm_prt_reg_tmp;
	register_console(&stm_console);
	dev_info(&pd->dev, "STM ready\n");
	return 0;
}

static int __exit stm_remove(struct platform_device *pd)
{
	unsigned i;
	unsigned int tscr;
	printk(KERN_EMERG "### stm_tty_remove\n");

	unregister_console(&stm_console);
	/* Disable STM */
	tscr = __raw_readl(stm_ctrl + STM_TCSR);
	__raw_writel(tscr & ~STM_TCSR_EN, stm_ctrl + STM_TCSR);
	/* Unlock trace funnel */
	__raw_writel(UNLOCK_MAGIC, fun + CSMR_LOCKACCESS);
	/* Disable port0 of trace funnel */
	CSTF_PORT_DISABLE(fun, 0);
	__raw_writel(0, fun + CSMR_LOCKACCESS);
	for (i = 0; i < STM_TTY_MINORS; ++i)
		tty_unregister_device(g_stm_tty_driver, i);
	return 0;
}

static struct platform_driver stm_pdriver = {
	.probe	=  stm_probe,
	.remove	= __exit_p(stm_remove),
	.driver	= {
			.name	= "stm",
			.owner	= THIS_MODULE,
	},
};


static int __init stm_tty_init(void)
{
	int ret;

	if (is_debug_disabled()) {
		ret = -ENOMEM;
		g_stm_tty_driver = NULL;
		goto err_alloc_tty_driver_failed;
	}

	g_stm_tty_driver = alloc_tty_driver(STM_TTY_MINORS);
	if (!g_stm_tty_driver) {
		printk(KERN_ERR "stm_tty_init: alloc_tty_driver failed\n");
		ret = -ENOMEM;
		goto err_alloc_tty_driver_failed;
	}
	g_stm_tty_driver->owner = THIS_MODULE;
	g_stm_tty_driver->driver_name = "stm";
	g_stm_tty_driver->name = "ttySTM";
	g_stm_tty_driver->major = 0; /* auto assign */
	g_stm_tty_driver->minor_start = 0;
	g_stm_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	g_stm_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	g_stm_tty_driver->init_termios = tty_std_termios;
	g_stm_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(g_stm_tty_driver, &stm_tty_ops);
	ret = tty_register_driver(g_stm_tty_driver);
	if (ret) {
		printk(KERN_ERR "stm_tty_init: tty_register_driver" \
				" failed, %d\n", ret);
		goto err_tty_register_driver_failed;
	}
	ret = platform_driver_register(&stm_pdriver);
	if (ret) {
		printk(KERN_ERR "stm_tty_init: platform_driver_register" \
				" failed, %d\n", ret);
		goto err_register_platform_failed;
	}
	return 0;

err_register_platform_failed:
	tty_unregister_driver(g_stm_tty_driver);
err_tty_register_driver_failed:
	put_tty_driver(g_stm_tty_driver);
	g_stm_tty_driver = NULL;
err_alloc_tty_driver_failed:
	return ret;
}

static void  __exit stm_tty_exit(void)
{
	int ret;

	if (is_debug_disabled())
		goto no_debug_exit;

	ret = tty_unregister_driver(g_stm_tty_driver);
	if (ret < 0)
		printk(KERN_ERR "stm_tty_exit: tty_unregister_driver" \
				" failed, %d\n", ret);
	else
		put_tty_driver(g_stm_tty_driver);

no_debug_exit:
	g_stm_tty_driver = NULL;
}

module_init(stm_tty_init);
module_exit(stm_tty_exit);
