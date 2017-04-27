/*
 * d2153-irq.c: IRQ support for Dialog D2153
 *
 * Copyright(c) 2012 Dialog Semiconductor Ltd.
 *
 * Author: Dialog Semiconductor Ltd. D. Chen, D. Patel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bug.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/kthread.h>

#include <asm/gpio.h>

#include <linux/d2153/pmic.h>
#include <linux/d2153/d2153_reg.h>
#include <linux/d2153/hwmon.h>
#include <linux/d2153/rtc.h>
#include <linux/d2153/core.h>

#include <mach/common.h>

#define D2153_NUM_IRQ_MASK_REGS			3

/* Number of IRQ and IRQ offset */
enum {
	D2153_INT_OFFSET_1 = 0,
	D2153_INT_OFFSET_2,
	D2153_INT_OFFSET_3,
	D2153_NUM_IRQ_EVT_REGS
};

/* struct of IRQ's register and mask */
struct d2153_irq_data {
	int reg;
	int mask;
};

int d2153_chip_irq;

static struct d2153_irq_data d2153_irqs[] = {
	/* EVENT Register A start */
	[D2153_IRQ_EVF] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_VF_MASK,
	},
	[D2153_IRQ_EADCIN1] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_ADCIN1_MASK,
	},
	[D2153_IRQ_ETBAT2] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_TBAT2_MASK,
	},
	[D2153_IRQ_EVDD_LOW] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_VDD_LOW_MASK,
	},
	[D2153_IRQ_EVDD_MON] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_VDD_MON_MASK,
	},
	[D2153_IRQ_EALARM] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_ALARM_MASK,
	},
	[D2153_IRQ_ESEQRDY] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_SEQ_RDY_MASK,
	},
	[D2153_IRQ_ETICK] = {
		.reg = D2153_INT_OFFSET_1,
		.mask = D2153_M_TICK_MASK,
	},
	/* EVENT Register B start */
	[D2153_IRQ_ENONKEY_LO] = {
		.reg = D2153_INT_OFFSET_2,
		.mask = D2153_M_NONKEY_LO_MASK,
	},
	[D2153_IRQ_ENONKEY_HI] = {
		.reg = D2153_INT_OFFSET_2,
		.mask = D2153_M_NONKEY_HI_MASK,
	},
	[D2153_IRQ_ENONKEY_HOLDON] = {
		.reg = D2153_INT_OFFSET_2,
		.mask = D2153_M_NONKEY_HOLD_ON_MASK,
	},
	[D2153_IRQ_ENONKEY_HOLDOFF] = {
		.reg = D2153_INT_OFFSET_2,
		.mask = D2153_M_NONKEY_HOLD_OFF_MASK,
	},
	[D2153_IRQ_ETBAT1] = {
		.reg = D2153_INT_OFFSET_2,
		.mask = D2153_M_TBAT1_MASK,
	},
	[D2153_IRQ_EADCEOM] = {
		.reg = D2153_INT_OFFSET_2,
		.mask = D2153_M_ADC_EOM_MASK,
	},
	/* EVENT Register C start */
 	[D2153_IRQ_ETA] = {
		.reg = D2153_INT_OFFSET_3,
		.mask = D2153_M_GPI_3_M_TA_MASK,
	},
	[D2153_IRQ_ENJIGON] = {
		.reg = D2153_INT_OFFSET_3,
		.mask = D2153_M_GPI_4_M_NJIG_ON_MASK,
	},
	[D2153_IRQ_EACCDET] = {
		.reg = D2153_INT_OFFSET_3,
		.mask = D2153_M_ACC_DET_MASK,
	},
	[D2153_IRQ_EJACKDET] = {
		.reg = D2153_INT_OFFSET_3,
		.mask = D2153_M_JACK_DET_MASK,
	},
};

void d2153_set_irq_disable(void)
{
	disable_irq(d2153_chip_irq);
}

void d2153_set_irq_enable(void)
{
	enable_irq(d2153_chip_irq);
}

static void d2153_irq_call_handler(struct d2153 *d2153, int irq)
{
	mutex_lock(&d2153->irq_mutex);

	if (d2153->irq[irq].handler) {
		d2153->irq[irq].handler(irq, d2153->irq[irq].data);

	} else {
		d2153_mask_irq(d2153, irq);
	}
	mutex_unlock(&d2153->irq_mutex);
}

/*
 * This is a threaded IRQ handler so can access I2C.  Since all
 * interrupts are clear on read the IRQ line will be reasserted and
 * the physical IRQ will be handled again if another interrupt is
 * asserted while we run - in the normal course of events this is a
 * rare occurrence so we save I2C/SPI reads.
 */
void d2153_irq_worker(struct work_struct *work)
{
	struct d2153 *d2153 = container_of(work, struct d2153, irq_work);
	u8 reg_val;
	u8 sub_reg[D2153_NUM_IRQ_EVT_REGS] = {0,};
	int read_done[D2153_NUM_IRQ_EVT_REGS];
	struct d2153_irq_data *data;
	int i;

	memset(&read_done, 0, sizeof(read_done));

	for (i = 0; i < ARRAY_SIZE(d2153_irqs); i++) {
		data = &d2153_irqs[i];

		if (!read_done[data->reg]) {
			d2153_reg_read(d2153, D2153_EVENT_A_REG + data->reg, &reg_val);
			sub_reg[data->reg] = reg_val;
			d2153_reg_read(d2153, D2153_IRQ_MASK_A_REG + data->reg, &reg_val);
			sub_reg[data->reg] &= ~reg_val;
			read_done[data->reg] = 1;
		}

    	if (sub_reg[data->reg] & data->mask) {
    		d2153_irq_call_handler(d2153, i);
			/* Now clear EVENT registers */
			d2153_set_bits(d2153, D2153_EVENT_A_REG + data->reg, d2153_irqs[i].mask);
		}
	}
	enable_irq(d2153->chip_irq);
	dev_info(d2153->dev, "IRQ Generated [d2153_irq_worker EXIT]\n");
}



static irqreturn_t d2153_irq(int irq, void *data)
{
	struct d2153 *d2153 = data;
	u8 reg_val;
	u8 sub_reg[D2153_NUM_IRQ_EVT_REGS] = {0,};
	int read_done[D2153_NUM_IRQ_EVT_REGS];
	struct d2153_irq_data *pIrq;
	int i;

	memset(&read_done, 0, sizeof(read_done));

	for (i = 0; i < ARRAY_SIZE(d2153_irqs); i++) {
		pIrq = &d2153_irqs[i];

		if (!read_done[pIrq->reg]) {
			d2153_reg_read(d2153, D2153_EVENT_A_REG + pIrq->reg, &reg_val);
			sub_reg[pIrq->reg] = reg_val;
			d2153_reg_read(d2153, D2153_IRQ_MASK_A_REG + pIrq->reg, &reg_val);
			sub_reg[pIrq->reg] &= ~reg_val;
			read_done[pIrq->reg] = 1;
		}

		if (sub_reg[pIrq->reg] & pIrq->mask) {
			d2153_irq_call_handler(d2153, i);
			/* Now clear EVENT registers */
			d2153_set_bits(d2153, D2153_EVENT_A_REG + pIrq->reg, d2153_irqs[i].mask);
		}
	}
	return IRQ_HANDLED;
}


int d2153_register_irq(struct d2153 * const d2153, int const irq,
			irq_handler_t handler, unsigned long flags,
			const char * const name, void * const data)
{
	if (irq < 0 || irq >= D2153_NUM_IRQ || !handler)
		return -EINVAL;

	if (d2153->irq[irq].handler)
		return -EBUSY;
	mutex_lock(&d2153->irq_mutex);
	d2153->irq[irq].handler = handler;
	d2153->irq[irq].data = data;
	mutex_unlock(&d2153->irq_mutex);
	/* DLG Test Print */
    dev_info(d2153->dev, "\nIRQ After MUTEX UNLOCK [%s]\n", __func__);

	d2153_unmask_irq(d2153, irq);
	return 0;
}
EXPORT_SYMBOL_GPL(d2153_register_irq);

int d2153_free_irq(struct d2153 *d2153, int irq)
{
	if (irq < 0 || irq >= D2153_NUM_IRQ)
		return -EINVAL;

	d2153_mask_irq(d2153, irq);

	mutex_lock(&d2153->irq_mutex);
	d2153->irq[irq].handler = NULL;
	mutex_unlock(&d2153->irq_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(d2153_free_irq);

int d2153_mask_irq(struct d2153 *d2153, int irq)
{
	return d2153_set_bits(d2153, D2153_IRQ_MASK_A_REG + d2153_irqs[irq].reg,
			       d2153_irqs[irq].mask);
}
EXPORT_SYMBOL_GPL(d2153_mask_irq);

int d2153_unmask_irq(struct d2153 *d2153, int irq)
{
	dev_info(d2153->dev, "\nIRQ[%d] Register [%d] MASK [%d]\n",irq, D2153_IRQ_MASK_A_REG + d2153_irqs[irq].reg, d2153_irqs[irq].mask);
	return d2153_clear_bits(d2153, D2153_IRQ_MASK_A_REG + d2153_irqs[irq].reg,
				 d2153_irqs[irq].mask);
}
EXPORT_SYMBOL_GPL(d2153_unmask_irq);

int d2153_irq_init(struct d2153 *d2153, int irq,
		    struct d2153_platform_data *pdata)
{
	int ret = -EINVAL;
	int reg_data, maskbit;

	if (0 == irq) {
		dev_err(d2153->dev, "No IRQ configured\n");
		return -EINVAL;
	}
	reg_data = 0xFFFFFF;
	d2153_block_write(d2153, D2153_EVENT_A_REG, D2153_NUM_IRQ_EVT_REGS, (u8 *)&reg_data);
	reg_data = 0;
	d2153_block_write(d2153, D2153_EVENT_A_REG, D2153_NUM_IRQ_EVT_REGS, (u8 *)&reg_data);

	/* Clear Mask register starting with Mask A*/
	maskbit = 0xFFFFFF;
	d2153_block_write(d2153, D2153_IRQ_MASK_A_REG, D2153_NUM_IRQ_MASK_REGS, (u8 *)&maskbit);

	mutex_init(&d2153->irq_mutex);

	if (irq) {
		ret = request_threaded_irq(irq, NULL, d2153_irq,
									IRQF_TRIGGER_LOW|IRQF_ONESHOT,
				  					"d2153", d2153);
		if (ret != 0) {
			dev_err(d2153->dev, "Failed to request IRQ: %d\n", irq);
			return ret;
		}
		dev_info(d2153->dev, "# IRQ configured [%d]\n", irq);
	}

	enable_irq_wake(irq);
	
	d2153->chip_irq = irq;	
	d2153_chip_irq = irq;
	
	return ret;
}

int d2153_irq_exit(struct d2153 *d2153)
{
	free_irq(d2153->chip_irq, d2153);
	return 0;
}
