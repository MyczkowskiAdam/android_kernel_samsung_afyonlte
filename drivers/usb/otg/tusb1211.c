/*
 * drivers/usb/otg/tusb1211.c
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb.h>
#include <linux/usb/otg.h>
#include <linux/workqueue.h>
#include <linux/usb/tusb1211.h>
#include <linux/usb/ulpi.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sh_clk.h>
#include <linux/pm_runtime.h>
#include <linux/usb/otg.h>
#include <mach/common.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <linux/usb/hcd.h>
#ifdef CONFIG_REGULATOR_TPS80031
#include <linux/regulator/consumer.h>
#endif
#define HOST_REQ_FLG 1
/*define prototype*/

static int tusb1211_init(struct otg_transceiver *otg);
static int tusb1211_start_hnp(struct otg_transceiver *otg);
static int tusb1211_start_srp(struct otg_transceiver *otg);
static int tusb1211_set_suspend(struct otg_transceiver *otg, int suspend);
static int tusb1211_set_peripheral(struct otg_transceiver *otg, struct usb_gadget *periph);
static void tusb1211_shutdown(struct otg_transceiver *otg);
static int tusb1211_set_host(struct otg_transceiver *otg, struct usb_bus *host);
static int tusb1211_set_vbus(struct otg_transceiver *otg, bool enabled);
static void tusb1211_reset(void);
static void tusb1211_enable_interrupt(struct otg_transceiver *otg);
static int tusb1211_io_write(struct otg_transceiver *otg, u32 val, u32 reg);
static int  tusb1211_io_read(struct otg_transceiver *otg, u32 reg);
static void tusb1211_chrg_vbus(struct tusb1211 *tusb, int on);
static void tusb1211_dischrg_vbus(struct tusb1211 *tusb, int on);

static struct otg_io_access_ops tusb1211_otg_io_ops = {
	.read = tusb1211_io_read,
	.write = tusb1211_io_write,
};

static struct class *tusb_class;
static struct tusb1211 *xceiver;

/************************************************************************/

#ifdef CONFIG_USB_OTG
static void set_otg_vbus(int enable)
{
	if (!!enable) {/*Enable VBUS*/
#if defined(CONFIG_CHARGER_SMB328A)
		smb328a_otg_enable_disable(1, 0);
#elif defined(CONFIG_CHARGER_SMB358)
		smb358a_otg_enable_disable(1);
#endif
	} else { /*Disable VBUS*/
#if defined(CONFIG_CHARGER_SMB328A)
		smb328a_otg_enable_disable(0, 0);
#elif defined(CONFIG_CHARGER_SMB358)
		smb358a_otg_enable_disable(0);
#endif
	}
}
#endif /* CONFIG_USB_OTG */

static struct class *tusb_class;
static struct tusb1211 *xceiver;

#ifdef CONFIG_USB_OTG
int set_otg_mode(int is_host)
{
	struct tusb1211 *tusb = xceiver;
	int ret = 0;
	if (NULL == tusb)
		return -EFAULT;
	if (is_host) {
		if (tusb->vbus_enable == 0) {
			printk(KERN_INFO"%s: >>>>>>>>>>>> SET HOST MODE "\
							"<<<<<<<<<<<<<<", __func__);
			tusb->otg.state = OTG_STATE_A_IDLE;
			tusb->otg.default_a = 1;
			printk(KERN_INFO "%s\n",
							otg_state_string(tusb->otg.state));
			otg_write_reg(&tusb->otg, 0x00, INTSTS0);
			otg_write_reg(&tusb->otg, 0x00, INTSTS1);
			tusb->vbus_enable = 1;
			ret = schedule_delayed_work(&tusb->vbus_work, 0);
			if (ret < 0)
				printk(KERN_WARNING"Error in scheduling wq\n");
		}
	} else {
		if (tusb->vbus_enable == 1) {
			printk(KERN_INFO"%s: >>>> SET DEVICE(FUNCTION) MODE "\
							"<<<<<<", __func__);
			otg_write_reg(&tusb->otg, 0x00, INTSTS0);
			otg_write_reg(&tusb->otg, 0x00, INTSTS1);
			tusb->otg.default_a = 0;
			tusb->vbus_enable = 0;
			ret = schedule_delayed_work(&tusb->vbus_work, 0);
			if (ret < 0)
				printk(KERN_WARNING"Error in scheduling wq\n");
		}
	}
	return 0;
}
EXPORT_SYMBOL(set_otg_mode);
#endif /* CONFIG_USB_OTG*/


static void usb_host_port_power(struct tusb1211 *tusb)
{
	char clk_name[16];
	struct clk *clk_dmac;
#ifdef CONFIG_USB_OTG
	int ret = 0;
#endif
#ifdef CONFIG_REGULATOR_TPS80031
	struct regulator *r8a66597_hcd_regulator = regulator_get(NULL, "vbus");
	if (!r8a66597_hcd_regulator) {
		return;
	}
#endif
	if (tusb->vbus_enable) {
#ifdef CONFIG_MFD_TPS80031
		disable_irq_nosync(602);
#endif
#if defined(CONFIG_PMIC_INTERFACE) && !defined(CONFIG_USB_OTG)
		disable_irq_nosync(595);
#endif
		ret = pm_runtime_get_sync(tusb->otg.dev);
		if (ret < 0)
			printk(KERN_ERR"Error in pm runtime syncing.\n");

		ret = clk_enable(tusb->clk);
		if (ret < 0)
			printk(KERN_ERR"Error in enabling clock.\n");

		ret = gpio_direction_output(TUSB_CS, 1);
		if (ret < 0)
			printk(KERN_ERR"Error setting GPIO output.\n");

		snprintf(clk_name, sizeof(clk_name), "usb%d_dmac", 0);
		/*
		 * We don't have any device resource defined
		 * for USBHS-DMAC
		 */
		clk_dmac = clk_get(NULL, clk_name);
		if (IS_ERR(clk_dmac)) {
			printk(KERN_INFO"cannot get clock \"%s\"\n",\
					clk_name);
			return;
		}

		ret = clk_enable(clk_dmac);
		if (ret < 0)
			printk(KERN_ERR"Error in enabling DMA clock.\n");

		ret = clk_enable(tusb->iclk);
		if (ret < 0) {
			printk(KERN_ERR"Error in enabling interface clock.\n");
			return;
		}

		tusb->pdata->module_start();
		otg_io_set_bits(&tusb->otg, USBE, SYSCFG);

		otg_io_set_bits(&tusb->otg, BEMPE | NRDYE | BRDYE, INTENB0);
		otg_io_set_bits(&tusb->otg, BRDY0, BRDYENB);
		otg_io_set_bits(&tusb->otg, BEMP0, BEMPENB);

		otg_io_set_bits(&tusb->otg, TRNENSEL, SOFCFG);

		otg_io_set_bits(&tusb->otg, SIGNE | SACKE, INTENB1);
		otg_io_set_bits(&xceiver->otg, VBCOMPE, INTENB1);

		otg_io_set_bits(&tusb->otg, DCFM, SYSCFG);
		otg_io_set_bits(&tusb->otg, DRPD, SYSCFG);
		otg_io_set_bits(&tusb->otg, HSE, SYSCFG);

		otg_io_clear_bits(&tusb->otg, DTCHE, INTENB1);
		otg_io_set_bits(&tusb->otg, ATTCHE, INTENB1);
		tusb->otg.state = OTG_STATE_A_WAIT_VRISE;
		printk("%s\n", otg_state_string(tusb->otg.state));
#ifdef CONFIG_REGULATOR_TPS80031
		if (!regulator_is_enabled(r8a66597_hcd_regulator)) {
			regulator_enable(r8a66597_hcd_regulator);
		}
#endif
#ifdef CONFIG_USB_OTG
		set_otg_vbus(1);
#endif
		} else {
#ifdef CONFIG_REGULATOR_TPS80031
		if (regulator_is_enabled(r8a66597_hcd_regulator)) {
			regulator_disable(r8a66597_hcd_regulator);
		}
#endif
#ifdef CONFIG_USB_OTG
		set_otg_vbus(0);
#endif
		tusb->pdata->module_start();
#ifdef CONFIG_MFD_TPS80031
		enable_irq(602);
#endif
#if defined(CONFIG_PMIC_INTERFACE) && !defined(CONFIG_USB_OTG)
		enable_irq(595);
#endif
		otg_write_reg(&tusb->otg, 0x05, BWAIT);
		otg_io_set_bits(&tusb->otg, HSE, SYSCFG);
		otg_io_set_bits(&tusb->otg, USBE, SYSCFG);

		otg_io_set_bits(&tusb->otg, CTRE, INTENB0);
		otg_io_set_bits(&tusb->otg, BEMPE | BRDYE, INTENB0);
		otg_io_set_bits(&tusb->otg, RESM | DVSE, INTENB0);

		otg_io_clear_bits(&tusb->otg, DRPD, SYSCFG);
		otg_io_set_bits(&tusb->otg, DPRPU, SYSCFG);
		if (tusb->iclk->usecount)
			clk_disable(tusb->iclk);
		ret = gpio_direction_output(TUSB_CS, 0);
		if (ret < 0)
			printk(KERN_ERR"Error in setting GPIO output.\n");
		if (tusb->clk->usecount)
			clk_disable(tusb->clk);
		ret = pm_runtime_put_sync(tusb->otg.dev);
		if (ret < 0)
			printk(KERN_ERR"Error in syncing pm_runtime_put.\n");

		tusb->otg.state = OTG_STATE_B_IDLE;
		printk("%s\n", otg_state_string(tusb->otg.state));
		}
}

static void tusb_vbus_work(struct work_struct *work)
{
	struct tusb1211 *tusb =
			container_of(work, struct tusb1211, vbus_work.work);
	usb_host_port_power(tusb);
}

static void tusb_vbus_off_work(struct work_struct *work)
{
#ifdef CONFIG_REGULATOR_TPS80031
	struct regulator *r8a66597_hcd_regulator = regulator_get(NULL, "vbus");
	if (!r8a66597_hcd_regulator) {
		return;
	}
	if (regulator_is_enabled(r8a66597_hcd_regulator)) {
		regulator_disable(r8a66597_hcd_regulator);
	}
#endif
#ifdef CONFIG_USB_OTG
	set_otg_vbus(0);
#endif
}

static ssize_t tusb1211_show(struct class *class, struct class_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%s\n", "srp");
}

static ssize_t tusb1211_store(struct class *class, struct class_attribute *attr,
								const char *buf, size_t count)
{
	struct tusb1211 *tusb = xceiver;
	xceiver->otg.state = OTG_STATE_A_WAIT_VFALL;
	printk("%s\n", otg_state_string(xceiver->otg.state));
	schedule_delayed_work(&tusb->vbus_off_work, 0);
	return count;
}

CLASS_ATTR(tusb_srp, S_IRUGO | S_IWUSR, tusb1211_show, tusb1211_store);

static ssize_t srp_show(struct class *class, struct class_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%s\n", "test_srp");
}

static ssize_t srp_store(struct class *class, struct class_attribute *attr,
								const char *buf, size_t count)
{
	otg_start_srp(&xceiver->otg);
	return count;
}

CLASS_ATTR(srp, S_IRUGO | S_IWUSR, srp_show, srp_store);

static ssize_t hnp_show(struct class *class, struct class_attribute *attr,
								char *buf)
{
	return sprintf(buf, "%s\n", "test_hnp");
}

static ssize_t hnp_store(struct class *class, struct class_attribute *attr,
								const char *buf, size_t count)
{
	otg_io_clear_bits(&xceiver->otg, UACT, DVSTCTR);
	if (xceiver->otg.state == OTG_STATE_A_HOST)
		xceiver->otg.state = OTG_STATE_A_SUSPEND;
	if (xceiver->otg.state == OTG_STATE_B_HOST)
		xceiver->otg.state = OTG_STATE_B_PERIPHERAL;
	printk("%s\n", otg_state_string(xceiver->otg.state));
	return count;
}

CLASS_ATTR(hnp, S_IRUGO | S_IWUSR, hnp_show, hnp_store);

static void tusb1211_chrg_vbus(struct tusb1211 *tusb, int on)
{
	if (on) {
		/* stop discharging, start charging */
		otg_io_clear_bits(&tusb->otg, DISCHRGVBUS, USBHS_PHYOTGCTR);
		otg_io_set_bits(&tusb->otg, CHRGVBUS, USBHS_PHYOTGCTR);
	} else {
		/* stop charging */
		otg_io_clear_bits(&tusb->otg, CHRGVBUS, USBHS_PHYOTGCTR);
	}
}

/* Discharge vbus through a resistor to ground */
static void tusb1211_dischrg_vbus(struct tusb1211 *tusb, int on)
{
	if (on) {
		/* stop charging, start discharging */
		otg_io_clear_bits(&tusb->otg, CHRGVBUS, USBHS_PHYOTGCTR);
		otg_io_set_bits(&tusb->otg, DISCHRGVBUS, USBHS_PHYOTGCTR);
	} else {
		/* stop discharging */
		otg_io_clear_bits(&tusb->otg, DISCHRGVBUS, USBHS_PHYOTGCTR);
	}
}
static void discharge_vbus_timer(unsigned long data)
{
	struct tusb1211 *tusb = (struct tusb1211 *)data;
	tusb1211_dischrg_vbus(tusb, 0);
}
static void charge_vbus_timer(unsigned long data)
{
	struct tusb1211 *tusb = (struct tusb1211 *)data;
	tusb1211_chrg_vbus(tusb, 0);
	tusb->otg.state = OTG_STATE_B_IDLE;
	printk ("%s\n", otg_state_string(tusb->otg.state));
}

static void data_pls_timer(unsigned long data)
{
	struct tusb1211 *tusb = (struct tusb1211 *)data;
	tusb1211_chrg_vbus(tusb, 1);
	mod_timer(&tusb->b_chrg_vbus_timer, jiffies + msecs_to_jiffies(TB_CHARGE_VBUS));
}

static void srp_fail_timer(unsigned long data)
{
	struct tusb1211 *tusb = (struct tusb1211 *)data;
	u16 vbusvalid_sts;
	vbusvalid_sts = tusb1211_phy_read(&tusb->otg, MPINTSTS) & VBUSVALID_STS;
	if(VBUSVALID_STS == vbusvalid_sts) {
		printk("SRP done!\n");
		tusb->otg.state = OTG_STATE_B_PERIPHERAL;
		printk ("%s\n", otg_state_string(tusb->otg.state));
	} else {
		printk("SRP fail!\n");
	}
}

static void check_se0_timer(unsigned long data)
{
	struct tusb1211 *tusb = (struct tusb1211 *)data;
	u16 line_sts;
	u16 syssts;

	/*Check the data line status*/
	spin_lock(&tusb->lock);
	syssts = otg_read_reg(&tusb->otg, TUSB_SYSSTS);
	spin_unlock(&tusb->lock);
	line_sts = syssts & LNST;

	if(SE0 == line_sts) {
		/* Pull-up DM */
		otg_io_set_bits(&tusb->otg, DPRPU, SYSCFG);
		mod_timer(&tusb->b_data_pls_timer, jiffies + msecs_to_jiffies(TB_DATA_PLS));
		mod_timer(&tusb->b_srp_fail_timer, jiffies + msecs_to_jiffies(TB_SRP_FAIL));
	} else {
		printk("SRP initial condition SE0 is not satisfied\n");
	}
}

/************************************************************************/

/*
 * tusb1211_reset(): Reset the TUSB1211.
 * Argument: None
 * return: None
 */
static void tusb1211_reset(void)
{
	gpio_direction_output(nTUSB_RST, 0);
	udelay(100);
	gpio_direction_output(nTUSB_RST, 1);
}

/*
 * tusb1211_enable_interrupt(): Enable the TUSB1211 interrupt.
 * @otg: otg transceiver data structure.
 * return: None
 */
static void tusb1211_enable_interrupt(struct otg_transceiver *otg)
{
	struct tusb1211 *tusb = container_of(otg, struct tusb1211, otg);
	spin_lock(&tusb->lock);
	otg_io_set_bits(otg, PHYINTR, PHYINTENRISE);
	otg_io_set_bits(otg, PHYINTF, PHYINTENFALL);
	otg_io_set_bits(otg, IDPUUP, USBHS_PHYOTGCTR);
	spin_unlock(&tusb->lock);
}

/*
 * tusb1211_init(): initialize the TUSB1211.
 * @otg: otg transceiver data structure.
 * return:
 *		0: normal termination.
 */
static int tusb1211_init(struct otg_transceiver *otg)
{
	struct tusb1211 *tusb = container_of(otg, struct tusb1211, otg);

	int ret = 0;
	/*Check the initialization status*/
	if (1 == tusb->init) {
#ifdef CONFIG_PRINT_DEBUG
	printk("The TUSB1211 have been initialized\n");
#endif
		return 0;
	}
	/*Enable vck3_clk clock*/
#ifdef CONFIG_HAVE_CLK
	if (!tusb->clk->usecount) {
		ret = clk_enable(tusb->clk);
		if (ret < 0)
			printk(KERN_ERR"Error in enabling TUSB interface clock.\n");
	}
	if (!tusb->iclk->usecount) {
		ret = clk_enable(tusb->iclk);
		if (ret < 0)
			printk(KERN_ERR"Error in enabling TUSB interface clock.\n");
	}
#endif

	/*Request gpio function*/
	/*TUSB_CS Function 0 PORT 130"*/
	gpio_request(TUSB_CS, NULL);
	/*nTUSB_RST Function 0 PORT 131"*/
	gpio_request(nTUSB_RST, NULL);
	/*Set TUSB_CS pin output = 1*/
	gpio_direction_output(TUSB_CS, 1);

	tusb1211_reset();

	tusb1211_enable_interrupt(&tusb->otg);

	tusb->otg.state = OTG_STATE_UNDEFINED;

	/*Set the initialization status*/
	tusb->init = 1;

	return 0;
}

/*
 * tusb1211_start_hnp(): Start the HNP.
 * @otg: otg transceiver data structure
 * return:
 *		0: normal termination
 */
static int tusb1211_start_hnp(struct otg_transceiver *otg)
{
	if ((otg->state == OTG_STATE_A_PERIPHERAL || otg->state == OTG_STATE_B_PERIPHERAL)
		&& (otg->flags == 0) ) {
			otg->flags |= HOST_REQ_FLG;
			msleep(2000);
	} else {
		printk("HNP is not valid\n");
	}
	return 0;
}

/*
 * tusb1211_start_srp(): Start the SRP.
 * @otg: otg transceiver data structure
 * return:
 *		0: normal termination
 *		-EAGAIN: The otg transceiver state is not valid.
 *		-EPROTO: The condition of the protocol is not suitable.
 *		-ENOTCONN: The session is not valid.
 */
static int tusb1211_start_srp(struct otg_transceiver *otg)
{
	struct tusb1211 *tusb = container_of(otg, struct tusb1211, otg);
	u16 syssts;
	u16 line_sts;
	u16 sessend;
	u16 mpint_sts;

	/*Check the otg transceiver state*/
	if(OTG_STATE_B_IDLE != tusb->otg.state) {
		printk("OTG state is not valid.\n");
		return -EAGAIN;
	}

	/* Check the vbus status */
	mpint_sts = tusb1211_phy_read(&tusb->otg, MPINTSTS);
	sessend = mpint_sts & SESSEND_STS;
	if (SESSEND_STS != sessend) {
		printk("Session is not end\n");
		return -EPROTO;
	}
	/* Discharge Vbus */
	otg_io_set_bits(&tusb->otg, DISCHRGVBUS, USBHS_PHYOTGCTR);
	/* Check data line status */
	spin_lock(&tusb->lock);
	syssts = otg_read_reg(otg, TUSB_SYSSTS);
	spin_unlock(&tusb->lock);
	line_sts = syssts & LNST;
	if(SE0 != line_sts) {
		printk("Line status is not idle\n");
		return -EPROTO;
	} else {
		mod_timer(&tusb->se0_srp_timer, jiffies + msecs_to_jiffies(TB_SE0_SRP));
	}

	tusb->otg.state = OTG_STATE_B_SRP_INIT;
	printk ("%s\n", otg_state_string(tusb->otg.state));
	return 0;
}

/*
 * tusb1211_set_suspend(): suspends or resumes the TUSB1211 OTG transceiver.
 * @otg: otg transceiver data structure.
 * @suspend: Indicate to suspend or resume the TUSB1211 OTG  transceiver.
 * 		@suspend = 1 : Suspend the TUSB1211 OTG transceiver.
 * 		@suspend = 0 : Resume the TUSB1211 OTG transceiver.
 * return:
 *		0: Normal termination.
 *		-ENODEV: TUSB1211 OTG transceiver has not been initialized yet.
 *		-EINVAL: Value of suspend argument is invalid.
  *		-EBUSY: Writing Function Control register isn't available.
 *		-ETIMEDOUT: Writing Function Control register cannot complete before timeout (10us).
 */
static int tusb1211_set_suspend(struct otg_transceiver *otg, int suspend)
{
	struct tusb1211 *tusb = container_of(otg, struct tusb1211, otg);
	int result; /* Variable keeps the return value when using otg_io_write() API*/
	int susmonbit; /* Variable monitors SUSMON bit in PHYFUNCTR register of R-Mobile H/W*/
	int suspendmbit; /* Variable monitors SUSPENDM bit in Function Control register of TUSB1211 OTG transceiver*/

	result = 0;
	susmonbit = 0;
	suspendmbit = 0;

	/* Check whether TUSB1211 OTG transceiver is initialized or not */
	if (0 == tusb->init) {
#ifdef CONFIG_PRINT_DEBUG
		printk("TUSB1211 OTG transceiver has not been initialized yet.\n");
#endif
		return -ENODEV;
	}

	/*Check the valid of argument*/
	if ((1 != suspend) && (0 != suspend)) {
#ifdef CONFIG_PRINT_DEBUG
		printk("suspend argument is not valid.\n");
#endif
		return -EINVAL;
	}

	/* Check the validity of suspend argument*/
	if (1 == suspend) {

		/* LOG output for suspending normal termination case:
			PHY is put into Low Power Mode
			TUSB1211 OTG transceiver suspends successfully
			0
		*/

		/* Prohibit to return atomatically from Low Power Mode by PHY-Int signal.*/
		otg_io_set_bits(otg,PINTM,LPCTRL);

		/* Prohibit to return atomatically from Low Power Mode by Hardware.*/
		otg_io_clear_bits(otg,HWUPM,LPCTRL);

		/* Clear SUSPENDM bit in Function Control register to 0 value for putting PHY into Low Power Mode*/
		result = otg_io_write(otg,ULPI_FUNC_CTRL_SUSPENDM,ULPI_CLR(ULPI_FUNC_CTRL));

		/* Checking result of clearing SUSPENDM bit in Function Control register*/
		if ( 0 != result) {
#ifdef CONFIG_PRINT_DEBUG
		printk(" otg_io_write() is not successful\n");
#endif
			return result;
		}
	} else if (0 == suspend) {

		/* LOG output for resuming normal termination case:
			Low Power Mode of PHY is exited
			TUSB1211 OTG transceiver resumes successfully
			0
		*/

		/* Take information about the state of the SuspendM signal to present PHY*/
		susmonbit = otg_read_reg(otg, OTG_PHYFUNCTR);
		susmonbit = (susmonbit & SUSMON);

		/* If TUSB1211 OTG transceiver was suspended, after that resuming it*/
		if (susmonbit != SUSMON ) {
			susmonbit = 0;
			/* Permit to return atomatically from Low Power Mode by Hardware.*/
			otg_io_set_bits(otg,HWUPM,LPCTRL);

			/*  Permit to return atomatically from Low Power Mode by PHY-Int signal.*/
			otg_io_clear_bits(otg,PINTM,LPCTRL);

			/* Wait resuming competion*/
			msleep(50);
		} else {
#ifdef CONFIG_PRINT_DEBUG
			printk("Value of suspend argument equal %d which it isn't valid for function of otg_set_suspend() API\n",suspend);
#endif
			return -EINVAL;
		}
	}
	return 0;
}

/*
 * tusb1211_set_peripheral(): bind/unbind function controller
 * @otg: otg transceiver data structure
 * @periph: function controller is transferred by User
 * return:
 *		0: normal termination
 *		-ENODEV: The OTG transceiver is not initialized.
 */
static int tusb1211_set_peripheral(struct otg_transceiver *otg,
							struct usb_gadget *periph)
{
#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_set_peripheral()\n");
#endif
	int id;
	struct tusb1211 *tusb = container_of(otg, struct tusb1211, otg);

	/* Check the initialization of TUSB1211 OTG transceiver */
	if (0 == tusb->init) {
		return -ENODEV;
	}

	id = otg_read_reg(otg, TUSB_SYSSTS);

	if (periph) {
		/* set gadget information to OTG data structure */
		otg->gadget = periph;

		/* Hold spin lock */
		spin_lock_irq(&tusb->lock);

		/* H/W contrains before setting DCFM bit */
		otg_io_clear_bits(otg,DPRPU|DRPD,SYSCFG);
		otg_io_clear_bits(otg,DCFM,SYSCFG);
		otg_io_set_bits(otg,DPRPU,SYSCFG);

		/* Setting H/W for peripheral operation */
		otg_io_clear_bits(otg,UACT|RWUPE|USBRST|RESUME,DVSTCTR);

		/* Release spin lock */
		spin_unlock_irq(&tusb->lock);

		if (0 == (id&IDMON)) {
			otg->state = OTG_STATE_A_PERIPHERAL;
		} else {
			otg->state = OTG_STATE_B_PERIPHERAL;
		}
	} else {
		/* Unbind function controller */
		otg->gadget = NULL;
		if (0 == (id&IDMON)){
			otg->state = OTG_STATE_A_IDLE;
		} else {
			otg->state = OTG_STATE_B_IDLE;
		}
	}

	return 0;
}

/*
 * tusb1211_shutdown(): shut down otg transceiver
 * @otg: otg transceiver data structure
 * return:
 *		0: normal termination
 *		-ENODEV: The OTG transceiver is not initialized.
 */
static void tusb1211_shutdown(struct otg_transceiver *otg)
{
#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_shutdown()\n");
#endif
	struct tusb1211 *tusb = container_of(otg, struct tusb1211, otg);

		/* Check the initialization of TUSB1211 OTG transceiver */
	if (0 == tusb->init) {
		return;
	}

	/* Hold spin lock */
	spin_lock_irq(&tusb->lock);

	/* Disable all interrupts */
	otg_io_clear_bits(otg,PHYINTENRISE,PHYINTR);
	otg_io_clear_bits(otg,PHYINTENFALL,PHYINTF);
#ifdef CONFIG_HAVE_CLK
	/* Disable extal2 external clock */
	if (tusb->clk->usecount)
		clk_disable(tusb->clk);
	if (tusb->iclk->usecount)
		clk_disable(tusb->iclk);
#endif

	/* Clear status of OTG transceiver */
	otg->state = OTG_STATE_UNDEFINED;

	/* Clear the intialization flag */
	tusb->init = 0;

	/* Reset TUSB1211 external IP */
	otg_io_set_bits(otg, PRESET, OTG_PHYFUNCTR);

	/* Release spin lock */
	spin_unlock_irq(&tusb->lock);

}

/*
 * tusb1211_set_host(): bind/unbind host controller
 * @otg: otg transceiver data structure
 * @host: host controller is transferred by User
 * return:
 *		0: normal termination
 *		-ENODEV: The OTG transceiver is not initialized.
 */
static int tusb1211_set_host(struct otg_transceiver *otg, struct usb_bus *host)
{

	struct tusb1211	*tusb = container_of(otg, struct tusb1211, otg);
#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_set_host()\n");
#endif
	/* Check the initialization of TUSB1211 OTG transceiver */
	if (0 == tusb->init) {
#ifdef CONFIG_PRINT_DEBUG
	printk("The TUSB1211 have not been initialized yet\n");
#endif
		return -ENODEV;
	}
	/* Check to bind host */
	if (host) {
		tusb->otg.host = host;
		/*	Check for OTG A-device or not*/
		if (0 == tusb->otg.default_a) {
			tusb->otg.state = OTG_STATE_B_HOST;
		} else {
			tusb->otg.state = OTG_STATE_A_HOST;
		}
	} else {
	/* Unbind host */
		tusb->otg.state = OTG_STATE_UNDEFINED;
		tusb->otg.host = NULL;
		return 0;
	}

	/* Bind host */
	/*
	* H/W setting
	*/
	spin_lock(&tusb->lock);
	/* H/W contrains before setting DCFM bit */
	otg_io_clear_bits(&tusb->otg, DPRPU | DRPD, SYSCFG);
	otg_io_set_bits(&tusb->otg, DCFM | DRPD, SYSCFG);

	/* Setting H/W for host operation */
	otg_io_set_bits(&tusb->otg, UACT | RWUPE | USBRST | RESUME, DVSTCTR);

	/* Enable interrupt Falling and Raising	*/
	otg_io_set_bits(&tusb->otg, IDGND_RISE | SESSEND_RISE | SESSVALID_RISE
							| VBUSVALID_RISE | HOSTDISCONNECT_RISE, PHYINTR);
	otg_io_set_bits(&tusb->otg, IDGND_FALL | SESSEND_FALL | SESSVALID_FALL
							| VBUSVALID_FALL | HOSTDISCONNECT_FALL, PHYINTF);

	spin_unlock(&tusb->lock);

	return 0;
}


/*
 * tusb1211_set_vbus(): enable/disable vbus supply
 * @otg: otg transceiver data structure
 * @enabled:
 *		true: enable vbus
 *		false: disable vbus
 * return:
 *		0: normal termination
 *		-ENODEV: Not register device.This error occurs when
 *				no PMIC device is registered to PMIC Interface.
 *				PMIC device has to be registered to PMIC Interface
 *				before calling this API.
 *		Others: I2C fault error codes. These errors occur
 *				when I2C transfer data doesn't finish normally.
 */
static int tusb1211_set_vbus(struct otg_transceiver *otg, bool enabled)
{
	int ret = 0;
	struct tusb1211	*tusb = container_of(otg, struct tusb1211, otg);
	if (0 == tusb->otg.default_a) {
#ifdef CONFIG_PRINT_DEBUG
		printk("Only A-device can supply VBus \n");
#endif
		return 0;
	}
	if (true == enabled) {
		/* change to boost mode */
#ifdef CONFIG_USB_OTG
		set_otg_vbus(1);
#endif
	} else {
		/* change to charger mode */
#ifdef CONFIG_USB_OTG
		set_otg_vbus(0);
#endif
	}
	if ( ret != 0 ) {
		dev_err(otg->dev, "otg_set_vbus: Error occurs when calling set_otg_vbus\n");
	}
	return ret;
}

/*
 * tusb1211_io_read: read value of the target register of TUSB1211 OTG transceiver.
* @otg: data structure to manage OTG requirements.
* @reg: register address of TUSB1211 OTG transceiver.
 * return:
 *     >=0: value of target register.
 *     -ENODEV: TUSB1211 OTG transceiver has not been initialized yet.
 *     -EINVAL: register address is invalid.
 *     -EBUSY: reading register isn't available.
 *     -ETIMEDOUT: reading register cannot complete before timeout (10us).
 */
static int  tusb1211_io_read(struct otg_transceiver *otg, u32 reg)
{
	struct tusb1211	*tusb = container_of(otg, struct tusb1211, otg);
	int cnt = 0;			/*count value for checking time-out*/
	unsigned long   flags;	/*spin-lock flag*/
	u16 spctrl;				/* value of PHY Single Access Control Register */

#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_io_read()\n");
#endif

	/*Check whether TUSB1211 OTG transceiver is initialized or not*/
	if (0 == tusb->init) {
		return -ENODEV;
	}

	/*Check whether value of register is valid or not*/
	if ((reg > TUSB1211_MAX_ADDR) || (reg == USB_ULPI_EXTENDED)){
		printk(KERN_ERR "tusb1211_io_read: Invalid register address\n");
		return -EINVAL;
	}

	/*Check the condition of reading*/
	spctrl = otg_read_reg(otg, USB_TUSB_SPCTRL);
	if ((spctrl & ULPI_READ) || (spctrl & ULPI_WRITE)) {
		printk(KERN_ERR "tusb1211_io_read: Not available\n");
		return -EBUSY;
	}

	/* Hold spin lock */
	spin_lock_irqsave(&tusb->lock, flags);

	/*Initiate read operation*/
	otg_write_reg(otg, reg, USB_TUSB_SPADDR); /*Set address of register needs to be read*/
	otg_io_set_bits(otg, ULPI_READ, USB_TUSB_SPCTRL); /*Enable reading operation*/
	otg_write_reg(otg, USB_START_READ, USB_PHYREAD);/*Start reading*/

	/* Release spin lock */
	spin_unlock_irqrestore(&tusb->lock, flags);

	/* wait at most 10us for completion of reading*/
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		spctrl = otg_read_reg(otg, USB_TUSB_SPCTRL);
		if (!(spctrl & ULPI_READ))
			break;
		udelay(1);
		cnt++;
	}

	/* Can't complete reading before timeout*/
	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		printk(KERN_ERR "otg_io_read: timeout\n");
		return -ETIMEDOUT;
	}

	/*Return the value of register*/
	return ULPI_DATA(otg_read_reg(otg, USB_TUSB_SPRDAT));
}

/*
 * tusb1211_io_write: write value to the target register of TUSB1211 OTG transceiver.
* @otg: data structure to manage OTG requirements.
* @val: setting value.
* @reg: register address of TUSB1211 OTG transceiver.
 * return:
 *     0: normal termination.
 *     -ENODEV: TUSB1211 OTG transceiver has not been initialized yet.
 *     -EINVAL: register address or written value is invalid.
 *     -EBUSY: writing register isn't available.
 *     -ETIMEDOUT: writting register cannot complete before timeout (10us).
 */
static int tusb1211_io_write(struct otg_transceiver *otg, u32 val, u32 reg)
{
	struct tusb1211	*tusb = container_of(otg, struct tusb1211, otg);
	int cnt = 0;			/*count value for checking time-out*/
	unsigned long   flags;	/*spin-lock flag*/
	u16 spctrl;				/* value of PHY Single Access Control Register */

#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_io_write()\n");
#endif

	/*Check whether TUSB1211 OTG transceiver is initialized or not*/
	if (0 == tusb->init) {
		return -ENODEV;
	}

	/*Check whether input value is valid or not*/
	if (reg > TUSB1211_MAX_ADDR || (reg == USB_ULPI_EXTENDED) || val > USB_MAX_REG_VAL) {
		printk("tusb1211_io_write: Invalid parameters\n");
		return -EINVAL;
	}

	/*Check the condition of writting*/
	spctrl = otg_read_reg(otg, USB_TUSB_SPCTRL);
	if ((spctrl & ULPI_READ) || (spctrl & ULPI_WRITE)) {
		printk("tusb1211_io_write: Not available\n");
		return -EBUSY;
	}

	/* Hold spin lock */
	spin_lock_irqsave(&tusb->lock, flags);

	/*Initiate write operation*/
	otg_write_reg(otg, reg, USB_TUSB_SPADDR); /*Set address of register needs to be written*/
	otg_write_reg(otg, ULPI_DATA(val), USB_TUSB_SPWDAT); /*Set written value*/
	otg_io_set_bits(otg, ULPI_WRITE, USB_TUSB_SPCTRL); /*Enable writting operation*/

	/* Release spin lock */
	spin_unlock_irqrestore(&tusb->lock, flags);

	/* wait at most 10us for completion of writting*/
	while (cnt < ULPI_IO_TIMEOUT_USEC) {
		spctrl = otg_read_reg(otg, USB_TUSB_SPCTRL);
		if (!(spctrl & ULPI_WRITE))
			break;
		udelay(1);
		cnt++;
	}

	/* Can't complete reading before timeout*/
	if (cnt >= ULPI_IO_TIMEOUT_USEC) {
		printk("otg_io_write: timeout\n");
		return -ETIMEDOUT;
	}

	/*Return 0 if sucessfully*/
	return 0;
}

/*
 * tusb1211_usb_remove(): remove OTG transceiver from platform
 * @pdev: platform which remove OTG transceiver
 * return:
 *		0: normal termination
 */
static int __exit tusb1211_remove(struct platform_device *pdev)
{
	int ret = 0;
#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_remove()\n");
#endif
	struct tusb1211		*tusb = dev_get_drvdata(&pdev->dev);

	del_timer(&tusb->b_srp_fail_timer);
	del_timer(&tusb->b_dischrg_vbus_timer);
	del_timer(&tusb->b_chrg_vbus_timer);
	del_timer(&tusb->b_data_pls_timer);
	del_timer(&tusb->se0_srp_timer);

	ret = cancel_delayed_work_sync(&tusb->vbus_work);
	if (ret < 0)
		printk(KERN_ERR"Error in cancelling delayed work queue.\n");
	ret = cancel_delayed_work_sync(&tusb->vbus_off_work);
	if (ret < 0)
		printk(KERN_ERR"Error in cancelling delayed work queue.\n");

	ret = pm_runtime_put(&pdev->dev);
	if (ret < 0)
		printk(KERN_ERR"Error in syncing pm runtime call.\n");
	pm_runtime_disable(&pdev->dev);
	/* Clear data information of otg transceiver */
	otg_set_transceiver(NULL);
	xceiver = NULL;

	platform_set_drvdata(pdev, NULL);
	/* free memory for structure tusb*/
	kfree(tusb);

	/* unmap register */
	iounmap(tusb->otg.io_priv);
#ifdef CONFIG_HAVE_CLK
	clk_put(tusb->iclk);
	clk_put(tusb->clk);
#endif
	return 0;
}

/*
 * tusb1211_probe: initialize USB OTG
* @pdev: USB OTG Controller being probed
 * return:
 *     0: successful
 *     -ENOMEM: insufficient memory.
 *     -ENODEV: can not get resource (memory)
 */
static int __devinit tusb1211_probe(struct platform_device *pdev)
{
	struct resource *res, *ires;
	void __iomem *reg = NULL;
	struct tusb1211 *tusb;
	int ret = 0;
#ifdef CONFIG_HAVE_CLK
	char clk_name[15];
#endif
#ifdef CONFIG_PRINT_DEBUG
	printk("tusb1211_probe()\n");
#endif

	/*Get the platform resource from platform device that register into linux device driver model*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		printk(KERN_ERR "platform_get_resource error.\n");
		return -ENODEV;
	}

	/*Get CPU access to register of USB OTG*/
	reg = ioremap(res->start, resource_size(res));
	if (reg == NULL) {
		printk(KERN_ERR "ioremap error.\n");
		return -ENOMEM;
	}

	ires = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	/* Allocate internal structure that keeps the information of transceiver */
	tusb = kzalloc(sizeof *tusb, GFP_KERNEL);
	if (tusb == NULL) {
		printk("Can't allocate the tusb1211");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, tusb);

	/* Enable pm runtime */
	pm_runtime_enable(&pdev->dev);
#ifdef CONFIG_HAVE_CLK
	/* Lookup USB-PHY clock */
	strcpy(clk_name, "vclk3_clk");
	tusb->clk = clk_get(NULL, clk_name);
	ret = IS_ERR(tusb->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot get clock \"%s\"\n", clk_name);
		goto err0;
	}
	/* USBHS interface clock */
	tusb->iclk = clk_get(&pdev->dev, NULL);
	ret = IS_ERR(tusb->iclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot get clock\n");
		clk_put(tusb->clk);
		goto err0;
	}
#endif

	/* Init the spinlock */
	spin_lock_init(&tusb->lock);

	/* Initialize USB OTG structure */
	tusb->otg.label = "tusb1211";
	tusb->otg.io_priv = reg;
	tusb->otg.io_ops = &tusb1211_otg_io_ops;

	tusb->otg.init = tusb1211_init;
	tusb->otg.shutdown = tusb1211_shutdown;
	tusb->otg.start_hnp = tusb1211_start_hnp;
	tusb->otg.start_srp = tusb1211_start_srp;
	tusb->otg.set_vbus = tusb1211_set_vbus;
	tusb->otg.set_host = tusb1211_set_host;
	tusb->otg.set_peripheral = tusb1211_set_peripheral;
	tusb->otg.set_suspend = tusb1211_set_suspend;

	tusb->otg.dev = &pdev->dev;
	tusb->pdata = pdev->dev.platform_data;
	otg_set_transceiver(&tusb->otg);

	tusb->vbus_enable = 0;
	INIT_DELAYED_WORK(&tusb->vbus_work, tusb_vbus_work);
	INIT_DELAYED_WORK(&tusb->vbus_off_work, tusb_vbus_off_work);

	/* Timer for SRP */
	init_timer(&tusb->se0_srp_timer);
	tusb->se0_srp_timer.function = check_se0_timer;
	tusb->se0_srp_timer.data = (unsigned long)tusb;

	/* Timer for Data Pulse */
	init_timer(&tusb->b_data_pls_timer);
	tusb->b_data_pls_timer.function = data_pls_timer;
	tusb->b_data_pls_timer.data = (unsigned long)tusb;

	/* Timer for VBus Charge */
	init_timer(&tusb->b_chrg_vbus_timer);
	tusb->b_chrg_vbus_timer.function = charge_vbus_timer;
	tusb->b_chrg_vbus_timer.data = (unsigned long)tusb;

	/* Timer for VBus discharge */
	init_timer(&tusb->b_dischrg_vbus_timer);
	tusb->b_dischrg_vbus_timer.function = discharge_vbus_timer;
	tusb->b_dischrg_vbus_timer.data = (unsigned long)tusb;

	/* Timers for SRP failure */
	init_timer(&tusb->b_srp_fail_timer);
	tusb->b_srp_fail_timer.function = srp_fail_timer;
	tusb->b_srp_fail_timer.data = (unsigned long)tusb;

	tusb_class = class_create(THIS_MODULE, "tusb");
	if (IS_ERR(tusb_class)) {
		ret = PTR_ERR(tusb_class);
		goto err5;
	}
	ret = class_create_file(tusb_class, &class_attr_tusb_srp);
	if (ret < 0) {
		printk("Can't create tusb class\n");
	}
	ret = class_create_file(tusb_class, &class_attr_srp);
	if (ret < 0) {
		printk("Can't create tusb class file system\n");
	}
	ret = class_create_file(tusb_class, &class_attr_hnp);
	if (ret < 0) {
		printk("Can't create tusb class file system\n");
	}

	xceiver = tusb;
	printk("%s\n", __func__);
	return 0;

err5:
	del_timer(&tusb->b_srp_fail_timer);
	del_timer(&tusb->b_dischrg_vbus_timer);
	del_timer(&tusb->b_chrg_vbus_timer);
	del_timer(&tusb->b_data_pls_timer);
	del_timer(&tusb->se0_srp_timer);
#ifdef CONFIG_HAVE_CLK
	clk_put(tusb->iclk);
	clk_put(tusb->clk);
err0:
#endif
	ret = pm_runtime_put(&pdev->dev);
	if (ret < 0)
		printk(KERN_ERR"Error in giving up pm runtime call.\n");
	pm_runtime_disable(&pdev->dev);
	kfree(tusb);
	iounmap(reg);
	return ret;
}
static struct platform_driver tusb1211_driver = {
	.probe = tusb1211_probe,
	.remove = tusb1211_remove,
	.driver = {
		.name = "tusb1211_driver",
		.owner = THIS_MODULE,
	},
};

static int __init tusb1211_module_init(void)
{
	int result;

	result = 0;
	result = platform_driver_register(&tusb1211_driver);
	if(result < 0) {
#ifdef CONFIG_PRINT_DEBUG
		printk("Can't init module tusb1211\n");
#endif
	}
	return result;
}
static void __exit tusb1211_module_exit(void)
{
	platform_driver_unregister(&tusb1211_driver);
}

module_init(tusb1211_module_init);
module_exit(tusb1211_module_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("TUSB1211 USB transceiver driver");
