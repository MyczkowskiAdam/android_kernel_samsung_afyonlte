/*
 * rt_boot_sub.c
 *		booting rt_cpu.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <mach/common.h>
#include <linux/io.h>
#include "rt_boot_drv.h"
#include "rt_boot_local.h"
#include "log_kernel.h"
#include <mach/r8a7373.h>

#define RESCNT_RT			(1 << 9)

#define RMSTPCR0_TLB		(1 << 31)
#define RMSTPCR0_IC			(1 << 30)
#define RMSTPCR0_OC			(1 << 29)
#define RMSTPCR0_INTCRT		(1 << 22)
#define RMSTPCR2_MFI		(1 << 13)

#define SRCR0_RT			(1 << 30)
#define SRCR2_MFI			(1 << 13)

#define MFIS_BASE_ADDR_PHYS     0xE6260000
#define MFIS_SIZE			(0x100)
static void __iomem *mfis_base;
#define MFIS_GSR			(mfis_base + 0x0004)
#define MFIS_IICR			(mfis_base + 0x0010)
#define MFIS_EICR			(mfis_base + 0x0014)

#define INTCRT_BASE2		(0xFFD20000)
#define INTCRT_SIZE2		(0x300)
static void __iomem *intcrt_base2;
#define INTCRT_IMR0S		(intcrt_base2 + 0x0080)
#define INTCRT_IMR12S		(intcrt_base2 + 0x00B0)
#define INTCRT_IMR0SA		(intcrt_base2 + 0x0180)
#define INTCRT_IMR12SA		(intcrt_base2 + 0x01B0)
#define INTCRT_BASE5		(0xFFD50000)
#define INTCRT_SIZE5		(0x200)
static void __iomem *intcrt_base5;
#define INTCRT_IMR0S3		(intcrt_base5 + 0x0080)
#define INTCRT_IMR12S3		(intcrt_base5 + 0x00B0)
#define INTCRT_IMR0SA3		(intcrt_base5 + 0x0180)
#define INTCRT_IMR12SA3		(intcrt_base5 + 0x01B0)


#define REG_IMGADDR			RBAR
#define REG_RTCPUCLOCK		RESCNT
#define REG_GSR				MFIS_GSR
#define REG_RTIIC			MFIS_EICR
#define REG_ARMIIC			MFIS_IICR

#define ARMIIC_RTBOOT		(0x00000010)
#define GSR_REQ_COMP		(0x00000001)

static char *kernel_rt_boot_path = "/boot/RTFM_SH4AL_DSP_MU200.bin";
static char *kernel_rt_cert_path = "/boot/mediafw.cert";

/* prototype */
static int set_screen_data(unsigned int disp_addr);

void do_ioremap_register(void)
{
	mfis_base = ioremap(MFIS_BASE_ADDR_PHYS, MFIS_SIZE);
	intcrt_base2 = ioremap(INTCRT_BASE2, INTCRT_SIZE2);
	intcrt_base5 = ioremap(INTCRT_BASE5, INTCRT_SIZE5);
}

void do_iounmap_register(void)
{
	iounmap(mfis_base);
	iounmap(intcrt_base2);
	iounmap(intcrt_base5);
}

int read_rt_image(unsigned int *addr)
{
	struct rt_boot_info info;
	struct rt_boot_info *bootaddr_info;
	int ret = 0;
	int retval;
	unsigned int data_size;
	unsigned char __iomem *data_addr = NULL;
	struct file *fp = NULL;

	MSG_MED("[RTBOOTK]IN |[%s]\n", __func__);

	memset(&info, 0, sizeof(info));

	if (addr == NULL) {
		MSG_ERROR("[RTBOOTK]   |addr is NULL\n");
		MSG_HIGH("[RTBOOTK]OUT|[%s] ret = %d\n", __func__, 1);
		return 1;
	}

	do {
		fp = filp_open(kernel_rt_boot_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			MSG_ERROR("[RTBOOTK]   |Error file open\n");
			ret = 1;
			break;
		}
		data_size = kernel_read(fp, RT_BOOT_SIZE, (char *)&info, sizeof(info));
		if (data_size != sizeof(info)) {
			MSG_ERROR("[RTBOOTK]   |Error file read (section info)\n");
			ret = 1;
			break;
		}
		MSG_LOW("Read RT Section header from RT Image\n");
		MSG_LOW("version                       = %c%c%c%c%c%c%c%c%c%c%c%c\n",
																info.version[0],
																info.version[1],
																info.version[2],
																info.version[3],
																info.version[4],
																info.version[5],
																info.version[6],
																info.version[7],
																info.version[8],
																info.version[9],
																info.version[10],
																info.version[11]);
		MSG_LOW("boot_addr                     = 0x%08x\n",	info.boot_addr);
		MSG_LOW("image_size                    = %08d\n",	info.image_size);
		MSG_LOW("load_flg                      = %08d\n",	info.load_flg);
		MSG_LOW("img[RT_LVL_1].section_start = 0x%08x\n",
				info.img[RT_LVL_1].section_start);
		MSG_LOW("img[RT_LVL_1].section_size  = %08d\n",
				info.img[RT_LVL_1].section_size);
		MSG_LOW("img[RT_LVL_2].section_start = 0x%08x\n",
				info.img[RT_LVL_2].section_start);
		MSG_LOW("img[RT_LVL_2].section_size  = %08d\n",
				info.img[RT_LVL_2].section_size);
		MSG_LOW("sh_pmb_offset              = 0x%08x\n", info.sh_pmb_offset);
		MSG_LOW("sh_pmb_nc_offset           = 0x%08x\n", info.sh_pmb_nc_offset);
		MSG_LOW("mfi_pmb_offset             = 0x%08x\n", info.mfi_pmb_offset);

#ifdef SECURE_BOOT_ENABLE
		data_addr = ioremap(PRIMARY_COPY_ADDR, info.image_size);
#else
		data_addr = ioremap(info.boot_addr, info.image_size);
#endif
		if (!data_addr) {
			MSG_ERROR("[RTBOOTK]   |ioremap Error section info\n");
			ret = 1;
			break;
		}
		data_size = kernel_read(fp, 0, (char *)data_addr, info.image_size);
		if (data_size != info.image_size) {
			MSG_ERROR("[RTBOOTK]   |Error file read (RT image) data_size =%d\n", data_size);
			ret = 1;
			break;
		}

		*addr = info.boot_addr;
		MSG_LOW("[RTBOOTK]   |boot_addr = 0x%08x\n", *addr);

		/* Init load_flg */
		bootaddr_info = (struct rt_boot_info *)(data_addr + RT_BOOT_SIZE);
		bootaddr_info->load_flg = 0;

		/* Set screen data */
		retval = set_screen_data(info.command_area_address + info.command_area_size - 32);
		if (0 != retval) {
			MSG_ERROR("[RTBOOTK]   |Error setting screen info\n");
			ret = 1;
			break;
		}

	} while (0);

	if (data_addr) {
		iounmap(data_addr);
	}

	if (!IS_ERR(fp)) {
		(void)filp_close(fp, NULL);
	}

	if (!ret) {
		g_rtboot_info = info;
	}

	MSG_MED("[RTBOOTK]OUT|[%s] ret = %d\n", __func__, ret);
	return ret;
}


void write_rt_imageaddr(unsigned int addr)
{
	unsigned int bootaddr = addr;
	/* Write RT image addr to register */
	bootaddr &= ~0xf;
	writel(bootaddr | 1, REG_IMGADDR);
}


void stop_rt_interrupt(void)
{
	void __iomem *reg;

	for (reg = INTCRT_IMR0SA; reg <= INTCRT_IMR12SA; reg += 4) {
		writeb(0xFF, reg);
	}

	for (reg = INTCRT_IMR0SA3; reg <= INTCRT_IMR12SA3; reg += 4) {
		writeb(0xFF, reg);
	}

	for (reg = INTCRT_IMR0S; reg <= INTCRT_IMR12S; reg += 4) {
		writeb(0xFF, reg);
	}

	for (reg = INTCRT_IMR0S3; reg <= INTCRT_IMR12S3; reg += 4) {
		writeb(0xFF, reg);
	}

}

void init_rt_register(void)
{
	/* initialize CPGA */
	MSG_LOW("[RTBOOTK]   |write RMSTPCR0 reg start\n");
	writel((readl(RMSTPCR0) & ~(RMSTPCR0_TLB|RMSTPCR0_IC|RMSTPCR0_OC|RMSTPCR0_INTCRT)), RMSTPCR0);
	while (0 != (readl(MSTPSR0) & (RMSTPCR0_TLB|RMSTPCR0_IC|RMSTPCR0_OC|RMSTPCR0_INTCRT)))
		;

	MSG_LOW("[RTBOOTK]   |write RMSTPCR2 reg start\n");
	writel(readl(RMSTPCR2) & ~RMSTPCR2_MFI, RMSTPCR2);
	while (0 != (readl(MSTPSR2) & RMSTPCR2_MFI))
		;

	/* Initialize MFI */
	writel(readl(SRCR2) & ~SRCR2_MFI, SRCR2);

	writel(0x00000000, REG_RTIIC);
	writel(0x00000000, REG_GSR);

}

void write_os_kind(unsigned int kind)
{
	writel(kind, REG_ARMIIC);
}

void start_rt_cpu(void)
{
	/* Reset RT */
	writel(readl(SRCR0) | SRCR0_RT, SRCR0);

	/* Enable RT clock */
	writel(readl(REG_RTCPUCLOCK) & ~RESCNT_RT, REG_RTCPUCLOCK);

	/* Unreset RT */
	writel(readl(SRCR0) & ~SRCR0_RT, SRCR0);

}


int wait_rt_cpu(unsigned int check_num)
{
	int ret = 1;
	unsigned int loop = check_num;

	while (loop != 0) {
		if (readl(REG_ARMIIC) == 0) {
			ret = 0;
			break;
		}
		MSG_LOW("RT boot waiting...(loop = %d)\n", loop);
		msleep(MSLEEP_WAIT_VALUE);
#if !(DEBUG)
		--loop;
#endif
	}
	return ret;
}


void write_req_comp(void)
{
	writel(ARMIIC_RTBOOT, REG_ARMIIC);
	writel(GSR_REQ_COMP, REG_GSR);
}

static int set_screen_data(unsigned int disp_addr)
{
	void __iomem *addr = NULL;
	struct screen_info screen[2];

	MSG_MED("[RTBOOTK]IN |[%s]\n", __func__);

	addr = ioremap(disp_addr, sizeof(screen));
	if (!addr) {
		MSG_ERROR("[RTBOOTK]   |Error ioremap\n");
		MSG_HIGH("[RTBOOTK]OUT|[%s] ret = 1\n", __func__);
		return 1;
	}

	/* LCD default setting */
	screen[0].height = SCREEN0_HEIGHT;
	screen[0].width  = SCREEN0_WIDTH;
	screen[0].stride = SCREEN0_STRIDE;
	screen[0].mode   = SCREEN0_MODE;

#ifdef CONFIG_FB_R_MOBILE_NT35510
	MSG_LOW("WVGA(NT35510) command mode setting.\n");
	screen[0].height = 800;
	screen[0].width  = 480;
	screen[0].stride = 480;
	screen[0].mode   = 1; /* COMMAND MODE */
#endif /* CONFIG_FB_R_MOBILE_NT35510 */

#ifdef CONFIG_FB_R_MOBILE_VX5B3D
	MSG_LOW("WSVGA(VX5B3D) video mode setting.\n");
	screen[0].height = 600;
	screen[0].width  = 1024;
	screen[0].stride = 1024;
	screen[0].mode   = 0; /* VIDEO MODE */
#endif /* CONFIG_FB_R_MOBILE_VX5B3D */

	screen[1].height = SCREEN1_HEIGHT;
	screen[1].width  = SCREEN1_WIDTH;
	screen[1].stride = SCREEN1_STRIDE;
	screen[1].mode   = SCREEN1_MODE;

	memcpy_toio(addr, &screen, sizeof(screen));

	iounmap(addr);

	MSG_MED("[RTBOOTK]OUT|[%s] ret = 0\n", __func__);

	return 0;
}

int read_rt_cert(unsigned int addr)
{
	unsigned char __iomem *data_addr = NULL;
	struct file *fp = NULL;
	struct kstat stbuf;
	int ret;
	int ret_size;

	MSG_MED("[RTBOOTK]IN |[%s]\n", __func__);

	do {
		ret = vfs_stat(kernel_rt_cert_path, &stbuf);
		if (ret != 0) {
			MSG_ERROR("[RTBOOTK]   |Error file stat\n");
			ret_size = 0;
			break;
		}

		fp = filp_open(kernel_rt_cert_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			MSG_ERROR("[RTBOOTK]   |Error file open\n");
			ret_size = 0;
			break;
		}

		data_addr = ioremap(addr, stbuf.size);
		if (!data_addr) {
			MSG_ERROR("[RTBOOTK]   |ioremap Error section info\n");
			ret_size = 0;
			break;
		}

		ret_size = kernel_read(fp, 0, (char *)data_addr, stbuf.size);
		if (ret_size != stbuf.size) {
			MSG_ERROR("[RTBOOTK]   |Error file read (RT image) size =%d\n", ret_size);
			ret_size = 0;
			break;
		}
	} while (0);

	if (data_addr) {
		iounmap(data_addr);
	}

	if (!IS_ERR(fp)) {
		if (fp)
			(void)filp_close(fp, NULL);
	}

	MSG_MED("[RTBOOTK]OUT|[%s] ret = %d\n", __func__, ret_size);

	return ret_size;
}
