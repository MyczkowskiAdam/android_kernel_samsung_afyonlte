/* audio_test.c
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
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

/*!
  @file		audio_test.c

  @brief	Audio test command source file.
*/



/*---------------------------------------------------------------------------*/
/* include files                                                             */
/*---------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/vcd/vcd.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include <mach/common.h>
#include <mach/pm.h>
#include <mach/r8a7373.h>
#include <linux/hwspinlock.h>
#include <linux/wakelock.h>
#include <linux/miscdevice.h>

#include <sound/soundpath/soundpath.h>
#include <sound/fsi_d2153.h>

#include <sound/soundpath/TP/audio_test_extern.h>
#include "audio_test.h"
#include "audio_test_reg.h"

/*---------------------------------------------------------------------------*/
/* typedef declaration (private)                                             */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* define macro declaration (private)                                        */
/*---------------------------------------------------------------------------*/
/*!
  @brief	Flag for check board revision.
*/
/* none */

/*!
  @brief	MAX wait time for wait queue for VCD.
*/
#define AUDIO_TEST_WATCH_CLK_TIME_OUT	(1000)

/*---------------------------------------------------------------------------*/
/* define function macro declaration (private)                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* enum declaration (private)                                                */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* structure declaration (private)                                           */
/*---------------------------------------------------------------------------*/
/*!
  @brief	Audio Test driver configuration structure.
*/
struct audio_test_priv {
	/***********************************/
	/* log                             */
	/***********************************/
	struct proc_dir_entry *log_entry;	/**< log level entry. */
	struct proc_dir_entry *pcmname_entry;	/**< pcm dump entry. */
	struct proc_dir_entry *proc_parent;	/**< proc parent. */
};

/*!
  @brief	Register setting table.
*/
struct audio_test_common_reg_table {
	u_int uiReg;				/**< Register address. */
	u_int uiValue;				/**< Register value. */
	u_int uiDelay;				/**< Register delay. */
	u_int uiClrbit;				/**< Register clear bit. */
};

/*---------------------------------------------------------------------------*/
/* prototype declaration (private)                                           */
/*---------------------------------------------------------------------------*/
/***********************************/
/* proc                            */
/***********************************/
static int audio_test_proc_set_device(u_int out_device_type);
static int audio_test_proc_start_scuw_loopback(u_int fsi_port);
static int audio_test_proc_stop_scuw_loopback(void);
static int audio_test_proc_start_tone(void);
static int audio_test_proc_stop_tone(void);
static int audio_test_proc_start_spuv_loopback(u_int fsi_port, u_int vqa_val,
						u_int delay_val);
static int audio_test_proc_stop_spuv_loopback(void);
static int audio_test_proc_start_sound_play(void);
static int audio_test_proc_stop_sound_play(void);
static void audio_test_proc_get_pcmname(
			struct audio_test_ioctl_pcmname_cmd *pcm);
static int audio_test_proc_set_pcmname(
			struct audio_test_ioctl_pcmname_cmd *pcm);
/***********************************/
/* HW write                        */
/***********************************/
static int audio_test_get_logic_addr(void);
static void audio_test_rel_logic_addr(void);
static int audio_test_loopback_setup(void);
static int audio_test_playback_setup(void);
static void audio_test_loopback_remove(void);
static void audio_test_playback_remove(void);
static void audio_test_audio_ctrl_func(enum audio_test_hw_val drv, int stat);
static void audio_test_common_set_register(enum audio_test_hw_val drv,
				struct audio_test_common_reg_table *reg_tbl,
				u_int size);
/***********************************/
/* callback for VCD                */
/***********************************/
static int audio_test_call_regist_watch(void);
static void audio_test_watch_start_clk_cb(void);
static void audio_test_watch_stop_clk_cb(void);
static void audio_test_watch_start_vcd_cb(void);
static void audio_test_watch_stop_vcd_cb(void);
/***********************************/
/* log level                       */
/***********************************/
static int audio_test_create_proc_entry(char *name,
					struct proc_dir_entry **proc_child);
static int audio_test_proc_log_read(char *page, char **start, off_t offset,
					int count, int *eof, void *data);
static int audio_test_proc_log_write(struct file *filp, const char *buffer,
					u_long count, void *data);
/***********************************/
/* pcm dump                        */
/***********************************/
static int audio_test_proc_pcm_read(char *page, char **start, off_t offset,
					int count, int *eof, void *data);
/***********************************/
/* callback                        */
/***********************************/
static int audio_test_open(struct inode *inode, struct file *filp);
static int audio_test_close(struct inode *inode, struct file *filp);
static long audio_test_ioctl(struct file *filp, u_int cmd, u_long arg);


/*---------------------------------------------------------------------------*/
/* global variable declaration                                               */
/*---------------------------------------------------------------------------*/
/***********************************/
/* log level                       */
/***********************************/
/*!
  @brief	Audio Test driver log level.
*/
#ifdef AUDIO_TEST_DBG
u_int audio_test_log_level = (AUDIO_TEST_LOG_ERR_PRINT |
				AUDIO_TEST_LOG_PROC_PRINT |
				AUDIO_TEST_LOG_FUNC_PRINT);
#else	/* !=AUDIO_TEST_DBG */
u_int audio_test_log_level = AUDIO_TEST_LOG_NO_PRINT;
#endif	/* AUDIO_TEST_DBG */
/***********************************/
/* ioctl                           */
/***********************************/
/*!
  @brief	Audio Test file operations.
*/
static const struct file_operations audio_test_fops = {
	.owner = THIS_MODULE,
	.open = audio_test_open,
	.release = audio_test_close,
	.unlocked_ioctl = audio_test_ioctl,
};

/*
  @brief	Audio Test misc device object
 */
static struct miscdevice audio_test_misc_dev = {
	.minor			= MISC_DYNAMIC_MINOR,
	.name			= AUDIO_TEST_DEVICE_NAME,
	.fops			= &audio_test_fops,
};
/***********************************/
/* internal parameter              */
/***********************************/
/*!
  @brief	Store the AudioTest driver config.
*/
static struct audio_test_priv *audio_test_conf;
/*!
  @brief	Output device type.
*/
static u_int audio_test_drv_out_device_type = AUDIO_TEST_DRV_OUT_SPEAKER;
/*!
  @brief	Loopback state.
*/
static u_int audio_test_pt_state = AUDIO_TEST_DRV_STATE_OFF;
/*!
  @brief	PCM name.
		pcmname[0][0] Playback Normal
		pcmname[0][1] Playback PT
		pcmname[1][0] Capture Normal
		pcmname[1][1] Capture PT
*/
static char audio_test_pcmname[AUDIO_TEST_DRV_PCMDIR_MAX]
				[AUDIO_TEST_DRV_PCMTYPE_MAX]
				[AUDIO_TEST_PCMNAME_MAX_LEN];
/***********************************/
/* HW clock flag                   */
/***********************************/
/*!
  @brief	Clock status flag.
*/
static u_int g_audio_test_clock_flag;
/***********************************/
/* HW logical address              */
/***********************************/
/*!
  @brief	CPGA register(soft reset) base address.
*/
static u_char __iomem *g_audio_test_ulSrstRegBase;
/*!
  @brief	FSI base address (PortA).
*/
static u_char __iomem *g_audio_test_fsi_Base;
/*!
  @brief	SCUW base address.
*/
static u_char __iomem *g_audio_test_scuw_Base;
/*!
  @brief	CLKGEN base address.
*/
static u_char __iomem *g_audio_test_clkgen_Base;
/***********************************/
/* Wait queue                      */
/***********************************/
/*!
  @brief	Wait queue for start voice process wake up.
*/
static DECLARE_WAIT_QUEUE_HEAD(g_watch_start_clk_queue);
static atomic_t g_audio_test_watch_start_clk;
/*!
  @brief	Wait queue for stop voice process wake up.
*/
static DECLARE_WAIT_QUEUE_HEAD(g_watch_stop_clk_queue);
static atomic_t g_audio_test_watch_stop_clk;
/*!
  @brief	Wait queue for start voice process wake up.
*/
static DECLARE_WAIT_QUEUE_HEAD(g_watch_start_vcd_queue);
static atomic_t g_audio_test_watch_start_vcd;
/***********************************/
/* PM setting                      */
/***********************************/
/*!
  @brief	Power domain.
*/
static struct device *g_audio_test_power_domain
			[AUDIO_TEST_POWER_DOMAIN_MAX];
/*!
  @brief	Power domain count.
*/
static size_t g_audio_test_power_domain_count;
/***********************************/
/* Wake lock setting               */
/***********************************/
/*!
  @brief	Wake lock count.
*/
struct wake_lock g_audio_test_wake_lock;

/***********************************/
/* Table for loopback              */
/***********************************/
static struct audio_test_common_reg_table audio_test_tbl_fsi_loopback[] = {
	/* Register			Value		Delay	Clear */
	/* Clock select register (CLK_SEL) */
	/* DSPS (0) : 1 (Bus clock(MP clock)) */
	{AUDIO_TEST_FSI_CLK_SEL,	0x00000001,	0,	0},
	/* Port A Clock Set Register (A_ACK_MD) */
	/* ACKMD (14to12) : 000 (512 fs), BPFMD (10to8) : 001 (64 bit/fs),
	   DIMD (4) : 0 (Slave operation), DOMD (0) : 0 (Slave operation) */
	{AUDIO_TEST_FSI_ACK_MD,		0x00000100,	0,	0},
	/* Port A Clock Invert control register (A_ACK_RV) */
	/* LRM (12) : 0 (Clock not inverted), BRM (8) : 0 (Clock not inverted),
	   LRS (4) : 0 (Clock not inverted), BRS (0) : 1 (Clock inverted) */
	{AUDIO_TEST_FSI_ACK_RV,		0x00000001,	0,	0},
	/* Port A/B Output Serial Format Register (A_DO_FMT) */
	/* BWS (21to20) : 00 (24 bits), DTMD (9to8) : 00 (PCM format),
	   FMT (6to4) : 011 (I2S (2 channels)), NCH (2to0) : 000 (1 channel) */
	{AUDIO_TEST_FSI_DO_FMT,		0x00000030,	0,	0},
	/* Port A Input Serial Format Register (A_DI_FMT) */
	/* BWS (21to20) : 00 (24bits), FMT (6to4) : 011 (I2S (2 channels)),
	   NCH (2to0) : 000 (1 channel) */
	{AUDIO_TEST_FSI_DI_FMT,		0x00000030,	0,	0},
	/* Port A MUTE State (A_MUTE_ST) */
	/* O_CH8 (15) : 0, O_CH7 (14) : 0, O_CH6 (13) : 0, O_CH5 (12) : 1,
	   O_CH4 (11) : 0, O_CH3 (10) : 0, O_CH2 (9) : 0, O_CH1 (8) : 1,
	   I_CH8 (7) : 0, I_CH7 (6) : 0, I_CH6 (5) : 0, I_CH5 (4) : 1,
	   I_CH4 (3) : 0, I_CH3 (2) : 0, I_CH2 (1) : 0, I_CH1 (0) : 1 */
	{AUDIO_TEST_FSI_MUTE,		0x00001111,	0,	0},
};
static struct audio_test_common_reg_table
				audio_test_tbl_scuw_scuw_loopback[] = {
	/* Register			Value		Delay	Clear */
	/* Selector Control Register 0 (SELCR_SEL0) */
	/* SEL (3to0) : 0110 (FSI-IF read port 1 data (from FSI2)) */
	{AUDIO_TEST_SCUW_SEL_SELCR0,	0x00000006,	0,	0},
	/* Selector Control Register 5 (SELCR_SEL5) */
	/* SEL (1to0) : 00 (SEL0 output data, clock supply to MIX0 halted) */
	{AUDIO_TEST_SCUW_SEL_SELCR5,	0x00000000,	0,	0},
	/* Selector Control Register 6 (SELCR_SEL6) */
	/* SEL (0) : 0 (SEL5 output data, clock supply to IIR0 halted) */
	{AUDIO_TEST_SCUW_SEL_SELCR6,	0x00000000,	0,	0},
	/* Selector Control Register 7 (SELCR_SEL7) */
	/* SEL (0) : 0 (SEL6 output data, clock supply to DVU0 halted) */
	{AUDIO_TEST_SCUW_SEL_SELCR7,	0x00000000,	0,	0},
	/* Selector Control Register 15 (SELCR_SEL15) */
	/* SEL (0) : 0 (SEL7 output data) */
	{AUDIO_TEST_SCUW_SEL_SELCR15,	0x00000000,	0,	0},
	/* Module stop register 1 (MSTP1_N) */
	/* MSTP0_N (0) : 1 (FSI-IF operates.) */
	{AUDIO_TEST_SCUW_MSTP1,		0x00000001,	0,	0},
	/* FSI IF Software Reset Register (SWRSR_FSIF) */
	/* SWRST (0) : 0 (Reset the FSI IF.) */
	{AUDIO_TEST_SCUW_FSIIF_SWRSR,	0x00000000,	0,	0},
	/* FSI IF Software Reset Register (SWRSR_FSIF) */
	/* SWRST (0) : 1 (FSI IF enters the operating state.) */
	{AUDIO_TEST_SCUW_FSIIF_SWRSR,	0x00000001,	0,	0},
	/* FSI IF Initialization Register (FSIIR) */
	/* INIT (0) : 1 (Initialization) */
	{AUDIO_TEST_SCUW_FSIIF_FSIIR,	0x00000001,	0,	0},
	/* FSI IF Audio Information Register (ADINR_FSIF0_W0) */
	/* CHNUM (3to0) : 0010 (2 channel) */
	{AUDIO_TEST_SCUW_FSIIF_ADINRW0,	0x00000002,	0,	0},
	/* FSI IF Audio Information Register (ADINR_FSIF0_R1) */
	/* CHNUM (3to0) : 0010 (2 channel) */
	{AUDIO_TEST_SCUW_FSIIF_ADINRR1,	0x00000002,	0,	0},
	/* FSI Write Address Control Register for Port 0 (WADCR_FSIF_0) */
	/* WAD (7to0) : 0x09 (FSI2 port A) */
	{AUDIO_TEST_SCUW_FSIIF_WADCR0,	0x00000009,	0,	0},
	/* FSI Read Address Control Register for Port 1 (RADCR_FSIF_1) */
	/* RAD (7to0) : 0x08 (FSI2 port A) */
	{AUDIO_TEST_SCUW_FSIIF_RADCR1,	0x00000008,	0,	0},
	/* FSI IF Initialization Register (FSIIR) */
	/* INIT (0) : 0 (Processing State) */
	{AUDIO_TEST_SCUW_FSIIF_FSIIR,	0x00000000,	0,	0},
};
static struct audio_test_common_reg_table
				audio_test_tbl_clkgen_scuw_loopback[] = {
	/* Register			Value		Delay	Clear */
	/* CLKG System control register (CLKGSYSCTL) */
	/* CKSEL (3) : 0 (Supplies EXTAL1 for core clock.),
	   CKSTP2 (2) : 0 (Supplies EXTAL2.),
	   CKSTP1 (1) : 0 (Supplies EXTAL1.), CSR (0) : 0 (Clears the reset.) */
	{AUDIO_TEST_CLKG_SYSCTL,	0x00000000,	0,	0},
	/* CLKG TIM select register1 (CLKGTIMSEL1) */
	/* PWMTIM (31to28) : 0000 (Select Port A TIM),
	   FFDTIM (27to24) : 0000 (Select CPU-FIFO1 TIM),
	   RECTIM2 (19to16) : 0000 (Select Port A TIM),
	   RECTIM1 (11to8) : 0010 (Select Port A TIM),
	   RECTIM0 (3to0) : 0000 (Select Port A TIM) */
	{AUDIO_TEST_CLKG_TIMSEL1,	0x00000200,	0,	0},
	/* CLKG common register (CLKGFSIACOM) */
	/* TDIV (27to24) : 0000 (Setting TDM-Adopter repeat time),
	   MODE (21to20) : 10 (Non-continuos clock mode),
	   FORM (17to16) : 01 (2ch(LR) format),
	   FS (14to12) : 010 (64fs), RATE (11to8) : 1000 (44.1kHz),
	   INV (5) : 0 (non-invert BCLK),
	   CLKGM (0) : 1 (Select CLKGEN master) */
	{AUDIO_TEST_CLKG_FSIACOM,	0x00212801,	0,	0},
	/* CLKG PULSE control register (CLKGPULSECTL) */
	/* VINTREV (9) : 0 (normal), VINTSEL (8) : 0 (HW VINT (from PAD).),
	   SLIMEN (7) : 0 (Disable.), FFDEN (6) : 0 (Disable.),
	   AUEN (5) : 0 (Disable.), SVEN (4) : 0 (Disable.),
	   CF1EN (3) : 0 (Disable.), CF0EN (2) : 0 (Disable.),
	   PBEN (1) : 0 (Disable.), PAEN (0) : 1 (Enable.) */
	{AUDIO_TEST_CLKG_PULSECTL,	0x00000001,	0,	0},
};

/***********************************/
/* Table for PCM name prefix       */
/***********************************/
static const char *audio_test_pcm_prefix[AUDIO_TEST_DRV_PCMDIR_MAX]
					[AUDIO_TEST_DRV_PCMTYPE_MAX] = {
	{ "Playback:     ",
	  "Playback(PT): " },
	{ "Capture:      ",
	  "Capture(PT):  " },
};

/*---------------------------------------------------------------------------*/
/* extern variable declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* extern function declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* inline function implementation                                            */
/*---------------------------------------------------------------------------*/
/* none */



/*---------------------------------------------------------------------------*/
/* function implementation                                                   */
/*---------------------------------------------------------------------------*/
/*------------------------------------*/
/* for private function               */
/*------------------------------------*/
/*!
  @brief	Process of setting device.

  @param	out_device_type [i] Output device.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_set_device(u_int out_device_type)
{
	int ret = 0;
	audio_test_log_efunc("out_dev[%d]", out_device_type);
	audio_test_drv_out_device_type = out_device_type;
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Norify loopback to sound driver.

  @param	dev_chg [i] Reason of device change.

  @return	Function results.

  @note		.
*/
static int audio_test_notify_loopback(u_int dev_chg)
{
	int ret = 0;
	u_int dev = 0;

	audio_test_log_efunc("dev_chg[%d]", dev_chg);

	switch (audio_test_drv_out_device_type) {
	case AUDIO_TEST_DRV_OUT_SPEAKER:
		dev = SNDP_OUT_SPEAKER;
		break;
	case AUDIO_TEST_DRV_OUT_HEADPHONE:
		dev = SNDP_OUT_WIRED_HEADPHONE;
		break;
	case AUDIO_TEST_DRV_OUT_EARPIECE:
		dev = SNDP_OUT_EARPIECE;
		break;
	default:
		audio_test_log_info("unknown output device");
		break;
	}

	ret = sndp_pt_loopback(SNDP_MODE_INCALL, dev, dev_chg);
	if (-ENODEV == ret)
		ret = 0;
	else if (0 != ret) {
		audio_test_log_err("sndp_pt_loopback");
		goto error;
	}

error:
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Norify playback to sound driver.

  @param	dev_chg [i] Reason of device change.

  @return	Function results.

  @note		.
*/
static int audio_test_notify_playback(u_int dev_chg)
{
	int ret = 0;
	u_int dev = 0;

	audio_test_log_efunc("dev_chg[%d]", dev_chg);

	switch (audio_test_drv_out_device_type) {
	case AUDIO_TEST_DRV_OUT_SPEAKER:
		audio_test_log_info("SPEAKER");
		dev = SNDP_OUT_SPEAKER;
		break;
	case AUDIO_TEST_DRV_OUT_HEADPHONE:
		audio_test_log_info("HEADPHONE");
		dev = SNDP_OUT_WIRED_HEADPHONE;
		break;
	case AUDIO_TEST_DRV_OUT_EARPIECE:
		audio_test_log_info("EARPIECE");
		dev = SNDP_OUT_EARPIECE;
		break;
	default:
		audio_test_log_info("unknown output device");
		ret = -ENODEV;
		goto error;
	}

	ret = sndp_pt_device_change(dev, dev_chg);
	if (0 != ret)
		audio_test_log_err("sndp_pt_device_change");

error:
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Copy process.

  @param	to [i] Dest ptr.
  @param	from [i] Source ptr.
  @param	len [i] Size.

  @return	.

  @note		.
*/
static inline void audio_test_strncpy(char *to, const char *from, size_t len)
{
	audio_test_log_efunc("");
	if (len > 0) {
		strncpy(to, from, len);
		*(to + len - 1)  = '\0';
	} else {
		audio_test_log_err("Length error ! len[%d]", len);
	}
	audio_test_log_rfunc("");
}

/*!
  @brief	Process of starting SCUW loopback.
			[AudioIC->FSI->SCUW->FSI->AudioIC].

  @param	fsi_port [i] FSI port.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_start_scuw_loopback(u_int fsi_port)
{
	int ret = 0;

	audio_test_log_efunc("fsi[%d]", fsi_port);

	/* Add not to be suspend in loopback */
	wake_lock(&g_audio_test_wake_lock);

	/***********************************/
	/* Setup                           */
	/***********************************/
	ret = audio_test_loopback_setup();
	if (0 != ret) {
		audio_test_log_err("audio_test_loopback_setup");
		goto error;
	}

	/***********************************/
	/* Enable SCUW clock               */
	/***********************************/
	audio_test_audio_ctrl_func(AUDIO_TEST_HW_SCUW,
				AUDIO_TEST_DRV_STATE_ON);

	/***********************************/
	/* Enable FSI clock                */
	/***********************************/
	audio_test_audio_ctrl_func(AUDIO_TEST_HW_FSI,
				AUDIO_TEST_DRV_STATE_ON);

	/***********************************/
	/* Enable CLKGEN clock             */
	/***********************************/
	audio_test_audio_ctrl_func(AUDIO_TEST_HW_CLKGEN,
				AUDIO_TEST_DRV_STATE_ON);

	/***********************************/
	/* Set SCUW register               */
	/***********************************/
	audio_test_common_set_register(AUDIO_TEST_HW_SCUW,
				audio_test_tbl_scuw_scuw_loopback,
				ARRAY_SIZE(audio_test_tbl_scuw_scuw_loopback));

	/***********************************/
	/* Set FSI register                */
	/***********************************/
	audio_test_common_set_register(AUDIO_TEST_HW_FSI,
				audio_test_tbl_fsi_loopback,
				ARRAY_SIZE(audio_test_tbl_fsi_loopback));

	/***********************************/
	/* Set CLKGEN register             */
	/***********************************/
	audio_test_common_set_register(AUDIO_TEST_HW_CLKGEN,
			audio_test_tbl_clkgen_scuw_loopback,
			ARRAY_SIZE(audio_test_tbl_clkgen_scuw_loopback));

	/***********************************/
	/* Clock reset                     */
	/***********************************/
	sh_modify_register32(
		(g_audio_test_fsi_Base + AUDIO_TEST_FSI_ACK_RST),
		0, 0x00000001);

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_ON;

error:
	/* Add not to be suspend in loopback */
	wake_unlock(&g_audio_test_wake_lock);

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of stopping SCUW loopback.
			[AudioIC->FSI->SCUW->FSI->AudioIC].

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_stop_scuw_loopback(void)
{
	int ret = 0;
	int i;

	audio_test_log_efunc("");

	/***********************************/
	/* Wait VCD                        */
	/***********************************/
	wait_event_interruptible_timeout(
		g_watch_stop_clk_queue,
		atomic_read(&g_audio_test_watch_stop_clk),
		msecs_to_jiffies(AUDIO_TEST_WATCH_CLK_TIME_OUT));
	atomic_set(&g_audio_test_watch_stop_clk, 0);

	/***********************************/
	/* Stop SCUW                       */
	/***********************************/
	audio_test_audio_ctrl_func(AUDIO_TEST_HW_SCUW,
				AUDIO_TEST_DRV_STATE_OFF);

	/***********************************/
	/* Stop FSI                        */
	/***********************************/
	audio_test_audio_ctrl_func(AUDIO_TEST_HW_FSI,
				AUDIO_TEST_DRV_STATE_OFF);

	/***********************************/
	/* Stop CLKGEN                     */
	/***********************************/
	audio_test_audio_ctrl_func(AUDIO_TEST_HW_CLKGEN,
				AUDIO_TEST_DRV_STATE_OFF);

	/* Disable the power domain */
	for (i = 0; i < g_audio_test_power_domain_count; i++) {
		ret = pm_runtime_put_sync(g_audio_test_power_domain[i]);
		if (0 != ret)
			audio_test_log_err(
				"pm_runtime_put_sync res[%d]\n", ret);
	}

	fsi_d2153_loopback_notify(FSI_D2153_LOOPBACK_STOP);

	/* Add not to be suspend in loopback */
	wake_unlock(&g_audio_test_wake_lock);

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_OFF;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of starting tone.

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_start_tone(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/* Add not to be suspend in loopback */
	wake_lock(&g_audio_test_wake_lock);

	/***********************************/
	/* Setup                           */
	/***********************************/
	ret = audio_test_loopback_setup();
	if (0 != ret) {
		audio_test_log_err("audio_test_loopback_setup");
		goto error;
	}

	/* Notify to Sound driver */
	/* for KeyTone Mix and Audience */
	ret = audio_test_notify_loopback(SNDP_EXTDEV_START);
	if (0 != ret) {
		audio_test_log_err("audio_test_notify_loopback");
		goto error;
	}

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_ON;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;

error:
	/* Add not to be suspend in loopback */
	wake_unlock(&g_audio_test_wake_lock);

	audio_test_log_err("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of stopping tone.

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_stop_tone(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/* Notify to Sound driver */
	/* for KeyTone Mix and Audience */
	ret = audio_test_notify_loopback(SNDP_EXTDEV_STOP);
	if (0 != ret)
		audio_test_log_err("audio_test_notify_loopback");

	/***********************************/
	/* Remove                          */
	/***********************************/
	audio_test_loopback_remove();

	/* Add not to be suspend in loopback */
	wake_unlock(&g_audio_test_wake_lock);

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_OFF;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of starting SPUV loopback.
			[AudioIC->FSI->SCUW->SPUV->SCUW->FSI->AudioIC].

  @param	fsi_port [i] FSI port.
  @param	vqa_val [i] VQA valid.
  @param	delay_val [i] Delay valid.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_start_spuv_loopback(u_int fsi_port, u_int vqa_val,
						u_int delay_val)
{
	int ret = 0;

	audio_test_log_efunc("");

	/* Add not to be suspend in loopback */
	wake_lock(&g_audio_test_wake_lock);

	/***********************************/
	/* Setup                           */
	/***********************************/
	ret = audio_test_loopback_setup();
	if (0 != ret) {
		audio_test_log_err("audio_test_loopback_setup");
		goto error;
	}

	/* Notify to Sound driver */
	/* for KeyTone Mix and Audience */
	ret = audio_test_notify_loopback(SNDP_EXTDEV_START);
	if (0 != ret) {
		audio_test_log_err("audio_test_notify_loopback");
		goto error;
	}

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_ON;

	/***********************************/
	/* Wait VCD                        */
	/***********************************/
	wait_event_interruptible(
		g_watch_start_vcd_queue,
		atomic_read(&g_audio_test_watch_start_vcd));
	if (AUDIO_TEST_VCD_NG == atomic_read(&g_audio_test_watch_start_vcd)) {
		audio_test_log_err("Wait VCD");
		atomic_set(&g_audio_test_watch_start_vcd, AUDIO_TEST_VCD_NONE);
		ret = -ECOMM;
		goto wait_error;
	}
	atomic_set(&g_audio_test_watch_start_vcd, AUDIO_TEST_VCD_NONE);

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;

error:
	/* Add not to be suspend in loopback */
	wake_unlock(&g_audio_test_wake_lock);
wait_error:
	audio_test_log_err("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of stopping SPUV loopback.
			[AudioIC->FSI->SCUW->SPUV->SCUW->FSI->AudioIC].

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_stop_spuv_loopback(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/* Notify to Sound driver */
	/* for KeyTone Mix and Audience */
	ret = audio_test_notify_loopback(SNDP_EXTDEV_STOP);
	if (0 != ret)
		audio_test_log_err("audio_test_notify_loopback");

	/***********************************/
	/* Remove                          */
	/***********************************/
	audio_test_loopback_remove();

	/* Add not to be suspend in loopback */
	wake_unlock(&g_audio_test_wake_lock);

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_OFF;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of starting playback.
			[FSI->AudioIC].

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_start_sound_play(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/***********************************/
	/* Setup                           */
	/***********************************/
	ret = audio_test_playback_setup();
	if (0 != ret) {
		audio_test_log_err("audio_test_playback_setup");
		goto error;
	}

	/* Notify to Sound driver */
	ret = audio_test_notify_playback(SNDP_ON);
	if (0 != ret) {
		audio_test_log_err("audio_test_notify_playback");
		goto error;
	}

	if (AUDIO_TEST_DRV_STATE_ON == audio_test_pt_state) {
		audio_test_log_info("already setting");
		return 0;
	}

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_ON;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;

error:
	(void)audio_test_notify_playback(SNDP_OFF);
	wake_unlock(&g_audio_test_wake_lock);
	audio_test_log_err("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of stopping playback.
			[FSI->AudioIC].

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_stop_sound_play(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/* Notify to Sound driver */
	ret = audio_test_notify_playback(SNDP_OFF);
	if (0 != ret)
		audio_test_log_err("audio_test_notify_playback");

	/***********************************/
	/* Remove                          */
	/***********************************/
	audio_test_playback_remove();

	audio_test_pt_state = AUDIO_TEST_DRV_STATE_OFF;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Process of setting call mode.

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_set_call_mode(u_int call_kind, u_int vqa_val,
					u_int delay_val)
{
	int ret = 0;
	struct vcd_execute_command cmd;
	struct vcd_call_option option;
	struct vcd_watch_fw_info info;

	audio_test_log_efunc("");

	memset(&cmd, 0, sizeof(cmd));
	memset(&option, 0, sizeof(option));
	memset(&info, 0, sizeof(info));

	cmd.command = VCD_COMMAND_WATCH_FW;
	info.start_fw = audio_test_watch_start_vcd_cb;
	info.stop_fw = audio_test_watch_stop_vcd_cb;
	cmd.arg = &info;
	ret = vcd_execute_test_call(&cmd);
	if (0 != ret) {
		audio_test_log_err("vcd_execute_test_call VCD_COMMAND_WATCH_FW err [%d]", ret);
		goto error;
	}

	memset(&cmd, 0, sizeof(cmd));

	cmd.command = VCD_COMMAND_SET_CALL_MODE;
	switch (call_kind) {
	case AUDIO_TEST_DRV_KIND_CALL:
		option.call_kind = VCD_CALL_KIND_CALL;
		break;
	case AUDIO_TEST_DRV_KIND_KIND_PCM_LB:
		option.call_kind = VCD_CALL_KIND_PCM_LB;
		if (AUDIO_TEST_DRV_STATE_OFF == vqa_val &&
			AUDIO_TEST_DRV_STATE_OFF == delay_val) {
			option.loopback_mode = VCD_LOOPBACK_MODE_INTERFACE;
		} else if (AUDIO_TEST_DRV_STATE_ON == vqa_val &&
			AUDIO_TEST_DRV_STATE_OFF == delay_val) {
			option.loopback_mode = VCD_LOOPBACK_MODE_PCM;
		} else {
			option.loopback_mode = VCD_LOOPBACK_MODE_DELAY;
		}
		break;
	case AUDIO_TEST_DRV_KIND_1KHZ:
		option.call_kind = VCD_CALL_KIND_1KHZ;
		break;
	default:
		audio_test_log_err("Unknown kind[%d]", call_kind);
		goto error;
		break;
	}
	cmd.arg = &option;
	ret = vcd_execute_test_call(&cmd);

error:
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Get loopcack state.

  @param	state [o] Loopback state.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_get_loopback_state(u_int *state)
{
	int ret = 0;

	audio_test_log_efunc("");

	*state = audio_test_pt_state;

	audio_test_log_rfunc("state[%d]", audio_test_pt_state);
	return ret;
}

/*!
  @brief	Get PCM name.

  @param	pcm [i/o] structure audio_test_ioctl_pcmname_cmd.

  @return	Function results.

  @note		.
*/
static void audio_test_proc_get_pcmname(
				struct audio_test_ioctl_pcmname_cmd *pcm)
{
	audio_test_log_efunc("");

	audio_test_strncpy(pcm->pcmname,
		audio_test_pcmname[pcm->pcmdirection][pcm->pcmtype],
		AUDIO_TEST_PCMNAME_MAX_LEN);

	audio_test_log_rfunc("PCM name [%s]",
			audio_test_pcmname[pcm->pcmdirection][pcm->pcmtype]);
}

/*!
  @brief	Set PCM name.

  @param	pcm [i] structure audio_test_ioctl_pcmname_cmd.

  @return	Function results.

  @note		.
*/
static int audio_test_proc_set_pcmname(
				struct audio_test_ioctl_pcmname_cmd *pcm)
{
	int ret = 0;

	audio_test_log_efunc("");

	if (AUDIO_TEST_DRV_PCMSTATE_OPEN == pcm->pcmstate)
		audio_test_strncpy(
			audio_test_pcmname[pcm->pcmdirection][pcm->pcmtype],
			pcm->pcmname,
			AUDIO_TEST_PCMNAME_MAX_LEN);
	else
		audio_test_pcmname[pcm->pcmdirection][pcm->pcmtype][0] = '\0';

	audio_test_log_rfunc("PCM name [%s]",
			audio_test_pcmname[pcm->pcmdirection][pcm->pcmtype]);
	return ret;
}

/*!
  @brief	Get logical address.

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_get_logic_addr(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/****************************************/
	/* Get CPGA(soft reset) Logical Address */
	/****************************************/
	g_audio_test_ulSrstRegBase =
		ioremap_nocache(CPG_SEMCTRL_BASE_PHYS,
					AUDIO_TEST_CPG_REG_MAX_SRST);
	if (!g_audio_test_ulSrstRegBase) {
		audio_test_log_err("Software Reset register ioremap failed");
		ret = -ENOMEM;
		goto error;
	}

	/***********************************/
	/* Get FSI Logical Address         */
	/***********************************/
	g_audio_test_fsi_Base =
		ioremap_nocache(FSI_BASE_PHYS,
					AUDIO_TEST_FSI_MAP_LEN);
	if (!g_audio_test_fsi_Base) {
		audio_test_log_err("fsi ioremap failed");
		ret = -ENOMEM;
		goto error;
	}

	/***********************************/
	/* Get SCUW Logical Address        */
	/***********************************/
	g_audio_test_scuw_Base =
		ioremap_nocache(SCUW_BASE_PHYS,
					AUDIO_TEST_SUCW_MAP_LEN);
	if (!g_audio_test_scuw_Base) {
		audio_test_log_err("scuw ioremap failed");
		ret = -ENOMEM;
		goto error;
	}

	/***********************************/
	/* Get CLKGEN Logical Address      */
	/***********************************/
	g_audio_test_clkgen_Base =
		ioremap_nocache(CLKGEN_BASE_PHYS,
					AUDIO_TEST_CLKGEN_MAP_LEN);
	if (!g_audio_test_clkgen_Base) {
		audio_test_log_err("clkgen ioremap failed");
		ret = -ENOMEM;
		goto error;
	}

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
error:
	/***********************************/
	/* Release logical address         */
	/***********************************/
	audio_test_rel_logic_addr();

	audio_test_log_err("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Release logical address.

  @param	.

  @return	.

  @note		.
*/
static void audio_test_rel_logic_addr(void)
{
	audio_test_log_efunc("");

	/***********************************/
	/* Release FSI Logical Address     */
	/***********************************/
	if (g_audio_test_fsi_Base) {
		iounmap(g_audio_test_fsi_Base);
		g_audio_test_fsi_Base = NULL;
	}

	/***********************************/
	/* Release SCUW Logical Address    */
	/***********************************/
	if (g_audio_test_scuw_Base) {
		iounmap(g_audio_test_scuw_Base);
		g_audio_test_scuw_Base = NULL;
	}

	/***********************************/
	/* Release CLKGEN Logical Address  */
	/***********************************/
	if (g_audio_test_clkgen_Base) {
		iounmap(g_audio_test_clkgen_Base);
		g_audio_test_clkgen_Base = NULL;
	}

	/*********************************************/
	/* Release CPGA(soft reset) Logical Address  */
	/*********************************************/
	if (g_audio_test_ulSrstRegBase) {
		iounmap(g_audio_test_ulSrstRegBase);
		g_audio_test_ulSrstRegBase = NULL;
	}

	audio_test_log_rfunc("");
}

/*!
  @brief	Setup loopback setting.

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_loopback_setup(void)
{
	int ret = 0;
	int res = 0;
	int i;
	struct snd_pcm_hw_params params;

	audio_test_log_efunc("");

	params.intervals[SNDRV_PCM_HW_PARAM_RATE
		- SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min = 16000;
	fsi_d2153_set_sampling_rate(&params);

	ret = fsi_d2153_loopback_notify(FSI_D2153_LOOPBACK_START);
	if (0 != ret) {
		audio_test_log_err("loopback_notify ret[%d]\n", ret);
		goto error;
	}

	/* Enable the power domain */
	for (i = 0; i < g_audio_test_power_domain_count; i++) {
		res = pm_runtime_get_sync(g_audio_test_power_domain[i]);
		if (!(0 == res || 1 == res)) {  /* 0:success 1:active */
			audio_test_log_err(
				"pm_runtime_get_sync res[%d]\n", res);
			ret = -1;
			goto error;
		}
	}

	/***********************************/
	/* Wait VCD                        */
	/***********************************/
	wait_event_interruptible_timeout(
		g_watch_start_clk_queue,
		atomic_read(&g_audio_test_watch_start_clk),
		msecs_to_jiffies(AUDIO_TEST_WATCH_CLK_TIME_OUT));
	atomic_set(&g_audio_test_watch_start_clk, 0);

error:
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Setup playback setting.

  @param	.

  @return	Function results.

  @note		.
*/
static int audio_test_playback_setup(void)
{
	int ret = 0;

	audio_test_log_efunc("");

	/* Playback side performs the same processing Loopback */
	ret = fsi_d2153_loopback_notify(FSI_D2153_LOOPBACK_START);
	if (0 != ret) {
		audio_test_log_err("loopback_notify ret[%d]\n", ret);
		goto error;
	}

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;

error:
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Remove loopback setting.

  @param	.

  @return	.

  @note		.
*/
static void audio_test_loopback_remove(void)
{
	int res = 0;
	int i;
	struct snd_pcm_hw_params params;

	audio_test_log_efunc("");

	/***********************************/
	/* Wait VCD                        */
	/***********************************/
	wait_event_interruptible_timeout(
		g_watch_stop_clk_queue,
		atomic_read(&g_audio_test_watch_stop_clk),
		msecs_to_jiffies(AUDIO_TEST_WATCH_CLK_TIME_OUT));
	atomic_set(&g_audio_test_watch_stop_clk, 0);

	/* Disable the power domain */
	for (i = 0; i < g_audio_test_power_domain_count; i++) {
		res = pm_runtime_put_sync(g_audio_test_power_domain[i]);
		if (0 != res)
			audio_test_log_err(
				"pm_runtime_put_sync res[%d]\n", res);
	}

	params.intervals[SNDRV_PCM_HW_PARAM_RATE
		- SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min = 48000;
	fsi_d2153_set_sampling_rate(&params);

	fsi_d2153_loopback_notify(FSI_D2153_LOOPBACK_STOP);

	audio_test_log_rfunc("");
}

/*!
  @brief	Remove playback setting.

  @param	.

  @return	.

  @note		.
*/
static void audio_test_playback_remove(void)
{
	audio_test_log_efunc("");

	fsi_d2153_loopback_notify(FSI_D2153_LOOPBACK_STOP);

	audio_test_log_rfunc("");
}

/*!
  @brief	Power management for CLKGEN/FSI/SCUW.

  @param	drv [i] H/W type.
  @param	stat [i] On/Off.

  @return	.

  @note		.
*/
static void audio_test_audio_ctrl_func(enum audio_test_hw_val drv, int stat)
{
	struct clk *clk;
	unsigned long flags = 0;
	int res = 0;

	audio_test_log_efunc("hw[%d] stat[%d]", drv, stat);

	audio_test_log_info("clock_flag[%#08x]", g_audio_test_clock_flag);

	switch (drv) {
	/***********************************/
	/* CLKGEN                          */
	/***********************************/
	case AUDIO_TEST_HW_CLKGEN:
		if (AUDIO_TEST_DRV_STATE_ON == stat) {
			/***********************************/
			/* Status ON                       */
			/***********************************/
			if (!(AUDIO_TEST_CLK_CLKGEN &
				g_audio_test_clock_flag)) {
				clk = clk_get(NULL, "clkgen");
				if (IS_ERR(clk)) {
					audio_test_log_err(
						"clkget(clkgen) error");
				} else {
					clk_enable(clk);
					clk_put(clk);
					audio_test_log_info(
						"clkgen clock enable");
				}

				res = hwspin_lock_timeout_irqsave(
					r8a7373_hwlock_cpg, 10, &flags);
				if (0 > res)
					audio_test_log_err("Can't lock cpg\n");

				/* Soft Reset */
				sh_modify_register32(AUDIO_TEST_CPG_SRCR2,
							0, 0x01000000);
				udelay(62);
				/* CLKGEN operates */
				sh_modify_register32(AUDIO_TEST_CPG_SRCR2,
							0x01000000, 0);

				if (0 <= res)
					hwspin_unlock_irqrestore(
						r8a7373_hwlock_cpg, &flags);

				g_audio_test_clock_flag |=
					AUDIO_TEST_CLK_CLKGEN;
			}
		} else {
			/***********************************/
			/* Status OFF                      */
			/***********************************/
			if ((AUDIO_TEST_CLK_CLKGEN &
				g_audio_test_clock_flag)) {
				clk = clk_get(NULL, "clkgen");
				if (IS_ERR(clk)) {
					audio_test_log_err(
						"clkget(clkgen) error");
				} else {
					clk_disable(clk);
					clk_put(clk);
					audio_test_log_info(
						"clkgen clock disable");
				}

				g_audio_test_clock_flag &=
					~AUDIO_TEST_CLK_CLKGEN;
			}
		}
		break;
	/***********************************/
	/* FSI                             */
	/***********************************/
	case AUDIO_TEST_HW_FSI:
		if (AUDIO_TEST_DRV_STATE_ON == stat) {
			/***********************************/
			/* Status ON                       */
			/***********************************/
			if (!(AUDIO_TEST_CLK_FSI &
				g_audio_test_clock_flag)) {
				clk = clk_get(NULL, "fsi");
				if (IS_ERR(clk)) {
					audio_test_log_err(
						"clkget(fsi) error");
				} else {
					clk_enable(clk);
					clk_put(clk);
					audio_test_log_info(
						"fsi clock enable");
				}

				res = hwspin_lock_timeout_irqsave(
					r8a7373_hwlock_cpg, 10, &flags);
				if (0 > res)
					audio_test_log_err("Can't lock cpg\n");

				/* Soft Reset */
				sh_modify_register32(AUDIO_TEST_CPG_SRCR3,
							0, 0x10000000);
				udelay(62);
				/* FSI operates */
				sh_modify_register32(AUDIO_TEST_CPG_SRCR3,
							0x10000000, 0);

				if (0 <= res)
					hwspin_unlock_irqrestore(
						r8a7373_hwlock_cpg, &flags);

				g_audio_test_clock_flag |= AUDIO_TEST_CLK_FSI;
			}
		} else {
			/***********************************/
			/* Status OFF                      */
			/***********************************/
			if ((AUDIO_TEST_CLK_FSI & g_audio_test_clock_flag)) {
				clk = clk_get(NULL, "fsi");
				if (IS_ERR(clk)) {
					audio_test_log_err(
						"clkget(fsi) error\n");
				} else {
					clk_disable(clk);
					clk_put(clk);
					audio_test_log_info(
						"fsi clock disable");
				}

				g_audio_test_clock_flag &= ~AUDIO_TEST_CLK_FSI;
			}
		}
		break;
	/***********************************/
	/* SCUW                            */
	/***********************************/
	case AUDIO_TEST_HW_SCUW:
		if (AUDIO_TEST_DRV_STATE_ON == stat) {
			/***********************************/
			/* Status ON                       */
			/***********************************/
			if (!(AUDIO_TEST_CLK_SCUW &
				g_audio_test_clock_flag)) {
				clk = clk_get(NULL, "scuw");
				if (IS_ERR(clk)) {
					audio_test_log_err(
						"clkget(scuw) error\n");
				} else {
					clk_enable(clk);
					clk_put(clk);
					audio_test_log_info(
						"scuw clock enable");
				}

				res = hwspin_lock_timeout_irqsave(
					r8a7373_hwlock_cpg, 10, &flags);
				if (0 > res)
					audio_test_log_err("Can't lock cpg\n");

				/* Soft Reset */
				sh_modify_register32(AUDIO_TEST_CPG_SRCR3,
							0, 0x04000000);
				udelay(62);
				/* SCUW operates */
				sh_modify_register32(AUDIO_TEST_CPG_SRCR3,
							0x04000000, 0);

				if (0 <= res)
					hwspin_unlock_irqrestore(
						r8a7373_hwlock_cpg, &flags);

				g_audio_test_clock_flag |= AUDIO_TEST_CLK_SCUW;
			}
		} else {
			/***********************************/
			/* Status OFF                      */
			/***********************************/
			if ((AUDIO_TEST_CLK_SCUW &
				g_audio_test_clock_flag)) {
				clk = clk_get(NULL, "scuw");
				if (IS_ERR(clk)) {
					audio_test_log_err(
						"clkget(scuw) error\n");
				} else {
					clk_disable(clk);
					clk_put(clk);
					audio_test_log_info(
						"scuw clock disable");
				}

				g_audio_test_clock_flag &=
					~AUDIO_TEST_CLK_SCUW;
			}
		}
		break;
	/***********************************/
	/* Unknown                         */
	/***********************************/
	default:
		audio_test_log_err("unknown function");
		break;
	}

	audio_test_log_rfunc("");
}

/*!
  @brief	Common registers setting function.

  @param	drv [i] H/W type.
  @param	reg_tbl [i] Register table.
  @param	size [i] Register table size.

  @return	.

  @note		.
*/
static void audio_test_common_set_register(enum audio_test_hw_val drv,
				struct audio_test_common_reg_table *reg_tbl,
				u_int size)
{
	int i;

	audio_test_log_efunc("hw[%d]", drv);

	for (i = 0; size > i; i++) {
		audio_test_log_info("reg[%#010x] val[%#010x] delay[%d]",
					reg_tbl[i].uiReg,
					reg_tbl[i].uiValue,
					reg_tbl[i].uiDelay);

		if (0 != reg_tbl[i].uiDelay) {
			/***********************************/
			/* Delay                           */
			/***********************************/
			/* 1000 micro over */
			if (AUDIO_TEST_COMMON_UDELAY_MAX <=
					reg_tbl[i].uiDelay)
				mdelay((reg_tbl[i].uiDelay /
					AUDIO_TEST_COMMON_UDELAY_MAX));
			else
				udelay(reg_tbl[i].uiDelay);
		} else if (0 != reg_tbl[i].uiClrbit) {
			/***********************************/
			/* Modify                          */
			/***********************************/
			/* CLKGEN */
			if (AUDIO_TEST_HW_CLKGEN == drv)
				sh_modify_register32(
					(g_audio_test_clkgen_Base +
						reg_tbl[i].uiReg),
					reg_tbl[i].uiClrbit,
					reg_tbl[i].uiValue);
			/* FSI */
			else if (AUDIO_TEST_HW_FSI == drv)
				sh_modify_register32(
					(g_audio_test_fsi_Base +
						reg_tbl[i].uiReg),
					reg_tbl[i].uiClrbit,
					reg_tbl[i].uiValue);
			/* SCUW */
			else
				sh_modify_register32(
					(g_audio_test_scuw_Base +
						reg_tbl[i].uiReg),
					reg_tbl[i].uiClrbit,
					reg_tbl[i].uiValue);
		} else {
			/***********************************/
			/* Register setting                */
			/***********************************/
			/* CLKGEN */
			if (AUDIO_TEST_HW_CLKGEN == drv)
				iowrite32(reg_tbl[i].uiValue,
					(g_audio_test_clkgen_Base +
						reg_tbl[i].uiReg));
			/* FSI */
			else if (AUDIO_TEST_HW_FSI == drv)
				iowrite32(reg_tbl[i].uiValue,
					(g_audio_test_fsi_Base +
						reg_tbl[i].uiReg));
			/* SCUW */
			else
				iowrite32(reg_tbl[i].uiValue,
					(g_audio_test_scuw_Base +
						reg_tbl[i].uiReg));
		}
	}

	audio_test_log_rfunc("");
}

/*!
  @brief	Set callback function for VCD Watch

  @param	.

  @return	Function results.

  @note		.
 */
static int audio_test_call_regist_watch(void)
{
	int ret = 0;
	struct vcd_execute_command cmd;
	struct vcd_watch_clkgen_info watch_clkgen_info;

	audio_test_log_efunc("");

	memset(&cmd, 0, sizeof(cmd));
	memset(&watch_clkgen_info, 0, sizeof(watch_clkgen_info));

	/***********************************/
	/* Set callback for VCD            */
	/***********************************/
	cmd.command = VCD_COMMAND_WATCH_CLKGEN;
	watch_clkgen_info.start_clkgen = audio_test_watch_start_clk_cb;
	watch_clkgen_info.stop_clkgen = audio_test_watch_stop_clk_cb;
	cmd.arg = &watch_clkgen_info;
	ret = vcd_execute_test_call(&cmd);
	if (0 != ret) {
		audio_test_log_err("vcd_execute_test_call");
		goto error;
	}

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
error:
	audio_test_log_err("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Wake up start clkgen callback function

  @param	.

  @return	.

  @note		.
 */
static void audio_test_watch_start_clk_cb(void)
{
	audio_test_log_efunc("");

	atomic_set(&g_audio_test_watch_start_clk, 1);
	wake_up_interruptible(&g_watch_start_clk_queue);

	audio_test_log_rfunc("");
}

/*!
  @brief	Wake up stop clkgen callback function

  @param	.

  @return	.

  @note		.
 */
static void audio_test_watch_stop_clk_cb(void)
{
	audio_test_log_efunc("");

	atomic_set(&g_audio_test_watch_stop_clk, 1);
	wake_up_interruptible(&g_watch_stop_clk_queue);

	audio_test_log_rfunc("");
}

/*!
  @brief	Wake up start vocoder callback function

  @param	.

  @return	.

  @note		.
 */
static void audio_test_watch_start_vcd_cb(void)
{
	audio_test_log_efunc("");

	atomic_set(&g_audio_test_watch_start_vcd, AUDIO_TEST_VCD_OK);
	wake_up_interruptible(&g_watch_start_vcd_queue);

	audio_test_log_rfunc("");
}

/*!
  @brief	firm stop vocoder callback function

  @param	.

  @return	.

  @note		.
 */
static void audio_test_watch_stop_vcd_cb(void)
{
	audio_test_log_efunc("");

	atomic_set(&g_audio_test_watch_start_vcd, AUDIO_TEST_VCD_NG);
	wake_up_interruptible(&g_watch_start_vcd_queue);

	audio_test_log_rfunc("");
}

/*!
  @brief	Create proc entry.

  @param	name [i] Directory in proc.
  @param	proc_child [o] Proc entry.

  @return	Function results.

  @note		.
*/
static int audio_test_create_proc_entry(char *name,
					struct proc_dir_entry **proc_child)
{
	int ret = 0;

	audio_test_log_efunc("name[%s]", name);

	*proc_child = create_proc_entry(name,
					(S_IRUGO | S_IWUGO),
					audio_test_conf->proc_parent);
	if (NULL != *proc_child) {
		if (strcmp(name, AUDIO_TEST_LOG_LEVEL) == 0) {
			(*proc_child)->read_proc = audio_test_proc_log_read;
			(*proc_child)->write_proc = audio_test_proc_log_write;
		} else if (strcmp(name, AUDIO_TEST_PCMNAME) == 0) {
			(*proc_child)->read_proc = audio_test_proc_pcm_read;
		} else {
			audio_test_log_err("parameter error.");
			ret = -EINVAL;
		}
	} else {
		audio_test_log_err("create failed for %s.", name);
		ret = -ENOMEM;
	}

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Read proc file.

  @param	page [o] Writing range.
  @param	start [o] .
  @param	offset [o] .
  @param	count [o] .
  @param	eof [o] End of file.
  @param	data [o] .

  @return	Data length.

  @note		.
*/
static int audio_test_proc_log_read(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	int len = 0;

	audio_test_log_efunc("");

	len = sprintf(page, "%#010x\n", (int)audio_test_log_level);
	*eof = 1;

	audio_test_log_rfunc("len[%d]", len);
	return len;
}

/*!
  @brief	Write proc file.

  @param	filp [i] File pointer.
  @param	buffer [i] Data buffer.
  @param	count [i] Data count.
  @param	data [i] Data.

  @return	Data count or Function results.

  @note		.
*/
static int audio_test_proc_log_write(struct file *filp, const char *buffer,
					u_long count, void *data)
{
	int ret = 0;
	int in = 0;
	char *temp = NULL;

	audio_test_log_efunc("filp[%p] buffer[%p] count[%ld] data[%p]",
			filp, buffer, count, data);

	temp = kmalloc(count, GFP_KERNEL);
	memset(temp, 0, count);
	audio_test_strncpy(temp, buffer, count);
	ret = kstrtoint(temp, 0, &in);
	kfree(temp);

	if (ret)
		return ret;

	audio_test_log_level = (u_int)in & AUDIO_TEST_LOG_LEVEL_MAX;

	audio_test_log_rfunc("count[%ld]", count);
	return count;
}

/*!
  @brief	Read proc file.

  @param	page [o] Writing range.
  @param	start [o] Not use.
  @param	offset [o] Not use.
  @param	count [o] Not use.
  @param	eof [o] End of file.
  @param	data [o] Not use.

  @return	Data length.

  @note		.
*/
static int audio_test_proc_pcm_read(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	int len = 0, i, j;
	char *pcmname;
	char *none = "--";

	audio_test_log_efunc("");

	for (i = 0; i < AUDIO_TEST_DRV_PCMDIR_MAX; i++) {
		for (j = 0; j < AUDIO_TEST_DRV_PCMTYPE_MAX; j++) {
			pcmname = audio_test_pcmname[i][j];
			if ('\0' == audio_test_pcmname[i][j][0])
				pcmname = none;
			len += sprintf(&page[len], "%s%s\n",
				audio_test_pcm_prefix[i][j],
				pcmname);
		}
	}

	*eof = 1;

	audio_test_log_rfunc("len[%d]", len);
	return len;
}

/*------------------------------------*/
/* for public function                */
/*------------------------------------*/
/*!
  @brief	Callback function of "open".

  @param	inode [i] I-NODE.
  @param	filp [i] File pointer.

  @return	Function results.

  @note		.
*/
int audio_test_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	audio_test_log_efunc("filp[%p]", filp);

	filp->private_data = audio_test_conf;

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Callback function of "close".

  @param	inode [i] I-NODE.
  @param	filp [i] File pointer.

  @return	Function results.

  @note		.
*/
int audio_test_close(struct inode *inode, struct file *filp)
{
	int ret = 0;

	audio_test_log_efunc("filp[%p]", filp);

	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Callback function of "ioctl".

  @param	filp [i] File pointer.
  @param	cmd [i] IOCTL command.
  @param	arg [i] Argument.

  @return	Function results.

  @note		.
*/
long audio_test_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	long ret = 0;
	struct audio_test_ioctl_cmd data;
	struct audio_test_ioctl_pcmname_cmd pcmdata;

	audio_test_log_efunc("filp[%p] cmd[0x%x]", filp, cmd);

	memset(&data, 0, sizeof(data));
	memset(&pcmdata, 0, sizeof(pcmdata));

	if (!access_ok(VERIFY_WRITE, (void __user *)arg,
					_IOC_SIZE(cmd))) {
		ret = -EFAULT;
		goto done;
	}
	if ((AUDIO_TEST_IOCTL_GET_PCMNAME == cmd) ||
		(AUDIO_TEST_IOCTL_SET_PCMNAME == cmd)) {
		if (copy_from_user(&pcmdata, (int __user *)arg,
						_IOC_SIZE(cmd))) {
			ret = -EFAULT;
			goto done;
		}
	} else {
		if (copy_from_user(&data, (int __user *)arg,
						_IOC_SIZE(cmd))) {
			ret = -EFAULT;
			goto done;
		}
	}

	switch (cmd) {
	case AUDIO_TEST_IOCTL_SETDEVICE:
		ret = audio_test_proc_set_device(data.out_device_type);
		break;

	case AUDIO_TEST_IOCTL_STARTSCUWLOOP:
		ret = audio_test_proc_start_scuw_loopback(data.fsi_port);
		break;

	case AUDIO_TEST_IOCTL_STOPSCUWLOOP:
		ret = audio_test_proc_stop_scuw_loopback();
		break;

	case AUDIO_TEST_IOCTL_STARTTONE:
		ret = audio_test_proc_start_tone();
		break;

	case AUDIO_TEST_IOCTL_STOPTONE:
		ret = audio_test_proc_stop_tone();
		break;

	case AUDIO_TEST_IOCTL_STARTSPUVLOOP:
		ret = audio_test_proc_start_spuv_loopback(data.fsi_port,
							data.vqa_val,
							data.delay_val);
		break;

	case AUDIO_TEST_IOCTL_STOPSPUVLOOP:
		ret = audio_test_proc_stop_spuv_loopback();
		break;

	case AUDIO_TEST_IOCTL_GETLBSTATE:
		ret = audio_test_proc_get_loopback_state(&data.pt_state);

		if (copy_to_user((int __user *)arg, &data,
						_IOC_SIZE(cmd))) {
			ret = -EFAULT;
			goto done;
		}
		break;

	case AUDIO_TEST_IOCTL_SETCALLMODE:
		ret = audio_test_proc_set_call_mode(data.call_kind,
						data.vqa_val,
						data.delay_val);
		break;

	case AUDIO_TEST_IOCTL_STARTSOUNDPLAY:
		ret = audio_test_proc_start_sound_play();
		break;

	case AUDIO_TEST_IOCTL_STOPSOUNDPLAY:
		ret = audio_test_proc_stop_sound_play();
		break;

	case AUDIO_TEST_IOCTL_GET_PCMNAME:
		audio_test_proc_get_pcmname(&pcmdata);
		if (copy_to_user((int __user *)arg, &pcmdata,
						_IOC_SIZE(cmd))) {
			ret = -EFAULT;
			goto done;
		}
		break;

	case AUDIO_TEST_IOCTL_SET_PCMNAME:
		ret = audio_test_proc_set_pcmname(&pcmdata);
		break;
	default:
		audio_test_log_err("unknown command");
		ret = -ENOTTY;
		break;
	}

done:
	audio_test_log_rfunc("ret[%ld]", ret);
	return ret;
}

/*!
  @brief	Initialize.

  @param	.

  @return	Function results.

  @note		.
*/
static int __init audio_test_init(void)
{
	int ret = 0;
	int i;
	struct audio_test_priv *dev_conf = NULL;

	audio_test_log_efunc("");

	/* register misc */
	ret = misc_register(&audio_test_misc_dev);
	if (ret) {
		audio_test_log_err("misc_register error.\n");
		goto rtn;
	}

	/***********************************/
	/* Get internal parameter area     */
	/***********************************/
	if (NULL == audio_test_conf) {
		dev_conf = kzalloc(sizeof(struct audio_test_priv), GFP_KERNEL);

		if (NULL == dev_conf) {
			ret = -ENOMEM;
			audio_test_log_err(
				"Could not allocate master. ret[%d]",
				ret);
			goto delete_misc;
		}
		audio_test_conf = dev_conf;
	}

	/***********************************/
	/* create file for log level entry */
	/***********************************/
	audio_test_conf->proc_parent = proc_mkdir(AUDIO_TEST_DRV_NAME, NULL);
	if (NULL != audio_test_conf->proc_parent) {
		ret = audio_test_create_proc_entry(AUDIO_TEST_LOG_LEVEL,
					&audio_test_conf->log_entry);
		if (0 != ret) {
			audio_test_log_err("Create proc. ret[%d]", ret);
			goto error;
		}
		ret = audio_test_create_proc_entry(AUDIO_TEST_PCMNAME,
					&audio_test_conf->pcmname_entry);
		if (0 != ret) {
			audio_test_log_err("Create proc2. ret[%d]", ret);
			goto error;
		}
	}

	/* Power domain setting */
	ret = power_domain_devices("snd-soc-audio-test",
				    g_audio_test_power_domain,
				    &g_audio_test_power_domain_count);
	if (0 != ret) {
		audio_test_log_err("Power domain setting ret[%d]\n", ret);
		goto error;
	}

	/* RuntimePM */
	for (i = 0; i < g_audio_test_power_domain_count; i++) {
		pm_runtime_enable(g_audio_test_power_domain[i]);
		ret = pm_runtime_resume(g_audio_test_power_domain[i]);
		if (ret < 0) {
			audio_test_log_err(
				"pm_runtime_get_sync[%d].\n", ret);
			goto error;
		}
	}

	/***********************************/
	/* Regist callback for VCD         */
	/***********************************/
	audio_test_call_regist_watch();

	/* Add not to be suspend in loopback */
	wake_lock_init(&g_audio_test_wake_lock,
		       WAKE_LOCK_SUSPEND,
		       "snd-soc-audio-test");

	/***********************************/
	/* Get logical address             */
	/***********************************/
	ret = audio_test_get_logic_addr();
	if (0 != ret) {
		audio_test_log_err("audio_test_get_logic_addr ret[%d]\n", ret);
		goto destroy_wakelock;
	}

	memset(&audio_test_pcmname, '\0', sizeof(audio_test_pcmname));

	goto rtn;

destroy_wakelock:
	/* Add not to be suspend in loopback */
	wake_lock_destroy(&g_audio_test_wake_lock);
error:
	for (i = 0; i < g_audio_test_power_domain_count; i++) {
		pm_runtime_disable(g_audio_test_power_domain[i]);
		g_audio_test_power_domain[i] = NULL;
	}

	if (audio_test_conf->log_entry)
		remove_proc_entry(AUDIO_TEST_LOG_LEVEL,
				audio_test_conf->proc_parent);
	if (audio_test_conf->pcmname_entry)
		remove_proc_entry(AUDIO_TEST_PCMNAME,
				audio_test_conf->proc_parent);
	if (audio_test_conf->proc_parent)
		remove_proc_entry(AUDIO_TEST_DRV_NAME, NULL);

	kfree(audio_test_conf);
	audio_test_conf = NULL;
delete_misc:
	misc_deregister(&audio_test_misc_dev);

rtn:
	audio_test_log_rfunc("ret[%d]", ret);
	return ret;
}

/*!
  @brief	Exit.

  @param	.

  @return	.

  @note		.
*/
static void __exit audio_test_exit(void)
{
	int i;
	audio_test_log_efunc("");

	misc_deregister(&audio_test_misc_dev);

	/***********************************/
	/* Remove log proc                 */
	/***********************************/
	if (audio_test_conf->log_entry)
		remove_proc_entry(AUDIO_TEST_LOG_LEVEL,
				audio_test_conf->proc_parent);
	if (audio_test_conf->pcmname_entry)
		remove_proc_entry(AUDIO_TEST_PCMNAME,
				audio_test_conf->proc_parent);
	if (audio_test_conf->proc_parent)
		remove_proc_entry(AUDIO_TEST_DRV_NAME, NULL);

	/* Add not to be suspend in loopback */
	wake_lock_destroy(&g_audio_test_wake_lock);

	/***********************************/
	/* Release logical address         */
	/***********************************/
	audio_test_rel_logic_addr();

	/***********************************/
	/* Free internal parameter area    */
	/***********************************/
	kfree(audio_test_conf);
	audio_test_conf = NULL;

	for (i = 0; i < g_audio_test_power_domain_count; i++) {
		pm_runtime_disable(g_audio_test_power_domain[i]);
		g_audio_test_power_domain[i] = NULL;
	}

	audio_test_log_rfunc("");
}

module_init(audio_test_init);
module_exit(audio_test_exit);

MODULE_LICENSE("GPL v2");
