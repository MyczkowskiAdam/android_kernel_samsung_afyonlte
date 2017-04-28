/*
 * pmdbg_pdc.c
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

#include "pmdbg_api.h"
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/errno.h>
#include <mach/pm.h>
#include <linux/io.h>

static int lst_cmd(char *, int);
static int set_cmd(char *, int);
static int pdc_init(void);
static void pdc_exit(void);

LOCAL_DECLARE_MOD(pdc, pdc_init, pdc_exit);

DECLARE_CMD(lst, lst_cmd);
DECLARE_CMD(set, set_cmd);


static int power_areas_info(char *buf)
{
	u32 reg_val;
	char *s = buf;
	FUNC_MSG_IN;

	reg_val = __raw_readl(PSTR);
	s += sprintf(s, "Power Areas Info:\n");
	s += sprintf(s, "PSTR(0x%p) = 0x%08x\n",
		PSTR, reg_val);
	s += sprintf(s, "A3SG = %s\n",
		(POWER_A3SG & reg_val) ? "ON" : "OFF");
	s += sprintf(s, "A3SP = %s\n",
		(POWER_A3SP & reg_val) ? "ON" : "OFF");
	s += sprintf(s, "A3R = %s\n",
		(POWER_A3R & reg_val) ? "ON" : "OFF");
	s += sprintf(s, "A4RM = %s\n",
		(POWER_A4RM & reg_val) ? "ON" : "OFF");
	s += sprintf(s, "A4MP = %s\n",
		(POWER_A4MP & reg_val) ? "ON" : "OFF");

	FUNC_MSG_RET(s-buf);
}

static int power_client_info(char *buf)
{
	char *s = buf;
	FUNC_MSG_IN;
	FUNC_MSG_RET(s-buf);
}

static int power_all_info(char *buf)
{
	char *s = buf;
	FUNC_MSG_IN;
	s += power_areas_info(s);
	s += power_client_info(s);
	FUNC_MSG_RET(s-buf);
}

/* lst <type>
 * type:
 * - pa: list of power areas status
 * - client: list of supported clients
 * - all: both pa and client
 * */
int lst_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int pos = 0;
	FUNC_MSG_IN;
	para = strim(para);

	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret =  -EINVAL;
		goto fail;
	}
	pos = ret;
	ret = strncmp(item, "pa", sizeof("pa"));
	if (0 == ret) {
		(void)power_areas_info(bufres);
		goto end;
	}
	ret = strncmp(item, "client", sizeof("client"));
	if (0 == ret) {
		(void)power_client_info(bufres);
		goto end;
	}

	ret = strncmp(item, "all", sizeof("all"));
	if (0 == ret) {
		(void)power_all_info(bufres);
		goto end;
	}

	ret = -EINVAL;

fail:
	sprintf(bufres, "FAILED");
end:
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}

/* set (f) <on/off> <pa> */
int set_cmd(char *para, int size)
{
	int ret = 0;
	FUNC_MSG_IN;

	sprintf(bufres, "Not support");

	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}


int pdc_init(void)
{
	int ret = 0;
	FUNC_MSG_IN;
	ADD_CMD(pdc, lst);
	ADD_CMD(pdc, set);

	FUNC_MSG_RET(ret);
}

void pdc_exit(void)
{

	FUNC_MSG_IN;
	DEL_CMD(pdc, lst);
	DEL_CMD(pdc, set);

	FUNC_MSG_OUT;
}



