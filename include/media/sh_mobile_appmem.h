/*
 * Function        : Composer/V4L2 driver for R Mobile
 *
 * Copyright (C) 2011-2012 Renesas Electronics Corporation
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
 * Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */


#ifndef __SH_MOBILE_APPMEM_H_
#define __SH_MOBILE_APPMEM_H_
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_APPMEM_SHAREMANGER
/*===============================================================*/
/* include                                                     */
/*===============================================================*/
#include <linux/mm.h>

/*===============================================================*/
/* structure                                                     */
/*===============================================================*/
struct appmem_handle {
	void *memhandle;
	int   app_id;
	int   offset;
	char  *key;

	/* following argument is reserved */
	struct list_head list;
	int              size;
	unsigned char    *vaddr;
	unsigned long    rtaddr;
	int              ref_count;

	unsigned int     op_apaddr;
	struct page      **op_pages;
};

/*===============================================================*/
/* external functions                                            */
/*===============================================================*/
extern struct appmem_handle *sh_mobile_appmem_alloc(int size, char *key);
extern struct appmem_handle *sh_mobile_appmem_share(int appid, char *key);
extern unsigned char *sh_mobile_appmem_getaddress( \
	struct appmem_handle *hndle, int offset);
extern unsigned long sh_mobile_appmem_getRTaddress(\
	struct appmem_handle *appmem, unsigned char *vadr);
extern int sh_mobile_appmem_free(struct appmem_handle *hndle);

extern void sh_mobile_appmem_debugmode(int mode);

/*===============================================================*/
/* define                                                        */
/*===============================================================*/

#define sh_mobile_appmem_getmemoryhandle(HANDLE)  \
	((HANDLE) ? ((HANDLE)->memhandle) : (void *)0)

/*===============================================================*/
/* structure                                                     */
/*===============================================================*/
struct rtmem_phys_handle {
	/* following argument is reserved */
	struct list_head list;

	int              size;
	unsigned long    rt_addr;
	unsigned long    phys_addr;
};

/*===============================================================*/
/* external functions                                            */
/*===============================================================*/
extern struct rtmem_phys_handle *sh_mobile_rtmem_physarea_register(
	int size, unsigned long addr);
extern void sh_mobile_rtmem_physarea_unregister(
	struct rtmem_phys_handle *handle);
extern unsigned long sh_mobile_rtmem_conv_phys2rtmem(unsigned long addr);
extern unsigned long sh_mobile_rtmem_conv_rt2physmem(unsigned long addr);

#endif
/* end CONFIG_MISC_R_MOBILE_COMPOSER_APPMEM_SHAREMANGER */
#endif
/* end __SH_MOBILE_APPMEM_H_ */

