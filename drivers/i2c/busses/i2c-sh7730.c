/*
 * I2C bus driver for the SH7730 I2C Interfaces.
 * Copyright (C) 2012
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/i2c/i2c-sh_mobile.h>
#include <linux/module.h>

/* BIT VALUE */
#define BIT_CLR  0
#define BIT_SET  1

static inline unsigned char read_reg(void *ioadder)
{
	return ioread8(ioadder);
}
static inline void write_reg(unsigned char val, void *ioadder)
{
	iowrite8(val, ioadder);
	return;
}

/* Register offset */
#define ICCR1   0   /* I2C bus control register 1 */
#define ICCR2   1   /* I2C bus control register 2 */
#define ICMR    2   /* I2C bus mode register */
#define ICIER   3   /* I2C bus interrupt enable register */
#define ICSR    4   /* I2C bus status register */
#define ICDRT   6   /* I2C bus transmit data register */
#define ICDRR   7   /* I2C bus receive data register */

/* ICCR1 bits */
#define CR1_ICE     0x80    /* 7 ICE       0     R/W I2C Bus Interface Enable */
#define CR1_RCVD    0x40    /* 6 RCVD      0     R/W Reception Disable */
#define CR1_MST     0x20    /* 5 MST       0     R/W Master/Slave Select */
#define CR1_TRS     0x10    /* 4 TRS       0     R/W Transmit/Receive Select */
#define CR1_CKS     0x0f    /* 3 CKS[3:0]  0000  R/W Transfer Clock Select */

/* ICCR1 Transfer Clock Select CR1_CKS */
			       /* Module clock = 48.0MHz -> */
#define CR1_CKS_MC0060  0x0    /* Module clock/60   -> 800.0 kHz */
#define CR1_CKS_MC0076  0x1    /* Module clock/76   -> 631.6 kHz */
#define CR1_CKS_MC0092  0x2    /* Module clock/92   -> 521.7 kHz */
#define CR1_CKS_MC0108  0x3    /* Module clock/108  -> 444.4 kHz */
#define CR1_CKS_MC0120  0x4    /* Module clock/120  -> 400.0 kHz */
#define CR1_CKS_MC0168  0x5    /* Module clock/168  -> 285.7 kHz */
#define CR1_CKS_MC0252  0x6    /* Module clock/252  -> 190.5 kHz */
#define CR1_CKS_MC0300  0x7    /* Module clock/300  Setting prohibited */
#define CR1_CKS_MC0240  0x8    /* Module clock/240  -> 200.0 kHz */
#define CR1_CKS_MC0304  0x9    /* Module clock/304  -> 157.9 kHz */
#define CR1_CKS_MC0368  0xa    /* Module clock/368  -> 130.4 kHz */
#define CR1_CKS_MC0432  0xb    /* Module clock/432  -> 111.1 kHz */
#define CR1_CKS_MC0480  0xc    /* Module clock/480  -> 100.0 kHz */
#define CR1_CKS_MC0672  0xd    /* Module clock/672  ->  71.4 kHz */
#define CR1_CKS_MC1008  0xe    /* Module clock/1008 ->  47.6 kHz */
#define CR1_CKS_MC1200  0xf    /* Module clock/1200 Setting prohibited */

/* ICCR2 bits */
#define CR2_BBSY    0x80    /* 7 BBSY   0 R/W Bus Busy */
#define CR2_SCP     0x40    /* 6 SCP    1 R/W Start/Stop Issue */
			    /*                Condition Disable */
#define CR2_SCLO    0x08    /* 3 SCLO   1 R   SCL Output Level */
#define CR2_IICRST  0x01    /* 1 IICRST 0 R/W IIC Control Part Reset */

/* ICMR bits */
#define MR_MLS  0x80    /* 7 MLS     0   R/W MSB-First/LSB-First Select */
#define MR_BCWP 0x08    /* 3 BCWP    1   R/W BC Write Protect */
#define MR_BC   0x07    /* 2 BC[2:0] 000 R/W Bit Counter */

/* ICMR MSB-First/LSB-First Select MR_MLS */
#define MR_MLS_MSB 0x0  /* 0: MSB-first */
#define MR_MLS_LSB 0x1  /* 1: LSB-first */

/* ICMR Bit Counter MR_BC */
#define MR_BC_9    0x0  /* 9 bits */
#define MR_BC_2    0x1  /* 2 bits */
#define MR_BC_3    0x2  /* 3 bits */
#define MR_BC_4    0x3  /* 4 bits */
#define MR_BC_5    0x4  /* 5 bits */
#define MR_BC_6    0x5  /* 6 bits */
#define MR_BC_7    0x6  /* 7 bits */
#define MR_BC_8    0x7  /* 8 bits */

/* ICIER bits */
#define IER_TIE     0x80    /* 7 TIE   0 R/W Transmit Interrupt Enable */
#define IER_TEIE    0x40    /* 6 TEIE  0 R/W Transmit End Interrupt Enable */
#define IER_RIE     0x20    /* 5 RIE   0 R/W Receive Interrupt Enable */
#define IER_NAKIE   0x10    /* 4 NAKIE 0 R/W NACK Receive Interrupt Enable */
#define IER_STIE    0x08    /* 3 STIE  0 R/W Stop Condition Detection */
			    /*               Interrupt Enable */
#define IER_ACKE    0x04    /* 2 ACKE  0 R/W Acknowledge Bit Judgment Select */
#define IER_ACKBR   0x02    /* 1 ACKBR 0 R   Receive Acknowledge */
#define IER_ACKBT   0x01    /* 0 ACKBT 0 R/W Transmit Acknowledge */

/* ICSR bits */
#define SR_TDRE   0x80    /* 7 TDRE   0 R/W Transmit Data Register Empty */
#define SR_TEND   0x40    /* 6 TEND   0 R/W Transmit End */
#define SR_RDRF   0x20    /* 5 RDRF   0 R/W Receive Data Full */
#define SR_NACKF  0x10    /* 4 NACKF  0 R/W No Acknowledge Detection Flag */
#define SR_STOP   0x08    /* 3 STOP   0 R/W Stop Condition Detection Flag */
#define SR_AL_OVE 0x04    /* 2 AL/OVE 0 R/W Arbitration Lost Flag/ */
			  /*		    Overrun Error Flag */
#define SR_AAS    0x02    /* 1 AAS    0 R/W Slave Address Recognition Flag */
#define SR_ADZ    0x01    /* 0 ADZ    0 R/W General Call Address */
			  /*		    Recognition Flag */

/* ICCR1 access */
static inline unsigned char read_ICCR1(void *iobase)
{
	return read_reg(iobase + ICCR1);
}
static inline void write_ICCR1(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICCR1);
	return;
}
static inline int Is_enable_CR1_ICE(unsigned char reg)
{
	return reg & CR1_ICE;
}
static inline int Is_disable_CR1_RCVD(unsigned char reg)
{
	return reg & CR1_RCVD;
}
static inline int Is_master_CR1_MST(unsigned char reg)
{
	return reg & CR1_MST;
}
static inline int Is_transmit_CR1_TRS(unsigned char reg)
{
	return reg & CR1_TRS;
}
static inline unsigned char get_CR1_CKS(unsigned char reg)
{
	return reg & CR1_CKS;
}
static inline unsigned char set_CR1_ICE(unsigned char val, unsigned char reg)
{
	reg &= ~CR1_ICE;
	if (val)
		reg |= CR1_ICE;
	return reg;
}
static inline unsigned char set_CR1_RCVD(unsigned char val, unsigned char reg)
{
	reg &= ~CR1_RCVD;
	if (val)
		reg |= CR1_RCVD;
	return reg;
}
static inline unsigned char set_CR1_MST(unsigned char val, unsigned char reg)
{
	reg &= ~CR1_MST;
	if (val)
		reg |= CR1_MST;
	return reg;
}
static inline unsigned char set_CR1_TRS(unsigned char val, unsigned char reg)
{
	reg &= ~CR1_TRS;
	if (val)
		reg |= CR1_TRS;
	return reg;
}
static inline unsigned char set_CR1_CKS(unsigned char val, unsigned char reg)
{
	reg &= ~CR1_CKS;

	switch (val) {
 /* case CR1_CKS_MC0060: break;
	case CR1_CKS_MC0076: break;
	case CR1_CKS_MC0092: break;
	case CR1_CKS_MC0108: break;
	case CR1_CKS_MC0120: break;
	case CR1_CKS_MC0168: break;
	case CR1_CKS_MC0252: break; */
	case CR1_CKS_MC0300:
	    /* Module clock/300 Setting prohibited */
	    val = CR1_CKS_MC0252;
	    break;
 /* case CR1_CKS_MC0240: break;
	case CR1_CKS_MC0304: break;
	case CR1_CKS_MC0368: break;
	case CR1_CKS_MC0432: break;
	case CR1_CKS_MC0480: break;
	case CR1_CKS_MC0672: break;
	case CR1_CKS_MC1008: break; */
	case CR1_CKS_MC1200:
	    /* Module clock/1200 Setting prohibited */
	    val = CR1_CKS_MC1008;
	    break;
	default:
	    break;
	}

	reg |= (val & CR1_CKS);

	return reg;
}
/* ICCR2 access */
static inline unsigned char read_ICCR2(void *iobase)
{
	return read_reg(iobase + ICCR2);
}
static inline void write_ICCR2(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICCR2);
	return;
}
static inline int Is_busy_CR2_BBSY(unsigned char reg)
{
	return reg & CR2_BBSY;
}
static inline int Is_high_CR2_SCLO(unsigned char reg)
{
	return reg & CR2_SCLO;
}
static inline unsigned char set_CR2_BBSY(unsigned char val, unsigned char reg)
{
	reg &= ~CR2_BBSY;
	if (val)
		reg |= CR2_BBSY;
	return reg;
}
static inline unsigned char set_CR2_SCP(unsigned char val, unsigned char reg)
{
	reg &= ~CR2_SCP;
	if (val)
		reg |= CR2_SCP;
	return reg;
}
static inline unsigned char set_CR2_IICRST(unsigned char val, unsigned char reg)
{
	reg &= ~CR2_IICRST;
	if (val)
		reg |= CR2_IICRST;
	return reg;
}

/* ICMR access */
static inline unsigned char read_ICMR(void *iobase)
{
	return read_reg(iobase + ICMR);
}
static inline void write_ICMR(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICMR);
	return;
}
static inline int Is_LSB_MR_MLS(unsigned char reg)
{
	return reg & MR_MLS;
}
static inline int Is_protect_MR_BCWP(unsigned char reg)
{
	return reg & MR_BCWP;
}
static inline unsigned char get_MR_BC(unsigned char reg)
{
	return reg & MR_BC;
}
static inline unsigned char set_MR_MLS(unsigned char val, unsigned char reg)
{
	reg &= ~MR_MLS;
	if (val)
		reg |= MR_MLS;
	return reg;
}
static inline unsigned char set_MR_BCWP(unsigned char val, unsigned char reg)
{
	reg &= ~MR_BCWP;
	if (val)
		reg |= MR_BCWP;
	return reg;
}
static inline unsigned char set_MR_BC(unsigned char val, unsigned char reg)
{
	reg &= ~MR_BC;
	if (val)
		reg |= MR_BC;
	return reg;
}

/* ICIER access */
static inline unsigned char read_ICIER(void *iobase)
{
	return read_reg(iobase + ICIER);
}
static inline void write_ICIER(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICIER);
	return;
}
static inline int Is_enable_IER_TIE(unsigned char reg)
{
	return reg & IER_TIE;
}
static inline int Is_enable_IER_TEIE(unsigned char reg)
{
	return reg & IER_TEIE;
}
static inline int Is_enable_IER_RIE(unsigned char reg)
{
	return reg & IER_RIE;
}
static inline int Is_enable_IER_NAKIE(unsigned char reg)
{
	return reg & IER_NAKIE;
}
static inline int Is_enable_IER_STIE(unsigned char reg)
{
	return reg & IER_STIE;
}
static inline int Is_halted_IER_ACKE(unsigned char reg)
{
	return reg & IER_ACKE;
}
static inline int Is_receive_ack_IER_ACKBR(unsigned char reg)
{
	return reg & IER_ACKBR;
}
static inline int Is_transmit_ack_IER_ACKBT(unsigned char reg)
{
	return reg & IER_ACKBT;
}
static inline unsigned char set_IER_TIE(unsigned char val, unsigned char reg)
{
	reg &= ~IER_TIE;
	if (val)
		reg |= IER_TIE;
	return reg;
}
static inline unsigned char set_IER_TEIE(unsigned char val, unsigned char reg)
{
	reg &= ~IER_TEIE;
	if (val)
		reg |= IER_TEIE;
	return reg;
}
static inline unsigned char set_IER_RIE(unsigned char val, unsigned char reg)
{
	reg &= ~IER_RIE;
	if (val)
		reg |= IER_RIE;
	return reg;
}
static inline unsigned char set_IER_NAKIE(unsigned char val, unsigned char reg)
{
	reg &= ~IER_NAKIE;
	if (val)
		reg |= IER_NAKIE;
	return reg;
}
static inline unsigned char set_IER_STIE(unsigned char val, unsigned char reg)
{
	reg &= ~IER_STIE;
	if (val)
		reg |= IER_STIE;
	return reg;
}
static inline unsigned char set_IER_ACKE(unsigned char val, unsigned char reg)
{
	reg &= ~IER_ACKE;
	if (val)
		reg |= IER_ACKE;
	return reg;
}
static inline unsigned char set_IER_ACKBT(unsigned char val, unsigned char reg)
{
	reg &= ~IER_ACKBT;
	if (val)
		reg |= IER_ACKBT;
	return reg;
}

/* ICSR access */
static inline unsigned char read_ICSR(void *iobase)
{
	return read_reg(iobase + ICSR);
}
static inline void write_ICSR(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICSR);
	return;
}
static inline int Is_empty_SR_TDRE(unsigned char reg)
{
	return reg & SR_TDRE;
}
static inline int Is_end_SR_TEND(unsigned char reg)
{
	return reg & SR_TEND;
}
static inline int Is_full_SR_RDRF(unsigned char reg)
{
	return reg & SR_RDRF;
}
static inline int Is_noack_SR_NACKF(unsigned char reg)
{
	return reg & SR_NACKF;
}
static inline int Is_stop_SR_STOP(unsigned char reg)
{
	return reg & SR_STOP;
}
static inline int Is_error_SR_AL_OVE(unsigned char reg)
{
	return reg & SR_AL_OVE;
}
static inline unsigned char set_SR_TDRE(unsigned char val, unsigned char reg)
{
	reg &= ~SR_TDRE;
	if (val)
		reg |= SR_TDRE;
	return reg;
}
static inline unsigned char set_SR_TEND(unsigned char val, unsigned char reg)
{
	reg &= ~SR_TEND;
	if (val)
		reg |= SR_TEND;
	return reg;
}
static inline unsigned char set_SR_RDRF(unsigned char val, unsigned char reg)
{
	reg &= ~SR_RDRF;
	if (val)
		reg |= SR_RDRF;
	return reg;
}
static inline unsigned char set_SR_NACKF(unsigned char val, unsigned char reg)
{
	reg &= ~SR_NACKF;
	if (val)
		reg |= SR_NACKF;
	return reg;
}
static inline unsigned char set_SR_STOP(unsigned char val, unsigned char reg)
{
	reg &= ~SR_STOP;
	if (val)
		reg |= SR_STOP;
	return reg;
}
static inline unsigned char set_SR_AL_OVE(unsigned char val, unsigned char reg)
{
	reg &= ~SR_AL_OVE;
	if (val)
		reg |= SR_AL_OVE;
	return reg;
}

/* ICDRT access */
static inline unsigned char read_ICDRT(void *iobase)
{
	return read_reg(iobase + ICDRT);
}
static inline void write_ICDRT(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICDRT);
	return;
}

/* ICDRR access */
static inline unsigned char read_ICDRR(void *iobase)
{
	unsigned char val = read_reg(iobase + ICDRR);

	return val;
}
static inline void write_ICDRR(unsigned char val, void *iobase)
{
	write_reg(val, iobase + ICDRR);
	return;
}

struct i2cm {
	void __iomem *iobase;
	struct i2c_adapter adap;
	struct clk *clk;

	/* message processing */
	struct i2c_msg	*msg;
#define IDF_SEND	0x01
#define IDF_RECV	0x02
#define IDF_STOP	0x04
#define IDF_LAST	0x08
#define IDF_RCVD	0x10
	int		flags;

#define IDS_DONE	1
#define IDS_ARBLOST	2
#define IDS_NACK	4
	int		status;
	struct completion xfer_done;

	int irq;
	struct resource *ioarea;
};

static irqreturn_t sh7730_i2c_irq(int irq, void *ptr)
{
	struct i2cm *id = ptr;
	struct i2c_msg *msg = id->msg;
	char *data = msg->buf;
	unsigned char dummy;
	unsigned char iccr1;
	unsigned char iccr2;
	unsigned char icier;
	unsigned char icsr;

	iccr1 = read_ICCR1(id->iobase);
	iccr2 = read_ICCR2(id->iobase);
	icsr  = read_ICSR(id->iobase);
	icier = read_ICIER(id->iobase);

	/* stop condition was sent */
	/* w-[14] */
	if (Is_stop_SR_STOP(icsr)) {

		/* r-[12] */
		icsr = read_ICSR(id->iobase);
		icsr = set_SR_STOP(BIT_CLR, icsr);
		write_ICSR(icsr, id->iobase);

		icier = set_IER_STIE(BIT_CLR, icier);
		write_ICIER(icier, id->iobase);

		id->status |= IDS_DONE;
		goto out;
	}

	/* error */
	if (Is_error_SR_AL_OVE(icsr)) {
		id->status |= IDS_DONE;
		id->status |= IDS_ARBLOST;
		goto out;
	}

	/* error */
	if (Is_receive_ack_IER_ACKBR(icier)) {
		icier = set_IER_TEIE(BIT_CLR, icier);
		write_ICIER(icier, id->iobase);

		id->status |= IDS_NACK;
		goto stop;
	}

	/* Issue the stop condition. */
	if (id->flags & IDF_LAST) {
		/* r-[9]*/
		icsr = read_ICSR(id->iobase);
		/* Check RDRF only in case of read */
		if ((id->flags & IDF_RECV) && !Is_full_SR_RDRF(icsr)) {
			/* Error condition */
			id->status |= IDS_DONE;
			goto out;
		}
		/* set RDRF if not read */
		if (!(id->flags & IDF_RECV)) {
			icsr = set_SR_RDRF(BIT_CLR, icsr);
			write_ICSR(icsr, id->iobase);
		}

		/* !! disable receive interrupt */
		icier = set_IER_ACKBT(BIT_CLR, icier);
		icier = set_IER_RIE(BIT_CLR, icier);
		write_ICIER(icier, id->iobase);

		goto stop;
	}

	if (id->flags & IDF_RECV) {
		if ((msg->len - 1) < 3) {
			/** WORKAROUND set RCVD  from (last-1) - 2*/
			iccr1 = set_CR1_RCVD(BIT_SET, iccr1);
			write_ICCR1(iccr1, id->iobase);
		}
		if (Is_end_SR_TEND(icsr)) {
			/* Clear TEND, select master receive mode, */
			/* and then clear TDRE. */
			/* r-[1] */
			icsr  = read_ICSR(id->iobase);
			icsr = set_SR_TEND(BIT_CLR, icsr);
			write_ICSR(icsr, id->iobase);

			iccr1 = set_CR1_TRS(BIT_CLR, iccr1);
			write_ICCR1(iccr1, id->iobase);

			icsr  = read_ICSR(id->iobase);
			icsr = set_SR_TDRE(BIT_CLR, icsr);
			write_ICSR(icsr, id->iobase);

			/* Set acknowledge to the transmit device. */
			icier = set_IER_TEIE(BIT_CLR, icier);
			/* r-[2] */
			icier = set_IER_ACKBT(BIT_CLR, icier);
			icier = set_IER_RIE(BIT_SET, icier);
			write_ICIER(icier, id->iobase);

			/* Check whether it is the (last receive - 1). */
			/* r-[5] */
			if ((id->flags & IDF_STOP)
			 && ((msg->len - 1) < 1)) {

				/* Set acknowledge of the final byte. */
				/* Disable continuous reception (RCVD = 1). */
				/* r-[7] */
				icier = set_IER_ACKBT(BIT_SET, icier);
				write_ICIER(icier, id->iobase);
				id->flags |= IDF_RCVD;
				id->flags |= IDF_LAST;
			}
			/* Dummy-read ICDDR. */
			/* r-[3] */
			dummy = read_ICDRR(id->iobase);
		/* r-[4] */
		} else if (Is_full_SR_RDRF(icsr)) {

			if ((id->flags & IDF_STOP)
			 && !(id->flags & IDF_RCVD)
			 && ((msg->len - 1) < 2)) {

				/* Set acknowledge of the final byte. */
				/* Disable continuous reception (RCVD = 1). */
				/* r-[7] */
				icier = set_IER_ACKBT(BIT_SET, icier);
				write_ICIER(icier, id->iobase);
				id->flags |= IDF_LAST;
			}

			/* Read the receive data. */
			/* r-[6,8] */
			*data++ = read_ICDRR(id->iobase);
			msg->len--;

		} else {
			id->status |= IDS_DONE;
			id->status |= IDS_ARBLOST;
			goto out;
		}

		if (msg->len < 1) {
			if (id->flags & IDF_STOP)
				id->flags |= IDF_LAST;
			else
				id->status |= IDS_DONE;
		}

	} else if (id->flags & IDF_SEND) {

		icsr  = read_ICSR(id->iobase);
		icsr = set_SR_TEND(BIT_CLR, icsr);
		write_ICSR(icsr, id->iobase);

		/* Write the transmit data. */
		if (msg->len > 0) {
			/* w-[7] */
			write_ICDRT(*data++, id->iobase);
			msg->len--;
		} else {
			icier = set_IER_TEIE(BIT_CLR, icier);
			write_ICIER(icier, id->iobase);

			if (id->flags & IDF_STOP)
				goto stop;
			else
				id->status |= IDS_DONE;
		}
	}

out:

	msg->buf = data;

	if (id->status & IDS_DONE) {

		if (id->flags & IDF_STOP) {
			/* r-[14] */
			iccr1 = set_CR1_RCVD(BIT_CLR, iccr1);
			write_ICCR1(iccr1, id->iobase);

			/* Set slave receive mode. */
			/* r-[15] */
			iccr1 = set_CR1_MST(BIT_CLR, iccr1);
			iccr1 = set_CR1_TRS(BIT_CLR, iccr1);
			write_ICCR1(iccr1, id->iobase);
		}

		id->msg = NULL;
		complete(&id->xfer_done);
	}

	return IRQ_HANDLED;

stop:
	/* Clear TEND, select master receive mode, and then clear TDRE. */
	icsr  = read_ICSR(id->iobase);

	/* w-[11] */
	icsr = set_SR_TEND(BIT_CLR, icsr);
	/* w-[12] */
	/* r-[10] */
	icsr = set_SR_STOP(BIT_CLR, icsr);
	write_ICSR(icsr, id->iobase);

	/* Transmit End Interrupt disable. */
	/* & Stop Condition Detection Interrupt Enable. */
	icier = set_IER_STIE(BIT_SET, icier);
	write_ICIER(icier, id->iobase);

	/* w-[13] */
	/* r-[11] */
	iccr2 = set_CR2_BBSY(BIT_CLR, iccr2);
	iccr2 = set_CR2_SCP(BIT_CLR, iccr2);
	write_ICCR2(iccr2, id->iobase);

	if (id->flags & IDF_RECV) {
		/*r-[12]*/
		icsr  = read_ICSR(id->iobase);

		if (Is_stop_SR_STOP(icsr)) {
			/* Read the receive data. */
			/* r-[13] */
			*data++ = read_ICDRR(id->iobase);
			msg->len--;
			icsr = set_SR_RDRF(BIT_CLR, icsr);
			write_ICSR(icsr, id->iobase);
		}
	}

	return IRQ_HANDLED;
}


/* prepare and start a master receive operation */
static void sh7730_i2c_mrecv(struct i2cm *id)
{
	unsigned char iccr1;
	unsigned char iccr2;
	unsigned char icier;

	id->flags |= IDF_RECV;

	iccr1 = read_ICCR1(id->iobase);
	iccr2 = read_ICCR2(id->iobase);
	icier = read_ICIER(id->iobase);

	/* Set master transmit mode. */
	iccr1 = set_CR1_MST(BIT_SET, iccr1);
	iccr1 = set_CR1_TRS(BIT_SET, iccr1);
	write_ICCR1(iccr1, id->iobase);

	/* Transmit End Interrupt Enable. */
	icier = set_IER_TEIE(BIT_SET, icier);
	write_ICIER(icier, id->iobase);

	/* Issue the start condition. */
	iccr2 = set_CR2_BBSY(BIT_SET, iccr2);
	iccr2 = set_CR2_SCP(BIT_CLR, iccr2);
	write_ICCR2(iccr2, id->iobase);

	/* Wait TDRE */
	{
		unsigned char icsr;
		int wait_count = 1000;
mr_wait:
		icsr  = read_ICSR(id->iobase);
		if (!Is_empty_SR_TDRE(icsr)) {
			udelay(10);
			if (wait_count--)
				goto mr_wait;
			dev_err(&id->adap.dev, "i2c-sh7730%d: "
					"mrecv empty!\n", id->adap.nr);
		}
	}

	/* Set the first byte (slave address + R) of transmit data. */
	write_ICDRT(((id->msg->addr << 1) | 1), id->iobase);

	return;
}

/* prepare and start a master send operation */
static void sh7730_i2c_msend(struct i2cm *id)
{
	unsigned char iccr1;
	unsigned char iccr2;
	unsigned char icier;

	id->flags |= IDF_SEND;

	iccr1 = read_ICCR1(id->iobase);
	iccr2 = read_ICCR2(id->iobase);
	icier = read_ICIER(id->iobase);

	/* Set master transmit mode. */
	iccr1 = set_CR1_MST(BIT_SET, iccr1);
	iccr1 = set_CR1_TRS(BIT_SET, iccr1);
	write_ICCR1(iccr1, id->iobase);

	/* Transmit End Interrupt Enable. */
	icier = set_IER_TEIE(BIT_SET, icier);
	write_ICIER(icier, id->iobase);

	/* Issue the start condition. */
	iccr2 = set_CR2_BBSY(BIT_SET, iccr2);
	iccr2 = set_CR2_SCP(BIT_CLR, iccr2);
	write_ICCR2(iccr2, id->iobase);

	/* Wait TDRE */
	{
		unsigned char icsr;
		int wait_count = 1000;
ms_wait:
		icsr  = read_ICSR(id->iobase);
		if (!Is_empty_SR_TDRE(icsr)) {
			udelay(10);
			if (wait_count--)
				goto ms_wait;
			dev_err(&id->adap.dev, "i2c-sh7730%d: "
					"msend empty!\n", id->adap.nr);
		}
	}

	/* Set the first byte (slave address + W) of transmit data. */
	write_ICDRT(((id->msg->addr << 1) | 0), id->iobase);

	return;
}

static int sh7730_i2c_master_xfer(struct i2c_adapter *adap,
				  struct i2c_msg *msgs,
				  int num)
{
	struct i2cm *id = adap->algo_data;
	struct i2c_msg w_msgs;
	unsigned char iccr2;
	int i, retr;

	pm_runtime_get_sync(adap->dev.parent);
	clk_enable(id->clk);

	/* Initial value */
	{
		unsigned char iccr1 = read_ICCR1(id->iobase);
		struct i2c_sh_mobile_platform_data *pd;
		pd = id->adap.dev.parent->platform_data;
		if (!pd) {
				dev_err(&adap->dev, "no platform_data!\n");
				clk_disable(id->clk);
        			pm_runtime_put_sync(adap->dev.parent);
        			return -ENODEV;
		}

		/* Transfer Clock Select */
		{
			unsigned char mc=0;
			unsigned long module_clk=0;
			unsigned long div=0;


			module_clk = clk_get_rate(id->clk);

			div = module_clk / pd->bus_speed;

			if (60 >= div)
				mc = CR1_CKS_MC0060;
			else if (76 >= div)
				mc = CR1_CKS_MC0076;
			else if (92 >= div)
				mc = CR1_CKS_MC0092;
			else if (108 >= div)
				mc = CR1_CKS_MC0108;
			else if (120 >= div)
				mc = CR1_CKS_MC0120;
			else if (168 >= div)
				mc = CR1_CKS_MC0168;
			else if (240 >= div)
				mc = CR1_CKS_MC0240;
			else if (252 >= div)
				mc = CR1_CKS_MC0252;
			else if (300 >= div)
				mc = CR1_CKS_MC0300;
			else if (304 >= div)
				mc = CR1_CKS_MC0304;
			else if (368 >= div)
				mc = CR1_CKS_MC0368;
			else if (432 >= div)
				mc = CR1_CKS_MC0432;
			else if (480 >= div)
				mc = CR1_CKS_MC0480;
			else if (672 >= div)
				mc = CR1_CKS_MC0672;
			else if (1008 >= div)
				mc = CR1_CKS_MC1008;
			else
				mc = CR1_CKS_MC1200;

			iccr1 = set_CR1_CKS(mc, iccr1);
		}

		/* This module is enabled for transfer operations. */
		iccr1 = set_CR1_ICE(BIT_SET, iccr1);

		write_ICCR1(iccr1, id->iobase);
	}

	iccr2 = read_ICCR2(id->iobase);

	/* Test the status of the SCL and SDA lines. */
	if (Is_busy_CR2_BBSY(iccr2)) {
		dev_err(&adap->dev, "i2c-sh7730%d: bus busy!\n", adap->nr);
		return -EBUSY;
	}

	i = 0;
	while (i < num) {
		retr = adap->retries;
retry:
		id->flags = ((i == (num-1)) ? IDF_STOP : 0);
		id->status = 0;
		w_msgs = *msgs;
		id->msg = &w_msgs;
		init_completion(&id->xfer_done);

		/* Wait Restart Condition */
		if ((1 < num)
		 && (0 < i)) {
			int wait_count = 1000;
wait:
			iccr2 = read_ICCR2(id->iobase);
			if (Is_high_CR2_SCLO(iccr2)) {
				udelay(10);
				if (wait_count--)
					goto wait;
				dev_err(&adap->dev, "i2c-sh7730%d: "
					"restart clk high!\n", adap->nr);
				num = -EIO;
				break;
			}
		}

		if (msgs->flags & I2C_M_RD)
			sh7730_i2c_mrecv(id);
		else
			sh7730_i2c_msend(id);

		wait_for_completion(&id->xfer_done);

		/* Wait Stop Condition */
		if (id->flags & IDF_STOP) {
			int retry_count = 1000;
again:
			iccr2 = read_ICCR2(id->iobase);
			if (Is_busy_CR2_BBSY(iccr2)) {
				udelay(10);
				if (retry_count--)
					goto again;
				dev_err(&adap->dev, "i2c-sh7730%d: "
					"non stop!\n", adap->nr);
				num = -EIO;
				break;
			}
		}

		if (id->status == 0) {
			dev_err(&adap->dev, "i2c-sh7730%d: "
					"errer I/O!\n", adap->nr);
			num = -EIO;
			break;
		}

		if (id->status & IDS_NACK) {
			/* wait a bit or i2c module stops working */
			mdelay(1);
			num = -EREMOTEIO;
			break;
		}

		if (id->status & IDS_ARBLOST) {
			if (retr--) {
				mdelay(2);
				goto retry;
			}
			num = -EREMOTEIO;
			break;
		}

		msgs++;
		i++;
	}

	id->msg = NULL;
	id->flags = 0;
	id->status = 0;
	clk_disable(id->clk);
	pm_runtime_put_sync(adap->dev.parent);

	return num;
}

static u32 sh7730_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm sh7730_i2c_algo = {
	.master_xfer	= sh7730_i2c_master_xfer,
	.functionality	= sh7730_i2c_func,
};

static int __devinit sh7730_i2c_probe(struct platform_device *pdev)
{
	struct i2c_sh_mobile_platform_data *pd;
	struct resource *res;
	struct i2cm *id;
	int size;
	int ret;
	pd = pdev->dev.platform_data;
	if (!pd) {
		dev_err(&pdev->dev, "no platform_data!\n");
		ret = -ENODEV;
		goto out0;
	}

	id = kzalloc(sizeof(struct i2cm), GFP_KERNEL);
	if (!id) {
		dev_err(&pdev->dev, "no mem for private data\n");
		ret = -ENOMEM;
		goto out0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mmio resources\n");
		ret = -ENODEV;
		goto out1;
	}

	size = resource_size(res);

	id->ioarea = request_mem_region(res->start, size, pdev->name);
	if (!id->ioarea) {
		dev_err(&pdev->dev, "mmio already reserved\n");
		ret = -EBUSY;
		goto out1;
	}

	id->iobase = ioremap(res->start, size);
	if (!id->iobase) {
		dev_err(&pdev->dev, "cannot ioremap\n");
		ret = -ENODEV;
		goto out2;
	}

	id->irq = platform_get_irq(pdev, 0);

	id->adap.nr = pdev->id;
	id->adap.algo = &sh7730_i2c_algo;
	id->adap.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	id->adap.retries = 3;
	id->adap.algo_data = id;
	id->adap.dev.parent = &pdev->dev;
	snprintf(id->adap.name, sizeof(id->adap.name),
		"i2c-sh7730 at %08lx", (unsigned long)res->start);

	id->clk = clk_get(&pdev->dev, NULL);

	if (IS_ERR(id->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		ret = PTR_ERR(id->clk);
		goto out3;
	}
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
	/* Get clock rate after clock is enabled */
	clk_enable(id->clk);

	/* Initial value */
	{
		unsigned char iccr1 = read_ICCR1(id->iobase);

		/* Transfer Clock Select */
		{
			unsigned char mc;
			unsigned long module_clk;
			unsigned long div;


			module_clk = clk_get_rate(id->clk);

			div = module_clk / pd->bus_speed;

			if (60 >= div)
				mc = CR1_CKS_MC0060;
			else if (76 >= div)
				mc = CR1_CKS_MC0076;
			else if (92 >= div)
				mc = CR1_CKS_MC0092;
			else if (108 >= div)
				mc = CR1_CKS_MC0108;
			else if (120 >= div)
				mc = CR1_CKS_MC0120;
			else if (168 >= div)
				mc = CR1_CKS_MC0168;
			else if (240 >= div)
				mc = CR1_CKS_MC0240;
			else if (252 >= div)
				mc = CR1_CKS_MC0252;
			else if (300 >= div)
				mc = CR1_CKS_MC0300;
			else if (304 >= div)
				mc = CR1_CKS_MC0304;
			else if (368 >= div)
				mc = CR1_CKS_MC0368;
			else if (432 >= div)
				mc = CR1_CKS_MC0432;
			else if (480 >= div)
				mc = CR1_CKS_MC0480;
			else if (672 >= div)
				mc = CR1_CKS_MC0672;
			else if (1008 >= div)
				mc = CR1_CKS_MC1008;
			else
				mc = CR1_CKS_MC1200;

			iccr1 = set_CR1_CKS(mc, iccr1);
		}

		/* This module is enabled for transfer operations. */
		iccr1 = set_CR1_ICE(BIT_SET, iccr1);

		write_ICCR1(iccr1, id->iobase);
	}

	clk_disable(id->clk);

	if (request_irq(id->irq, sh7730_i2c_irq, IRQF_DISABLED,
			"i2c-sh7730", id)) {
		dev_err(&pdev->dev, "cannot get irq %d\n", id->irq);
		ret = -EBUSY;
		goto out3;
	}

	pm_runtime_put_sync(&pdev->dev);
	ret = i2c_add_numbered_adapter(&id->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "reg adap failed: %d\n", ret);
		goto out4;
	}

	platform_set_drvdata(pdev, id);

	dev_info(&pdev->dev, "%lu kHz i2c-sh7730 %08x irq %d\n",
		 pd->bus_speed/1000, res->start, id->irq);

	return 0;

out4:
	free_irq(id->irq, id);
out3:
	iounmap(id->iobase);
out2:
	release_resource(id->ioarea);
	kfree(id->ioarea);
out1:
	kfree(id);
out0:
	return ret;
}

static int sh7730_i2c_remove(struct platform_device *pdev)
{
	struct i2cm *id = platform_get_drvdata(pdev);

	i2c_del_adapter(&id->adap);
	free_irq(id->irq, id);
	clk_put(id->clk);
	iounmap(id->iobase);
	release_resource(id->ioarea);
	pm_runtime_disable(&pdev->dev);
	kfree(id->ioarea);
	kfree(id);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int sh7730_i2c_suspend(struct device *dev)
{
	return 0;
}

static int sh7730_i2c_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sh7730_i2c_dev_pm_ops = {
	.runtime_suspend = sh7730_i2c_suspend,
	.runtime_resume = sh7730_i2c_resume,
};

static struct platform_driver sh7730_i2c_drv = {
	.driver	= {
		.name	= "i2c-sh7730",
		.owner	= THIS_MODULE,
		.pm	= &sh7730_i2c_dev_pm_ops,
	},
	.probe		= sh7730_i2c_probe,
	.remove		= sh7730_i2c_remove,
};

static int __init sh7730_i2c_init(void)
{
	return platform_driver_register(&sh7730_i2c_drv);
}

static void __exit sh7730_i2c_exit(void)
{
	platform_driver_unregister(&sh7730_i2c_drv);
}

module_init(sh7730_i2c_init);
module_exit(sh7730_i2c_exit);

MODULE_DESCRIPTION("SH7730 I2C bus driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:i2c-sh7730");
