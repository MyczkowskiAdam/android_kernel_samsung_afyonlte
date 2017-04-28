/*
 * include/linux/usb/tusb1211.h
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

#ifndef TUSB1211_H
#define TUSB1211_H


#include <linux/usb/otg.h>
#include <mach/r8a7373.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/usb/ulpi.h>
/*
 * Register Map
 */

#define MOTGCTRL	0x012A



/**Bits Default value
*********************/
#define	PINTM	0x100
#define	RSME	0x4000
#define	HWUPM	0x80
#define	SUSMON	0x4000

/**Registers address
*********************/

#define	LPCTRL	0x0100


#define	OTG_PHYFUNCTR	0x0104
extern void smb328a_otg_enable_disable(int onoff, int cable);
extern void smb358a_otg_enable_disable(int en);

#ifdef CONFIG_USB_OTG
int set_otg_mode(int is_host);
#endif

static inline u16 otg_read_reg(struct otg_transceiver *otg, u32 reg)
{
	return ioread16(otg->io_priv + reg);
}
static inline int otg_write_reg(struct otg_transceiver *otg, u32 val, u32 reg)
{
	 iowrite16(val, otg->io_priv + reg);
	 return 0;
}

static inline void otg_modify_reg(struct otg_transceiver *otg, u16 clr_mask, u16 set_mask, u32 reg)
{
	u16 tmp;
	tmp = ioread16(otg->io_priv + reg);
	tmp &= (~clr_mask);
	tmp |= set_mask;
	iowrite16(tmp, otg->io_priv + reg);
}

#define otg_io_clear_bits(otg, val, reg)	\
			otg_modify_reg(otg, val, 0, reg)
#define otg_io_set_bits(otg, val, reg)	\
			otg_modify_reg(otg, 0, val, reg)



#define USB_PHYREAD	0X011E /*H'E689 011E*/
#define USB_TUSB_SPADDR	0X0138 /*H'E689 0138*/
#define USB_TUSB_SPWDAT	0X013A /*H'E689 013A*/
#define USB_TUSB_SPCTRL	0X013C /*H'E689 013C*/
#define USB_TUSB_SPRDAT	0X013E /*H'E689 013E*/
#define USB_PHYRD	0x0001

static inline u16 tusb1211_phy_read(struct otg_transceiver *otg, u32 reg)
{
	u16 monreg;
	otg_io_set_bits(otg, USB_PHYRD, USB_PHYREAD);
	while(otg_read_reg(otg, USB_PHYREAD) & USB_PHYRD)
		;
	monreg = otg_read_reg(otg, reg);
	return monreg;
}

#define TUSB1211_MAX_ADDR	0X90			/*Maximum value of TUSB1211 registers' address*/
#define USB_START_READ 		0X01			/*Start reading register*/
#define USB_MAX_REG_VAL		0xFF			/*8 bits register max value*/
#define USB_ULPI_EXTENDED	0x2F			/*Address of extended register set*/

#define ULPI_ADDR(n)          	((n) & 63)	/*Get 6 last bits*/
#define ULPI_IO_TIMEOUT_USEC	10	/*Time out for accessing to registers*/
#define ULPI_READ				(1 << 1)	/*Availabe to read register*/
#define ULPI_WRITE				0X01			/*Availabe to write register*/
#define ULPI_START_READ			0X01			/*Start reading register*/
#define ULPI_DATA(n)			(n & 255)	/*Get 8 last bits*/
#define TUSB_CS	GPIO_PORT130
#define nTUSB_RST	GPIO_PORT131

#define USBHS_PHYOTGCTR	0x010A

#define USEVBUS		0x8000
#define DVBUSEX 	0x4000
#define DRVVBUS 	0x2000
#define CHRGVBUS 	0x1000
#define DISCHRGVBUS 0x0800
#define DMPUDWN 	0x0400
#define DPPUDWN 	0x0200
#define IDPUUP 		0x0100


#define PHYINTR 0x010C

#define PHYINTENRISE	0x1F

#define IDGND_RISE 		0x10
#define SESSEND_RISE    0x08
#define SESSVALID_RISE  0x04
#define VBUSVALID_RISE  0x02
#define HOSTDISCONNECT_RISE 0x01

#define PHYINTF 0x0110

#define PHYINTENFALL	0x1F00

#define IDGND_FALL 		0x1000
#define SESSEND_FALL    0x0800
#define SESSVALID_FALL  0x0400
#define VBUSVALID_FALL  0x0200
#define HOSTDISCONNECT_FALL 0x0100

#define MPINTSTS 0x0132

#define IDGND_STS 		0x10
#define SESSEND_STS    0x08
#define SESSVALID_STS  0x04
#define VBUSVALID_STS  0x02
#define HOSTDISCONNECT_STS 0x01



#define VB_SESS_END	0x0000
#define VSESS_VLD	0x4000
#define VA_VBUS_VLD	0x8000
#define VBUS_VLD	0xC000


#define OTG_PHYFUNCTR	0x0104
#define PRESET 0x2000

#define TUSB1211_ID_POLL_TIME 500

struct tusb1211_platform_data {
	void (*module_start)(void);
};

struct tusb1211 {
	spinlock_t	lock;
#ifdef CONFIG_HAVE_CLK
	struct clk *clk;
	struct clk *iclk;
#endif
	struct otg_transceiver	otg;
	unsigned			init:1;
	struct tusb1211_platform_data *pdata;

	struct timer_list se0_srp_timer;
	struct timer_list b_data_pls_timer;
	struct timer_list b_chrg_vbus_timer;
	struct timer_list b_dischrg_vbus_timer;
	struct timer_list b_srp_fail_timer;

	struct delayed_work	vbus_work;
	struct delayed_work	vbus_off_work;
	unsigned power:1;
	unsigned vbus_enable:1;
	int vbus_irq;
	int id_irq;
};

/* Register definitions */
#define SYSCFG		0x00
#define BWAIT		0x02
#define TUSB_SYSSTS		0x04
#define SYSSTS0		0x04
#define SYSSTS1		0x06
#define DVSTCTR		0x08
#define DVSTCTR0	0x08
#define DVSTCTR1	0x0A
#define TESTMODE	0x0C
#define PINCFG		0x0E
#define DMA0CFG		0x10
#define DMA1CFG		0x12
#define CFIFO		0x14
#define D0FIFO		0x18
#define D1FIFO		0x1C
#define CFIFOSEL	0x20
#define CFIFOCTR	0x22
#define CFIFOSIE	0x24
#define D0FIFOSEL	0x28
#define D0FIFOCTR	0x2A
#define D1FIFOSEL	0x2C
#define D1FIFOCTR	0x2E
#define INTENB0		0x30
#define INTENB1		0x32
#define INTENB2		0x34
#define BRDYENB		0x36
#define NRDYENB		0x38
#define BEMPENB		0x3A
#define SOFCFG		0x3C
#define INTSTS0		0x40
#define INTSTS1		0x42
#define INTSTS2		0x44
#define BRDYSTS		0x46
#define NRDYSTS		0x48
#define BEMPSTS		0x4A
#define FRMNUM		0x4C
#define UFRMNUM		0x4E
#define USBADDR		0x50
#define USBREQ		0x54
#define USBVAL		0x56
#define USBINDX		0x58
#define USBLENG		0x5A
#define DCPCFG		0x5C
#define DCPMAXP		0x5E
#define DCPCTR		0x60
#define PIPESEL		0x64
#define PIPECFG		0x68
#define PIPEBUF		0x6A
#define PIPEMAXP	0x6C
#define PIPEPERI	0x6E
#define PIPE1CTR	0x70
#define PIPE2CTR	0x72
#define PIPE3CTR	0x74
#define PIPE4CTR	0x76
#define PIPE5CTR	0x78
#define PIPE6CTR	0x7A
#define PIPE7CTR	0x7C
#define PIPE8CTR	0x7E
#define PIPE9CTR	0x80
#define PIPE1TRE	0x90
#define PIPE1TRN	0x92
#define PIPE2TRE	0x94
#define PIPE2TRN	0x96
#define PIPE3TRE	0x98
#define PIPE3TRN	0x9A
#define PIPE4TRE	0x9C
#define	PIPE4TRN	0x9E
#define	PIPE5TRE	0xA0
#define	PIPE5TRN	0xA2
#if defined(USBHS_TYPE_BULK_PIPES_12)
#define	PIPEBTRE	0xA4
#define	PIPEBTRN	0xA6
#define	PIPECTRE	0xA8
#define	PIPECTRN	0xAA
#define	PIPEDTRE	0xAC
#define	PIPEDTRN	0xAE
#define	PIPEETRE	0xB0
#define	PIPEETRN	0xB2
#define	PIPEFTRE	0xB4
#define	PIPEFTRN	0xB6
#define	PIPE9TRE	0xB8
#define	PIPE9TRN	0xBA
#define	PIPEATRE	0xBC
#define	PIPEATRN	0xBE
#endif
#define DEVADD0		0xD0
#define DEVADD1		0xD2
#define DEVADD2		0xD4
#define DEVADD3		0xD6
#define DEVADD4		0xD8
#define DEVADD5		0xDA
#define DEVADD6		0xDC
#define DEVADD7		0xDE
#define DEVADD8		0xE0
#define DEVADD9		0xE2
#define DEVADDA		0xE4

/* System Configuration Control Register */
#define	XTAL		0xC000	/* b15-14: Crystal selection */
#define	  XTAL48	 0x8000	  /* 48MHz */
#define	  XTAL24	 0x4000	  /* 24MHz */
#define	  XTAL12	 0x0000	  /* 12MHz */
#define	XCKE		0x2000	/* b13: External clock enable */
#define	PLLC		0x0800	/* b11: PLL control */
#define	SCKE		0x0400	/* b10: USB clock enable */
#define	PCSDIS		0x0200	/* b9: not CS wakeup */
#define	LPSME		0x0100	/* b8: Low power sleep mode */
#define	HSE		0x0080	/* b7: Hi-speed enable */
#define	DCFM		0x0040	/* b6: Controller function select  */
#define	DRPD		0x0020	/* b5: D+/- pull down control */
#define	DPRPU		0x0010	/* b4: D+ pull up control */
#define	USBE		0x0001	/* b0: USB module operation enable */

/* CPU bus wait */
#define BUSWAIT		0x0004

/* System Configuration Status Register */
#define	OVCBIT		0x8000	/* b15-14: Over-current bit */
#define	OVCMON		0xC000	/* b15-14: Over-current monitor */
#define	SOFEA		0x0020	/* b5: SOF monitor */
#define	IDMON		0x0004	/* b3: ID-pin monitor */
#define	LNST		0x0003	/* b1-0: D+, D- line status */
#define	  SE1		 0x0003	  /* SE1 */
#define	  FS_KSTS	 0x0002	  /* Full-Speed K State */
#define	  FS_JSTS	 0x0001	  /* Full-Speed J State */
#define	  LS_JSTS	 0x0002	  /* Low-Speed J State */
#define	  LS_KSTS	 0x0001	  /* Low-Speed K State */
#define	  SE0		 0x0000	  /* SE0 */

/* Device State Control Register */
#define HNPBTOA		0x0800	/* b11: switches HNPBTOA*/
#define	EXTLP0		0x0400	/* b10: External port */
#define	VBOUT		0x0200	/* b9: VBUS output */
#define	WKUP		0x0100	/* b8: Remote wakeup */
#define	RWUPE		0x0080	/* b7: Remote wakeup sense */
#define	USBRST		0x0040	/* b6: USB reset enable */
#define	RESUME		0x0020	/* b5: Resume enable */
#define	UACT		0x0010	/* b4: USB bus enable */
#define	RHST		0x0007	/* b1-0: Reset handshake status */
#define	  HSPROC	 0x0004	  /* HS handshake is processing */
#define	  HSMODE	 0x0003	  /* Hi-Speed mode */
#define	  FSMODE	 0x0002	  /* Full-Speed mode */
#define	  LSMODE	 0x0001	  /* Low-Speed mode */
#define	  UNDECID	 0x0000	  /* Undecided */

/* Test Mode Register */
#define	UTST			0x000F	/* b3-0: Test select */
#define	  H_TST_PACKET		 0x000C	  /* HOST TEST Packet */
#define	  H_TST_SE0_NAK		 0x000B	  /* HOST TEST SE0 NAK */
#define	  H_TST_K		 0x000A	  /* HOST TEST K */
#define	  H_TST_J		 0x0009	  /* HOST TEST J */
#define	  H_TST_NORMAL		 0x0000	  /* HOST Normal Mode */
#define	  P_TST_PACKET		 0x0004	  /* PERI TEST Packet */
#define	  P_TST_SE0_NAK		 0x0003	  /* PERI TEST SE0 NAK */
#define	  P_TST_K		 0x0002	  /* PERI TEST K */
#define	  P_TST_J		 0x0001	  /* PERI TEST J */
#define	  P_TST_NORMAL		 0x0000	  /* PERI Normal Mode */

/* Data Pin Configuration Register */
#define	LDRV			0x8000	/* b15: Drive Current Adjust */
#define	  VIF1			  0x0000		/* VIF = 1.8V */
#define	  VIF3			  0x8000		/* VIF = 3.3V */
#define	INTA			0x0001	/* b1: USB INT-pin active */

/* DMAx Pin Configuration Register */
#define	DREQA			0x4000	/* b14: Dreq active select */
#define	BURST			0x2000	/* b13: Burst mode */
#define	DACKA			0x0400	/* b10: Dack active select */
#define	DFORM			0x0380	/* b9-7: DMA mode select */
#define	  CPU_ADR_RD_WR		 0x0000	  /* Address + RD/WR mode (CPU bus) */
#define	  CPU_DACK_RD_WR	 0x0100	  /* DACK + RD/WR mode (CPU bus) */
#define	  CPU_DACK_ONLY		 0x0180	  /* DACK only mode (CPU bus) */
#define	  SPLIT_DACK_ONLY	 0x0200	  /* DACK only mode (SPLIT bus) */
#define	DENDA			0x0040	/* b6: Dend active select */
#define	PKTM			0x0020	/* b5: Packet mode */
#define	DENDE			0x0010	/* b4: Dend enable */
#define	OBUS			0x0004	/* b2: OUTbus mode */

/* CFIFO/DxFIFO Port Select Register */
#define	RCNT		0x8000	/* b15: Read count mode */
#define	REW		0x4000	/* b14: Buffer rewind */
#define	DCLRM		0x2000	/* b13: DMA buffer clear mode */
#define	DREQE		0x1000	/* b12: DREQ output enable */
#define	  MBW_8		 0x0000	  /*  8bit */
#define	  MBW_16	 0x0400	  /* 16bit */
#define	  MBW_32	 0x0800   /* 32bit */
#define	BIGEND		0x0100	/* b8: Big endian mode */
#define	  BYTE_LITTLE	 0x0000		/* little dendian */
#define	  BYTE_BIG	 0x0100		/* big endifan */
#define	ISEL		0x0020	/* b5: DCP FIFO port direction select */
#define	CURPIPE		0x000F	/* b2-0: PIPE select */

/* CFIFO/DxFIFO Port Control Register */
#define	BVAL		0x8000	/* b15: Buffer valid flag */
#define	BCLR		0x4000	/* b14: Buffer clear */
#define	FRDY		0x2000	/* b13: FIFO ready */
#define	DTLN		0x0FFF	/* b11-0: FIFO received data length */

/* Interrupt Enable Register 0 */
#define	VBSE	0x8000	/* b15: VBUS interrupt */
#define	RSME	0x4000	/* b14: Resume interrupt */
#define	SOFE	0x2000	/* b13: Frame update interrupt */
#define	DVSE	0x1000	/* b12: Device state transition interrupt */
#define	CTRE	0x0800	/* b11: Control transfer stage transition interrupt */
#define	BEMPE	0x0400	/* b10: Buffer empty interrupt */
#define	NRDYE	0x0200	/* b9: Buffer not ready interrupt */
#define	BRDYE	0x0100	/* b8: Buffer ready interrupt */

/* Interrupt Enable Register 1 */
#define	OVRCRE		0x8000	/* b15: Over-current interrupt */
#define VBCOMP      0x8000  /* b15: Over-current interrupt */
#define	VBCOMPE		0x8000	/* b15: Vbus compare interrupt */
#define	BCHGE		0x4000	/* b14: USB us chenge interrupt */
#define	DTCHE		0x1000	/* b12: Detach sense interrupt */
#define	ATTCHE		0x0800	/* b11: Attach sense interrupt */
#define	EOFERRE		0x0040	/* b6: EOF error interrupt */
#define	SIGNE		0x0020	/* b5: SETUP IGNORE interrupt */
#define	SACKE		0x0010	/* b4: SETUP ACK interrupt */

/* BRDY Interrupt Enable/Status Register */
#define	BRDY9		0x0200	/* b9: PIPE9 */
#define	BRDY8		0x0100	/* b8: PIPE8 */
#define	BRDY7		0x0080	/* b7: PIPE7 */
#define	BRDY6		0x0040	/* b6: PIPE6 */
#define	BRDY5		0x0020	/* b5: PIPE5 */
#define	BRDY4		0x0010	/* b4: PIPE4 */
#define	BRDY3		0x0008	/* b3: PIPE3 */
#define	BRDY2		0x0004	/* b2: PIPE2 */
#define	BRDY1		0x0002	/* b1: PIPE1 */
#define	BRDY0		0x0001	/* b1: PIPE0 */

/* NRDY Interrupt Enable/Status Register */
#define	NRDY9		0x0200	/* b9: PIPE9 */
#define	NRDY8		0x0100	/* b8: PIPE8 */
#define	NRDY7		0x0080	/* b7: PIPE7 */
#define	NRDY6		0x0040	/* b6: PIPE6 */
#define	NRDY5		0x0020	/* b5: PIPE5 */
#define	NRDY4		0x0010	/* b4: PIPE4 */
#define	NRDY3		0x0008	/* b3: PIPE3 */
#define	NRDY2		0x0004	/* b2: PIPE2 */
#define	NRDY1		0x0002	/* b1: PIPE1 */
#define	NRDY0		0x0001	/* b1: PIPE0 */

/* BEMP Interrupt Enable/Status Register */
#define	BEMP9		0x0200	/* b9: PIPE9 */
#define	BEMP8		0x0100	/* b8: PIPE8 */
#define	BEMP7		0x0080	/* b7: PIPE7 */
#define	BEMP6		0x0040	/* b6: PIPE6 */
#define	BEMP5		0x0020	/* b5: PIPE5 */
#define	BEMP4		0x0010	/* b4: PIPE4 */
#define	BEMP3		0x0008	/* b3: PIPE3 */
#define	BEMP2		0x0004	/* b2: PIPE2 */
#define	BEMP1		0x0002	/* b1: PIPE1 */
#define	BEMP0		0x0001	/* b0: PIPE0 */

/* SOF Pin Configuration Register */
#define	TRNENSEL	0x0100	/* b8: Select transaction enable period */
#define	BRDYM		0x0040	/* b6: BRDY clear timing */
#define	INTL		0x0020	/* b5: Interrupt sense select */
#define	EDGESTS		0x0010	/* b4:  */
#define	SOFMODE		0x000C	/* b3-2: SOF pin select */
#define	  SOF_125US	 0x0008	  /* SOF OUT 125us Frame Signal */
#define	  SOF_1MS	 0x0004	  /* SOF OUT 1ms Frame Signal */
#define	  SOF_DISABLE	 0x0000	  /* SOF OUT Disable */

/* Interrupt Status Register 0 */
#define	VBINT	0x8000	/* b15: VBUS interrupt */
#define	RESM	0x4000	/* b14: Resume interrupt */
#define	SOFR	0x2000	/* b13: SOF frame update interrupt */
#define	DVST	0x1000	/* b12: Device state transition interrupt */
#define	CTRT	0x0800	/* b11: Control transfer stage transition interrupt */
#define	BEMP	0x0400	/* b10: Buffer empty interrupt */
#define	NRDY	0x0200	/* b9: Buffer not ready interrupt */
#define	BRDY	0x0100	/* b8: Buffer ready interrupt */
#define	VBSTS	0x0080	/* b7: VBUS input port */
#define	DVSQ	0x0070	/* b6-4: Device state */
#define	  DS_SPD_CNFG	 0x0070	  /* Suspend Configured */
#define	  DS_SPD_ADDR	 0x0060	  /* Suspend Address */
#define	  DS_SPD_DFLT	 0x0050	  /* Suspend Default */
#define	  DS_SPD_POWR	 0x0040	  /* Suspend Powered */
#define	  DS_SUSP	 0x0040	  /* Suspend */
#define	  DS_CNFG	 0x0030	  /* Configured */
#define	  DS_ADDS	 0x0020	  /* Address */
#define	  DS_DFLT	 0x0010	  /* Default */
#define	  DS_POWR	 0x0000	  /* Powered */
#define	DVSQS		0x0030	/* b5-4: Device state */
#define	VALID		0x0008	/* b3: Setup packet detected flag */
#define	CTSQ		0x0007	/* b2-0: Control transfer stage */
#define	  CS_SQER	 0x0006	  /* Sequence error */
#define	  CS_WRND	 0x0005	  /* Control write nodata status stage */
#define	  CS_WRSS	 0x0004	  /* Control write status stage */
#define	  CS_WRDS	 0x0003	  /* Control write data stage */
#define	  CS_RDSS	 0x0002	  /* Control read status stage */
#define	  CS_RDDS	 0x0001	  /* Control read data stage */
#define	  CS_IDST	 0x0000	  /* Idle or setup stage */

/* Interrupt Status Register 1 */
#define	OVRCR		0x8000	/* b15: Over-current interrupt */
#define	BCHG		0x4000	/* b14: USB bus chenge interrupt */
#define	DTCH		0x1000	/* b12: Detach sense interrupt */
#define	ATTCH		0x0800	/* b11: Attach sense interrupt */
#define	EOFERR		0x0040	/* b6: EOF-error interrupt */
#define	SIGN		0x0020	/* b5: Setup ignore interrupt */
#define	SACK		0x0010	/* b4: Setup acknowledge interrupt */

/* Frame Number Register */
#define	OVRN		0x8000	/* b15: Overrun error */
#define	CRCE		0x4000	/* b14: Received data error */
#define	FRNM		0x07FF	/* b10-0: Frame number */

/* Micro Frame Number Register */
#define	UFRNM		0x0007	/* b2-0: Micro frame number */

/* Default Control Pipe Maxpacket Size Register */
/* Pipe Maxpacket Size Register */
#define	DEVSEL	0xF000	/* b15-14: Device address select */
#define	MAXP	0x007F	/* b6-0: Maxpacket size of default control pipe */

/* Default Control Pipe Control Register */
#define	BSTS		0x8000	/* b15: Buffer status */
#define	SUREQ		0x4000	/* b14: Send USB request  */
#define	CSCLR		0x2000	/* b13: complete-split status clear */
#define	CSSTS		0x1000	/* b12: complete-split status */
#define	SUREQCLR	0x0800	/* b11: stop setup request */
#define	SQCLR		0x0100	/* b8: Sequence toggle bit clear */
#define	SQSET		0x0080	/* b7: Sequence toggle bit set */
#define	SQMON		0x0040	/* b6: Sequence toggle bit monitor */
#define	PBUSY		0x0020	/* b5: pipe busy */
#define	PINGE		0x0010	/* b4: ping enable */
#define	CCPL		0x0004	/* b2: Enable control transfer complete */
#define	PID		0x0003	/* b1-0: Response PID */
#define	  PID_STALL11	 0x0003	  /* STALL */
#define	  PID_STALL	 0x0002	  /* STALL */
#define	  PID_BUF	 0x0001	  /* BUF */
#define	  PID_NAK	 0x0000	  /* NAK */

/* Pipe Window Select Register */
#define	PIPENM		0x0007	/* b2-0: Pipe select */

/* Pipe Configuration Register */
#define	R8A66597_TYP	0xC000	/* b15-14: Transfer type */
#define	  R8A66597_ISO	 0xC000		  /* Isochronous */
#define	  R8A66597_INT	 0x8000		  /* Interrupt */
#define	  R8A66597_BULK	 0x4000		  /* Bulk */
#define	R8A66597_BFRE	0x0400	/* b10: Buffer ready interrupt mode select */
#define	R8A66597_DBLB	0x0200	/* b9: Double buffer mode select */
#define	R8A66597_CNTMD	0x0100	/* b8: Continuous transfer mode select */
#define	R8A66597_SHTNAK	0x0080	/* b7: Transfer end NAK */
#define	R8A66597_DIR	0x0010	/* b4: Transfer direction select */
#define	R8A66597_EPNUM	0x000F	/* b3-0: Eendpoint number select */

/* Pipe Buffer Configuration Register */
#define	BUFSIZE		0x7C00	/* b14-10: Pipe buffer size */
#define	BUFNMB		0x007F	/* b6-0: Pipe buffer number */
#define	PIPE0BUF	256
#define	PIPExBUF	64

/* Pipe Maxpacket Size Register */
#define	MXPS		0x07FF	/* b10-0: Maxpacket size */

/* Pipe Cycle Configuration Register */
#define	IFIS	0x1000	/* b12: Isochronous in-buffer flush mode select */
#define	IITV	0x0007	/* b2-0: Isochronous interval */

/* Pipex Control Register */
#define	BSTS	0x8000	/* b15: Buffer status */
#define	INBUFM	0x4000	/* b14: IN buffer monitor (Only for PIPE1 to 5) */
#define	CSCLR	0x2000	/* b13: complete-split status clear */
#define	CSSTS	0x1000	/* b12: complete-split status */
#define	ATREPM	0x0400	/* b10: Auto repeat mode */
#define	ACLRM	0x0200	/* b9: Out buffer auto clear mode */
#define	SQCLR	0x0100	/* b8: Sequence toggle bit clear */
#define	SQSET	0x0080	/* b7: Sequence toggle bit set */
#define	SQMON	0x0040	/* b6: Sequence toggle bit monitor */
#define	PBUSY	0x0020	/* b5: pipe busy */
#define	PID	0x0003	/* b1-0: Response PID */

/* PIPExTRE */
#define	TRENB		0x0200	/* b9: Transaction counter enable */
#define	TRCLR		0x0100	/* b8: Transaction counter clear */

/* PIPExTRN */
#define	TRNCNT		0xFFFF	/* b15-0: Transaction counter */

/* DEVADDx */
#define	UPPHUB		0x7800
#define	HUBPORT		0x0700
#define	USBSPD		0x00C0
#define	RTPORT		0x0001


/* Timer for A-device reponsed to SRP from B-device */
#define	TA_SRP_RSPNS	(4900)	/* a_idle, max 4.9 seconds */

/* SE0 Timers */
#define	TB_SE0_SRP	(2)		/* b_idle, min 2ms */

/* D+ PULL UP Timers */
#define	TB_DATA_PLS	(10)

/* VBUS CHARGE Timers */
#define TB_CHARGE_VBUS	(30)

/* VBUS DISCHARGE Timers */
#define TB_DISCHARGE_VBUS	(60)

/* SRP Initiate Time */
#define	TB_SRP_INIT	(100)	/* b_srp_init */

/* SRP Fail Time */
#define	TB_SRP_FAIL	(6000)	/* b_srp_init, min 5s - max 6s */

#endif /* TUSB1211_H */
