/*
 * pmdbg_core.c
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
#include "pmdbg_core.h"
#include <linux/interrupt.h>
#include <linux/irq.h>

/* Module header*/
MODULE_DESCRIPTION("PM Debug information");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Renesas Mobile Corp.");

DECLARE_MOD(hw);
DECLARE_MOD(sysc);
DECLARE_MOD(cpg);
DECLARE_MOD(pdc);
DECLARE_MOD(dfs);
DECLARE_MOD(idle);
DECLARE_MOD(suspend);
DECLARE_MOD(mem);
DECLARE_MOD(dbgpin);
DECLARE_MOD(cpu);

struct pmdbg_mod *pmdbg_mod_list[] = {
	&MOD_OBJ(hw),
	&MOD_OBJ(sysc),
	&MOD_OBJ(cpg),
	&MOD_OBJ(pdc),
	&MOD_OBJ(dfs),
	&MOD_OBJ(idle),
	&MOD_OBJ(suspend),
	&MOD_OBJ(mem),
	&MOD_OBJ(dbgpin),
	&MOD_OBJ(cpu),
};

static int __init init_pmdbg(void);
static void __exit exit_pmdbg(void);

#ifndef CONFIG_ARM_TZ
/* for TDBG interrupt */
static irqreturn_t TDBG_request_irq(int irq, void *dev_id);
static unsigned int pmdbg_has_irq;
static int pmdbg_irq = -1;
#endif /*CONFIG_ARM_TZ*/
#define TDBG_SPI 81U

/*APIs*/
struct kobject *pmdbg_kobj;

/* Register debug module*/
int register_mod(struct pmdbg_mod *mod)
{
	int ret = 0;
	FUNC_MSG_IN;

	ret = sysfs_create_file(pmdbg_kobj, mod->attr);
	if (ret) {
		MSG("sysfs_create_file: failed (%d)", ret);
		goto end;
	}

end:
	FUNC_MSG_RET(ret);
}

/* Unregister debug module*/
int unregister_mod(struct pmdbg_mod *mod)
{
	FUNC_MSG_IN;

	sysfs_remove_file(pmdbg_kobj, mod->attr);

	FUNC_MSG_RET(0);
}

int run_cmd(const char *buf, int size, struct list_head *cmd_lst)
{
	char cmd_name[CMD_SIZE];
	char para[PAR_SIZE];
	int ret = 0;
	int cmd_sz = 0;
	int para_sz = 0;
	struct pmdbg_cmd *cmd;
	FUNC_MSG_IN;

	ret = parse_param(buf, size, cmd_name, &cmd_sz, para, &para_sz);
	CHK_JUMP(ret, end);
	list_for_each_entry(cmd, cmd_lst, entry) {
		if (0 == strncmp(cmd_name, cmd->name, cmd_sz)) {
			ret = cmd->run(para, para_sz);
			break;
		}
		ret = -EINVAL;
	}
end:
	FUNC_MSG_RET(ret);
}

/* Register command for debug module*/
int register_cmd(struct list_head *cmd_lst, struct pmdbg_cmd *cmd)
{
	int ret = 0;

	FUNC_MSG_IN;

	list_add_tail(&cmd->entry, cmd_lst);

	FUNC_MSG_RET(ret);
}

/* Unregister command for debug module*/
int unregister_cmd(struct list_head *cmd_lst, struct pmdbg_cmd *cmd)
{
	int ret = 0;

	FUNC_MSG_IN;

	list_del_init(&cmd->entry);

	FUNC_MSG_RET(ret);
}

int parse_param(const char *buf, int size, char *cmd,
				int *cmd_sz, char *par, int *para_sz)
{
	int ret = 0;
	int i = 0;
	int cnt = 0;

	FUNC_MSG_IN;
	do {
		if (buf[i] == ' ' || buf[i] == '\n' || i >= size) {
			*cmd_sz = cnt;
			if (cnt < size) {
				*para_sz = size - cnt - 1;
				memcpy(par, &buf[i+1], *para_sz);
			}
			break;
		} else
			cmd[cnt] = buf[i];

		i++;
		cnt++;
	} while (i < size);
	FUNC_MSG_RET(ret);
}

int get_word(const char *buf, int size, int pos, char *out, int *out_sz)
{
	int i = pos;
	int cnt = 0;

	FUNC_MSG_IN;
	memset(out, 0, PAR_SIZE);
	do {
		if (IS_CHAR(buf[i]) && !IS_CHAR_SPACE(buf[i])) {
			out[cnt] = buf[i];
			cnt++;
		} else {
			if (cnt != 0)
				break;
		}
		i++;
	} while (i < size);
	*out_sz = cnt;
	FUNC_MSG_RET(i);

}

#ifndef CONFIG_ARM_TZ
/* TDBG-IRQ Initialization for DEBUG LINKUP REQUEST */
/* Following code is available in NON-TRUSTZONE Mode  */
static void init_TDBG_interrupt(void)
{
	int r;
	pmdbg_irq = gic_spi(TDBG_SPI);
	set_irq_flags(pmdbg_irq, IRQF_VALID | IRQF_NOAUTOEN);
	r = request_irq(pmdbg_irq, TDBG_request_irq, IRQF_DISABLED,
					"DEBUG_LINKUP_REQ", NULL);
	if (0 > r) {
		free_irq(pmdbg_irq, NULL);
		pmdbg_has_irq = 0;
	} else {
		pmdbg_has_irq = 1;
		enable_irq(pmdbg_irq);
	}
}

/* TDBG-IRQ interrupt handler */
/* Following code is available in NON-TRUSTZONE Mode  */
static irqreturn_t TDBG_request_irq(int irq, void *dev_id)
{
	pmdbg_dbgpin_to_dbgmode();
	/* Disable interrupt of TDBG because it requires only ONCE. */
	disable_irq_nosync(irq);
	return IRQ_HANDLED;
}
#endif

static int __init init_pmdbg(void)
{
	int ret = 0;
	int i = 0;
	FUNC_MSG_IN;

	pmdbg_kobj = kobject_create_and_add("pmdbg", NULL);
	if (!pmdbg_kobj) {
		ret = -ENOMEM;
		goto end;
	}
	for (i = 0; i < ARRAY_SIZE(pmdbg_mod_list); i++) {
		register_mod(pmdbg_mod_list[i]);
		pmdbg_mod_list[i]->init();

	}
#ifndef CONFIG_ARM_TZ
	init_TDBG_interrupt();
#endif

end:
	FUNC_MSG_RET(ret);
	return ret;
}


static void __exit exit_pmdbg(void)
{
	int i = 0;
	FUNC_MSG_IN;

	for (i = 0; i < ARRAY_SIZE(pmdbg_mod_list); i++) {
		pmdbg_mod_list[i]->exit();
		unregister_mod(pmdbg_mod_list[i]);

	}
	if (pmdbg_kobj) {
		kobject_del(pmdbg_kobj);
		pmdbg_kobj = NULL;
	}
#ifndef CONFIG_ARM_TZ
	if (pmdbg_has_irq) {
		free_irq(pmdbg_irq, NULL);
		pmdbg_has_irq = 0;
	}
#endif
	FUNC_MSG_OUT;
}


module_init(init_pmdbg);
module_exit(exit_pmdbg);

