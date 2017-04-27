/*
 * V4L2 Driver for SuperH Mobile RCU interface
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * Copyright (C) 2008 Magnus Damm
 *
 * Based on V4L2 Driver for PXA camera host - "pxa_camera.c",
 *
 * Copyright (C) 2006, Sascha Hauer, Pengutronix
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* for debug */
#undef DEBUG
/* #define DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/sh_clk.h>
#include <linux/spinlock.h>
#include <linux/earlysuspend.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/soc_camera.h>
#include <media/sh_mobile_rcu.h>
#include <media/sh_mobile_csi2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-mediabus.h>
#include <media/soc_mediabus.h>

#include <media/videobuf2-memops.h>
#include <mach/hardware.h>
#include <mach/pm.h>

/* camera device info */
bool cam_class_init;
struct class *camera_class;      /* /sys/class/camera */
struct device *sec_main_cam_dev; /* /sys/class/camera/rear/rear_type */
EXPORT_SYMBOL(sec_main_cam_dev);
struct device *sec_sub_cam_dev;  /* /sys/class/camera/rear/rear_type */
EXPORT_SYMBOL(sec_sub_cam_dev);

static int cam_status0;
static int cam_status1;

static bool rear_flash_state;
static int (*rear_flash_set)(int, int);
static bool dump_addr_flg;

#define SH_RCU_DUMP_LOG_ENABLE

#ifdef SH_RCU_DUMP_LOG_ENABLE
static struct page *dumplog_page;
static unsigned int dumplog_order;
static unsigned int dumplog_init_cnt;
static unsigned int *dumplog_addr;
static unsigned int *dumplog_ktbl;
static unsigned int *dumplog_max_idx;
static unsigned int *dumplog_cnt_idx;
spinlock_t lock_log;
#define SH_RCU_DUMP_LOG_SIZE_ALL (1024*1024)
#define SH_RCU_DUMP_LOG_SIZE_USER (800*1024)
#define SH_RCU_DUMP_LOG_OFFSET (8)
#define SH_RCU_GET_TIME() sh_mobile_rcu_get_timeval()
#define SH_RCU_TIMEVAL2USEC(x) (x.tv_sec * 1000000 + x.tv_usec)
#else  /* SH_RCU_DUMP_LOG_ENABLE */
#define SH_RCU_DUMP_LOG_SIZE_ALL		(0)
#define SH_RCU_DUMP_LOG_SIZE_USER		(0)
#endif /* SH_RCU_DUMP_LOG_ENABLE */

#define RCU_MERAM_ACTST1 (0x50)
#define RCU_MERAM_QSEL2	 (0x44)

#define RCU_MERAM_CTRL   (0x00)
#define RCU_MERAM_BSIZE  (0x04)
#define RCU_MERAM_MCNF   (0x08)
#define RCU_MERAM_SSARA  (0x10)
#define RCU_MERAM_SSARB  (0x14)
#define RCU_MERAM_SBSIZE (0x18)
#define RCU_MERAM_DBG    (0x1C)

#define RCU_MERAM_CTRL_C   (0x20)
#define RCU_MERAM_BSIZE_C  (0x24)
#define RCU_MERAM_MCNF_C   (0x28)
#define RCU_MERAM_SSARA_C  (0x30)
#define RCU_MERAM_SSARB_C  (0x34)
#define RCU_MERAM_SBSIZE_C (0x38)
#define RCU_MERAM_DBG_C    (0x3C)

#define RCU_MERAM_CH_RCU0	(36)
#define RCU_MERAM_CH_RCU1	(38)
#define RCU_MERAM_CH(x)		(x)
#define RCU_MERAM_CH_C(x)	((x)+1)
#define RCU_MERAM_BUFF		(1824)
#define RCU_MERAM_FRAMEA	(0)
#define RCU_MERAM_FRAMEB	(1)

#define RCU_MERAM_MAX_CONTBUF	(16)
#define RCU_MERAM_CONTI_BUF	(1)
#define RCU_MERAM_BLOCK		(0)
#define RCU_MERAM_MAX_LINEBUF	(16)
#define RCU_MERAM_GETLINEBUF(x)	((1024 >= x) ? 1 : \
				((2048 >= x) ? 2 : (4096 >= x) ? 4 : 8))

#define RCU_MERAM_STPSEQ_NORMAL	(0)
#define RCU_MERAM_STPSEQ_FORCE	(1)
#define RCU_MERAM_STP_FORCE	(2)

#define RCU_IPMMU_IMCTCR1	(0x00)
#define RCU_IPMMU_IMCTCR2	(0x04)
#define RCU_IPMMU_IMSTR		(0x08)
#define RCU_IPMMU_IMEAR		(0x0C)
#define RCU_IPMMU_IMASID	(0x10)
#define RCU_IPMMU_IMTTBR	(0x14)
#define RCU_IPMMU_IMTTBCR	(0x18)

#define RCU_POWAREA_MNG_ENABLE

#ifdef RCU_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif

#define SH_RCU_SNDCMD_SND			(0)
#define SH_RCU_SNDCMD_RCV			(1)
#define SH_RCU_SNDCMD_SNDRCV			(2)

struct sh_mobile_rcu_snd_cmd {
	unsigned int func;
	unsigned int snd_size;
	unsigned char *snd_buf;
	unsigned int rcv_size;
	unsigned char *rcv_buf;
};

/* alignment */
#define ALIGNxK(size, x)	(((unsigned int)(size) + (x*0x400-1)) \
					& ~(x*0x400-1))
#define ALIGN32(size)		(((unsigned int)(size) + 31) & ~31)
#define ALIGN4(size)		(((unsigned int)(size) +  3) &  ~3)
#define ALIGN8(size)		(((unsigned int)(size) +  7) &  ~7)
#define ALIGN_PACK20(size)	((unsigned int)((((unsigned int)(size)) + 15) \
					/ 16) * 20)
#define ALIGN_PACK24(size)	((unsigned int)((((unsigned int)(size)) + 15) \
					/ 16) * 24)

#define ROUNDDOWN(size, X)	(0 == (X) ? (size) : ((size) / (X)) * (X))

#define PRINT_FOURCC(fourcc)	(fourcc) & 0xff, ((fourcc) >> 8) & 0xff, ((fourcc) >> 16) & 0xff, ((fourcc) >> 24) & 0xff

/* register offsets for r8a73734 */

/* Capture start register */
/* Capture Mode control register */
/* Capture control register */
/* Capture Paralell Interface Control register */
/* Capture offset register */
/* Capture Parallel Interface Horizontal Cycle resgster */
/* RCU register control register */
/* RCU register forcible control register */
/* Capture size clip register */
/* Capture destination width register */
/* Capture data address Y register */
/* Capture data address C register */
/* Capture bundle destination size register */
/* Firewall operating control register */
/* Capture data output control register */
/* Capture event interrupt enable register */
/* Capture event flag clear register */
/* Capture status register */
/* Capture software reset register */
/* Capture data capacity register */
/* Capture data address Y register 2 */
/* Capture data address C register 2 */
/* Extra Capture Control Register */
/* Extra Capture data address register */
/* Extra Capture capacity register */
/* Extra Firewall operating control register */
#define RCAPSR				0x0000UL
#define RCAPCR				0x0004UL
#define RCAMCR				0x0008UL
#define RCPICR				0x000CUL
#define RCAMOR				0x0010UL
#define RCPIHCR				0x0014UL
#define RCRCNTR				0x0028UL
#define RCRCMPR				0x002CUL
#define RCSZR				0x0034UL
#define RCDWDR				0x0038UL
#define RCDAYR				0x003CUL
#define RCDACR				0x0040UL
#define RCBDSR				0x004CUL
#define RCFWCR				0x005CUL
#define RCDOCR				0x0064UL
#define RCEIER				0x0070UL
#define RCETCR				0x0074UL
#define RCSTSR				0x007CUL
#define RCSRTR				0x0080UL
#define RCDSSR				0x0084UL
#define RCDAYR2				0x0090UL
#define RCDACR2				0x0094UL
#define RCECNTR				0x0200UL
#define RCEDAYR				0x0204UL
#define RCEDSSR				0x0214UL
#define RCEFWCR				0x0228UL

#define RCMON1				0x00E0UL
#define RCMON2				0x00E4UL
#define RCMON3				0x00E8UL
#define RCMON4				0x00ECUL
#define RCMON5				0x00F0UL
#define RCMON6				0x00F4UL

#define RCAPSR_CPKIL		16	/* offset of RCAPSR.CPKIL */
#define RCAPSR_CSE			1	/* offset of RCAPSR.CSE */
#define RCAPSR_CE			0	/* offset of RCAPSR.CE */
#define RCAPCR_MTCM			20	/* offset of RCAPCR.MTCM */
#define RCAPCR_CTNCP		16	/* offset of RCAPCR.CTNCP */
#define RCAMCR_OCNT			28	/* offset of RCAMCR.OCNT */
#define RCAMCR_RPAC			24	/* offset of RCAMCR.RPAC */
#define RCAMCR_UCON			22	/* offset of RCAMCR.UCON */
#define RCAMCR_CMPMD		20	/* offset of RCAMCR.CMPMD */
#define RCAMCR_E810B		19	/* offset of RCAMCR.E810B */
#define RCAMCR_E810I		18	/* offset of RCAMCR.E810I */
#define RCAMCR_RAWTYP		16	/* offset of RCAMCR.RAWTYP */
#define RCAMCR_VSS2			15	/* offset of RCAMCR.VSS2 */
#define RCAMCR_HSSM			13	/* offset of RCAMCR.HSSM */
#define RCAMCR_HSS2			12	/* offset of RCAMCR.HSS2 */
#define RCAMCR_CFMT			4	/* offset of RCAMCR.CFMT */
#define RCPICR_IFS			13	/* offset of RCPICR.IFS */
#define RCPICR_DTARY		8	/* offset of RCPICR.DTARY */
#define RCPICR_CKPOL		2	/* offset of RCPICR.CKPOL */
#define RCPICR_VDPOL		1	/* offset of RCPICR.VDPOL */
#define RCPICR_HDPOL		0	/* offset of RCPICR.HDPOL */
#define RCAMOR_VOFST		16	/* offset of RCAMOR.VOFST */
#define RCAMOR_HOFST		0	/* offset of RCAMOR.HOFST */
#define RCPIHCR_STC			16	/* offset of RCPIHCR.STC */
#define RCPIHCR_ENDC		0	/* offset of RCPIHCR.ENDC */
#define RCRCNTR_RSS			17	/* offset of RCRCNTR.RSS */
#define RCRCNTR_RSC			16	/* offset of RCRCNTR.RSC */
#define RCRCNTR_RS			1	/* offset of RCRCNTR.RS */
#define RCRCNTR_RC			0	/* offset of RCRCNTR.RC */
#define RCRCMPR_RSA			16	/* offset of RCRCMPR.RSA */
#define RCRCMPR_RA			0	/* offset of RCRCMPR.RA */
#define RCSZR_VFCLP			16	/* offset of RCSZR.VFCLP */
#define RCSZR_HFCLP			0	/* offset of RCSZR.HFCLP */
#define RCDWDR_CHDW			0	/* offset of RCDWDR.CHDW */
#define RCDAYR_CAYR			0	/* offset of RCDAYR.CAYR */
#define RCDACR_CACR			0	/* offset of RCDACR.CACR */
#define RCBDSR_CBVS			0	/* offset of RCBDSR.CBVS */
#define RCFWCR_WLE			31	/* offset of RCFWCR.WLE */
#define RCFWCR_WLSE			0	/* offset of RCFWCR.WLSE */
#define RCDOCR_CBE			16	/* offset of RCDOCR.CBE */
#define RCDOCR_T420			15	/* offset of RCDOCR.T420 */
#define RCDOCR_H420			12	/* offset of RCDOCR.H420 */
#define RCDOCR_C1BS			9	/* offset of RCDOCR.C1BS */
#define RCDOCR_COLS			2	/* offset of RCDOCR.COLS */
#define RCDOCR_COWS			1	/* offset of RCDOCR.COWS */
#define RCDOCR_COBS			0	/* offset of RCDOCR.COBS */
#define RCEIER_DFOE			28	/* offset of RCEIER.DFOE */
#define RCEIER_ISPOE		24	/* offset of RCEIER.ISPOE */
#define RCEIER_EFWFE		23	/* offset of RCEIER.EFWFE */
#define RCEIER_FWFE			22	/* offset of RCEIER.FWFE */
#define RCEIER_EVBPE		21	/* offset of RCEIER.EVBPE */
#define RCEIER_VBPE			20	/* offset of RCEIER.VBPE */
#define RCEIER_ECDTOFE		17	/* offset of RCEIER.ECDTOFE */
#define RCEIER_CDTOFE		16	/* offset of RCEIER.CDTOFE */
#define RCEIER_CPBE2E		13	/* offset of RCEIER.CPBE2E */
#define RCEIER_CPBE1E		12	/* offset of RCEIER.CPBE1E */
#define RCEIER_VDE			9	/* offset of RCEIER.VDE */
#define RCEIER_HDE			8	/* offset of RCEIER.HDE */
#define RCEIER_EVDE			7	/* offset of RCEIER.EVDE */
#define RCEIER_IGRWE		4	/* offset of RCEIER.IGRWE */
#define RCEIER_CPSEE		2	/* offset of RCEIER.CPSEE */
#define RCEIER_CPEE			0	/* offset of RCEIER.CPEE */
#define RCETCR_DFO			28	/* offset of RCETCR.DFO */
#define RCETCR_ISPO			24	/* offset of RCETCR.ISPO */
#define RCETCR_EFWF			23	/* offset of RCETCR.EFWF */
#define RCETCR_FWF			22	/* offset of RCETCR.FWF */
#define RCETCR_EVBP			21	/* offset of RCETCR.EVBP */
#define RCETCR_VBP			20	/* offset of RCETCR.VBP */
#define RCETCR_ECDTOF		17	/* offset of RCETCR.ECDTOF */
#define RCETCR_CDTOF		16	/* offset of RCETCR.CDTOF */
#define RCETCR_CPBE2		13	/* offset of RCETCR.CPBE2 */
#define RCETCR_CPBE1		12	/* offset of RCETCR.CPBE1 */
#define RCETCR_VD			9	/* offset of RCETCR.VD */
#define RCETCR_HD			8	/* offset of RCETCR.HD */
#define RCETCR_EVD			7	/* offset of RCETCR.EVD */
#define RCETCR_IGRW			4	/* offset of RCETCR.IGRW */
#define RCETCR_CPSE			2	/* offset of RCETCR.CPSE */
#define RCETCR_CPE			0	/* offset of RCETCR.CPE */
#define RCSTSR_CRSST		25	/* offset of RCSTSR.CRSST */
#define RCSTSR_CRST			24	/* offset of RCSTSR.CRST */
#define RCSTSR_CPREQ		9	/* offset of RCSTSR.CPREQ */
#define RCSTSR_CPACK		8	/* offset of RCSTSR.CPACK */
#define RCSTSR_CPTSON		1	/* offset of RCSTSR.CPTSON */
#define RCSTSR_CPTON		0	/* offset of RCSTSR.CPTON */
#define RCSRTR_ALLRST		0	/* offset of RCSRTR.ALLRST */
#define RCDSSR_CDSS			0	/* offset of RCDSSR.CDSS */
#define RCDAYR2_CAYR2		0	/* offset of RCDAYR2.CAYR2 */
#define RCDACR2_CACR2		0	/* offset of RCDACR2.CACR2 */
#define RCECNTR_CTNCP		4	/* offset of RCECNTR.CTNCP */
#define RCECNTR_JPG			1	/* offset of RCECNTR.JPG */
#define RCEDAYR_CAYR		0	/* offset of RCEDAYR.CAYR */
#define RCEDSSR_CDSS		0	/* offset of RCEDSSR.CDSS */
#define RCEFWCR_WLE			31	/* offset of RCEFWCR.WLE */
#define RCEFWCR_WLSE		0	/* offset of RCEFWCR.WLSE */

#define SH_RCU_MODE_IMAGE		0x00	/* image capture mode */
#define SH_RCU_MODE_DATA		0x01	/* data fech mode */
#define SH_RCU_MODE_ENABLE		0x02	/* data enable fetch mode */

#define SH_RCU_STREAMING_OFF	0x00	/* streaming OFF */
#define SH_RCU_STREAMING_ON		0x01	/* streaming ON */

#define SH_RCU_RAWTYP_RAW8		0x00	/* RAW8 format */
#define SH_RCU_RAWTYP_RAW10		0x01	/* RAW10 format */
#define SH_RCU_RAWTYP_RAW12		0x02	/* RAW12 format */

/* 16bit Enhancing */
#define SH_RCU_RPAC_ENHANCE		0x00
/* Packing(MIPI-CSI2 standerd) */
#define SH_RCU_RPAC_PACKING		0x01

/* Transferred to the bus in 4-burst 1-transfer (4QW x 1) units */
#define SH_RCU_MTCM_SIZE_1		0x00
/* Transferred to the bus in 4-burst 2-transfer (4QW x 2) units */
#define SH_RCU_MTCM_SIZE_2		0x01
/* Transferred to the bus in 4-burst 4-transfer (4QW x 4) units */
#define SH_RCU_MTCM_SIZE_4		0x02
/* Transferred to the bus in 4-burst 8-transfer (4QW x 8) units */
#define SH_RCU_MTCM_SIZE_8		0x03
/* Image input data is fetched in the order of Cb, Y0, Cr, and Y1. */
#define SH_RCU_ORDER_UYVY		0x00
/* Image input data is fetched in the order of Cr, Y0, Cb, and Y1. */
#define SH_RCU_ORDER_VYUY		0x01
/* Image input data is fetched in the order of Y0, Cb, Y1, and Cr. */
#define SH_RCU_ORDER_YUYV		0x02
/* Image input data is fetched in the order of Y0, Cr, Y1, and Cb. */
#define SH_RCU_ORDER_YVYU		0x03

#define SH_RCU_MAX_WIDTH	8188	/* max width size  [pixels] */
#define SH_RCU_MAX_HEIGHT	8188	/* max height size [lines] */
#define SH_RCU_MIN_WIDTH	48		/* min width size  [pixels] */
#define SH_RCU_MIN_HEIGHT	48		/* min height size [lines] */

#define SH_RCU_BUF_INIT		(0x1234)

#undef DEBUG_GEOMETRY
#ifdef DEBUG_GEOMETRY
#define dev_geo	dev_info
#else
#define dev_geo	dev_dbg
#endif

/* per video frame buffer */
struct sh_mobile_rcu_buffer {
	struct vb2_buffer vb; /* v4l buffer must be first */
	struct list_head queue;
	int init_flg;
};

struct sh_mobile_rcu_dev {
	struct soc_camera_host ici;
	struct soc_camera_device *icd;
	struct platform_device *csi2_pdev;

	unsigned int irq;
	void __iomem *base;
	void __iomem *base_meram;
	void __iomem *base_meram_ch;
	void __iomem *ipmmu;
	int meram_frame;
	int meram_ch;
	u32 meram_ctrl[2];
	u32 meram_bsize[2];
	u32 meram_mcnf[2];
	u32 meram_sbsize[2];

	u32 kick;

	u32 int_status;
	size_t video_limit;
	size_t buf_total;

	spinlock_t lock;		/* Protects video buffer lists */
	struct list_head capture;
	struct vb2_buffer *active;
	struct vb2_alloc_ctx *alloc_ctx;

	struct sh_mobile_rcu_info *pdata;

	/* static max sizes either from platform data or default */
	int max_width;
	int max_height;

	int sequence;

	unsigned int image_mode:2;
	unsigned int output_mode:2;
	unsigned int streaming:1;
	unsigned int output_offset;
	unsigned int output_pack;
	unsigned int output_meram;
	unsigned int output_ispthinned;
	unsigned int zsl;
	unsigned int output_ext;

	struct clk *iclk;
	struct clk *fclk;
	struct clk *mclk;

	unsigned int mmap_size;
	struct page **mmap_pages;

	int rcu_early_suspend_clock_state;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend rcu_suspend_pm;
#endif /* CONFIG_HAS_EARLYSUSPEND */
};

struct sh_mobile_rcu_cam {
	/* RCU offsets */
	unsigned int rcu_left;
	unsigned int rcu_top;
	/* RCU output size */
	unsigned int width;
	unsigned int height;

	const struct soc_mbus_pixelfmt *extra_fmt;
	enum v4l2_mbus_pixelcode code;
};

static struct sh_mobile_rcu_buffer *to_rcu_vb(struct vb2_buffer *vb)
{
	return container_of(vb, struct sh_mobile_rcu_buffer, vb);
}

static void rcu_write(struct sh_mobile_rcu_dev *priv,
		      unsigned long reg_offs, u32 data)
{
	iowrite32(data, priv->base + reg_offs);
}

static u32 rcu_read(struct sh_mobile_rcu_dev *priv, unsigned long reg_offs)
{
	return ioread32(priv->base + reg_offs);
}

static u32 meram_read(struct sh_mobile_rcu_dev *priv, unsigned long reg_offs)
{
	return ioread32(priv->base_meram + reg_offs);
}

static void meram_ch_write(struct sh_mobile_rcu_dev *priv,
		      unsigned long reg_offs, u32 data)
{
	iowrite32(data, priv->base_meram_ch + reg_offs);
}

static u32 meram_ch_read(struct sh_mobile_rcu_dev *priv, unsigned long reg_offs)
{
	return ioread32(priv->base_meram_ch + reg_offs);
}

static void meram_ch_stop(struct sh_mobile_rcu_dev *priv,
	unsigned long reg_offs, u32 stp_set)
{
	u32 regMECTRL;
	regMECTRL = meram_ch_read(priv, reg_offs);
	if (!(0x20 & regMECTRL))
		meram_ch_write(priv, reg_offs, regMECTRL | stp_set);
}

static u32 meram_get_bsz(u32 bv_bsz)
{
	u32 ret = 1;
	u32 bsz;
	u32 i;

	bsz = bv_bsz & 0x7;
	for (i = 0; i < bsz; i++)
		ret *= 2;
	return ret;
}

static void meram_ch_wait(struct sh_mobile_rcu_dev *priv)
{
	int i = 0;
	u32 reg;
	u32 size;
	u32 kwbnm, bv_bsz;
	if (RCU_MERAM_FRAMEA == priv->meram_frame) {
		reg = RCU_MERAM_DBG;
		kwbnm = (meram_ch_read(priv, RCU_MERAM_MCNF) >> 28) + 1;
		bv_bsz = meram_get_bsz(
			meram_ch_read(priv, RCU_MERAM_CTRL) >> 28);
		size = meram_ch_read(priv, RCU_MERAM_SBSIZE);
	} else {
		reg = RCU_MERAM_DBG_C;
		kwbnm = (meram_ch_read(priv, RCU_MERAM_MCNF_C) >> 28) + 1;
		bv_bsz = meram_get_bsz(
			meram_ch_read(priv, RCU_MERAM_CTRL_C) >> 28);
		size = meram_ch_read(priv, RCU_MERAM_SBSIZE_C);
	}
	size = ~(size * bv_bsz * kwbnm - 1) & 0x03FFFFFF;

	while (1000 > i) {
		if ((rcu_read(priv, RCMON6) & size)
			== meram_ch_read(priv, reg))
			break;
		udelay(1);
		i++;
	}

	if (1000 <= i)
		dev_err(priv->icd->parent,
			"MERAM Wait error %s is active:"
			"  RCMON6[%08x] "
			"     DBG[%08x] "
			"    size[%08x] "
			"   count[%08x]\n",
			priv->meram_frame ? "A" : "B",
			rcu_read(priv, RCMON6),
			meram_ch_read(priv, reg), size, i);

}

static void meram_stop_seq(struct sh_mobile_rcu_dev *pcdev, u32 mode)
{
	u32 reg_set = 0x20;		/* NORMAL */
	u32 read_reg = 0;
	u32 reg_actst1;

	if (mode == RCU_MERAM_STPSEQ_FORCE) {
		reg_set = 0x60;
	}

	if ((SH_RCU_MODE_IMAGE == pcdev->image_mode) && (!pcdev->output_ext)) {
		meram_ch_stop(pcdev, RCU_MERAM_CTRL, reg_set);
		meram_ch_stop(pcdev, RCU_MERAM_CTRL_C, reg_set);
		return;
	}
	reg_actst1 = meram_read(pcdev, RCU_MERAM_ACTST1);
	reg_actst1 =
		(0x3 & (reg_actst1 >> (RCU_MERAM_CH(pcdev->meram_ch) - 32)));

	if (RCU_MERAM_FRAMEA == pcdev->meram_frame) {
		if ((RCU_MERAM_STPSEQ_NORMAL != mode) && !(reg_actst1 & 0x1)) {
			dev_warn(pcdev->icd->parent,
				"MERAM FORCE NoAct %s :"
				"  ACTST1[%08x] "
				"    CTRL[%08x] "
				"  CTRL_C[%08x]\n",
				pcdev->meram_frame ? "B" : "A",
				meram_read(pcdev, RCU_MERAM_ACTST1),
				meram_ch_read(pcdev, RCU_MERAM_CTRL),
				meram_ch_read(pcdev, RCU_MERAM_CTRL_C));
			return;
		}

		meram_ch_wait(pcdev);

		meram_ch_stop(pcdev, RCU_MERAM_CTRL, reg_set);
		read_reg = meram_ch_read(pcdev, RCU_MERAM_CTRL_C);
		if (read_reg & 0x60)
			dev_warn(pcdev->icd->parent,
				"MERAM Next %s is active:"
				"  ACTST1[%08x] "
				"    CTRL[%08x] "
				"  CTRL_C[%08x]\n",
				pcdev->meram_frame ? "A" : "B",
				meram_read(pcdev, RCU_MERAM_ACTST1),
				meram_ch_read(pcdev, RCU_MERAM_CTRL),
				read_reg);
	} else {
		if ((RCU_MERAM_STPSEQ_NORMAL != mode) && !(reg_actst1 & 0x2)) {
			dev_warn(pcdev->icd->parent,
				"MERAM FORCE NoAct %s :"
				"  ACTST1[%08x] "
				"    CTRL[%08x] "
				"  CTRL_C[%08x]\n",
				pcdev->meram_frame ? "B" : "A",
				meram_read(pcdev, RCU_MERAM_ACTST1),
				meram_ch_read(pcdev, RCU_MERAM_CTRL),
				meram_ch_read(pcdev, RCU_MERAM_CTRL_C));
			return;
		}

		meram_ch_wait(pcdev);

		meram_ch_stop(pcdev, RCU_MERAM_CTRL_C, reg_set);
		read_reg = meram_ch_read(pcdev, RCU_MERAM_CTRL);
		if (read_reg & 0x60)
			dev_warn(pcdev->icd->parent,
				"MERAM Next %s is active:"
				"  ACTST1[%08x] "
				"    CTRL[%08x] "
				"  CTRL_C[%08x]\n",
				pcdev->meram_frame ? "A" : "B",
				meram_read(pcdev, RCU_MERAM_ACTST1),
				read_reg,
				meram_ch_read(pcdev, RCU_MERAM_CTRL_C));
	}
	if (read_reg & 0x60)
		dev_err(pcdev->icd->parent,
			"MERAM switch%s :"
			"  ACTST1[%08x] "
			"    CTRL[%08x] "
			"  CTRL_C[%08x] "
			"    READ[%08x]\n",
			pcdev->meram_frame ? "B" : "A",
			meram_read(pcdev, RCU_MERAM_ACTST1),
			meram_ch_read(pcdev, RCU_MERAM_CTRL),
			meram_ch_read(pcdev, RCU_MERAM_CTRL_C),
			read_reg);
}

static void sh_mobile_rcu_dump_reg(struct sh_mobile_rcu_dev *pcdev)
{
	dev_err(pcdev->icd->parent,
		"  RCAPSR[%08x] "
		"  RCAPCR[%08x] "
		"  RCAMCR[%08x] "
		"  RCPICR[%08x]\n",
		rcu_read(pcdev, RCAPSR),
		rcu_read(pcdev, RCAPCR),
		rcu_read(pcdev, RCAMCR),
		rcu_read(pcdev, RCPICR));
	dev_err(pcdev->icd->parent,
		"  RCAMOR[%08x] "
		" RCPIHCR[%08x] "
		" RCRCNTR[%08x] "
		" RCRCMPR[%08x]\n",
		rcu_read(pcdev, RCAMOR),
		rcu_read(pcdev, RCPIHCR),
		rcu_read(pcdev, RCRCNTR),
		rcu_read(pcdev, RCRCMPR));
	dev_err(pcdev->icd->parent,
		"   RCSZR[%08x] "
		"  RCDWDR[%08x] "
		"  RCDAYR[%08x] "
		"  RCDACR[%08x]\n",
		rcu_read(pcdev, RCSZR),
		rcu_read(pcdev, RCDWDR),
		rcu_read(pcdev, RCDAYR),
		rcu_read(pcdev, RCDACR));
	dev_err(pcdev->icd->parent,
		"  RCBDSR[%08x] "
		"  RCFWCR[%08x] "
		"  RCDOCR[%08x] "
		"  RCEIER[%08x]\n",
		rcu_read(pcdev, RCBDSR),
		rcu_read(pcdev, RCFWCR),
		rcu_read(pcdev, RCDOCR),
		rcu_read(pcdev, RCEIER));
	dev_err(pcdev->icd->parent,
		"  RCETCR[%08x] "
		"  RCSTSR[%08x] "
		"  RCSRTR[%08x] "
		"  RCDSSR[%08x]\n",
		rcu_read(pcdev, RCETCR),
		rcu_read(pcdev, RCSTSR),
		rcu_read(pcdev, RCSRTR),
		rcu_read(pcdev, RCDSSR));
#if 0
	dev_err(pcdev->icd->parent,
		" RCDAYR2[%08x] "
		" RCDACR2[%08x] "
		" RCECNTR[%08x] "
		" RCEDAYR[%08x]\n",
		rcu_read(pcdev, RCDAYR2),
		rcu_read(pcdev, RCDACR2),
		rcu_read(pcdev, RCECNTR),
		rcu_read(pcdev, RCEDAYR));

	dev_err(pcdev->icd->parent,
		" RCEDSSR[%08x] "
		" RCEFWCR[%08x]\n",
		rcu_read(pcdev, RCEDSSR),
		rcu_read(pcdev, RCEFWCR));
#endif
	dev_err(pcdev->icd->parent,
		"  RCMON1[%08x] "
		"  RCMON2[%08x] "
		"  RCMON3[%08x] "
		"  RCMON4[%08x]\n",
		rcu_read(pcdev, RCMON1),
		rcu_read(pcdev, RCMON2),
		rcu_read(pcdev, RCMON3),
		rcu_read(pcdev, RCMON4));
	dev_err(pcdev->icd->parent,
		"  RCMON5[%08x] "
		"  RCMON6[%08x]\n",
		rcu_read(pcdev, RCMON5),
		rcu_read(pcdev, RCMON6));
	if (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram) {
		dev_err(pcdev->icd->parent,
			" IMCTCR1[%08x] "
			" IMCTCR2[%08x] "
			"   IMSTR[%08x] "
			"   IMEAR[%08x]\n",
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMCTCR1),
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMCTCR2),
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMSTR),
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMEAR));
		dev_err(pcdev->icd->parent,
			"  IMASID[%08x] "
			"  IMTTBR[%08x] "
			" IMTTBCR[%08x]\n",
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMASID),
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMTTBR),
			*(u32 *) (pcdev->ipmmu + RCU_IPMMU_IMTTBCR));
		dev_err(pcdev->icd->parent,
			"  ACTST1[%08x] "
			"    CTRL[%08x] "
			"   BSIZE[%08x] "
			"    MCNF[%08x]\n",
			meram_read(pcdev, RCU_MERAM_ACTST1),
			meram_ch_read(pcdev, RCU_MERAM_CTRL),
			meram_ch_read(pcdev, RCU_MERAM_BSIZE),
			meram_ch_read(pcdev, RCU_MERAM_MCNF));

		dev_err(pcdev->icd->parent,
			"   SSARA[%08x] "
			"   SSARB[%08x] "
			"  SBSIZE[%08x] "
			"     DBG[%08x]\n",
			meram_ch_read(pcdev, RCU_MERAM_SSARA),
			meram_ch_read(pcdev, RCU_MERAM_SSARB),
			meram_ch_read(pcdev, RCU_MERAM_SBSIZE),
			meram_ch_read(pcdev, RCU_MERAM_SBSIZE + 4));

		if (SH_RCU_MODE_IMAGE == pcdev->image_mode) {
			dev_err(pcdev->icd->parent,
				"                   "
				"  CTRL_C[%08x] "
				" BSIZE_C[%08x] "
				"  MCNF_C[%08x]\n",
				meram_ch_read(pcdev, RCU_MERAM_CTRL_C),
				meram_ch_read(pcdev, RCU_MERAM_BSIZE_C),
				meram_ch_read(pcdev, RCU_MERAM_MCNF_C));

			dev_err(pcdev->icd->parent,
				" SSARA_C[%08x] "
				" SSARB_C[%08x] "
				"SBSIZE_C[%08x] "
				"   DBG_C[%08x]\n",
				meram_ch_read(pcdev, RCU_MERAM_SSARA_C),
				meram_ch_read(pcdev, RCU_MERAM_SSARB_C),
				meram_ch_read(pcdev, RCU_MERAM_SBSIZE_C),
				meram_ch_read(pcdev, RCU_MERAM_SBSIZE_C + 4));
		}
	}
}

static void sh_mobile_rcu_meram_reset(struct sh_mobile_rcu_dev *pcdev)
{
	int i;
	u32 reg_actst1;
	for (i = 0; i < 10000; i++) {
		reg_actst1 = meram_read(pcdev, RCU_MERAM_ACTST1);
		reg_actst1 &=
			(3 << (RCU_MERAM_CH(pcdev->meram_ch) - 32));
		if (!reg_actst1)
			break;
		udelay(1);
	}
	if (10000 <= i) {
		dev_err(pcdev->icd->parent,
			"MERAM not stop %s-MODE,FRAME-%s\n",
			pcdev->image_mode ? "DATA" : "IMAGE",
			pcdev->meram_frame ? "B" : "A");
		sh_mobile_rcu_dump_reg(pcdev);
	} else
		dev_geo(pcdev->icd->parent,
			"MERAM stop %s :"
			" ACTST1[%08x] "
			" CTRL[%08x] "
			" CTRL_C[%08x]\n",
			pcdev->meram_frame ? "B" : "A",
			meram_read(pcdev, RCU_MERAM_ACTST1),
			meram_ch_read(pcdev, RCU_MERAM_CTRL),
			meram_ch_read(pcdev, RCU_MERAM_CTRL_C));

	if ((SH_RCU_MODE_IMAGE == pcdev->image_mode) && (!pcdev->output_ext)) {
		meram_ch_write(pcdev, RCU_MERAM_BSIZE,
			pcdev->meram_bsize[RCU_MERAM_FRAMEA]);
		meram_ch_write(pcdev, RCU_MERAM_BSIZE_C,
			pcdev->meram_bsize[RCU_MERAM_FRAMEB]);
		meram_ch_write(pcdev, RCU_MERAM_MCNF,
			pcdev->meram_mcnf[RCU_MERAM_FRAMEA]);
		meram_ch_write(pcdev, RCU_MERAM_MCNF_C,
			pcdev->meram_mcnf[RCU_MERAM_FRAMEB]);
		meram_ch_write(pcdev, RCU_MERAM_SBSIZE,
			pcdev->meram_sbsize[RCU_MERAM_FRAMEA]);
		meram_ch_write(pcdev, RCU_MERAM_SBSIZE_C,
			pcdev->meram_sbsize[RCU_MERAM_FRAMEB]);
		return;
	}

	if (RCU_MERAM_FRAMEA == pcdev->meram_frame) {
		meram_ch_write(pcdev, RCU_MERAM_BSIZE,
			pcdev->meram_bsize[RCU_MERAM_FRAMEA]);
		meram_ch_write(pcdev, RCU_MERAM_MCNF,
			pcdev->meram_mcnf[RCU_MERAM_FRAMEA]);
		meram_ch_write(pcdev, RCU_MERAM_SBSIZE,
			pcdev->meram_sbsize[RCU_MERAM_FRAMEA]);
	} else {
		meram_ch_write(pcdev, RCU_MERAM_BSIZE_C,
			pcdev->meram_bsize[RCU_MERAM_FRAMEB]);
		meram_ch_write(pcdev, RCU_MERAM_MCNF_C,
			pcdev->meram_mcnf[RCU_MERAM_FRAMEB]);
		meram_ch_write(pcdev, RCU_MERAM_SBSIZE_C,
			pcdev->meram_sbsize[RCU_MERAM_FRAMEB]);
	}
}

static int sh_mobile_rcu_soft_reset(struct sh_mobile_rcu_dev *pcdev)
{
	int i, success = 0;
	struct soc_camera_device *icd = pcdev->icd;

	rcu_write(pcdev, RCAPSR, 1 << RCAPSR_CPKIL); /* reset */

	/* wait RCSTSR.CPTON bit */
	for (i = 0; i < 1000; i++) {
		if (!(rcu_read(pcdev, RCSTSR) & (1 << RCSTSR_CPTON))) {
			success++;
			break;
		}
		udelay(1);
	}

	/* wait CAPSR.CPKIL bit */
	for (i = 0; i < 1000; i++) {
		if (!(rcu_read(pcdev, RCAPSR) & (1 << RCAPSR_CPKIL))) {
			success++;
			break;
		}
		udelay(1);
	}


	if (2 != success) {
		dev_err(icd->parent, "soft reset time out\n");
		return -EIO;
	} else {
		dev_warn(icd->parent, "soft reset success\n");
	}

	return 0;
}

int sh_mobile_rcu_bytes_per_line(u32 width,
const struct soc_mbus_pixelfmt *mf, struct sh_mobile_rcu_dev *pcdev)
{
	int bytes_per_line = -EINVAL;

	if (SH_RCU_MODE_IMAGE == pcdev->image_mode)
		bytes_per_line = soc_mbus_bytes_per_line(ALIGN32(width), mf);
	else
		bytes_per_line = ALIGN4(soc_mbus_bytes_per_line(width, mf));

	return bytes_per_line;
}

/*
 *  Videobuf operations
 */

/*
 * .queue_setup() is called to check, whether the driver can accept the
 *		  requested number of buffers and to fill in plane sizes
 *		  for the current frame format if required
 */
static int sh_mobile_rcu_videobuf_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			unsigned int *count, unsigned int *num_planes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct soc_camera_device *icd =
			container_of(vq, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	int bytes_per_line;
	unsigned int height;
#if 0
	if (fmt) {
		const struct soc_camera_format_xlate *xlate =
					soc_camera_xlate_by_fourcc(icd,
					fmt->fmt.pix.pixelformat);
		if (!xlate)
			return -EINVAL;
		bytes_per_line = soc_mbus_bytes_per_line(fmt->fmt.pix.width,
							 xlate->host_fmt);
		height = fmt->fmt.pix.height;
	} else {
		/* Called from VIDIOC_REQBUFS or in compatibility mode */
		bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
						icd->current_fmt->host_fmt);
		height = icd->user_height;
	}
#else
	bytes_per_line = sh_mobile_rcu_bytes_per_line(
						icd->user_width,
						icd->current_fmt->host_fmt,
						pcdev);

	dev_geo(icd->parent, "%s(): stride=%d width=%d packing=%d bps=%d\n",
		__func__, bytes_per_line, icd->user_width,
		icd->current_fmt->host_fmt->packing,
		icd->current_fmt->host_fmt->bits_per_sample);

	height = icd->user_height;
#endif
	if (bytes_per_line < 0)
		return bytes_per_line;

	sizes[0] = bytes_per_line * height;

	alloc_ctxs[0] = pcdev->alloc_ctx;

	if (!vq->num_buffers)
		pcdev->sequence = 0;

	if (!*count)
		*count = 2;

	/* If *num_planes != 0, we have already verified *count. */
	if (pcdev->video_limit && !*num_planes) {
		size_t size = PAGE_ALIGN(sizes[0]) * *count;

		if (size + pcdev->buf_total > pcdev->video_limit)
			*count = (pcdev->video_limit - pcdev->buf_total) /
				PAGE_ALIGN(sizes[0]);
	}

	*num_planes = 1;

	dev_dbg(icd->parent, "count=%d, size=%u\n", *count, sizes[0]);

	return 0;
}

/* acknowledge magical interrupt sources */
#define RCU_RCETCR_MAGIC	0x11F33395
/* prohibited register access interrupt bit */
#define RCU_RCETCR_IGRW		(1 << RCETCR_IGRW)
/* vbp error */
#define RCU_RCETCR_VBP		(1 << RCETCR_VBP)
/* depacking-fifo was overflow */
#define RCU_RCETCR_DFO		(1 << RCETCR_DFO)

#define RCU_RCETCR_CPE		(1 << RCETCR_CPE)

#define RCU_RCETCR_ISPO		(1 << RCETCR_ISPO)

#define RCU_RCETCR_CDTOF	(1 << RCETCR_CDTOF)

#define RCU_RCETCR_VD		(1 << RCETCR_VD)

/* continuous capture mode (if set) */
#define RCU_RCAPCR_CTNCP	(1 << RCAPCR_CTNCP)
/* one-frame capture end interrupt */
#define RCU_RCEIER_CPEE		(1 << RCEIER_CPEE)
/* vbp error */
#define RCU_RCEIER_VBPE		(1 << RCEIER_VBPE)
/* depacking-fifo was overflow */
#define RCU_RCEIER_DFOE		(1 << RCEIER_DFOE)
/* output of one frame is completed to ISP module. */
#define RCU_RCEIER_ISPOE	(1 << RCEIER_ISPOE)

#define RCU_RCEIER_CDTOFE	(1 << RCEIER_CDTOFE)

#define RCU_RCEIER_VDE		(1 << RCEIER_VDE)

/* error mask */
#define RCU_RCETCR_MASK_MEM_ISP	(RCU_RCETCR_CPE | RCU_RCETCR_VBP | \
					RCU_RCETCR_DFO | RCU_RCETCR_CDTOF)
#define RCU_RCETCR_MASK_MEM_ISP2	(RCU_RCETCR_VBP | RCU_RCETCR_DFO | \
					RCU_RCETCR_CDTOF)
#define RCU_RCETCR_MASK_MEM_ISP3	(RCU_RCETCR_ISPO | RCU_RCETCR_VBP)
#define RCU_RCETCR_MASK_MEM_ISP4	(RCU_RCETCR_ISPO | \
					RCU_RCETCR_MASK_MEM_ISP2)
#define RCU_RCETCR_ERR_MASK	(RCU_RCETCR_VBP | RCU_RCETCR_DFO | \
					RCU_RCETCR_CDTOF)
/* interrupt enable mask */
#define RCU_RCEIER_MASK		(RCU_RCEIER_ISPOE | RCU_RCEIER_VBPE | \
					RCU_RCEIER_DFOE | RCU_RCEIER_CDTOFE)
#define RCU_RCEIER_MASK2	(RCU_RCEIER_ISPOE | RCU_RCEIER_VBPE | \
					RCU_RCEIER_DFOE | RCU_RCEIER_CDTOFE | \
					RCU_RCEIER_VDE)
#define RCU_RCEIER_MASK3	(RCU_RCEIER_VBPE | RCU_RCEIER_DFOE | \
					RCU_RCEIER_CDTOFE)
/* interrupt enable mask */
#define RCU_RCEIER_MASK_MEM	(RCU_RCEIER_CPEE  | RCU_RCEIER_VBPE | \
					RCU_RCEIER_DFOE | RCU_RCEIER_CDTOFE)
/* interrupt enable mask */
#define RCU_RCEIER_MASK_MEM_ISP	(RCU_RCEIER_ISPOE | RCU_RCEIER_MASK3)

static bool sh_mobile_rcu_intr_log(struct sh_mobile_rcu_dev *pcdev, u32 err,
	char *log)
{
	bool is_log = false;
	bool is_cdtof = false;
	bool is_dfo = false;
	bool is_vbp = false;
	if (err & RCU_RCETCR_CDTOF) {
		is_cdtof = true;
		is_log = true;
	}
	if (err & RCU_RCETCR_DFO) {
		is_dfo = true;
		is_log = true;
	}
	if (err & RCU_RCETCR_VBP)
		is_vbp = true;

	dev_err(pcdev->icd->parent, "%s Interrupt[%08x]%s%s%s\n",
		log, err,
		is_cdtof ? " CDTOF(error)" : "",
		is_dfo ? " DFO(error)" : "",
		is_vbp ? " VBP" : "");

	return is_log;
}

/*
 * return value doesn't reflex the success/failure to queue the new buffer,
 * but rather the status of the previous buffer.
 */
static int sh_mobile_rcu_capture(struct sh_mobile_rcu_dev *pcdev, u32 irq)
{
	dma_addr_t phys_addr_top;
	u32 status, rcamcr, rceier;
	int ret = 0;
	struct soc_camera_device *icd = pcdev->icd;
	bool is_log = false;
	/*
	 * The hardware is _very_ picky about this sequence. Especially
	 * the RCU_RCETCR_MAGIC value. It seems like we need to acknowledge
	 * several not-so-well documented interrupt sources in CETCR.
	 */
	if (SH_RCU_OUTPUT_MEM == pcdev->output_mode)
		rceier = RCU_RCEIER_MASK_MEM;
	else if (SH_RCU_OUTPUT_ISP == pcdev->output_mode)
		rceier = RCU_RCEIER_MASK3;
	else
		rceier = RCU_RCEIER_MASK_MEM_ISP;

	rcu_write(pcdev, RCEIER, rcu_read(pcdev, RCEIER) & ~rceier);
	if (!irq)
		status = rcu_read(pcdev, RCETCR);
	else
		status = irq;
	rcu_write(pcdev, RCEIER, rcu_read(pcdev, RCEIER) | rceier);

	if (SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode) {
		status |= pcdev->int_status;
		pcdev->int_status = 0;
	}
	/*
	 * When a VBP interrupt occurs, a capture end interrupt does not occur
	 * and the image of that frame is not captured correctly. So, soft reset
	 * is needed here.
	 * When a DFO interrupt occurs, depacking-fifo was overflow. When the
	 * data input from the camera exceeds the processing performance in RCU,
	 * this interrupt is generated.
	 */
	if (status & RCU_RCETCR_ERR_MASK) {
		is_log = sh_mobile_rcu_intr_log(pcdev, status, "capture");

		if (is_log)
		sh_mobile_rcu_dump_reg(pcdev);

		sh_mobile_rcu_soft_reset(pcdev);

		if (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram) {
			sh_mobile_rcu_intr_log(pcdev, status, "capture MERAM");
			dev_err(pcdev->icd->parent,
				"MERAM stop %s-MODE,FRAME-%s\n",
				pcdev->image_mode ? "DATA" : "IMAGE",
				pcdev->meram_frame ? "B" : "A");
			meram_stop_seq(pcdev, RCU_MERAM_STPSEQ_FORCE);
			dev_geo(pcdev->icd->parent, "%s:meram clear\n",
				__func__);
			sh_mobile_rcu_meram_reset(pcdev);
		}
		if (is_log)
		sh_mobile_rcu_dump_reg(pcdev);
		ret = -EIO;
	}

	rcu_write(pcdev, RCETCR, ~status);

	/* ISP output */
	/* Memory and ISP output mode (Active buffer doesn't exist.) */
	if ((SH_RCU_OUTPUT_ISP     == pcdev->output_mode) ||
		(SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode &&
		!pcdev->active)) {
		/* set output mode */
		rcamcr = rcu_read(pcdev, RCAMCR);
		rcamcr = rcamcr & ~(3 << RCAMCR_OCNT);
		rcu_write(pcdev, RCAMCR, rcamcr |
			(SH_RCU_OUTPUT_ISP << RCAMCR_OCNT));

		if (rcu_read(pcdev, RCDAYR))
			dev_geo(pcdev->icd->parent,
				"NULL Write RCETCR[%08x],RCDAYR[%08x]\n",
				status, rcu_read(pcdev, RCDAYR));
		rcu_write(pcdev, RCDAYR, 0);
		rcu_write(pcdev, RCDACR, 0);
		rcu_write(pcdev, RCFWCR, 0);

	/* Memory and ISP output mode (Active buffer exists.) */
	/* Memory output (Active buffer exists.) */
	} else if ((SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode &&
			pcdev->active) ||
			(SH_RCU_OUTPUT_MEM == pcdev->output_mode &&
			pcdev->active)) {
		phys_addr_top = pcdev->active->v4l2_planes[0].m.userptr;

		if ((SH_RCU_OUTPUT_SDRAM != pcdev->output_meram)
			&& (SH_RCU_MODE_IMAGE == pcdev->image_mode)
			&& (!pcdev->output_ext)) {
			if (RCU_MERAM_FRAMEA == pcdev->meram_frame) {
				meram_ch_write(pcdev, RCU_MERAM_SSARA,
					phys_addr_top);
				phys_addr_top = 0xE0000000;
				pcdev->meram_frame = RCU_MERAM_FRAMEB;
			} else {
				meram_ch_write(pcdev, RCU_MERAM_SSARB,
					phys_addr_top);
				phys_addr_top = 0xE4000000;
				pcdev->meram_frame = RCU_MERAM_FRAMEA;
			}
		} else if (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram) {
			if (RCU_MERAM_FRAMEA == pcdev->meram_frame) {
				meram_ch_write(pcdev, RCU_MERAM_SSARA_C,
					phys_addr_top);
				phys_addr_top = 0xE8000000;
				pcdev->meram_frame = RCU_MERAM_FRAMEB;
			} else {
				meram_ch_write(pcdev, RCU_MERAM_SSARA,
					phys_addr_top);
				phys_addr_top = 0xE0000000;
				pcdev->meram_frame = RCU_MERAM_FRAMEA;
			}
		}
		dev_geo(pcdev->icd->parent,
			"MERAM kick  %s :"
			"  ACTST1[%08x] "
			"    CTRL[%08x] "
			"  CTRL_C[%08x]\n",
			pcdev->meram_frame ? "B" : "A",
			meram_read(pcdev, RCU_MERAM_ACTST1),
			meram_ch_read(pcdev, RCU_MERAM_CTRL),
			meram_ch_read(pcdev, RCU_MERAM_CTRL_C));

		rcu_write(pcdev, RCDAYR, phys_addr_top);
		if (!ret)
			dev_geo(pcdev->icd->parent,
				"set RCDAYR[%08x][%08x]\n",
				(u32)phys_addr_top,
				(u32)pcdev->active->v4l2_planes[0].m.userptr);

		if ((SH_RCU_MODE_IMAGE == pcdev->image_mode)
			&& (!pcdev->output_ext)) {
			phys_addr_top = pcdev->active->v4l2_planes[0].m.userptr;
			if (SH_RCU_OUTPUT_OFFSET_32B == pcdev->output_offset)
				phys_addr_top += ALIGN32(icd->user_width)
					* ALIGN32(icd->user_height);
			else
				phys_addr_top += ALIGN32(icd->user_width)
					* icd->user_height;
			if (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram) {
				if (RCU_MERAM_FRAMEB == pcdev->meram_frame) {
					meram_ch_write(pcdev, RCU_MERAM_SSARA_C,
						phys_addr_top);
					phys_addr_top = 0xE0000000 | (1 << 27);
				} else {
					meram_ch_write(pcdev, RCU_MERAM_SSARB_C,
						phys_addr_top);
					phys_addr_top = 0xE4000000 | (1 << 27);
				}
			}
			rcu_write(pcdev, RCDACR, phys_addr_top);
		} else {
			rcu_write(pcdev, RCDACR, 0);
		}
		/* set output mode */
		/*
		 * When there is not an active buffer,
		 * output mode is set in SH_RCU_OUTPUT_MEM_ISP.
		 * Therefore output mode is set again.
		 */
		rcamcr = rcu_read(pcdev, RCAMCR);
		rcamcr = rcamcr & ~(3 << RCAMCR_OCNT);
		rcu_write(pcdev, RCAMCR, rcamcr |
			(pcdev->output_mode << RCAMCR_OCNT));

		rcu_write(pcdev, RCFWCR, 0);

	} else {
		dev_err(pcdev->icd->parent,
			"warning route! mode[%d],active[%x],RCETCR[%08X]\n",
			(u32)pcdev->output_mode, (u32)pcdev->active, status);
		return ret;
	}

	if (!pcdev->zsl)
		if (SH_RCU_OUTPUT_ISP != pcdev->output_mode) {
			rcu_write(pcdev, RCAPSR, 1 << RCAPSR_CE);
			pcdev->kick++;
		}

	return ret;
}

static int sh_mobile_rcu_videobuf_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd =
		container_of(vb->vb2_queue, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct sh_mobile_rcu_buffer *buf;
	int bytes_per_line = -EINVAL;
	unsigned long size;

	if (SH_RCU_OUTPUT_ENHANCING == pcdev->output_pack)
		bytes_per_line = sh_mobile_rcu_bytes_per_line(icd->user_width,
				icd->current_fmt->host_fmt, pcdev);
	else {
		if (V4L2_PIX_FMT_SBGGR10 == icd->current_fmt->host_fmt->fourcc)
			bytes_per_line = ALIGN8(ALIGN_PACK20(icd->user_width));
		else
			bytes_per_line = ALIGN_PACK24(icd->user_width);
	}

	dev_dbg(icd->parent, "%s(vb=0x%p): 0x%lx %lu\n", __func__,
		vb, vb->v4l2_planes[0].m.userptr, vb2_get_plane_payload(vb, 0));

	if (bytes_per_line < 0)
		return bytes_per_line;

	buf = to_rcu_vb(vb);

	/* Added list head initialization on alloc */
	WARN(!list_empty(&buf->queue), "Buffer %p on queue!\n", vb);

#ifdef DEBUG
	/*
	 * This can be useful if you want to see if we actually fill
	 * the buffer with something
	 */
	if (vb2_plane_vaddr(vb, 0)) {
		memset(vb2_plane_vaddr(vb, 0), 0xaa,
			vb2_get_plane_payload(vb, 0));
	}
#endif

	BUG_ON(NULL == icd->current_fmt);

	size = icd->user_height * bytes_per_line;

	if (vb->v4l2_planes[0].length < size) {
		dev_err(icd->parent, "Buffer too small (%d < %lu)\n",
			vb->v4l2_planes[0].length, size);
		return -ENOBUFS;
	}

	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void sh_mobile_rcu_videobuf_queue(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd =
		container_of(vb->vb2_queue, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct sh_mobile_rcu_buffer *buf = to_rcu_vb(vb);
	unsigned long flags;

	dev_dbg(icd->parent, "%s(vb=0x%p): 0x%p %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));

	spin_lock_irqsave(&pcdev->lock, flags);
	list_add_tail(&buf->queue, &pcdev->capture);

	if (!pcdev->active) {
		pcdev->active = vb;
		if ((SH_RCU_STREAMING_ON == pcdev->streaming) &&
			(SH_RCU_OUTPUT_MEM == pcdev->output_mode)) {
			/* restart capture */
			sh_mobile_rcu_capture(pcdev, 0);
		}
	}
	spin_unlock_irqrestore(&pcdev->lock, flags);
}

static void sh_mobile_rcu_videobuf_release(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd =
	container_of(vb->vb2_queue, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_buffer *buf = to_rcu_vb(vb);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	unsigned long flags;

	dev_geo(icd->parent, "%s(): active=0x%p vb=0x%p\n", __func__,
		pcdev->active, vb);

	spin_lock_irqsave(&pcdev->lock, flags);

	if (pcdev->active == vb) {
		pcdev->active = NULL;
	}

	/*
	 * Doesn't hurt also if the list is empty, but it hurts, if queuing the
	 * buffer failed, and .buf_init() hasn't been called
	 */
/*	if (buf->queue.next) */
	/* Doesn't hurt also if the list is empty */
	if (SH_RCU_BUF_INIT == buf->init_flg)
		list_del_init(&buf->queue);

	pcdev->buf_total -= PAGE_ALIGN(vb2_plane_size(vb, 0));
	dev_dbg(icd->parent, "%s() %zu bytes buffers\n", __func__,
		pcdev->buf_total);

	spin_unlock_irqrestore(&pcdev->lock, flags);
}

static int sh_mobile_rcu_videobuf_init(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd =
	container_of(vb->vb2_queue, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;

	pcdev->buf_total += PAGE_ALIGN(vb2_plane_size(vb, 0));
	dev_dbg(icd->parent, "%s() %zu bytes buffers\n", __func__,
		pcdev->buf_total);

	/* This is for locking debugging only */
	INIT_LIST_HEAD(&to_rcu_vb(vb)->queue);
	to_rcu_vb(vb)->init_flg = SH_RCU_BUF_INIT;
	return 0;
}

static int sh_mobile_rcu_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct soc_camera_device *icd =
			container_of(q, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	unsigned long flags;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	int ret = 0;
	unsigned int meram_tmp = 0;

	dev_geo(icd->parent, "%s():\n", __func__);

	if (SH_RCU_OUTPUT_ISP_FULL != pcdev->output_ispthinned) {
		if (2 != (pcdev->icd->user_height % 4)) {
			dev_err(pcdev->icd->parent,
				"%s :W/A size error(%d)\n",
				__func__, pcdev->icd->user_height);
			return -1;
		}
	}

	spin_lock_irqsave(&pcdev->lock, flags);

	pcdev->kick = 0;
	pcdev->streaming = SH_RCU_STREAMING_ON;
	rcu_write(pcdev, RCEIER, 0);

	if (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram) {
#if 0
		meram_write(pcdev, RCU_MERAM_QSEL2,
			meram_read(pcdev, RCU_MERAM_QSEL2)
				| (3 << (RCU_MERAM_CH(pcdev->meram_ch) - 32)));
#endif
		if ((SH_RCU_MODE_IMAGE == pcdev->image_mode)
			&& (pcdev->output_ext)) {
			int cont_num = 4;
			u32 yc_mode;
			u32 wdr = rcu_read(pcdev, RCDWDR);
			yc_mode = rcu_read(pcdev, RCDOCR) & (1 << RCDOCR_T420);
			dev_geo(pcdev->icd->parent,
				"%s:meram start\n", __func__);
			/* A/B frame setting */
			meram_tmp = ALIGNxK(wdr * icd->user_height, cont_num);
			meram_tmp /= 0x400 * cont_num;

			pcdev->meram_bsize[RCU_MERAM_FRAMEA] =
				((meram_tmp - 1) << 16) |
				(0x400 * cont_num - 1);
			pcdev->meram_bsize[RCU_MERAM_FRAMEB] =
				((meram_tmp - 1) << 16) |
				(0x400 * cont_num - 1);

			meram_tmp = RCU_MERAM_MAX_CONTBUF / cont_num - 1;
			pcdev->meram_mcnf[RCU_MERAM_FRAMEA] = (meram_tmp << 16);
			pcdev->meram_mcnf[RCU_MERAM_FRAMEB] = (meram_tmp << 16);

			pcdev->meram_sbsize[RCU_MERAM_FRAMEA] = 0x400
				* cont_num;
			pcdev->meram_sbsize[RCU_MERAM_FRAMEB] = 0x400
				* cont_num;

			meram_tmp = 0;
			pcdev->meram_ctrl[RCU_MERAM_FRAMEA] =
				((pcdev->output_meram - 1) << 16) | 0x70A
					| (meram_tmp << 28);
			pcdev->meram_ctrl[RCU_MERAM_FRAMEB] =
				((pcdev->output_meram + 15) << 16) | 0x70A
					| (meram_tmp << 28);

		} else if ((SH_RCU_MODE_IMAGE == pcdev->image_mode)
			&& (!pcdev->output_ext)) {
			dev_geo(pcdev->icd->parent,
				"%s:meram start\n", __func__);
			/* y frame setting */
			meram_tmp = RCU_MERAM_GETLINEBUF(icd->user_width)
				* (1 << RCU_MERAM_BLOCK);
			if (RCU_MERAM_MAX_LINEBUF < (meram_tmp * 2)) {
				dev_err(pcdev->icd->parent,
					"%s:meram error CONTI[%d],BLOCK[%d]\n",
					__func__, meram_tmp, RCU_MERAM_BLOCK);
				return -1;
			}
			pcdev->meram_bsize[RCU_MERAM_FRAMEA] =
				((icd->user_height - 1) << 16)
					| (icd->user_width - 1);

			meram_tmp = RCU_MERAM_MAX_LINEBUF /
				RCU_MERAM_GETLINEBUF(icd->user_width) - 1;
			pcdev->meram_mcnf[RCU_MERAM_FRAMEA] = (meram_tmp << 16);
			pcdev->meram_sbsize[RCU_MERAM_FRAMEA] = icd->user_width;
			pcdev->meram_ctrl[RCU_MERAM_FRAMEA] =
				((pcdev->output_meram - 1) << 16) | 0x702
					| (RCU_MERAM_BLOCK << 28);

			/* c frame setting */
			if (rcu_read(pcdev, RCDOCR) & (1 << RCDOCR_T420)) {
				pcdev->meram_bsize[RCU_MERAM_FRAMEB] =
					((icd->user_height / 2 - 1) << 16)
						| (icd->user_width - 1);
			} else {
				pcdev->meram_bsize[RCU_MERAM_FRAMEB] =
					((icd->user_height - 1) << 16)
						| (icd->user_width - 1);
			}
			pcdev->meram_mcnf[RCU_MERAM_FRAMEB] = (meram_tmp << 16);
			pcdev->meram_sbsize[RCU_MERAM_FRAMEB] = icd->user_width;
			pcdev->meram_ctrl[RCU_MERAM_FRAMEB] =
				((pcdev->output_meram + 15) << 16) | 0x702
					| (RCU_MERAM_BLOCK << 28);

			rcu_write(pcdev, RCDWDR, 0x1000);
		} else {
			int cont_num = RCU_MERAM_CONTI_BUF;
			u32 wdr = rcu_read(pcdev, RCDWDR);
			dev_geo(pcdev->icd->parent,
				"%s:meram start\n", __func__);
			while (1) {
				meram_tmp = ALIGNxK(wdr * icd->user_height,
					cont_num);
				meram_tmp /= 0x400 * cont_num;
				if (0x1000 > meram_tmp)
					break;
				else
					cont_num *= 2;
			}
			pcdev->meram_bsize[RCU_MERAM_FRAMEA] = ((meram_tmp - 1)
				<< 16) | (0x400 * cont_num - 1);
			pcdev->meram_bsize[RCU_MERAM_FRAMEB] = ((meram_tmp - 1)
				<< 16) | (0x400 * cont_num - 1);

			meram_tmp = cont_num * (1 << RCU_MERAM_BLOCK);
			if (RCU_MERAM_MAX_CONTBUF < (meram_tmp * 2)) {
				dev_err(pcdev->icd->parent,
					"%s:meram error CONTI[%d],BLOCK[%d]\n",
					__func__, cont_num, RCU_MERAM_BLOCK);
				return -1;
			}
			meram_tmp = RCU_MERAM_MAX_CONTBUF / cont_num - 1;
			pcdev->meram_mcnf[RCU_MERAM_FRAMEA] = (meram_tmp << 16);
			pcdev->meram_mcnf[RCU_MERAM_FRAMEB] = (meram_tmp << 16);
			pcdev->meram_sbsize[RCU_MERAM_FRAMEA] = 0x400
				* cont_num;
			pcdev->meram_sbsize[RCU_MERAM_FRAMEB] = 0x400
				* cont_num;
			pcdev->meram_ctrl[RCU_MERAM_FRAMEA] =
				((pcdev->output_meram - 1) << 16) | 0x70A
					| (RCU_MERAM_BLOCK << 28);
			pcdev->meram_ctrl[RCU_MERAM_FRAMEB] =
				((pcdev->output_meram - 1
					+ RCU_MERAM_MAX_CONTBUF) << 16) | 0x70A
					| (RCU_MERAM_BLOCK << 28);
		}
		meram_ch_write(pcdev, RCU_MERAM_BSIZE,
			pcdev->meram_bsize[RCU_MERAM_FRAMEA]);
		meram_ch_write(pcdev, RCU_MERAM_BSIZE_C,
			pcdev->meram_bsize[RCU_MERAM_FRAMEB]);
			meram_ch_write(pcdev, RCU_MERAM_MCNF,
			pcdev->meram_mcnf[RCU_MERAM_FRAMEA]);
			meram_ch_write(pcdev, RCU_MERAM_MCNF_C,
			pcdev->meram_mcnf[RCU_MERAM_FRAMEB]);
			meram_ch_write(pcdev, RCU_MERAM_SBSIZE,
			pcdev->meram_sbsize[RCU_MERAM_FRAMEA]);
			meram_ch_write(pcdev, RCU_MERAM_SBSIZE_C,
			pcdev->meram_sbsize[RCU_MERAM_FRAMEB]);
			meram_ch_write(pcdev, RCU_MERAM_CTRL,
			pcdev->meram_ctrl[RCU_MERAM_FRAMEA]);
			meram_ch_write(pcdev, RCU_MERAM_CTRL_C,
			pcdev->meram_ctrl[RCU_MERAM_FRAMEB]);
	}

	dev_geo(pcdev->icd->parent,
		"MERAM start %s :"
		"  ACTST1[%08x] "
		"    CTRL[%08x] "
		"  CTRL_C[%08x]\n",
		pcdev->meram_frame ? "B" : "A",
		meram_read(pcdev, RCU_MERAM_ACTST1),
		meram_ch_read(pcdev, RCU_MERAM_CTRL),
		meram_ch_read(pcdev, RCU_MERAM_CTRL_C));

	if (SH_RCU_OUTPUT_ISP == pcdev->output_mode) {
		/*rcu_write(pcdev, RCEIER, RCU_RCEIER_MASK3);*/
		rcu_write(pcdev, RCAPCR,
			rcu_read(pcdev, RCAPCR) | RCU_RCAPCR_CTNCP);
		rcu_write(pcdev, RCAPSR, 1 << RCAPSR_CE); /* start capture */
	} else {
		sh_mobile_rcu_capture(pcdev, 0);
	}

	spin_unlock_irqrestore(&pcdev->lock, flags);

	ret = v4l2_device_call_until_err(sd->v4l2_dev, 0,
				video, s_stream, 1);
	if (ret) {
		dev_err(pcdev->icd->parent,
			"%s :Error v4l2_device_call_until_err(%d)\n",
				__func__, ret);
	}

	return 0;
}

static int sh_mobile_rcu_stop_streaming(struct vb2_queue *q)
{
	struct soc_camera_device *icd =
			container_of(q, struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct list_head *buf_head, *tmp;
	unsigned long flags;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	int ret = 0;
	int i = 0;

	dev_geo(icd->parent, "%s():\n", __func__);

	ret = v4l2_device_call_until_err(sd->v4l2_dev, 0, video, s_stream, 0);
	if (ret) {
		dev_err(pcdev->icd->parent,
			"%s :Error v4l2_device_call_until_err(%d)\n",
				__func__, ret);
	}

	spin_lock_irqsave(&pcdev->lock, flags);

	list_for_each_safe(buf_head, tmp, &pcdev->capture)
		list_del_init(buf_head);

	pcdev->streaming = SH_RCU_STREAMING_OFF;

	if (SH_RCU_OUTPUT_ISP == pcdev->output_mode) {
		rcu_write(pcdev, RCAPSR, 0);
		rcu_write(pcdev, RCAPCR,
			rcu_read(pcdev, RCAPCR) & ~RCU_RCAPCR_CTNCP);
	}

	spin_unlock_irqrestore(&pcdev->lock, flags);

	for (i = 0; i < 1000; i++) {
		if ((!pcdev->kick) && !(rcu_read(pcdev, RCSTSR) & 0x00000001))
			break;
		mdelay(1);
	}
	if (1000 <= i) {
		dev_err(pcdev->icd->parent,
			"Stop error RCU is Active [%08x] kick is %d\n",
			rcu_read(pcdev, RCSTSR), pcdev->kick);
		ret = v4l2_device_call_until_err(sd->v4l2_dev, 0,
			video, s_stream, 2);
		if (ret) {
			dev_err(pcdev->icd->parent,
				"%s :Error v4l2_device_call_until_err"
				"[s_stream](%d)\n",
				__func__, ret);
		}
		//sh_mobile_rcu_soft_reset(pcdev);
		if (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram)
			meram_stop_seq(pcdev, RCU_MERAM_STPSEQ_FORCE);
		ret = -1;
	} else
		ret = 0;

	if (!ret && (SH_RCU_OUTPUT_SDRAM != pcdev->output_meram)) {
		u32 reg_actst1;
		meram_stop_seq(pcdev, RCU_MERAM_STP_FORCE);
		dev_geo(pcdev->icd->parent, "%s:meram stop\n", __func__);
		for (i = 0; i < 10000; i++) {
			reg_actst1 = meram_read(pcdev, RCU_MERAM_ACTST1);
			reg_actst1 &=
				(3 << (RCU_MERAM_CH(pcdev->meram_ch) - 32));
			if (!reg_actst1)
				break;
			udelay(1);
		}
		if (10000 <= i) {
			dev_err(pcdev->icd->parent,
				"MERAM not stop %s-MODE,FRAME-%s\n",
				pcdev->image_mode ? "DATA" : "IMAGE",
				pcdev->meram_frame ? "B" : "A");
			sh_mobile_rcu_dump_reg(pcdev);
		} else
			dev_geo(pcdev->icd->parent,
				"MERAM stop  %s :"
				"  ACTST1[%08x] "
				"    CTRL[%08x] "
				"  CTRL_C[%08x]\n",
				pcdev->meram_frame ? "B" : "A",
				meram_read(pcdev, RCU_MERAM_ACTST1),
				meram_ch_read(pcdev, RCU_MERAM_CTRL),
				meram_ch_read(pcdev, RCU_MERAM_CTRL_C));
#if 0
		meram_write(pcdev, RCU_MERAM_QSEL2,
			meram_read(pcdev, RCU_MERAM_QSEL2)
				& ~(3 << (RCU_MERAM_CH(pcdev->meram_ch) - 32)));
#endif
	}
	return 0;
}

static struct vb2_ops sh_mobile_rcu_videobuf_ops = {
	.queue_setup		= sh_mobile_rcu_videobuf_setup,
	.buf_prepare		= sh_mobile_rcu_videobuf_prepare,
	.buf_queue			= sh_mobile_rcu_videobuf_queue,
	.buf_cleanup		= sh_mobile_rcu_videobuf_release,
	.buf_init			= sh_mobile_rcu_videobuf_init,
	.wait_prepare		= soc_camera_unlock,
	.wait_finish		= soc_camera_lock,
	.start_streaming	= sh_mobile_rcu_start_streaming,
	.stop_streaming		= sh_mobile_rcu_stop_streaming,
};

static irqreturn_t sh_mobile_rcu_irq(int irq, void *data)
{
	struct sh_mobile_rcu_dev *pcdev = data;
	struct vb2_buffer *vb;
	int ret;
	unsigned long flags;
	u32 regRCETCR, regRCDAYR;
	int sub_ret = 0;
	u32 sub_status = 0;
	struct v4l2_subdev *sd;

	spin_lock_irqsave(&pcdev->lock, flags);

	/* stream on */
	if (SH_RCU_STREAMING_ON == pcdev->streaming) {

		regRCETCR = rcu_read(pcdev, RCETCR);
		regRCDAYR = rcu_read(pcdev, RCDAYR);
		vb = pcdev->active;

		/* ISP output */
		/* Memory and ISP output mode (Active buffer doesn't exist.) */
		if ((SH_RCU_OUTPUT_ISP == pcdev->output_mode)
			|| ((SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode)
				&& (!regRCDAYR)
			&& (regRCETCR & RCU_RCETCR_MASK_MEM_ISP4))) {
			sh_mobile_rcu_capture(pcdev, regRCETCR);

		/* Memory and ISP output mode (Active buffer exists.) */
		/* Memory output (Active buffer exists.) */
		} else if (((SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode)
			&& (regRCDAYR)
			&& (regRCETCR & RCU_RCETCR_MASK_MEM_ISP3))
			|| (SH_RCU_OUTPUT_MEM == pcdev->output_mode && vb)) {

			if (regRCETCR & RCU_RCETCR_ERR_MASK) {
				/* Skip MERAM clear in error case */
				sh_mobile_rcu_intr_log(pcdev, regRCETCR,
					"Skip MERAM clear routine!!");
			} else {
				if (SH_RCU_OUTPUT_SDRAM
						!= pcdev->output_meram) {
					meram_stop_seq(pcdev,
						RCU_MERAM_STPSEQ_NORMAL);
					dev_geo(pcdev->icd->parent,
						"%s:meram clear\n", __func__);
				}
			}

			if (0 == pcdev->kick)
				dev_warn(pcdev->icd->parent,
					"streaming route error kick is zero!\n");
			else
				pcdev->kick--;

			list_del_init(&to_rcu_vb(vb)->queue);

			if (!list_empty(&pcdev->capture))
				pcdev->active =
					&list_entry(pcdev->capture.next,
					struct sh_mobile_rcu_buffer, queue)->vb;
			else
				pcdev->active = NULL;

			ret = sh_mobile_rcu_capture(pcdev, regRCETCR);
			do_gettimeofday(&vb->v4l2_buf.timestamp);

			sd = soc_camera_to_subdev(pcdev->icd);
			sub_ret = v4l2_device_call_until_err(sd->v4l2_dev, 0,
				video, g_input_status, &sub_status);
			if (sub_ret) {
				dev_err(pcdev->icd->parent,
					"%s :Error "
					"v4l2_device_call_until_err(%d)\n",
					__func__, ret);
				ret = -1;
			} else {
				if (sub_status) {
					/* sub device error */
					ret = -1;
				}
			}

			if (!ret) {
				vb->v4l2_buf.field = V4L2_FIELD_NONE;
				vb->v4l2_buf.sequence = pcdev->sequence++;
			}
			vb2_buffer_done(vb,
			ret < 0 ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);

		} else if ((SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode)
				&& (regRCETCR & RCU_RCETCR_MASK_MEM_ISP2)) {
			rcu_write(pcdev, RCEIER, rcu_read(pcdev, RCEIER) &
				~RCU_RCEIER_CDTOFE);
			pcdev->int_status |= regRCETCR &
				RCU_RCETCR_MASK_MEM_ISP2;
			rcu_write(pcdev, RCETCR,
				~(regRCETCR & RCU_RCETCR_MASK_MEM_ISP2));
			sh_mobile_rcu_intr_log(pcdev, regRCETCR, "pre Error");
		} else {
			rcu_write(pcdev, RCETCR, ~RCU_RCETCR_MAGIC);
			dev_err(pcdev->icd->parent,
				"%s:error sequence RCETCR[%08x],RCDAYR[%08x]\n",
				__func__, regRCETCR, regRCDAYR);
		}
	} else {
		regRCETCR = rcu_read(pcdev, RCETCR);
		regRCDAYR = rcu_read(pcdev, RCDAYR);
		rcu_write(pcdev, RCETCR, ~RCU_RCETCR_MAGIC);
		dev_warn(pcdev->icd->parent,
			"%s:not stream sequence RCETCR[%08x],RCDAYR[%08x]\n",
			__func__, regRCETCR, regRCDAYR);
		if (0 == pcdev->kick)
			dev_warn(pcdev->icd->parent,
				"not streaming route error kick is zero!\n");
		else
			pcdev->kick--;
	}

	spin_unlock_irqrestore(&pcdev->lock, flags);

	return IRQ_HANDLED;
}

static struct v4l2_subdev *find_csi2(struct sh_mobile_rcu_dev *pcdev)
{
	struct v4l2_subdev *sd;

	if (!pcdev->csi2_pdev)
		return NULL;

	v4l2_device_for_each_subdev(sd, &pcdev->ici.v4l2_dev)
		if (&pcdev->csi2_pdev->dev == v4l2_get_subdevdata(sd))
			return sd;

	return NULL;
}

/* Called with .video_lock held */
static int sh_mobile_rcu_add_device(struct soc_camera_device *icd)
{
#ifdef RCU_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct v4l2_subdev *csi2_sd;
	int ret;

	dev_geo(icd->parent, "%s():\n", __func__);

	csi2_sd = find_csi2(pcdev);
	if (csi2_sd) {
		csi2_sd->grp_id = soc_camera_grp_id(icd);
		v4l2_set_subdev_hostdata(csi2_sd, icd);
	}

	ret = v4l2_subdev_call(csi2_sd, core, s_power, 1);
	if (ret < 0 && ret != -ENOIOCTLCMD && ret != -ENODEV) {
		pm_runtime_put_sync(ici->v4l2_dev.dev);
		return ret;
	}

	/*
	 * -ENODEV is special: either csi2_sd == NULL or the CSI-2 driver
	 * has not found this soc-camera device among its clients
	 */
	if (ret == -ENODEV && csi2_sd)
		csi2_sd->grp_id = 0;

	if (pcdev->icd)
		return -EBUSY;

#ifdef RCU_POWAREA_MNG_ENABLE
	dev_info(icd->parent, "Start A4LC power area(RCU)\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		dev_warn(icd->parent, "powarea_start_notify err!\n");

	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	/* request irq */
	ret = request_irq(pcdev->irq, sh_mobile_rcu_irq, IRQF_DISABLED,
			  dev_name(pcdev->ici.v4l2_dev.dev), pcdev);
	if (ret) {
		dev_err(pcdev->ici.v4l2_dev.dev, "Unable to register RCU interrupt.\n");
#ifdef RCU_POWAREA_MNG_ENABLE
		system_handle = system_pwmng_new();
		powarea_start_notify.handle         = system_handle;
		powarea_start_notify.powerarea_name = RT_PWMNG_POWERAREA_A4LC;
		ret = system_pwmng_powerarea_end_notify(
			&powarea_start_notify);
		if (ret != SMAP_LIB_PWMNG_OK) {
			dev_warn(icd->parent,
				"powarea_end_notify err!\n");
		}
		pmg_delete.handle = system_handle;
		system_pwmng_delete(&pmg_delete);
#endif /* RCU_POWAREA_MNG_ENABLE */
		return ret;
	}

	pcdev->rcu_early_suspend_clock_state =
		disable_early_suspend_clock();

	dev_info(icd->parent,
		 "SuperH Mobile RCU driver attached to camera %d\n",
		 icd->devnum);

	pm_runtime_get_sync(ici->v4l2_dev.dev);

	pcdev->buf_total = 0;

	pcdev->icd = icd;

	sh_mobile_rcu_soft_reset(pcdev);

	pcdev->icd = icd;

	return 0;
}

/* Called with .video_lock held */
static void sh_mobile_rcu_remove_device(struct soc_camera_device *icd)
{
#ifdef RCU_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_delete pmg_delete;
	int ret;
#endif

	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct v4l2_subdev *csi2_sd = find_csi2(pcdev);
	unsigned long flags;

	dev_geo(icd->parent, "%s(): streaming=%d active=%p\n",
		__func__, pcdev->streaming, pcdev->active);

	BUG_ON(icd != pcdev->icd);

	/* disable capture, disable interrupts */
	rcu_write(pcdev, RCEIER, 0);
	sh_mobile_rcu_soft_reset(pcdev);
	pcdev->streaming = SH_RCU_STREAMING_OFF;
	/* make sure active buffer is canceled */
	spin_lock_irqsave(&pcdev->lock, flags);
	if (pcdev->active) {
		list_del_init(&to_rcu_vb(pcdev->active)->queue);
		vb2_buffer_done(pcdev->active, VB2_BUF_STATE_ERROR);
		pcdev->active = NULL;
	}
	spin_unlock_irqrestore(&pcdev->lock, flags);

	pm_runtime_put_sync(ici->v4l2_dev.dev);

	free_irq(pcdev->irq, pcdev);

	dev_info(icd->parent,
		 "SuperH Mobile RCU driver detached from camera %d\n",
		 icd->devnum);

	kfree(pcdev->mmap_pages);
	pcdev->mmap_pages = NULL;

#ifdef RCU_POWAREA_MNG_ENABLE
	dev_info(icd->parent, "End A4LC power area(RCU)\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK) {
		dev_warn(icd->parent, "powarea_end_notify err!\n");
		pmg_delete.handle = system_handle;
		system_pwmng_delete(&pmg_delete);
		return;
	}

	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	v4l2_subdev_call(csi2_sd, core, s_power, 0);
	if (csi2_sd)
		csi2_sd->grp_id = 0;

	if (0 == pcdev->rcu_early_suspend_clock_state)
		enable_early_suspend_clock();

	pcdev->icd = NULL;
}

/* rect is guaranteed to not exceed the scaled camera rectangle */
static void sh_mobile_rcu_set_rect(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_cam *cam = icd->host_priv;
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	u32 rcamor = 0, rcszr = 0, rcdwdr = 0;
	int bytes_per_line;

	dev_geo(icd->parent, "Crop %ux%u@%u:%u\n",
		icd->user_width, icd->user_height, cam->rcu_left, cam->rcu_top);

	if (SH_RCU_MODE_IMAGE == pcdev->image_mode) {
		if (pcdev->output_ext)
			rcdwdr = ALIGN32(icd->user_width * 2);
		else
			rcdwdr = ALIGN32(icd->user_width);

	} else {
		if (SH_RCU_OUTPUT_ENHANCING == pcdev->output_pack)
			bytes_per_line = sh_mobile_rcu_bytes_per_line(
				icd->user_width, icd->current_fmt->host_fmt,
				pcdev);
		else
			if (V4L2_PIX_FMT_SBGGR10 ==
				icd->current_fmt->host_fmt->fourcc)
				bytes_per_line =
					ALIGN8(ALIGN_PACK20(icd->user_width));
			else
				bytes_per_line = ALIGN_PACK24(icd->user_width);

		if (bytes_per_line < 0)
			rcdwdr = ALIGN4(icd->user_width * 2);
		else
			rcdwdr = bytes_per_line;
	}

	/* Set RCAMOR, RCSZR, RCDWDR */
	rcamor = cam->rcu_left | (cam->rcu_top << RCAMOR_VOFST);
	if (pcdev->output_ext)
		rcszr = (icd->user_width * 2)
			| (icd->user_height << RCSZR_VFCLP);
	else
		rcszr = icd->user_width | (icd->user_height << RCSZR_VFCLP);

	dev_geo(icd->parent,
		"RCAMOR 0x%08x, RCSZR 0x%08x, RCDWDR 0x%08x\n",
		rcamor, rcszr, rcdwdr);

	rcu_write(pcdev, RCAMOR, rcamor);
	rcu_write(pcdev, RCSZR,  rcszr);
	rcu_write(pcdev, RCDWDR, rcdwdr);
}

/* Find the bus subdevice driver, e.g., CSI2 */
static struct v4l2_subdev *find_bus_subdev(struct sh_mobile_rcu_dev *pcdev,
					   struct soc_camera_device *icd)
{
	if (pcdev->csi2_pdev) {
		struct v4l2_subdev *csi2_sd = find_csi2(pcdev);
		if (csi2_sd && csi2_sd->grp_id == soc_camera_grp_id(icd))
			return csi2_sd;
	}

	return soc_camera_to_subdev(icd);
}

#define RCU_BUS_FLAGS (V4L2_MBUS_MASTER |	\
		V4L2_MBUS_PCLK_SAMPLE_RISING |	\
		V4L2_MBUS_HSYNC_ACTIVE_HIGH |	\
		V4L2_MBUS_HSYNC_ACTIVE_LOW |	\
		V4L2_MBUS_VSYNC_ACTIVE_HIGH |	\
		V4L2_MBUS_VSYNC_ACTIVE_LOW |	\
		V4L2_MBUS_DATA_ACTIVE_HIGH)

/* Capture is not running, no interrupts, no locking needed */
static int sh_mobile_rcu_set_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = find_bus_subdev(pcdev, icd);
	struct sh_mobile_rcu_cam *cam = icd->host_priv;
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	unsigned long common_flags = RCU_BUS_FLAGS;
	u32 rcapcr = 0, rcamcr = 0, rcpicr = 0, rcdocr = 0;
	unsigned yuv_lineskip;
	int ret;

	/*
	 * If the client doesn't implement g_mbus_config, we just use our
	 * platform data
	 */
	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret) {
		common_flags = soc_mbus_config_compatible(&cfg,
							  common_flags);
		if (!common_flags)
			return -EINVAL;
	} else if (ret != -ENOIOCTLCMD) {
		return ret;
	}

	dev_geo(icd->parent, "%s():\n", __func__);

	/* Make choises, based on platform preferences */
	if ((common_flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH) &&
	    (common_flags & V4L2_MBUS_HSYNC_ACTIVE_LOW)) {
		if (pcdev->pdata->flags & SH_RCU_FLAG_HSYNC_LOW)
			common_flags &= ~V4L2_MBUS_HSYNC_ACTIVE_HIGH;
		else
			common_flags &= ~V4L2_MBUS_HSYNC_ACTIVE_LOW;
	}

	if ((common_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH) &&
	    (common_flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)) {
		if (pcdev->pdata->flags & SH_RCU_FLAG_VSYNC_LOW)
			common_flags &= ~V4L2_MBUS_VSYNC_ACTIVE_HIGH;
		else
			common_flags &= ~V4L2_MBUS_VSYNC_ACTIVE_LOW;
	}

/*	ret = icd->ops->set_bus_param(icd, common_flags);
	if (ret < 0)
		return ret;*/

	/* setting RCAMCR */
	rcamcr |= pcdev->image_mode << RCAMCR_CFMT;
	if (SH_RCU_OUTPUT_MEM_ISP == pcdev->output_mode)
		rcamcr |= SH_RCU_OUTPUT_ISP << RCAMCR_OCNT;
	else
		rcamcr |= pcdev->output_mode << RCAMCR_OCNT;

	if (SH_RCU_OUTPUT_ISP_FULL != pcdev->output_ispthinned)
		rcamcr |= pcdev->output_ispthinned << RCAMCR_HSS2;

	yuv_lineskip = 0;
	switch (icd->current_fmt->host_fmt->fourcc) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		rcamcr |= SH_RCU_RAWTYP_RAW8 << RCAMCR_RAWTYP;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		rcamcr |= SH_RCU_RAWTYP_RAW10 << RCAMCR_RAWTYP;
		rcamcr |= (pcdev->output_pack ?
				(SH_RCU_RPAC_PACKING << RCAMCR_RPAC) :
				(SH_RCU_RPAC_ENHANCE << RCAMCR_RPAC));
		switch (cam->code) {
		case V4L2_MBUS_FMT_SBGGR8_1X8:
		case V4L2_MBUS_FMT_SGBRG8_1X8:
		case V4L2_MBUS_FMT_SGRBG8_1X8:
		case V4L2_MBUS_FMT_SRGGB8_1X8:
			rcamcr |= 1 << RCAMCR_E810B;
			rcamcr |= 1 << RCAMCR_E810I;
			break;
		default:
			break;
		}
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		rcamcr |= SH_RCU_RAWTYP_RAW12 << RCAMCR_RAWTYP;
		rcamcr |= (pcdev->output_pack ?
				(SH_RCU_RPAC_PACKING << RCAMCR_RPAC) :
				(SH_RCU_RPAC_ENHANCE << RCAMCR_RPAC));
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		yuv_lineskip = 1; /* skip for NV12/21, no skip for NV16/61 */
		/* fall-through */
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		if (pcdev->output_ext)
			rcamcr |= 1 << RCAMCR_CFMT;
		switch (cam->code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
			/* Cb0, Y0, Cr0, Y1 */
			rcpicr |= SH_RCU_ORDER_UYVY << RCPICR_DTARY;
			break;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			/* Cr0, Y0, Cb0, Y1 */
			rcpicr |= SH_RCU_ORDER_VYUY << RCPICR_DTARY;
			break;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			/* Y0, Cb0, Y1, Cr0 */
			rcpicr |= SH_RCU_ORDER_YUYV << RCPICR_DTARY;
			break;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			/* Y0, Cr0, Y1, Cb0 */
			rcpicr |= SH_RCU_ORDER_YVYU << RCPICR_DTARY;
			break;
		default:
			BUG();
		}
		break;
	default:
		break;
	}

	/* setting RCPICR */
	rcpicr |=
		common_flags & V4L2_MBUS_VSYNC_ACTIVE_LOW ? \
		1 << RCPICR_VDPOL : 0;
	rcpicr |=
		common_flags & V4L2_MBUS_HSYNC_ACTIVE_LOW ? \
		1 << RCPICR_HDPOL : 0;
	if (!pcdev->pdata->csi2)
		rcpicr |= 1 << RCPICR_IFS;	/* Paralell I/F */

	/* setting RCAPCR */
	rcapcr |= SH_RCU_MTCM_SIZE_8 << RCAPCR_MTCM;

	/* setting RCDOCR */
	/*
	 * A few words about byte order (observed in Big Endian mode)
	 *
	 * In data fetch mode bytes are received in chunks of 8 bytes.
	 * D0, D1, D2, D3, D4, D5, D6, D7 (D0 received first)
	 *
	 * The data is however by default written to memory in reverse order:
	 * D7, D6, D5, D4, D3, D2, D1, D0 (D7 written to lowest byte)
	 *
	 * The lowest three bits of RCDOCR allows us to do swapping,
	 * using 7 we swap the data bytes to match the incoming order:
	 * D0, D1, D2, D3, D4, D5, D6, D7
	 */
	rcdocr |= 1 << RCDOCR_COLS;
	rcdocr |= 1 << RCDOCR_COWS;
	rcdocr |= 1 << RCDOCR_COBS;

	if (yuv_lineskip)
		rcdocr |= 1 << RCDOCR_T420;	/* output YUV420 */

		if (icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV21 ||
		icd->current_fmt->host_fmt->fourcc == V4L2_PIX_FMT_NV61) {
			/* swap U, V to change from NV1x->NVx1 */
			rcdocr |= 1 << RCDOCR_C1BS;
	}

	sh_mobile_rcu_set_rect(icd);
	mdelay(1);

	dev_geo(icd->parent,
		"RCAPCR 0x%08x, RCAMCR 0x%08x, RCPICR 0x%08x, RCDOCR 0x%08x\n",
		rcapcr, rcamcr, rcpicr, rcdocr);

	rcu_write(pcdev, RCAPCR,  rcapcr);
	rcu_write(pcdev, RCAMCR,  rcamcr);
	rcu_write(pcdev, RCPICR,  rcpicr);
	rcu_write(pcdev, RCDOCR,  rcdocr);
	rcu_write(pcdev, RCPIHCR, 0);
	rcu_write(pcdev, RCRCNTR, 0);
	rcu_write(pcdev, RCRCMPR, 0);
	rcu_write(pcdev, RCBDSR,  0);

/*	dev_dbg(icd->parent, "S_FMT successful for %c%c%c%c %ux%u\n",
		PRINT_FOURCC(pixfmt), icd->user_width, icd->user_height);*/

	dev_dbg(icd->parent, "> RCAPSR  : 0x%08X\n",
		rcu_read(pcdev, RCAPSR));
	dev_dbg(icd->parent, "> RCAPCR  : 0x%08X\n",
		rcu_read(pcdev, RCAPCR));
	dev_dbg(icd->parent, "> RCAMCR  : 0x%08X\n",
		rcu_read(pcdev, RCAMCR));
	dev_dbg(icd->parent, "> RCPICR  : 0x%08X\n",
		rcu_read(pcdev, RCPICR));
	dev_dbg(icd->parent, "> RCAMOR  : 0x%08X\n",
		rcu_read(pcdev, RCAMOR));
	dev_dbg(icd->parent, "> RCPIHCR : 0x%08X\n",
		rcu_read(pcdev, RCPIHCR));
	dev_dbg(icd->parent, "> RCRCNTR : 0x%08X\n",
		rcu_read(pcdev, RCRCNTR));
	dev_dbg(icd->parent, "> RCRCMPR : 0x%08X\n",
		rcu_read(pcdev, RCRCMPR));
	dev_dbg(icd->parent, "> RCSZR   : 0x%08X\n",
		rcu_read(pcdev, RCSZR));
	dev_dbg(icd->parent, "> RCDWDR  : 0x%08X\n",
		rcu_read(pcdev, RCDWDR));
	dev_dbg(icd->parent, "> RCDAYR  : 0x%08X\n",
		rcu_read(pcdev, RCDAYR));
	dev_dbg(icd->parent, "> RCDACR  : 0x%08X\n",
		rcu_read(pcdev, RCDACR));
	dev_dbg(icd->parent, "> RCBDSR  : 0x%08X\n",
		rcu_read(pcdev, RCBDSR));
	dev_dbg(icd->parent, "> RCFWCR  : 0x%08X\n",
		rcu_read(pcdev, RCFWCR));
	dev_dbg(icd->parent, "> RCDOCR  : 0x%08X\n",
		rcu_read(pcdev, RCDOCR));
	dev_dbg(icd->parent, "> RCEIER  : 0x%08X\n",
		rcu_read(pcdev, RCEIER));
	dev_dbg(icd->parent, "> RCETCR  : 0x%08X\n",
		rcu_read(pcdev, RCETCR));
	dev_dbg(icd->parent, "> RCSTSR  : 0x%08X\n",
		rcu_read(pcdev, RCSTSR));
	dev_dbg(icd->parent, "> RCSRTR  : 0x%08X\n",
		rcu_read(pcdev, RCSRTR));
	dev_dbg(icd->parent, "> RCDSSR  : 0x%08X\n",
		rcu_read(pcdev, RCDSSR));
	dev_dbg(icd->parent, "> RCDAYR2 : 0x%08X\n",
		rcu_read(pcdev, RCDAYR2));
	dev_dbg(icd->parent, "> RCDACR2 : 0x%08X\n",
		rcu_read(pcdev, RCDACR2));
	dev_dbg(icd->parent, "> RCECNTR : 0x%08X\n",
		rcu_read(pcdev, RCECNTR));
	dev_dbg(icd->parent, "> RCEDAYR : 0x%08X\n",
		rcu_read(pcdev, RCEDAYR));
	dev_dbg(icd->parent, "> RCEDSSR : 0x%08X\n",
		rcu_read(pcdev, RCEDSSR));
	dev_dbg(icd->parent, "> RCEFWCR : 0x%08X\n",
		rcu_read(pcdev, RCEFWCR));

	/* not in bundle mode: skip RCBDSR, RCDAYR2, RCDACR2 */
	return 0;
}

static int sh_mobile_rcu_try_bus_param(struct soc_camera_device *icd,
				       unsigned char buswidth)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = find_bus_subdev(pcdev, icd);
	unsigned long common_flags = RCU_BUS_FLAGS;
	struct v4l2_mbus_config cfg = {.type = V4L2_MBUS_PARALLEL,};
	int ret;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if (!ret)
		common_flags = soc_mbus_config_compatible(&cfg,
							  common_flags);
	else if (ret != -ENOIOCTLCMD)
		return ret;

	if (!common_flags || buswidth > 8 ||
		!(common_flags & SOCAM_DATAWIDTH_8)) {
		return -EINVAL;
	}

	return 0;
}

static const struct soc_mbus_pixelfmt sh_mobile_rcu_formats[] = {
	{
		.fourcc				= V4L2_PIX_FMT_SBGGR8,
		.name				= "Bayer 8 BGGR",
		.bits_per_sample	= 8,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SGBRG8,
		.name				= "Bayer 8 GBRG",
		.bits_per_sample	= 8,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SGRBG8,
		.name				= "Bayer 8 GRBG",
		.bits_per_sample	= 8,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SRGGB8,
		.name				= "Bayer 8 RGGB",
		.bits_per_sample	= 8,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SBGGR10,
		.name				= "Bayer 10 BGGR",
		.bits_per_sample	= 10,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SGBRG10,
		.name				= "Bayer 10 GBRG",
		.bits_per_sample	= 10,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SGRBG10,
		.name				= "Bayer 10 GRBG",
		.bits_per_sample	= 10,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SRGGB10,
		.name				= "Bayer 10 RGGB",
		.bits_per_sample	= 10,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SBGGR12,
		.name				= "Bayer 12 BGGR",
		.bits_per_sample	= 12,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SGBRG12,
		.name				= "Bayer 12 GBRG",
		.bits_per_sample	= 12,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SGRBG12,
		.name				= "Bayer 12 GRBG",
		.bits_per_sample	= 12,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_SRGGB12,
		.name				= "Bayer 12 RGGB",
		.bits_per_sample	= 12,
		.packing			= SOC_MBUS_PACKING_EXTEND16,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_NV12,
		.name				= "NV12",
		.bits_per_sample	= 12,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_NV21,
		.name				= "NV21",
		.bits_per_sample	= 12,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_NV16,
		.name				= "NV16",
		.bits_per_sample	= 16,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	}, {
		.fourcc				= V4L2_PIX_FMT_NV61,
		.name				= "NV61",
		.bits_per_sample	= 16,
		.packing			= SOC_MBUS_PACKING_NONE,
		.order				= SOC_MBUS_ORDER_LE,
	},
};

static int client_g_rect(struct v4l2_subdev *sd, struct v4l2_rect *rect);

#if 0
static struct soc_camera_device *ctrl_to_icd(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct soc_camera_device,
							ctrl_handler);
}
#endif

static int sh_mobile_rcu_s_ctrl(struct v4l2_ctrl *ctrl)
{
#if 0
	struct soc_camera_device *icd = ctrl_to_icd(ctrl);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;

	switch (ctrl->id) {
	case V4L2_CID_SHARPNESS:
		switch (icd->current_fmt->host_fmt->fourcc) {
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
			rcu_write(pcdev, CLFCR, !ctrl->val);
			return 0;
		}
		break;
	}

	return -EINVAL;
#else
	return 0;
#endif
}

static const struct v4l2_ctrl_ops sh_mobile_rcu_ctrl_ops = {
	.s_ctrl = sh_mobile_rcu_s_ctrl,
};

static int sh_mobile_rcu_get_formats(struct soc_camera_device *icd,
		unsigned int idx, struct soc_camera_format_xlate *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	int ret, n, j, k;
	int formats = 0;
	struct sh_mobile_rcu_cam *cam;
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt *fmt;

	dev_geo(dev, "%s(): idx=%d xlate=%p\n", __func__, idx, xlate);

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret < 0)
		/* No more formats */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code);
	if (!fmt) {
		dev_warn(dev, "unsupported format code #%u: 0x%x\n", idx, code);
		return 0;
	}

	if (!pcdev->pdata->csi2) {
		/* Are there any restrictions in the CSI-2 case? */
		ret = sh_mobile_rcu_try_bus_param(icd, fmt->bits_per_sample);
		if (ret < 0)
			return 0;
	}

	if (!icd->host_priv) {
		struct v4l2_mbus_framefmt mf;
		struct v4l2_rect rect;
		int shift = 0;

		/* Add our control */
/*
		v4l2_ctrl_new_std(&icd->ctrl_handler, &sh_mobile_rcu_ctrl_ops,
				  V4L2_CID_SHARPNESS, 0, 1, 1, 0);
		if (icd->ctrl_handler.error)
			return icd->ctrl_handler.error;
*/

		/* FIXME: subwindow is lost between close / open */

		/* First time */
		ret = v4l2_subdev_call(sd, video, g_mbus_fmt, &mf);
		if (ret < 0)
			return ret;

		/*
		 * All currently existing RCU implementations support 2560x1920
		 * or larger frames. If the sensor is proposing too big a frame,
		 * don't bother with possibly supportred by the RCU larger
		 * sizes, just try VGA multiples. If needed, this can be
		 * adjusted in the future.
		 */
/*		while ((mf.width > pcdev->max_width ||
			mf.height > pcdev->max_height) && shift < 4) {*/
		while ((mf.width > 3264 || mf.height > 2448) && shift < 4) {
			/* Try 2560x1920, 1280x960, 640x480, 320x240 */
			mf.width	= 2560 >> shift;
			mf.height	= 1920 >> shift;
			ret = v4l2_device_call_until_err(sd->v4l2_dev,
					soc_camera_grp_id(icd), video,
					s_mbus_fmt, &mf);
			if (ret < 0)
				return ret;
			shift++;
		}

		if (shift == 4) {
			dev_err(dev, "Failed to configure the client below %ux%x\n",
				mf.width, mf.height);
			return -EIO;
		}

		dev_geo(dev, "camera fmt %ux%u\n", mf.width, mf.height);

		/* Cache current client geometry */
		ret = client_g_rect(sd, &rect);
		if (ret < 0)
			return ret;

		cam = kzalloc(sizeof(*cam), GFP_KERNEL);
		if (!cam)
			return -ENOMEM;

		/* We are called with current camera crop,
		   initialise subrect with it */
		/* initialise rect with it */
		cam->width		= mf.width;
		cam->height		= mf.height;
		cam->rcu_left	= rect.left;
		cam->rcu_top	= rect.top;

		icd->host_priv = cam;
	} else {
		cam = icd->host_priv;
	}

	/* Beginning of a pass */
	if (!idx)
		cam->extra_fmt = NULL;

	switch (code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[4];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SGBRG8_1X8:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[5];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SGRBG8_1X8:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[6];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SRGGB8_1X8:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[7];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[4];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SGBRG10_1X10:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[5];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SGRBG10_1X10:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[6];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SRGGB10_1X10:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[7];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SBGGR12_1X12:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[8];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SGBRG12_1X12:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[9];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SGRBG12_1X12:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[10];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_SRGGB12_1X12:
		formats++;
		if (xlate) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[11];
			xlate->code		= code;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				xlate->host_fmt->name, code);
			xlate++;
		}
		break;
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
	case V4L2_MBUS_FMT_YVYU8_2X8:
		if (cam->extra_fmt)
			break;

		/*
		 * Our case is simple so far: for any of the above four camera
		 * formats we add all our four synthesized NV* formats, so,
		 * just marking the device with a single flag suffices. If
		 * the format generation rules are more complex, you would have
		 * to actually hang your already added / counted formats onto
		 * the host_priv pointer and check whether the format you're
		 * going to add now is already there.
		 */
		cam->extra_fmt = sh_mobile_rcu_formats;

		n = 4;	/* support formats */
		j = 12;	/* offset */
		formats += n;
		for (k = j; xlate && k < n + j; k++) {
			xlate->host_fmt	= &sh_mobile_rcu_formats[k];
			xlate->code	= code;
			xlate++;
			dev_dbg(dev, "Providing format %s using code 0x%x\n",
				sh_mobile_rcu_formats[k].name, code);
		}
		break;
	default:
		dev_dbg(dev, "unsupported code 0x%x\n", code);
		return 0;
	}

	/* Generic pass-through */
	formats++;
	if (xlate) {
		xlate->host_fmt	= fmt;
		xlate->code	= code;
		xlate++;
		dev_dbg(dev, "Providing format %s in pass-through mode\n",
			fmt->name);
	}

	return formats;
}

static void sh_mobile_rcu_put_formats(struct soc_camera_device *icd)
{
	dev_geo(icd->parent, "%s():\n", __func__);

	kfree(icd->host_priv);
	icd->host_priv = NULL;
}

/* Get and store current client crop */
static int client_g_rect(struct v4l2_subdev *sd, struct v4l2_rect *rect)
{
	struct v4l2_crop crop;
	struct v4l2_cropcap cap;
	int ret;

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_subdev_call(sd, video, g_crop, &crop);
	if (!ret) {
		*rect = crop.c;
		return ret;
	}

	/* Camera driver doesn't support .g_crop(), assume default rectangle */
	cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = v4l2_subdev_call(sd, video, cropcap, &cap);
	if (!ret)
		*rect = cap.defrect;

	return ret;
}

/* Alignment is multiple of 2^(walign or haling). */
static void get_alignment(__u32 pixfmt, unsigned int output_mode,
				unsigned int *walign, unsigned int *haling)
{
	*haling = 0;
	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		*walign = 1;
		break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		*walign = (SH_RCU_OUTPUT_ISP == output_mode) ? 0 : 2;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		*walign = (SH_RCU_OUTPUT_ISP == output_mode) ? 0 : 1;
		break;
	default:
		*walign = 0;
		break;
	}
}

/* Similar to set_crop multistage iterative algorithm */
static int sh_mobile_rcu_set_fmt(struct soc_camera_device *icd,
				 struct v4l2_format *f)
{
	struct device *dev = icd->parent;
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct sh_mobile_rcu_cam *cam = icd->host_priv;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	const struct soc_camera_format_xlate *xlate;
	int ret;
	unsigned int walign, halign;
	struct v4l2_rect rect;

	dev_geo(dev,
		"%s(): S_FMT(%c%c%c%c, %ux%u)\n",
		__func__, PRINT_FOURCC(pixfmt), pix->width, pix->height);

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(dev,
		"Format %c%c%c%c not found\n", PRINT_FOURCC(pixfmt));
		return -EINVAL;
	}

	/* set format */
	mf.width		= pix->width;
	mf.height		= pix->height;
	mf.field		= pix->field;
	mf.colorspace	= pix->colorspace;
	mf.code			= xlate->code;

	ret = v4l2_device_call_until_err(sd->v4l2_dev,
			0, video, s_mbus_fmt, &mf);
	dev_geo(dev, "fmt %ux%u, requested %ux%u\n",
		mf.width, mf.height, pix->width, pix->height);
	if (ret < 0)
		return ret;

	if (mf.code != xlate->code)
		return -EINVAL;

	/* Cache current client geometry */
	ret = client_g_rect(sd, &rect);
	if (ret < 0)
		return ret;

	get_alignment(pixfmt, pcdev->output_mode, &walign, &halign);
	mf.width  = ROUNDDOWN(mf.width,  1<<walign);
	mf.height = ROUNDDOWN(mf.height, 1<<halign);
	if (mf.width  < pix->width)
		pix->width  = mf.width;
	if (mf.height < pix->height)
		pix->height = mf.height;

	/* Prepare RCU crop */
	cam->width			= pix->width;
	cam->height			= pix->height;
	cam->rcu_left		= rect.left;
	cam->rcu_top		= rect.top;
	cam->code			= xlate->code;
	pix->field			= V4L2_FIELD_NONE;
	pix->colorspace		= mf.colorspace;
	icd->current_fmt	= xlate;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		pcdev->image_mode = SH_RCU_MODE_IMAGE;
		break;
	default:
		pcdev->image_mode = SH_RCU_MODE_DATA;
	}

	dev_geo(dev, "rcu crop: width=%d height=%d left=%d top=%d\n",
		cam->width, cam->height, cam->rcu_left, cam->rcu_top);

	return 0;
}

static int sh_mobile_rcu_try_fmt(struct soc_camera_device *icd,
				 struct v4l2_format *f)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int ret;
	unsigned int walign, halign;

	dev_geo(icd->parent, "%s(): TRY_FMT(%c%c%c%c, %ux%u)\n",
		__func__, PRINT_FOURCC(pixfmt), pix->width, pix->height);

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(icd->parent, "Format %c%c%c%c not found\n",
					PRINT_FOURCC(pixfmt));
		return -EINVAL;
	}

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		/* ISP module does not support YUV format. */
		if (pcdev->output_mode != SH_RCU_OUTPUT_MEM)
			return -EINVAL;
		break;
	default:
		break;
	}

	/* FIXME: calculate using depth and bus width */

	get_alignment(pixfmt, pcdev->output_mode, &walign, &halign);
	v4l_bound_align_image(&pix->width,  SH_RCU_MIN_WIDTH,
				SH_RCU_MAX_WIDTH,  walign,
				&pix->height, SH_RCU_MIN_HEIGHT,
				SH_RCU_MAX_HEIGHT, halign, 0);

	/* limit to sensor capabilities */
	mf.width	= pix->width;
	mf.height	= pix->height;
	mf.field	= pix->field;
	mf.code		= xlate->code;
	mf.colorspace	= pix->colorspace;

	ret = v4l2_device_call_until_err(sd->v4l2_dev, 0, video,
			try_mbus_fmt, &mf);
	dev_geo(icd->parent, "try_fmt %ux%u, requested %ux%u\n",
		mf.width, mf.height, pix->width, pix->height);
	if (ret < 0)
		return ret;

	mf.width  = ROUNDDOWN(mf.width,  1<<walign);
	mf.height = ROUNDDOWN(mf.height, 1<<halign);
	if (mf.width  < pix->width)
		pix->width  = mf.width;
	if (mf.height < pix->height)
		pix->height = mf.height;

	pix->bytesperline =
		sh_mobile_rcu_bytes_per_line(pix->width,
				xlate->host_fmt, pcdev);
	if ((int)pix->bytesperline < 0)
		return pix->bytesperline;
	pix->sizeimage = pix->height * pix->bytesperline;

	pix->field		= mf.field;
	pix->colorspace	= mf.colorspace;

	dev_geo(icd->parent,
		"pix format: width=%d height=%d stride=%d sizeimage=%d\n",
		pix->width, pix->height, pix->bytesperline, pix->sizeimage);

	return ret;
}

static unsigned int sh_mobile_rcu_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	dev_geo(icd->parent, "%s():\n", __func__);

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static int sh_mobile_rcu_querycap(struct soc_camera_host *ici,
				  struct v4l2_capability *cap)
{
	dev_geo(ici->v4l2_dev.dev, "%s():\n", __func__);

	strlcpy(cap->card, "SuperH_Mobile_RCU", sizeof(cap->card));
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static void *sh_mobile_rcu_contig_get_userptr(
	void *alloc_ctx, unsigned long vaddr,
	unsigned long size, int write)
{
/*	return NULL;*/
	return (void *)1;
}

static void sh_mobile_rcu_contig_put_userptr(void *mem_priv)
{
	return;
}

static int sh_mobile_rcu_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	unsigned long vm_addr = vma->vm_start;
	struct soc_camera_device *icd = buf_priv;
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	int ret = 0;
	int i = 0;

	unsigned int page_num;
	page_num = ((vma->vm_end - vma->vm_start) + PAGE_SIZE - 1)
		/ PAGE_SIZE;

#ifdef SH_RCU_DUMP_LOG_ENABLE
	if (dump_addr_flg) {
		dump_addr_flg = false;
		ret = remap_pfn_range(vma, vm_addr, page_to_pfn(dumplog_page),
			(vma->vm_end - vma->vm_start), vma->vm_page_prot);
		if (ret) {
			dev_err(icd->parent,
			"%s:remap_pfn_range err(%d)\n", __func__, ret);
			return -1;
		}
		return 0;
	}
#endif /* SH_RCU_DUMP_LOG_ENABLE */

	if (pcdev->mmap_size <= page_num) {
		dev_err(icd->parent,
			"%s:size error(%d <= %d)\n", __func__,
			pcdev->mmap_size, page_num);
		return -1;
	}
	if (!pcdev->mmap_pages) {
		dev_err(icd->parent,
			"%s:mmap_pages NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < page_num; i++) {
		ret = remap_pfn_range(vma, vm_addr,
			page_to_pfn(pcdev->mmap_pages[i+1]), PAGE_SIZE,
			vma->vm_page_prot);
		if (ret) {
			dev_err(icd->parent,
			"%s:remap_pfn_range error(%d)\n", __func__,
			ret);
			return -1;
		}
		vm_addr += PAGE_SIZE;
	}

	kfree(pcdev->mmap_pages);
	pcdev->mmap_pages = 0;
	pcdev->mmap_size = 0;
	return 0;
}

const struct vb2_mem_ops sh_mobile_rcu_memops = {
	.get_userptr	= sh_mobile_rcu_contig_get_userptr,
	.put_userptr	= sh_mobile_rcu_contig_put_userptr,
	.mmap		= sh_mobile_rcu_mmap,
};

static int sh_mobile_rcu_init_videobuf(struct vb2_queue *q,
				       struct soc_camera_device *icd)
{
	dev_geo(icd->parent, "%s():\n", __func__);

	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = icd;
	q->ops = &sh_mobile_rcu_videobuf_ops;
	q->mem_ops = &sh_mobile_rcu_memops;
	q->buf_struct_size = sizeof(struct sh_mobile_rcu_buffer);

	return vb2_queue_init(q);
}

static int sh_mobile_rcu_get_ctrl(struct soc_camera_device *icd,
				  struct v4l2_control *ctrl)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	dev_geo(icd->parent, "%s():\n", __func__);

	switch (ctrl->id) {
	case V4L2_CID_GET_TUNING:
		return v4l2_subdev_call(sd, core, g_ctrl, ctrl);
	case V4L2_CID_GET_DUMP_SIZE_ALL:
		dump_addr_flg = true;
		ctrl->value = SH_RCU_DUMP_LOG_SIZE_ALL;
		return 0;
	case V4L2_CID_GET_DUMP_SIZE_USER:
		ctrl->value = SH_RCU_DUMP_LOG_SIZE_USER;
		return 0;
	case V4L2_CID_GET_CAMSTS0:
		ctrl->value = cam_status0;
		return 0;
	case V4L2_CID_GET_CAMSTS1:
		ctrl->value = cam_status1;
		return 0;
	}

	return -ENOIOCTLCMD;
}

static bool is_state_rear_flash(void)
{
	return rear_flash_state;
}

void sh_mobile_rcu_flash(int led)
{
	int set_state = SH_RCU_LED_OFF;
	if (led) {
		set_state = SH_RCU_LED_ON;
		rear_flash_state = false;
	} else {
		set_state = SH_RCU_LED_OFF;
		rear_flash_state = true;
	}
	if (rear_flash_set)
		rear_flash_set(set_state, SH_RCU_LED_MODE_PRE);
	return;
}

void sh_mobile_rcu_init_dumplog(void)
{
#ifdef SH_RCU_DUMP_LOG_ENABLE
	dumplog_init_cnt++;
	if (1 < dumplog_init_cnt) {
		/* initialized */
		return;
	}

	dumplog_order = get_order(SH_RCU_DUMP_LOG_SIZE_ALL);
	if ((PAGE_SIZE << dumplog_order) > SH_RCU_DUMP_LOG_SIZE_ALL) {
		/* size is pressed down */
		dumplog_order--;
	}
	dumplog_page = alloc_pages(GFP_USER, dumplog_order);
	memset(page_address(dumplog_page), 0, SH_RCU_DUMP_LOG_SIZE_ALL);

	spin_lock_init(&lock_log);

	dumplog_addr = page_address(dumplog_page);
	dumplog_ktbl = dumplog_addr +
		SH_RCU_DUMP_LOG_SIZE_USER / sizeof(unsigned int);
	dumplog_max_idx = dumplog_addr +
		(SH_RCU_DUMP_LOG_SIZE_ALL / sizeof(unsigned int) - 1);
	dumplog_cnt_idx = dumplog_max_idx - 1;
	*dumplog_addr = (unsigned int)virt_to_phys((void *)dumplog_addr);
	*dumplog_max_idx =
		(SH_RCU_DUMP_LOG_SIZE_ALL - SH_RCU_DUMP_LOG_SIZE_USER) /
		sizeof(unsigned int) - SH_RCU_DUMP_LOG_OFFSET - 1;
	*dumplog_cnt_idx = 0;
#endif /* SH_RCU_DUMP_LOG_ENABLE */
	return;
}

void sh_mobile_rcu_deinit_dumplog(void)
{
#ifdef SH_RCU_DUMP_LOG_ENABLE
	dumplog_init_cnt--;
	if (0 < dumplog_init_cnt) {
		/* Don't free */
		return;
	}

	free_pages((unsigned long)dumplog_addr, dumplog_order);

	dumplog_addr = NULL;
	dumplog_order = 0;
#endif /* SH_RCU_DUMP_LOG_ENABLE */
	return;
}

#ifdef SH_RCU_DUMP_LOG_ENABLE
static unsigned long sh_mobile_rcu_get_timeval(void)
{
	struct timeval tv;
	unsigned long usec;

	do_gettimeofday(&tv);
	usec = SH_RCU_TIMEVAL2USEC(tv);
	/* clock is 1MHz */
	return 0xFFFFFFFF - usec;
}
static void sh_mobile_rcu_set_dumplog(unsigned int id, unsigned int time)
{
	unsigned int *data = &dumplog_ktbl[*dumplog_cnt_idx * 2];

	*dumplog_cnt_idx = (*dumplog_cnt_idx + 1) % *dumplog_max_idx;

	data[0]	= id;
	data[1]	= time;
	return;
}
#endif /* SH_RCU_DUMP_LOG_ENABLE */

void sh_mobile_rcu_event_time_func(unsigned short id)
{
#ifdef SH_RCU_DUMP_LOG_ENABLE
	spin_lock(&lock_log);

	sh_mobile_rcu_set_dumplog((0xE1000000 | id), SH_RCU_GET_TIME());

	spin_unlock(&lock_log);
#endif /* SH_RCU_DUMP_LOG_ENABLE */
	return;
}

void sh_mobile_rcu_event_time_data(unsigned short id, unsigned int data)
{
#ifdef SH_RCU_DUMP_LOG_ENABLE
	unsigned int	tmu_cnt;

	spin_lock(&lock_log);

	tmu_cnt = SH_RCU_GET_TIME();

	sh_mobile_rcu_set_dumplog((0xD1000000 | id), tmu_cnt);
	sh_mobile_rcu_set_dumplog(data, tmu_cnt);

	spin_unlock(&lock_log);
#endif /* SH_RCU_DUMP_LOG_ENABLE */
	return;
}

static int sh_mobile_rcu_send_command(struct i2c_client *client,
	struct sh_mobile_rcu_snd_cmd *snd_cmd)
{
	char *snd_buf;
	char *rcv_buf;
	struct i2c_msg msg[2];
	int ret = 0;

	memset(msg, 0, sizeof(msg));

	switch (snd_cmd->func) {
	case SH_RCU_SNDCMD_SND:
		/* parameter check */
		if (0 == snd_cmd->snd_size) {
			dev_err(&client->dev, "%s[%d]:snd_size is zero\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		if (!snd_cmd->snd_buf) {
			dev_err(&client->dev, "%s[%d]:snd_buf is NULL\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		snd_buf = kmalloc(snd_cmd->snd_size, GFP_KERNEL);
		if (!snd_buf) {
			dev_err(&client->dev, "%s[%d]:kmalloc error\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		if (copy_from_user(snd_buf, (int __user *) snd_cmd->snd_buf,
			snd_cmd->snd_size)) {
			dev_err(&client->dev, "%s[%d]:copy_from_user error\n",
				__func__, __LINE__);
			kfree(snd_buf);
			return -EIO;
		}
		msg[0].addr = client->addr;
		msg[0].flags = client->flags & I2C_M_TEN;
		msg[0].len = snd_cmd->snd_size;
		msg[0].buf = (char *) snd_buf;
		ret = i2c_transfer(client->adapter, msg, 1);
		if (0 > ret) {
			dev_err(&client->dev, "%s[%d]:i2c_transfer error %d\n",
				__func__, __LINE__, ret);
			kfree(snd_buf);
			return ret;
		}
		kfree(snd_buf);
		break;
	case SH_RCU_SNDCMD_RCV:
		/* parameter check */
		if (0 == snd_cmd->rcv_size) {
			dev_err(&client->dev, "%s[%d]:rcv_size is zero\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		if (!snd_cmd->rcv_buf) {
			dev_err(&client->dev, "%s[%d]:rcv_buf is NULL\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		rcv_buf = kmalloc(snd_cmd->rcv_size, GFP_KERNEL);
		if (!rcv_buf) {
			dev_err(&client->dev, "%s[%d]:kmalloc error\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		msg[0].addr = client->addr;
		msg[0].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
		msg[0].len = snd_cmd->rcv_size;
		msg[0].buf = (char *) rcv_buf;
		ret = i2c_transfer(client->adapter, msg, 1);
		if (0 > ret) {
			dev_err(&client->dev, "%s[%d]:i2c_transfer error %d\n",
				__func__, __LINE__, ret);
			kfree(rcv_buf);
			return ret;
		}
		if (copy_to_user((int __user *) snd_cmd->rcv_buf, rcv_buf,
			snd_cmd->rcv_size)) {
			dev_err(&client->dev, "%s[%d]:copy_to_user error\n",
				__func__, __LINE__);
			kfree(rcv_buf);
			return -EIO;
		}

		kfree(rcv_buf);
		break;
	case SH_RCU_SNDCMD_SNDRCV:
		/* parameter check */
		if (0 == snd_cmd->snd_size) {
			dev_err(&client->dev, "%s[%d]:snd_size is zero\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		if (!snd_cmd->snd_buf) {
			dev_err(&client->dev, "%s[%d]:snd_buf is NULL\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		if (0 == snd_cmd->rcv_size) {
			dev_err(&client->dev, "%s[%d]:rcv_size is zero\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		if (!snd_cmd->rcv_buf) {
			dev_err(&client->dev, "%s[%d]:rcv_buf is NULL\n",
				__func__, __LINE__);
			return -EINVAL;
		}

		snd_buf = kmalloc(snd_cmd->snd_size, GFP_KERNEL);
		if (!snd_buf) {
			dev_err(&client->dev, "%s[%d]:kmalloc error\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		rcv_buf = kmalloc(snd_cmd->rcv_size, GFP_KERNEL);
		if (!rcv_buf) {
			dev_err(&client->dev, "%s[%d]:kmalloc error\n",
				__func__, __LINE__);
			kfree(snd_buf);
			return -ENOMEM;
		}
		if (copy_from_user(snd_buf, (int __user *) snd_cmd->snd_buf,
			snd_cmd->snd_size)) {
			dev_err(&client->dev, "%s[%d]:copy_from_user error\n",
				__func__, __LINE__);
			kfree(snd_buf);
			kfree(rcv_buf);
			return -EIO;
		}
		msg[0].addr = client->addr;
		msg[0].flags = client->flags & I2C_M_TEN;
		msg[0].len = snd_cmd->snd_size;
		msg[0].buf = (char *) snd_buf;
		msg[1].addr = client->addr;
		msg[1].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
		msg[1].len = snd_cmd->rcv_size;
		msg[1].buf = (char *) rcv_buf;
		ret = i2c_transfer(client->adapter, msg, 2);
		if (0 > ret) {
			dev_err(&client->dev, "%s[%d]:i2c_transfer error %d\n",
				__func__, __LINE__, ret);
			kfree(snd_buf);
			kfree(rcv_buf);
			return ret;
		}
		if (copy_to_user((int __user *) snd_cmd->rcv_buf, rcv_buf,
			snd_cmd->rcv_size)) {
			dev_err(&client->dev, "%s[%d]:copy_to_user error\n",
				__func__, __LINE__);
			kfree(snd_buf);
			kfree(rcv_buf);
			return -EIO;
		}
		kfree(snd_buf);
		kfree(rcv_buf);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return 0;
}

static int sh_mobile_rcu_set_ctrl(struct soc_camera_device *icd,
				  struct v4l2_control *ctrl)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct sh_mobile_rcu_dev *pcdev = ici->priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sh_mobile_rcu_snd_cmd snd_cmd;
	unsigned int mmap_page_info[2];
	unsigned int page_num = 0;
	int ret = 0;
	unsigned long flags;

	dev_geo(icd->parent, "%s(): id=0x%x value=0x%x\n",
			__func__, ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_SET_SNDCMD:
		if (copy_from_user(
			(void *)&snd_cmd, (int __user *) ctrl->value,
			sizeof(snd_cmd))) {
			dev_err(&client->dev, "%s[%d]:copy_from_user error\n",
				__func__, __LINE__);
			return -EIO;
		}
		return sh_mobile_rcu_send_command(client, &snd_cmd);
	case V4L2_CID_SET_OUTPUT_MODE:
		if (SH_RCU_STREAMING_ON == pcdev->streaming)
			return -EBUSY;
		if ((SH_RCU_OUTPUT_ISP     != ctrl->value) &&
		    (SH_RCU_OUTPUT_MEM_ISP != ctrl->value) &&
		    (SH_RCU_OUTPUT_MEM     != ctrl->value) &&
		    (SH_RCU_OUTPUT_MEM_EXT != ctrl->value)) {
			return -EINVAL;
		}
		if (SH_RCU_OUTPUT_MEM_EXT == ctrl->value) {
			pcdev->output_ext = 1;
			pcdev->output_mode = SH_RCU_OUTPUT_MEM;
		} else {
			pcdev->output_ext = 0;
			pcdev->output_mode = ctrl->value;
		}
		return 0;
	case V4L2_CID_SET_OUTPUT_OFFSET:
		pcdev->output_offset = ctrl->value;
		return 0;
	case V4L2_CID_SET_OUTPUT_PACK:
		pcdev->output_pack = ctrl->value;
		return 0;
	case V4L2_CID_SET_OUTPUT_MERAM:
		pcdev->output_meram = ctrl->value;
		return 0;
	case V4L2_CID_SET_OUTPUT_ISPTHINNED:
		pcdev->output_ispthinned = ctrl->value;
		return 0;
	case V4L2_CID_SET_LED:
		if ((pcdev->pdata->led) && is_state_rear_flash())
			return pcdev->pdata->led((ctrl->value & SH_RCU_LED_ON),
			(ctrl->value & SH_RCU_LED_MODE_PRE));
		return 0;
	case V4L2_CID_SET_ZSL:
		pcdev->zsl = ctrl->value;
		if (!pcdev->zsl) {
			spin_lock_irqsave(&pcdev->lock, flags);
			rcu_write(pcdev, RCAPSR, 1 << RCAPSR_CE);
			pcdev->kick++;
			spin_unlock_irqrestore(&pcdev->lock, flags);
		}
		return 0;
	case V4L2_CID_SET_MMAP_PAGE:
		kfree(pcdev->mmap_pages);
		pcdev->mmap_pages = NULL;

		if (copy_from_user(mmap_page_info,
			(int __user *)ctrl->value,
				sizeof(mmap_page_info))) {
			dev_err(icd->parent,
				"%s:copy_from_user error(%d)\n",
				__func__, ret);
			return -EIO;
		}

		page_num = (mmap_page_info[0] + PAGE_SIZE - 1)
				/ PAGE_SIZE;
		pcdev->mmap_pages = kmalloc(page_num *
			sizeof(struct page *), GFP_KERNEL);
		if (NULL == pcdev->mmap_pages) {
			dev_err(icd->parent,
				"%s:kmalloc error\n",
				__func__);
			return -1;
		}

		if (copy_from_user(pcdev->mmap_pages,
			(int __user *)mmap_page_info[1],
				page_num * sizeof(struct page *))) {
			dev_err(icd->parent,
				"%s:copy_from_user error(%d)\n",
				__func__, ret);
			kfree(pcdev->mmap_pages);
			pcdev->mmap_pages = NULL;
			return -EIO;
		}
		pcdev->mmap_size = page_num;
		return 0;
	case V4L2_CID_SET_CAMSTS0:
		cam_status0 = ctrl->value;
		return 0;
	case V4L2_CID_SET_CAMSTS1:
		cam_status1 = ctrl->value;
		return 0;
	}
	return -ENOIOCTLCMD;
}

static struct soc_camera_host_ops sh_mobile_rcu_host_ops = {
	.owner		= THIS_MODULE,
	.add		= sh_mobile_rcu_add_device,
	.remove		= sh_mobile_rcu_remove_device,
	.get_formats	= sh_mobile_rcu_get_formats,
	.put_formats	= sh_mobile_rcu_put_formats,
	.set_fmt	= sh_mobile_rcu_set_fmt,
	.try_fmt	= sh_mobile_rcu_try_fmt,
	.set_ctrl	= sh_mobile_rcu_set_ctrl,
	.get_ctrl	= sh_mobile_rcu_get_ctrl,
	.poll		= sh_mobile_rcu_poll,
	.querycap	= sh_mobile_rcu_querycap,
	.set_bus_param	= sh_mobile_rcu_set_bus_param,
	.init_videobuf2	= sh_mobile_rcu_init_videobuf,
};

struct bus_wait {
	struct notifier_block	notifier;
	struct completion	completion;
	struct device		*dev;
};

static int bus_notify(struct notifier_block *nb,
		      unsigned long action, void *data)
{
	struct device *dev = data;
	struct bus_wait *wait = container_of(nb, struct bus_wait, notifier);

	dev_geo(dev, "%s():\n", __func__);

	if (wait->dev != dev)
		return NOTIFY_DONE;

	switch (action) {
	case BUS_NOTIFY_UNBOUND_DRIVER:
		/* Protect from module unloading */
		wait_for_completion(&wait->completion);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sh_mobile_rcu_early_suspend(struct early_suspend *h)
{
	return;
}

static void sh_mobile_rcu_late_resume(struct early_suspend *h)
{
	return;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int __devinit sh_mobile_rcu_probe(struct platform_device *pdev)
{
	struct sh_mobile_rcu_dev *pcdev;
	struct resource *res;
	void __iomem *base;
	unsigned int irq;
	int err = 0;
	struct bus_wait wait = {
		.completion = COMPLETION_INITIALIZER_ONSTACK(wait.completion),
		.notifier.notifier_call = bus_notify,
	};
	struct sh_mobile_rcu_companion *csi2;

	dev_err(&pdev->dev, "%s():\n", __func__);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || (int)irq <= 0) {
		dev_err(&pdev->dev, "Not enough RCU platform resources.\n");
		err = -ENODEV;
		goto exit;
	}

	pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "Could not allocate pcdev\n");
		err = -ENOMEM;
		goto exit;
	}

	sh_mobile_rcu_init_dumplog();
#ifdef SH_RCU_DUMP_LOG_ENABLE
	dev_info(&pdev->dev, "%s():EOSCAMERA_RAMDUMP_VIRT_ADDR=0x%X\n",
		__func__, (unsigned int)dumplog_addr);
	dev_info(&pdev->dev, "%s():EOSCAMERA_RAMDUMP_PHYS_ADDR=0x%X\n",
		__func__, (unsigned int)virt_to_phys((void *)dumplog_addr));
#endif /* SH_RCU_DUMP_LOG_ENABLE */

	INIT_LIST_HEAD(&pcdev->capture);
	spin_lock_init(&pcdev->lock);

	pcdev->pdata = pdev->dev.platform_data;
	if (!pcdev->pdata) {
		err = -EINVAL;
		dev_err(&pdev->dev, "RCU platform data not set.\n");
		goto exit_kfree;
	}

	pcdev->max_width = pcdev->pdata->max_width ? : 2560;
	pcdev->max_height = pcdev->pdata->max_height ? : 1920;

	base = ioremap_nocache(res->start, resource_size(res));
	if (!base) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap RCU registers.\n");
		goto exit_kfree;
	}

	pcdev->irq = irq;
	pcdev->base = base;

	base = ioremap_nocache(0xE5500000, 0x100);
	if (!base) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap MERAM registers.\n");
		goto exit_iounmap;
	}
	pcdev->base_meram = base;

	if (!strcmp(pcdev->pdata->mod_name, "sh_mobile_rcu.0"))
		pcdev->meram_ch = RCU_MERAM_CH_RCU0;
	else
		pcdev->meram_ch = RCU_MERAM_CH_RCU1;

	base = ioremap_nocache(0xE5500400 +
		(RCU_MERAM_CH(pcdev->meram_ch) * 0x20), 0x40);
	if (!base) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap MERAM registers.\n");
		goto exit_iounmap;
	}
	pcdev->base_meram_ch = base;

	base = ioremap_nocache(0xFE951000, 0x20);
	if (!base) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap IPMMU registers.\n");
		goto exit_iounmap;
	}
	pcdev->ipmmu = base;

#ifdef CONFIG_HAS_EARLYSUSPEND
	pcdev->rcu_suspend_pm.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	pcdev->rcu_suspend_pm.suspend = sh_mobile_rcu_early_suspend;
	pcdev->rcu_suspend_pm.resume = sh_mobile_rcu_late_resume;
	register_early_suspend(&pcdev->rcu_suspend_pm);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	pcdev->video_limit = 0; /* only enabled if second resource exists */
	pcdev->image_mode = SH_RCU_MODE_DATA;
	pcdev->output_mode = SH_RCU_OUTPUT_ISP;
	pcdev->output_offset = SH_RCU_OUTPUT_OFFSET_32B;
	pcdev->output_pack = SH_RCU_OUTPUT_ENHANCING;
	pcdev->output_meram = SH_RCU_OUTPUT_SDRAM;
	pcdev->meram_frame = RCU_MERAM_FRAMEA;
	pcdev->output_ispthinned = SH_RCU_OUTPUT_ISP_FULL;
	pcdev->zsl = 0;
	pcdev->output_ext = 0;
	pcdev->streaming = SH_RCU_STREAMING_OFF;
	pcdev->kick = 0;

	pcdev->iclk = clk_get(NULL, "icb");
	if (IS_ERR(pcdev->iclk)) {
		pcdev->iclk = NULL;
		dev_err(&pdev->dev, "cannot get clock \"icb\"\n");
	}
	pcdev->fclk = clk_get(&pdev->dev, pcdev->pdata->mod_name);
	if (IS_ERR(pcdev->fclk)) {
		pcdev->fclk = NULL;
		dev_err(&pdev->dev, "cannot get clock \"%s\"\n",
				pcdev->pdata->mod_name);
	}
	pcdev->mclk = clk_get(NULL, "meram");
	if (IS_ERR(pcdev->mclk)) {
		pcdev->mclk = NULL;
		dev_err(&pdev->dev, "cannot get clock \"meram\"\n");
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		err = dma_declare_coherent_memory(&pdev->dev, res->start,
						  res->start,
						  resource_size(res),
						  DMA_MEMORY_MAP |
						  DMA_MEMORY_EXCLUSIVE);
		if (!err) {
			dev_err(&pdev->dev, "Unable to declare RCU memory.\n");
			err = -ENXIO;
			goto exit_iounmap;
		}

		pcdev->video_limit = resource_size(res);
	}

	platform_set_drvdata(pdev, pcdev);

	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume(&pdev->dev);

	pcdev->ici.priv = pcdev;
	pcdev->ici.v4l2_dev.dev = &pdev->dev;
	pcdev->ici.nr = pdev->id;
	pcdev->ici.drv_name = dev_name(&pdev->dev);
	pcdev->ici.ops = &sh_mobile_rcu_host_ops;

	pcdev->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(pcdev->alloc_ctx)) {
		err = PTR_ERR(pcdev->alloc_ctx);
		goto exit_free_clk;
	}

	err = soc_camera_host_register(&pcdev->ici);
	if (err)
		goto exit_free_ctx;

	if ((!rear_flash_set) && pcdev->pdata->led)
		rear_flash_set = pcdev->pdata->led;

	/* CSI2 interfacing */
	csi2 = pcdev->pdata->csi2;
	dev_geo(&pdev->dev, "CSI2 interface %p\n", csi2);
	if (csi2) {
		struct platform_device *csi2_pdev =
			platform_device_alloc("sh-mobile-csi2", csi2->id);
		struct sh_csi2_pdata *csi2_pdata = csi2->platform_data;

		if (!csi2_pdev) {
			err = -ENOMEM;
			goto exit_host_unregister;
		}

		pcdev->csi2_pdev		= csi2_pdev;
/*
		err = platform_device_add_data(csi2_pdev,
				csi2_pdata, sizeof(*csi2_pdata));
		if (err < 0)
			goto exit_pdev_put;
*/

		csi2_pdata->v4l2_dev		= &pcdev->ici.v4l2_dev;

		csi2_pdev->resource		= csi2->resource;
		csi2_pdev->num_resources	= csi2->num_resources;

		csi2_pdev->dev.platform_data	= csi2_pdata;

		err = platform_device_add(csi2_pdev);
		if (err < 0)
			goto exit_pdev_put;

		wait.dev = &csi2_pdev->dev;

		err = bus_register_notifier(&platform_bus_type, &wait.notifier);
		if (err < 0)
			goto exit_pdev_unregister;

		/*
		 * From this point the driver module will not unload, until
		 * we complete the completion.
		 */

		if (!csi2_pdev->dev.driver) {
			complete(&wait.completion);
			/* Either too late, or probing failed */
			bus_unregister_notifier(
					&platform_bus_type, &wait.notifier);
			err = -ENXIO;
			goto exit_pdev_unregister;
		}

		/*
		 * The module is still loaded, in the worst case it is hanging
		 * in device release on our completion. So, _now_ dereferencing
		 * the "owner" is safe!
		 */

		err = try_module_get(csi2_pdev->dev.driver->owner);

		/* Let notifier complete, if it has been locked */
		complete(&wait.completion);
		bus_unregister_notifier(&platform_bus_type, &wait.notifier);
		if (!err) {
			err = -ENODEV;
			goto exit_pdev_unregister;
		}
	}

	dev_err(&pdev->dev, "%s():out\n", __func__);
	return 0;

exit_pdev_unregister:
	platform_device_del(pcdev->csi2_pdev);
exit_pdev_put:
	pcdev->csi2_pdev->resource = NULL;
	platform_device_put(pcdev->csi2_pdev);
exit_host_unregister:
	soc_camera_host_unregister(&pcdev->ici);
exit_free_ctx:
	vb2_dma_contig_cleanup_ctx(pcdev->alloc_ctx);
exit_free_clk:
	pm_runtime_disable(&pdev->dev);
	if (platform_get_resource(pdev, IORESOURCE_MEM, 1))
		dma_release_declared_memory(&pdev->dev);
exit_iounmap:
	if (pcdev->base)
		iounmap(pcdev->base);
	if (pcdev->base_meram)
		iounmap(pcdev->base_meram);
	if (pcdev->base_meram_ch)
		iounmap(pcdev->base_meram_ch);
	if (pcdev->ipmmu)
		iounmap(pcdev->ipmmu);
exit_kfree:
	kfree(pcdev);
exit:
	return err;
}

static int __devexit sh_mobile_rcu_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct sh_mobile_rcu_dev *pcdev = container_of(soc_host,
					struct sh_mobile_rcu_dev, ici);
	struct platform_device *csi2_pdev = pcdev->csi2_pdev;

	dev_geo(&pdev->dev, "%s():\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&pcdev->rcu_suspend_pm);
#endif /* CONFIG_HAS_EARLYSUSPEND */
	soc_camera_host_unregister(soc_host);
	pm_runtime_disable(&pdev->dev);
	if (platform_get_resource(pdev, IORESOURCE_MEM, 1))
		dma_release_declared_memory(&pdev->dev);
	iounmap(pcdev->base);
	iounmap(pcdev->base_meram);
	iounmap(pcdev->base_meram_ch);
	iounmap(pcdev->ipmmu);
	vb2_dma_contig_cleanup_ctx(pcdev->alloc_ctx);
	if (csi2_pdev && csi2_pdev->dev.driver) {
		struct module *csi2_drv = csi2_pdev->dev.driver->owner;
		platform_device_del(csi2_pdev);
		csi2_pdev->resource = NULL;
		platform_device_put(csi2_pdev);
		module_put(csi2_drv);
	}
	sh_mobile_rcu_deinit_dumplog();
	kfree(pcdev);

	return 0;
}

static int sh_mobile_rcu_suspend(struct device *dev)
{
	return 0;
}

static int sh_mobile_rcu_resume(struct device *dev)
{
	return 0;
}


static int sh_mobile_rcu_runtime_suspend(struct device *dev)
{
	struct sh_mobile_rcu_dev *pcdev;
	if (NULL == dev)
		return 0;

	pcdev = dev_get_drvdata(dev);

	/*
	 * This driver re-initializes all registers after
	 * pm_runtime_get_sync() anyway so there is no need
	 * to save and restore registers here.
	 */

	if (NULL == pcdev) {
		printk(KERN_ERR "%s, pcdev is NULL\n", __func__);
		return 0;
	}

	if (NULL != pcdev->iclk)
		clk_disable(pcdev->iclk);
	else {
		dev_err(pcdev->icd->parent,
			"%s, pcdev->iclk is NULL\n", __func__);
	}

	if (NULL != pcdev->fclk)
		clk_disable(pcdev->fclk);
	else {
		dev_err(pcdev->icd->parent,
			"%s, pcdev->fclk is NULL\n", __func__);
	}

	if (NULL != pcdev->mclk)
		clk_disable(pcdev->mclk);
	else {
		dev_err(pcdev->icd->parent,
			"%s, pcdev->mclk is NULL\n", __func__);
	}

	return 0;
}

static int sh_mobile_rcu_runtime_resume(struct device *dev)
{
	struct sh_mobile_rcu_dev *pcdev;
	if (NULL == dev)
		return 0;

	pcdev = dev_get_drvdata(dev);

	if (NULL == pcdev) {
		printk(KERN_ERR "%s, pcdev is NULL\n", __func__);
		return 0;
	}

	if (NULL != pcdev->iclk)
		clk_enable(pcdev->iclk);
	else {
		dev_err(pcdev->icd->parent,
			"%s, pcdev->iclk is NULL\n", __func__);
	}

	if (NULL != pcdev->fclk)
		clk_enable(pcdev->fclk);
	else {
		dev_err(pcdev->icd->parent,
			"%s, pcdev->fclk is NULL\n", __func__);
	}

	if (NULL != pcdev->mclk)
		clk_enable(pcdev->mclk);
	else {
		dev_err(pcdev->icd->parent,
			"%s, pcdev->mclk is NULL\n", __func__);
	}

	return 0;

}

static const struct dev_pm_ops sh_mobile_rcu_dev_pm_ops = {
	.suspend = sh_mobile_rcu_suspend,
	.resume = sh_mobile_rcu_resume,
	.runtime_suspend = sh_mobile_rcu_runtime_suspend,
	.runtime_resume = sh_mobile_rcu_runtime_resume,
};

static struct platform_driver sh_mobile_rcu_driver = {
	.driver		= {
		.name	= "sh_mobile_rcu",
		.pm	= &sh_mobile_rcu_dev_pm_ops,
	},
	.probe		= sh_mobile_rcu_probe,
	.remove		= __devexit_p(sh_mobile_rcu_remove),
};

static int __init sh_mobile_rcu_init(void)
{
	/* initialise camera device info */
	cam_class_init = false;
	sec_main_cam_dev = NULL;
	sec_sub_cam_dev = NULL;
	rear_flash_state = true;
	rear_flash_set = NULL;
	cam_status0 = -1;
	cam_status1 = -1;
	dump_addr_flg = false;
#ifdef SH_RCU_DUMP_LOG_ENABLE
	dumplog_order = 0;
	dumplog_init_cnt = 0;
	dumplog_addr = NULL;
	dumplog_ktbl = NULL;
	dumplog_max_idx = NULL;
	dumplog_cnt_idx = NULL;
#endif /* SH_RCU_DUMP_LOG_ENABLE */

	/* Whatever return code */
	request_module("sh_mobile_csi2");
	return platform_driver_register(&sh_mobile_rcu_driver);
}

static void __exit sh_mobile_rcu_exit(void)
{
	platform_driver_unregister(&sh_mobile_rcu_driver);
}

module_init(sh_mobile_rcu_init);
module_exit(sh_mobile_rcu_exit);

MODULE_DESCRIPTION("SuperH Mobile RCU driver");
MODULE_AUTHOR("Renesas Mobile Corp.");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0.6");
MODULE_ALIAS("platform:sh_mobile_rcu");
