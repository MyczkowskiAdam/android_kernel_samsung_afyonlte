/*
 *  MMC Oops/Panic logger
 *
 *  Copyright (C) 2010 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *  Copyright (C) 2011 Renesas Mobile Corp.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kmsg_dump.h>
#include <linux/slab.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/mmcoops.h>

#include <linux/rmu2_cmt15.h>

#include "../../staging/android/logger.h"
#include <sec_hal_cmn.h>
#include <mach/r8a7373.h>

#define BLOCK_SIZE_EMMC		512UL
#define RECORD_SIZE		8

#ifdef CONFIG_CRASHLOG_DDR
static void *adr;
#endif

#ifdef CONFIG_ARM_TZ
static sec_reset_info *reset_info;
#endif

static DEFINE_SPINLOCK(mmc_oops_lock);
static int dump_oops = 0;

module_param(dump_oops, int, 0600);
MODULE_PARM_DESC(dump_oops,
		"set to 1 to dump oopses, 0 to only dump panics (default 1)");


#define add_to_buf(_base, _count, _src, _size)	\
do {						\
	memcpy(_base + _count, _src, _size);	\
	_count += _size;			\
} while (0)

typedef struct {
	char	*name;
	unsigned long	log_size;
}logcat_st;

static struct mmcoops_context {
	struct kmsg_dumper	dump;
#ifdef CONFIG_CRASHLOG_EMMC
	struct mmc_card		*card;
	struct device		*mmc_dev;
	struct platform_device	*pdev;
	unsigned long		next_record;
	unsigned long		max_record;
	struct kobject		kobj;
	struct completion	kobj_unregister;
#endif
	unsigned long		start;
	unsigned long		size;
	unsigned long		record_size;
	unsigned long		kmsg_size;
	logcat_st		logcat[4];
	char			*virt_addr;
	u32			local_version;
	char			soft_version[32];
	u32			next_counter;
} oops_cxt;


#ifdef CONFIG_CRASHLOG_EMMC

#define dev_to_mmc_card(d)	container_of(d, struct mmc_card, dev)

struct oops_attr {
	struct attribute attr;
	ssize_t (*show)(struct mmcoops_context *, char *);
	ssize_t (*store)(struct mmcoops_context *, const char *, size_t count);
};

#define attr_ro(_name)				\
static struct oops_attr mmc_oops_##_name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define attr_rw(_name)				\
static struct oops_attr mmc_oops_##_name =	\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define show_one(_name, _fmt)				\
static ssize_t show_##_name				\
(struct mmcoops_context *cxt, char *buf)		\
{							\
	return sprintf(buf, _fmt, cxt->_name);		\
}

show_one(start, "%lu\n");
show_one(size, "%lu\n");
show_one(record_size, "%lu\n");
show_one(next_record, "%lu\n");
show_one(next_counter, "%u\n");
show_one(local_version, "0x%08x\n");

static ssize_t show_soft_version(struct mmcoops_context *cxt, char *buf)
{
	return sprintf(buf, "%s", &cxt->soft_version[0]);
}

#define store_one(_name, _fmt)						\
static ssize_t store_##_name						\
(struct mmcoops_context *cxt, const char *buf, size_t count)		\
{									\
	unsigned int ret = -EINVAL;					\
	unsigned long val;						\
									\
	ret = sscanf(buf, _fmt, &val);					\
	if (ret != 1)							\
		return -EINVAL;						\
									\
	cxt->_name = val;						\
									\
	return count;							\
}

store_one(next_record, "%lu");
store_one(next_counter, "%lu");
store_one(local_version, "0x%08lx");

static ssize_t store_soft_version(struct mmcoops_context *cxt,
				  const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	char soft_version[32];

	ret = sscanf(buf, "%s",&soft_version[0]);
	if (ret == 0)
		return -EINVAL;

	memcpy(cxt->soft_version, soft_version, sizeof(soft_version));

	return count;
}

void set_record_size(struct mmcoops_context *cxt, unsigned long size)
{
	cxt->record_size = size;
	cxt->max_record = cxt->size / cxt->record_size;
	if (cxt->next_record > cxt->max_record - 1)
		cxt->next_record = 0;
}

static ssize_t store_record_size(struct mmcoops_context *cxt,
				 const char *buf, size_t count)
{
	unsigned int ret = -EINVAL;
	unsigned long size;

	ret = sscanf(buf, "%lu", &size);
	if (ret != 1)
		return -EINVAL;


	/* Need at least 3 blocks
	 * One block each for the header, kmesg and footer
	 */
	if (size > cxt->size || size < 3)
		return -EINVAL;

	set_record_size(cxt, size);

	return count;
}

attr_ro(start);
attr_ro(size);
attr_rw(record_size);
attr_rw(next_record);
attr_rw(next_counter);
attr_rw(local_version);
attr_rw(soft_version);

static struct attribute *default_attrs[] = {
	&mmc_oops_start.attr,
	&mmc_oops_size.attr,
	&mmc_oops_record_size.attr,
	&mmc_oops_next_record.attr,
	&mmc_oops_next_counter.attr,
	&mmc_oops_local_version.attr,
	&mmc_oops_soft_version.attr,
	NULL
};

#define to_cxt(k) container_of(k, struct mmcoops_context, kobj)
#define to_attr(a) container_of(a, struct oops_attr, attr)


static struct platform_driver mmcoops_driver;

static int mmc_oops_get(struct mmcoops_context *cxt)
{
	if (!try_module_get(mmcoops_driver.driver.owner))
		return 0;

	if (!kobject_get(&cxt->kobj)) {
		module_put(mmcoops_driver.driver.owner);
		return 0;
	}

	return 1;
}

static void mmc_oops_put(struct mmcoops_context *cxt)
{
	kobject_put(&cxt->kobj);
	module_put(mmcoops_driver.driver.owner);
}

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct mmcoops_context *cxt = to_cxt(kobj);
	struct oops_attr *oattr = to_attr(attr);
	ssize_t ret = -EIO;

	spin_lock(&mmc_oops_lock);
	mmc_oops_get(cxt);
	if (oattr->show)
		ret = oattr->show(cxt, buf);
	spin_unlock(&mmc_oops_lock);

	mmc_oops_put(cxt);
	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct mmcoops_context *cxt = to_cxt(kobj);
	struct oops_attr *oattr = to_attr(attr);
	ssize_t ret = -EIO;

	spin_lock(&mmc_oops_lock);
	mmc_oops_get(cxt);
	if (oattr->store)
		ret = oattr->store(cxt, buf, count);
	spin_unlock(&mmc_oops_lock);

	mmc_oops_put(cxt);
	return ret;
}

static void mmc_oops_sysfs_release(struct kobject *kobj)
{
	struct mmcoops_context *cxt = to_cxt(kobj);
	pr_debug("last reference is dropped\n");
	complete(&cxt->kobj_unregister);
}

static const struct sysfs_ops sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_mmc_oops = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
	.release	= mmc_oops_sysfs_release,
};

static int add_sysfs_files(struct mmcoops_context *cxt)
{
	int ret = -EINVAL;

	init_completion(&cxt->kobj_unregister);

	/* prepare interface data */
	ret = kobject_init_and_add(&cxt->kobj, &ktype_mmc_oops,
				   &cxt->card->dev.kobj, "mmc_oops");
	if (ret)
		goto err;

	return 0;
err:
	kobject_put(&cxt->kobj);
	wait_for_completion(&cxt->kobj_unregister);
	dev_err(&cxt->card->dev, "failed to register mmc_oops sysfs hooks\n");
	return ret;
}

static void remove_sysfs_files(struct mmcoops_context *cxt)
{
	struct completion *cmp;

	if (!cxt->card)
		return;

	spin_lock(&mmc_oops_lock);

	cmp = &cxt->kobj_unregister;
	spin_unlock(&mmc_oops_lock);
	kobject_put(&cxt->kobj);

	/* we need to make sure that the underlying kobj is actually
	 * not referenced anymore by anybody before we proceed with
	 * unloading.
	 */
	pr_debug("waiting for dropping of refcount\n");
	wait_for_completion(cmp);
	pr_debug("wait complete\n");
}
#include <linux/mmc/sh_mmcif.h>

#define CMD_SET_RTYP_6B		((0 << 23) | (1 << 22)) /* R1/R1b/R3/R4/R5 */
#define CMD_SET_WDAT		(1 << 19) /* 1: on data, 0: no data */
#define CMD_SET_DWEN		(1 << 18) /* 1: write, 0: read */

enum mmcif_state {
	STATE_IDLE,
	STATE_REQUEST,
	STATE_IOS,
};

struct sh_mmcif_host {
	struct mmc_host *mmc;
	struct mmc_data *data;
	struct platform_device *pd;
	struct clk *hclk;
	unsigned int clk;
	int bus_width;
	int timing;
	bool sd_error;
	long timeout;
	void __iomem *addr;
	struct completion intr_wait;
	enum mmcif_state state;
	spinlock_t lock;
	bool power;
	bool card_present;

	/* DMA support */
	struct dma_chan		*chan_rx;
	struct dma_chan		*chan_tx;
	struct completion	dma_complete;
	bool			dma_active;

	u32 buf_acc;
};

static void mmc_panic_mmc_reset(struct mmcoops_context *cxt)
{
	struct sh_mmcif_host *host = platform_get_drvdata(cxt->pdev);

	sh_mmcif_boot_init(host->addr);

        /* In data transfer mode: Set clock to Bus clock/4 (about 20Mhz) */
        sh_mmcif_writel(host->addr, MMCIF_CE_CLK_CTRL,
					CLK_ENABLE | CLKDIV_2 | SRSPTO_256 |
                        SRBSYTO_29 | SRWDTO_29 | SCCSTO_29);

        /* CMD9 - Get CSD */
        sh_mmcif_boot_cmd(host->addr, 0x09806000, 0x00010000);

        /* CMD7 - Select the card */
        sh_mmcif_boot_cmd(host->addr, 0x07400000, 0x00010000);

        /* CMD16 - Set the block size */
        sh_mmcif_boot_cmd(host->addr, 0x10400000, BLOCK_SIZE_EMMC);
}

static int __mmc_panic_write(struct mmcoops_context *cxt,
			     char *buf, unsigned long start)
{
	int i;
	u32 opc;
	struct sh_mmcif_host *host = platform_get_drvdata(cxt->pdev);

	sh_mmcif_writel(host->addr, MMCIF_CE_BLOCK_SET, 0);
	sh_mmcif_writel(host->addr, MMCIF_CE_BLOCK_SET, BLOCK_SIZE_EMMC);

	/* CMD13 - Status */
	if (sh_mmcif_boot_cmd(host->addr, 0x0d400000, 0x00010000))
		return -1;
	if (sh_mmcif_readl(host->addr, MMCIF_CE_RESP0) != 0x0900)
		return -1;

	/* CMD24 - Write */
	opc = CMD_SET_RTYP_6B | CMD_SET_WDAT | CMD_SET_DWEN | \
		(MMC_WRITE_BLOCK << 24);
	/*	if (sh_mmcif_boot_cmd(host->addr, opc, start * BLOCK_SIZE)) */	
	if (sh_mmcif_boot_cmd(host->addr, opc, start ))
		return -1;
	if (sh_mmcif_readl(host->addr, MMCIF_CE_RESP0) != 0x900)
		return -1;
	if (sh_mmcif_boot_cmd_poll(host->addr, 0x00200000) < 0)
		return -1;

	for (i = 0; i < BLOCK_SIZE_EMMC; i += 4) {
		unsigned long x;
		memcpy(&x, buf + i, 4);
		sh_mmcif_writel(host->addr, MMCIF_CE_DATA, x);
	}
	if (sh_mmcif_boot_cmd_poll(host->addr, 0x00800000) < 0)
		return -1;

	return 0;
}
static void mmc_panic_write(struct mmcoops_context *cxt,
			    char *buf, unsigned long start)
{
	int i = 10;
	
	/* param check */
	if( MMCOOPS_START_OFFSET > start )
	{
		printk(KERN_ERR "%s[%d]: param error not write  start[0x%lx]\n", __func__, __LINE__, start);
		return;
	}
	if( (MMCOOPS_START_OFFSET + MMCOOPS_LOG_SIZE - 1) < start )
	{
		printk(KERN_ERR "%s[%d]: param error not write start[0x%lx]\n", __func__,__LINE__,  start);
		return;
	}

	while (i-- && __mmc_panic_write(cxt, buf, start)) {
		printk("%s: write failed, reset\n", __func__);
		mmc_panic_mmc_reset(cxt);
	}
}
#define MMCREG		15

static bool enable_clock_if_disabled(void)
{
	u32 smstpcr3;
	smstpcr3 = __raw_readl(SMSTPCR3);

	if (!(smstpcr3 & (1 << MMCREG)))
		return false;

	__raw_writel(smstpcr3 & ~(1 << MMCREG), SMSTPCR3);
	return true;
}

static void disable_clock(void)
{
	__raw_writel(__raw_readl(SMSTPCR3) | (1 << MMCREG), SMSTPCR3);
}
static void mmc_log_write(	struct mmcoops_context *cxt,
								unsigned long	offset,
								unsigned long	emmc_log_size,
								const char *s1,
								unsigned long 	l1,
								const char *s2,
								unsigned long 	l2)
{
	int	i = 0;
	unsigned long l1_cpy, l2_cpy;
	
	{
		unsigned long max_size = emmc_log_size * BLOCK_SIZE_EMMC;
		if (l2 > max_size) {
			s2 += l2 - max_size;
			l2 = max_size;
			l1 = 0;
		} else if (l1 + l2 > max_size) {
			s1 += l1 + l2 - max_size;
			l1 = max_size - l2;
		}
	}

	for (i = 1; i < emmc_log_size + 1; i++) {
		if (l1 >= BLOCK_SIZE_EMMC) {
			l1_cpy = BLOCK_SIZE_EMMC;
			l2_cpy = 0;
		} else {
			l1_cpy = l1;
			l2_cpy = min(l2, BLOCK_SIZE_EMMC - l1_cpy);
			if(BLOCK_SIZE_EMMC > l1_cpy + l2_cpy)
				memset(cxt->virt_addr, '\0', BLOCK_SIZE_EMMC);
		}

		memcpy(cxt->virt_addr, s1 , l1_cpy);
		memcpy(cxt->virt_addr + l1_cpy, s2, l2_cpy);

		mmc_panic_write(cxt, cxt->virt_addr, cxt->start + 
			(cxt->next_record * cxt->record_size) + offset + i);

		l1 -= l1_cpy;
		s1 += l1_cpy;
		l2 -= l2_cpy;
		s2 += l2_cpy;
	}
	
	return;
}
#endif //CONFIG_CRASHLOG_EMMC

#ifdef CONFIG_CRASHLOG_DDR
static void ddr_panic_write(char *buf, unsigned long start,
					unsigned long offset)
{
	unsigned char *adr_bak = NULL;
	unsigned int cnt = 0;

	/* param check */
	if (MMCOOPS_START_OFFSET > start) {
		printk(KERN_ERR "%s[%d]: param error not write start[0x%lx]\n",
						__func__, __LINE__, start);
		return;
	}

	adr_bak = (char *)(adr + (offset * BLOCK_SIZE_EMMC));

	for (cnt = 0 ; cnt < BLOCK_SIZE_EMMC ; cnt++) {
		__raw_writeb(*buf, adr_bak);
		adr_bak++;
		buf++;
		}
}
static void mmc_log_write_ddr(	struct mmcoops_context *cxt,
								unsigned long	offset,
								unsigned long	emmc_log_size,
								const char *s1,
								unsigned long 	l1,
								const char *s2,
								unsigned long 	l2)
{
	int	i = 0;
	unsigned long l1_cpy, l2_cpy;
	
	{
		unsigned long max_size = emmc_log_size * BLOCK_SIZE_EMMC;
		if (l2 > max_size) {
			s2 += l2 - max_size;
			l2 = max_size;
			l1 = 0;
		} else if (l1 + l2 > max_size) {
			s1 += l1 + l2 - max_size;
			l1 = max_size - l2;
		}
	}
	for (i = 1; i < emmc_log_size + 1; i++) {
		if (l1 >= BLOCK_SIZE_EMMC) {
			l1_cpy = BLOCK_SIZE_EMMC;
			l2_cpy = 0;
		} else {
			l1_cpy = l1;
			l2_cpy = min(l2, BLOCK_SIZE_EMMC - l1_cpy);
			if(BLOCK_SIZE_EMMC > l1_cpy + l2_cpy)
				memset(cxt->virt_addr, '\0', BLOCK_SIZE_EMMC);
		}

		memcpy(cxt->virt_addr, s1 , l1_cpy);
		memcpy(cxt->virt_addr + l1_cpy, s2, l2_cpy);
		ddr_panic_write(cxt->virt_addr, cxt->start, offset + i);
		l1 -= l1_cpy;
		s1 += l1_cpy;
		l2 -= l2_cpy;
		s2 += l2_cpy;
	}
	
	return;
}
#endif //CONFIG_CRASHLOG_DDR


static unsigned char dump_cnt = 0;

static void mmcoops_do_dump(struct kmsg_dumper *dumper,
		enum kmsg_dump_reason reason, const char *s1, unsigned long l1,
		const char *s2, unsigned long l2)
{
	struct mmcoops_context *cxt = container_of(dumper,
			struct mmcoops_context, dump);
#ifdef CONFIG_CRASHLOG_EMMC
	struct mmc_card *card = cxt->card;
	bool clock_enabled;
#endif
#ifdef CONFIG_ARM_TZ
	bool status = SEC_HAL_CMN_RES_FAIL;
#endif
	int count = 0, loop_cnt = 0;
	size_t		head = 0;	
	size_t		w_off = 0;	
	size_t		size = 0;
	unsigned long	offset = 0;
	unsigned char	*pbuf = NULL;
	const char	*pstr1 = NULL;
	unsigned long	strlen1 = 0;
	const char	*pstr2 = NULL;
	unsigned long	strlen2 = 0;

#ifdef CONFIG_CRASHLOG_EMMC
	if (!card)
		return;
#endif
	/* Only dump oopses if dump_oops is set */
	if ((reason != KMSG_DUMP_PANIC) && !dump_oops)
		return;

	rmu2_cmt_stop();

	if (cxt->size < cxt->record_size)
		return;
	spin_lock(&mmc_oops_lock);

	if(dump_cnt != 0){
		spin_unlock(&mmc_oops_lock);
		while(1);
	}
	dump_cnt++;
	spin_unlock(&mmc_oops_lock);

#ifdef CONFIG_CRASHLOG_EMMC
	/* WA to avoid BUG_ON message,
	 * when we are in panic and try to schedule
	 */
	while (card->host->claimed) {
		printk(KERN_ERR "host was already claimed, releasing it"
				"forcefully, %s %d\n", __func__, __LINE__);
		mmc_release_host(card->host);
	}
	printk("%s[%d] mmc_claim_host() [start]\n", __func__, __LINE__);
	mmc_claim_host(card->host);
	printk("%s[%d] mmc_claim_host() [ end ]\n", __func__, __LINE__);

	disable_irq(platform_get_irq(cxt->pdev, 0));
	disable_irq(platform_get_irq(cxt->pdev, 1));

	clock_enabled = enable_clock_if_disabled();
	mmc_panic_mmc_reset(cxt);

	/* Header data check */
	if (MMCOOPS_RECORD_CNT <= cxt->next_record) {
		printk(KERN_ERR "mmc_oops next_record value error [%lu]\n",
							cxt->next_record);
		/* Error 0 reset */
		cxt->next_record = 0;
	}
#endif /* CONFIG_CRASHLOG_EMMC */
	/* Record ((cxt->record-size * 2) kbytes)
	 * +--------------------------------------------+
	 * | counter (4 bytes)				| [ Header ]
	 * +--------------------------------------------+
	 * | local version (4 bytes)			| [ Header ]
	 * +--------------------------------------------+
	 * | soft version (32 bytes)			| [ Header ]
	 * +--------------------------------------------+
	 * | timestamp secs (8 bytes)			| [ Header ]
	 * +--------------------------------------------+
	 * | timestamp nsec (4 bytes)			| [ Header ]
	 * +--------------------------------------------+
	 * | reserved (460 bytes)			| [ Header ]
	 * +--------------------------------------------+
	 * | kmesg (((cxt->record-size * 2) - 1) kbytes)|
	 * +--------------------------------------------+
	 * | logcat main ((cxt->logcat_main_size * 2) kbytes)|
	 * +--------------------------------------------+
	 * | logcat system ((cxt->logcat_system_size * 2) kbytes)|
	 * +--------------------------------------------+
	 * | logcat radio ((cxt->logcat_radio_size * 2) kbytes)|
	 * +--------------------------------------------+
	 * | logcat events ((cxt->logcat_events_size * 2) kbytes)|
	 * +--------------------------------------------+
	 * | reserved (508 bytes)			| [ Footer ]
	 * +--------------------------------------------+
	 * | counter (4 bytes)				| [ Footer ]
	 * +--------------------------------------------+
	 */

	count = 0;
	memset(cxt->virt_addr, '\0', BLOCK_SIZE_EMMC);
	add_to_buf(cxt->virt_addr, count, &cxt->next_counter,
		   sizeof(cxt->next_counter));
	add_to_buf(cxt->virt_addr, count, &cxt->local_version,
	           sizeof(cxt->local_version));
	add_to_buf(cxt->virt_addr, count, cxt->soft_version,
	           sizeof(cxt->soft_version));
	{
		struct timespec now;
		u64 sec;
		u32 nsec;

		getnstimeofday(&now);
		sec = (u64) now.tv_sec;
		nsec = (u32) now.tv_nsec;
		add_to_buf(cxt->virt_addr, count, &sec, sizeof(sec));
		add_to_buf(cxt->virt_addr, count, &nsec, sizeof(nsec));
	}


#ifdef CONFIG_ARM_TZ
	/*secure reset reason*/
	if (reset_info != NULL)
		status = sec_hal_reset_info_get(reset_info);
	if (SEC_HAL_CMN_RES_OK == status) {
		if (reset_info->interrupt_addr != 0x00) {
			add_to_buf(cxt->virt_addr, count,
				&reset_info->hw_reset_type,
				sizeof(reset_info->hw_reset_type));
			add_to_buf(cxt->virt_addr, count, &reset_info->reason,
						sizeof(reset_info->reason));
			add_to_buf(cxt->virt_addr, count,
				&reset_info->interrupt_addr,
				sizeof(reset_info->interrupt_addr));

			if (reset_info->hw_reset_type == POWER_UP_RESET)
				printk(KERN_INFO
					"Hw_Reset_Type:Power up Reset\n");
			else if (reset_info->hw_reset_type == SOFT_RESET)
				printk(KERN_INFO "Hw_Reset_Type:Soft Reset\n");

			if (reset_info->reason == SEC_RESET_CMT1_5_EXPIRED ||
					reset_info->reason == 0x00) {
				printk(KERN_INFO
					"Reset_Reason:CMT1_5 Expired\n");
			}

			printk(KERN_INFO
				"FIQ_return_addr:%x\n",
				reset_info->interrupt_addr);
		}
	} else {
			printk(KERN_ERR "%s[%d]: Failed to read reset reason\n",
							__func__, __LINE__);
	}
#endif /*CONFIG_ARM_TZ*/


#ifdef CONFIG_CRASHLOG_EMMC
	mmc_panic_write(cxt, cxt->virt_addr, cxt->start +
			(cxt->next_record * cxt->record_size));
#endif

#ifdef CONFIG_CRASHLOG_DDR
	ddr_panic_write(cxt->virt_addr, cxt->start, 0);
#endif

	/* kmsg is written in emmc */
#ifdef CONFIG_CRASHLOG_EMMC
	mmc_log_write(cxt, 0, cxt->kmsg_size, s1, l1, s2, l2);
#endif
#ifdef CONFIG_CRASHLOG_DDR
	mmc_log_write_ddr(cxt, 0, cxt->kmsg_size, s1, l1, s2, l2);
#endif

	/* logcat is written in emmc */
	offset = cxt->kmsg_size;
	for(loop_cnt = 0; loop_cnt < 4; loop_cnt++){
		get_logcat_bufinfo(cxt->logcat[loop_cnt].name, &pbuf, &w_off, &head, &size);
		if(NULL != pbuf){
			if(w_off > head){
				pstr1 = pbuf + head;
				strlen1 = w_off - head;
				pstr2 = pbuf;
				strlen2 = 0;
			}else{
				pstr1 = pbuf + head;
				strlen1 = size - head;
				pstr2 = pbuf;
				strlen2 = w_off;
			}
			
#ifdef CONFIG_CRASHLOG_EMMC
			mmc_log_write(cxt, offset,
					cxt->logcat[loop_cnt].log_size, pstr1,
					strlen1, pstr2, strlen2);
#endif

#ifdef CONFIG_CRASHLOG_DDR
			mmc_log_write_ddr(cxt, offset,
				cxt->logcat[loop_cnt].log_size, pstr1,
				strlen1, pstr2, strlen2);
#endif
			
			offset += cxt->logcat[loop_cnt].log_size;
		}
		else
		{
			printk(KERN_ERR "mmc_oops logcat get buf memory failed [%s]\n",cxt->logcat[loop_cnt].name);
			offset += cxt->logcat[loop_cnt].log_size;
		}
	}
#ifdef CONFIG_CRASHLOG_EMMC
	count = BLOCK_SIZE_EMMC - sizeof(cxt->next_counter);
	memset(cxt->virt_addr, '\0', BLOCK_SIZE_EMMC);
	add_to_buf(cxt->virt_addr, count, &cxt->next_counter,
		   sizeof(cxt->next_counter));
	mmc_panic_write(cxt, cxt->virt_addr, cxt->start +
			((cxt->next_record + 1) * cxt->record_size) - 1);

	cxt->next_record = (cxt->next_record + 1) % cxt->max_record;

	cxt->next_counter++;
	if (clock_enabled)
		disable_clock();

	enable_irq(platform_get_irq(cxt->pdev, 0));
	enable_irq(platform_get_irq(cxt->pdev, 1));
	mmc_release_host(card->host);
#endif /* CONFIG_CRASHLOG_EMMC */
	printk(KERN_INFO "mmc_oops dump complete\n");
}

#ifdef CONFIG_CRASHLOG_EMMC
static int match_card(struct mmcoops_context *cxt, struct mmc_card *card)
{
	return ((mmc_card_mmc(card) || mmc_card_sd(card)) &&
		mmc_dev(card->host) == cxt->mmc_dev);
}

static int mmc_oops_probe(struct mmc_card *card)
{
	struct mmcoops_context *cxt = &oops_cxt;

	if (!match_card(cxt, card))
		return -ENODEV;

	cxt->card = card;

	if (add_sysfs_files(cxt)) {
		cxt->card = NULL;
		return -ENODEV;
	}

	printk("%s[%d] %s\n", __func__, __LINE__, mmc_hostname(card->host));

	/* Return non-zero to allow another driver to bind to this device */
	printk("%s[%d] %s: Do not eject card\n",
	       __func__, __LINE__, mmc_hostname(card->host));
	return -ENODEV;
}

/*
 * Note this does not get called as mmc_oops_probe() always
 * returns non-zero. This means that mmc_oops should only
 * be used with non-ejectable (or at least not ejected) media.
 */
static void mmc_oops_remove(struct mmc_card *card)
{
	struct mmcoops_context *cxt = &oops_cxt;

	if (!match_card(cxt, card))
		return;

	remove_sysfs_files(cxt);

	cxt->card = NULL;
}

static struct mmc_driver mmc_driver = {
	.drv		= {
		.name	= "mmc_oops",
	},
	.probe		= mmc_oops_probe,
	.remove		= mmc_oops_remove,
};
#endif /* CONFIG_CRASHLOG_EMMC */

static int __init mmcoops_probe(struct platform_device *pdev)
{
	struct mmcoops_platform_data *pdata = pdev->dev.platform_data;
	struct mmcoops_context *cxt = &oops_cxt;
	unsigned long record_size;
	int err = -EINVAL;

	if (!pdata) {
		dev_warn(&pdev->dev, "No platform data\n");
		return -EINVAL;
	}

	record_size = pdata->record_size ? pdata->record_size : RECORD_SIZE;
	if (pdata->size < record_size)
		dev_warn(&pdev->dev, "mmcoops size is less than the default record size\n");
	cxt->size = pdata->size;

#ifdef CONFIG_CRASHLOG_EMMC
	set_record_size(cxt, record_size);
	cxt->mmc_dev = &pdata->pdev->dev;
	cxt->pdev = pdata->pdev;
	cxt->next_record = 0;
	cxt->next_counter = 0;
#endif
	cxt->start = pdata->start;
	cxt->kmsg_size = pdata->kmsg_size;
	
	cxt->logcat[0].name = LOGGER_LOG_MAIN;
	cxt->logcat[0].log_size = pdata->logcat_main_size;

	cxt->logcat[1].name = LOGGER_LOG_SYSTEM;
	cxt->logcat[1].log_size = pdata->logcat_system_size;

	cxt->logcat[2].name = LOGGER_LOG_RADIO;
	cxt->logcat[2].log_size = pdata->logcat_radio_size;

	cxt->logcat[3].name = LOGGER_LOG_EVENTS;
	cxt->logcat[3].log_size = pdata->logcat_events_size;


	cxt->local_version = pdata->local_version;
	memcpy(cxt->soft_version, pdata->soft_version,
	       sizeof(cxt->soft_version));
#ifdef CONFIG_CRASHLOG_EMMC
	
	/* FIXME how to get the given mmc deivce instead of bind/unbind */
	err = mmc_register_driver(&mmc_driver);
	if (err)
		return err;
	cxt->card = NULL;
#endif
#ifdef CONFIG_CRASHLOG_DDR
	cxt->next_counter = 1;
	cxt->record_size = record_size;
#endif

	cxt->virt_addr = kmalloc(BLOCK_SIZE_EMMC, GFP_KERNEL);
	if (!cxt->virt_addr)
		goto kmalloc_failed;

	cxt->dump.dump = mmcoops_do_dump;
	err = kmsg_dump_register(&cxt->dump);
	if (err) {
		printk(KERN_ERR "mmcoops: registering kmsg dumper failed");
		goto kmsg_dump_register_failed;
	}

#ifdef CONFIG_CRASHLOG_DDR
	adr = ioremap(MMCOOPS_START_OFFSET, MAX_LOG_SIZE_ON_DDR);

	if (adr == NULL) {
		printk(KERN_ERR "ioremap for MMCOOPS_START_OFFSET is failed\n");
		err = -ENOMEM;
		goto ioremap_failed;
	}
#endif

#ifdef CONFIG_ARM_TZ
	reset_info = kmalloc(sizeof(sec_reset_info), GFP_KERNEL);
	if (NULL == reset_info) {
		printk(KERN_ERR "kmalloc failed in function %s at line %d\n",
						__func__, __LINE__);
	}
#endif

	return err;

#ifdef CONFIG_CRASHLOG_DDR
ioremap_failed:
	kmsg_dump_unregister(&cxt->dump);
#endif
kmsg_dump_register_failed:
	kfree(cxt->virt_addr);
kmalloc_failed:
#ifdef CONFIG_CRASHLOG_EMMC
	mmc_unregister_driver(&mmc_driver);
#endif
	return err;
}

static int __exit mmcoops_remove(struct platform_device *pdev)
{
	struct mmcoops_context *cxt = &oops_cxt;

	if (kmsg_dump_unregister(&cxt->dump) < 0)
		printk(KERN_WARNING "mmcoops: colud not unregister kmsg dumper");
	kfree(cxt->virt_addr);

#ifdef CONFIG_CRASHLOG_DDR
	if (adr)
		iounmap(adr);
#endif

#ifdef CONFIG_CRASHLOG_EMMC
	mmc_unregister_driver(&mmc_driver);
#endif

	return 0;
}

static struct platform_driver mmcoops_driver = {
	.remove			= __exit_p(mmcoops_remove),
	.probe			= mmcoops_probe,
	.driver			= {
		.name		= "mmcoops",
		.owner		= THIS_MODULE,
	},
};

static int __init mmcoops_init(void)
{
	return platform_driver_register(&mmcoops_driver);
}

static void __exit mmcoops_exit(void)
{
	platform_driver_unregister(&mmcoops_driver);
}

subsys_initcall(mmcoops_init);
module_exit(mmcoops_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kyungmin Park <kyungmin.park@samsung.com>");
MODULE_DESCRIPTION("MMC Oops/Panic Looger");
