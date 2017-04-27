/*
 * pmdbg_api.h
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
#ifndef __TST_PM_API__
#define __TST_PM_API__

#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/errno.h>

#define MOD_SIZE	32
#define RES_SIZE	512
#define CMD_SIZE	32
#define PAR_SIZE	128

/*#define ENABLE_DBG_MSG*/
#define ENABLE_MSG_INFO

#ifdef ENABLE_DBG_MSG

#define MSG(...)	\
	do {\
		printk(KERN_INFO "TST: " __VA_ARGS__);\
		printk(KERN_INFO "\n"); \
	} while (0);

#else /*!ENABLE_DBG_MSG*/

#define MSG(...)

#endif /*ENABLE_DBG_MSG*/

#ifdef ENABLE_MSG_INFO

#define MSG_INFO(...)	\
	do { \
		printk(KERN_INFO __VA_ARGS__);\
		printk(KERN_INFO "\n"); \
	} while (0);

#else /*!ENABLE_MSG_INFO*/

#define MSG_INFO(...)

#endif /*ENABLE_MSG_INFO*/

#define FUNC_MSG_IN			MSG(">> %s", __func__)
#define FUNC_MSG_OUT		MSG("<< %s", __func__)
#define FUNC_MSG_RET(ret)	\
	do { \
		MSG("<< %s (%d)", __func__, ret);\
		return ret; \
	} while (0);

typedef	int (*cmd_run)(char *, int);
typedef	void (*pmdbg_mod_show_cb)(char **);
typedef	int (*pmdbg_mod_init_cb)(void);
typedef	void (*pmdbg_mod_exit_cb)(void);

struct pmdbg_mod {
	char				name[MOD_SIZE];
	struct kobject		kobj;
	struct attribute	*attr;
	pmdbg_mod_init_cb	init;
	pmdbg_mod_exit_cb	exit;
	pmdbg_mod_show_cb	show;
};

struct pmdbg_cmd {
	char				name[CMD_SIZE];
	int					cmd_size;
	cmd_run				run;
	struct list_head	entry;
};


extern int register_mod(struct pmdbg_mod *mod);
extern int unregister_mod(struct pmdbg_mod *mod);
extern int register_cmd(struct list_head *cmd_lst, struct pmdbg_cmd *cmd);
extern int unregister_cmd(struct list_head *cmd_lst, struct pmdbg_cmd *cmd);
extern int run_cmd(const char *buf, int size, struct list_head *cmd_lst);
extern int get_word(const char *buf, int size, int pos, char *out, int *out_sz);

#define PMDBG_FUNC_SHOW(_name) \
static ssize_t _name##_mod_show(struct kobject *, \
			struct kobj_attribute *, char *);\
static ssize_t _name##_mod_show(struct kobject *kobj, \
			struct kobj_attribute *attr, char *buf)\
{\
	int ret = 0; \
	char *s = NULL;\
	FUNC_MSG_IN; \
	(void)kobj;\
	(void)attr;\
	if (MOD_OBJ(_name).show) { \
		MOD_OBJ(_name).show(&s);\
		ret = sprintf(buf, "%s\n", s);\
	} \
	else \
		ret = sprintf(buf, "%s\n", bufres);\
	MSG("%s", buf);\
	FUNC_MSG_RET(ret);\
}

#define PMDBG_FUNC_STORE(_name) \
static ssize_t _name##_mod_store(struct kobject *, struct kobj_attribute *,\
			const char *, size_t);\
static ssize_t _name##_mod_store(struct kobject *kobj, \
			struct kobj_attribute *attr, const char *buf, \
			size_t n)\
{\
	int ret = n; \
	FUNC_MSG_IN; \
	(void)kobj;\
	(void)attr;\
	memset(bufres, 0, RES_SIZE);\
	ret = run_cmd(buf, n, &_name##_cmd_list);\
	ret = n;\
	FUNC_MSG_RET(ret);\
}
#define MOD_OBJ(_name)		_name##_pmdbg_mod
#define DECLARE_MOD(_name)	\
	extern struct pmdbg_mod MOD_OBJ(_name)

#define PMDBG_FUNC(_name) \
				extern struct pmdbg_mod MOD_OBJ(_name);\
				PMDBG_FUNC_SHOW(_name);\
				PMDBG_FUNC_STORE(_name);\

#define DECLARE_CMD(_name, _func)\
static struct pmdbg_cmd _name##_pmdbg_cmd = {	\
	.name	= __stringify(_name),\
	.run	= _func,			\
};


#define ADD_CMD(_mod, _name)\
	register_cmd(&_mod##_cmd_list, &_name##_pmdbg_cmd)
#define DEL_CMD(_mod, _name)\
	unregister_cmd(&_mod##_cmd_list, &_name##_pmdbg_cmd)

#define PMDBG_ATTR(_name) \
static struct kobj_attribute _name##_mod_attr =	\
		__ATTR(_name, 0644, _name##_mod_show, _name##_mod_store);


#define LOCAL_DECLARE_MOD(_name, _init, _exit)\
static char bufres[RES_SIZE] = {0};\
static LIST_HEAD(_name##_cmd_list);\
PMDBG_FUNC(_name);\
PMDBG_ATTR(_name);\
struct pmdbg_mod MOD_OBJ(_name) = \
{	\
	.name	= __stringify(_name),	\
	.attr	= &_name##_mod_attr.attr,\
	.init	= _init, \
	.exit	= _exit, \
	.show	= NULL\
}

#define LOCAL_DECLARE_MOD_SHOW(_name, _init, _exit, _show)\
static char bufres[RES_SIZE] = {0};\
static LIST_HEAD(_name##_cmd_list);\
PMDBG_FUNC(_name);\
PMDBG_ATTR(_name);\
struct pmdbg_mod MOD_OBJ(_name) = \
{	\
	.name	= __stringify(_name),	\
	.attr	= &_name##_mod_attr.attr,\
	.init	= _init, \
	.exit	= _exit, \
	.show	= _show\
}



#endif /*__TST_PM_API__*/
