/*
 * pmdbg_core.h
 *
 * Copyright (C) 2011-2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __TST_PM_CORE__
#define __TST_PM_CORE__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sysfs.h>

#include "pmdbg_api.h"
#include <linux/pm.h>
#include <mach/irqs.h>


#define CHK_JUMP(_ret, _label)	\
			do { \
				if (_ret)\
					goto _label;\
			} while (0);

#define IS_CHAR(c)		((c >= 32) && (c <= 126))
#define IS_DIGIT(c)		((c >= 48) && (c <= 57))
#define IS_ALPHABET(c)	(((c >= 65) && (c <= 90)) || ((c >= 97) && (c <= 122)))
#define IS_CHAR_SPACE(c)	(c == 32)
#define TO_LOWER(c)		(((c >= 65) && (c <= 90)) ? (c + 32) : c)
#define TO_UPPER(c)		(((c >= 97) && (c <= 22)) ? (c - 32) : c)

int parse_param(const char *buf, int size, char *cmd,
			int *cmd_sz, char *par, int *para_sz);

extern void pmdbg_dbgpin_to_dbgmode(void);

#endif /*__TST_PM_CORE__*/
