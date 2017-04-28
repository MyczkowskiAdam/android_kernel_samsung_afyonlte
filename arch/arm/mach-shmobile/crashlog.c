/*
 * arch/arm/mach-shmobile/crashlog.c
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <asm/io.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/ioport.h>

#include <mach/crashlog.h>
#include <mach/r8a7373.h>

#ifndef CONFIG_IRQ_TRACE
char *tmplog_nocache_address;
#endif

void crashlog_r_local_ver_write(char *soft_version)
{
	void __iomem *adr = 0;
	void __iomem *adr_bak = 0;
	char	version_name[CRASHLOG_R_LOCAL_VER_LENGTH];
	int	revision_version_length;
	unsigned char i;

	adr = ioremap(CRASHLOG_R_LOCAL_VER_LOCATE,
				CRASHLOG_R_LOCAL_VER_LENGTH);
	adr_bak = adr;
	if (adr) {
		revision_version_length = strlen(linux_banner);
		snprintf(soft_version, CRASHLOG_R_LOCAL_VER_LENGTH, "%s %s",
				RMC_LOCAL_VERSION,
				(linux_banner + revision_version_length - 25));

		strncpy(version_name, RMC_LOCAL_VERSION,
				CRASHLOG_R_LOCAL_VER_LENGTH);

		for (i = 0 ; i < CRASHLOG_R_LOCAL_VER_LENGTH ; i++) {
			__raw_writeb(version_name[i], adr);
			adr++;
		}

		iounmap(adr_bak);
	}
}

void crashlog_reset_log_write()
{
	void __iomem *adr = NULL;
	u8 reg = 0;

	crashlog_kmsg_init();
	crashlog_logcat_init();

	/* kmsg */
	/* printk(KERN_ERR "log_buf_address=0x%08x\n", log_buf_address); */
	adr = ioremap(CRASHLOG_KMSG_LOCATE, 16);
	if (adr == NULL) {
		printk(KERN_ERR "crashlog reset log initialization failed\n");
		return;
	}
	__raw_writel(log_buf_address, adr);
	__raw_writel(log_buf_len_address, adr + 4);
	__raw_writel(log_end_address, adr + 8);
	__raw_writel(logged_chars_address, adr + 12);
	iounmap(adr);

	/* log_cat_main */
	/* printk(KERN_ERR "log_main_buffer_address=0x%08x\n",
				log_main_buffer_address); */
	adr = ioremap(CRASHLOG_LOGCAT_MAIN_LOCATE, 16);
	if (adr == NULL) {
		printk(KERN_ERR "crashlog reset log initialization failed\n");
		return;
	}
	__raw_writel(log_main_buffer_address, adr);
	__raw_writel(log_main_size_address, adr + 4);
	__raw_writel(log_main_w_off_address, adr + 8);
	__raw_writel(log_main_head_address, adr + 12);
	iounmap(adr);

	/* log_cat_events */
	/* printk(KERN_ERR "log_events_buffer_address=0x%08x\n",
				log_events_buffer_address); */
	adr = ioremap(CRASHLOG_LOGCAT_EVENT_LOCATE, 16);
	if (adr == NULL) {
		printk(KERN_ERR "crashlog reset log initialization failed\n");
		return;
	}
	__raw_writel(log_events_buffer_address, adr);
	__raw_writel(log_events_size_address, adr + 4);
	__raw_writel(log_events_w_off_address, adr + 8);
	__raw_writel(log_events_head_address, adr + 12);
	iounmap(adr);

	/* log_cat_radio */
	/* printk(KERN_ERR "log_radio_buffer_address=0x%08x\n",
				log_radio_buffer_address); */
	adr = ioremap(CRASHLOG_LOGCAT_RADIO_LOCATE, 16);
	if (adr == NULL) {
		printk(KERN_ERR "crashlog reset log initialization failed\n");
		return;
	}
	__raw_writel(log_radio_buffer_address, adr);
	__raw_writel(log_radio_size_address, adr + 4);
	__raw_writel(log_radio_w_off_address, adr + 8);
	__raw_writel(log_radio_head_address, adr + 12);
	iounmap(adr);

	/* log_cat_system */
	adr = ioremap(CRASHLOG_LOGCAT_SYSTEM_LOCATE, 16);
	if (adr == NULL) {
		printk(KERN_ERR "crashlog reset log initialization failed\n");
		return;
	}
	__raw_writel(log_system_buffer_address, adr);
	__raw_writel(log_system_size_address, adr + 4);
	__raw_writel(log_system_w_off_address, adr + 8);
	__raw_writel(log_system_head_address, adr + 12);
	iounmap(adr);

	/* andriod init */
	reg = __raw_readb(STBCHR2);
	__raw_writeb((reg | APE_RESETLOG_INIT_COMPLETE), STBCHR2);

	/*Developer option to debug Reset Log*/
	/*	reg = __raw_readb(STBCHR3);*/
	/*	__raw_writeb((reg | APE_RESETLOG_DEBUG), STBCHR3);*/
}

void crashlog_init_tmplog(void)
{
#ifndef CONFIG_IRQ_TRACE
	if (request_mem_region(TMPLOG_ADDRESS, TMPLOG_TOTAL_SIZE,
					"tmplog-nocache")) {
		tmplog_nocache_address = (char __force *)
				ioremap_nocache(TMPLOG_ADDRESS,
						TMPLOG_TOTAL_SIZE);
		memcpy(tmplog_nocache_address, "CrashLog Temporary Area" , 24);
	}

	/* write STBCHR3 for debug*/
	/*	reg = __raw_readb(STBCHR3);*/
	/*	__raw_writeb((reg | APE_RESETLOG_TMPLOG_END), STBCHR3); */
#else
	void __iomem *adr;
	void __iomem *tmp;
	u8 reg = 0;

	if (tmplog_nocache_address == 0) {
		/* Request Trace Log Memory region for I/O remapping */
		request_mem_region(TMPLOG_ADDRESS, TMPLOG_TOTAL_SIZE,
							 "tmplog-nocache");
		tmp = ioremap_nocache(TMPLOG_ADDRESS, TMPLOG_TOTAL_SIZE);
		for (adr = tmp; adr < (tmp+TMPLOG_TOTAL_SIZE); adr += 4)
			__raw_writel((unsigned int __force)adr+0x10000000, adr);
		tmplog_nocache_address = (char __force *)tmp;
	}
	reg = __raw_readb(STBCHR3);
	/* Set bit APE_RESETLOG_TRACELOG of STBCHR3 for tracelog */
	__raw_writeb((reg | APE_RESETLOG_TRACELOG), STBCHR3);
#endif /* CONFIG_IRQ_TRACE */
	return;
}
