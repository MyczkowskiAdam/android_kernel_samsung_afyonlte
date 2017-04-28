/*
 * pmdbg_idle.c
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

#include "pmdbg_idle.h"

static struct idle_info last_item[CONFIG_NR_CPUS][NO_LAST_ITEM];
static int cur_last_idx[CONFIG_NR_CPUS];
static struct timeval beforeTime[CONFIG_NR_CPUS];
static struct timeval afterTime[CONFIG_NR_CPUS];
static char state_name[PM_STATE_NOTIFY_CORESTANDBY_2+1][32] = {
	[0] = "",
	[PM_STATE_NOTIFY_SLEEP] = "Sleep",
	[PM_STATE_NOTIFY_SLEEP_LOWFREQ] = "Sleep low-freq",
	[PM_STATE_NOTIFY_CORESTANDBY] = "CoreStandby",
	[PM_STATE_NOTIFY_WAKEUP] = "Wakeup",
	[PM_STATE_NOTIFY_SUSPEND] = "Suspend",
	[PM_STATE_NOTIFY_RESUME] = "Resume",
	[PM_STATE_NOTIFY_CORESTANDBY_2] = "CoreStandby_2",
};

static char buf_reg[1024];
static int mon_state;
static struct wake_lock idle_wakelock;
static int mon_all;


static struct pm_state_notify idle_notify = {
	.name = "PMDBG IDLE",
	.notify = &idle_notify_cb
};

static struct pm_state_notify_confirm idle_confirm = {
	.name = "PMDBG IDLE",
	.confirm = &idle_confirm_cb
};



LOCAL_DECLARE_MOD_SHOW(idle, idle_init, idle_exit, idle_show);

DECLARE_CMD(mon, monitor_cmd);
DECLARE_CMD(sup, suppress_cmd);
DECLARE_CMD(wakelock, wakelock_cmd);

static unsigned int idle_confirm_cb(void)
{

	return 0;
}

static unsigned int idle_notify_cb(int state)
{
	int cpuid = smp_processor_id();

	switch (state) {
	case PM_STATE_NOTIFY_SLEEP:
	case PM_STATE_NOTIFY_SLEEP_LOWFREQ:
	case PM_STATE_NOTIFY_CORESTANDBY:
	case PM_STATE_NOTIFY_CORESTANDBY_2:
		cur_last_idx[cpuid]++;
		if (cur_last_idx[cpuid] >= NO_LAST_ITEM)
			cur_last_idx[cpuid] = 0;

		last_item[cpuid][cur_last_idx[cpuid]].state = state;
		last_item[cpuid][cur_last_idx[cpuid]].is_active = 0;
		do_gettimeofday(&beforeTime[cpuid]);

		last_item[cpuid][cur_last_idx[cpuid]].start_us =
				beforeTime[cpuid].tv_sec * 1000000
				+ beforeTime[cpuid].tv_usec;
		if (mon_all) {
			MSG_INFO("CPU %d: Enter %s at %uus\n", cpuid,
			state_name[last_item[cpuid][cur_last_idx[cpuid]].state],
			last_item[cpuid][cur_last_idx[cpuid]].start_us);
		}
		break;
	case PM_STATE_NOTIFY_WAKEUP:
		do_gettimeofday(&afterTime[cpuid]);
		last_item[cpuid][cur_last_idx[cpuid]].is_active = 1;
		last_item[cpuid][cur_last_idx[cpuid]].idle_time =
				(afterTime[cpuid].tv_sec -
				beforeTime[cpuid].tv_sec) * 1000000
				+ (afterTime[cpuid].tv_usec -
				beforeTime[cpuid].tv_usec);
		if (mon_all)
			MSG_INFO("CPU %d: Leave\n", cpuid);
		break;

	};

	return 0;
}

static int start_monitor(void)
{
	int ret = 0;
	FUNC_MSG_IN;
	register_pm_state_notify_confirm(&idle_confirm);
	register_pm_state_notify(&idle_notify);
	if (!ret)
		mon_state = 1;
	FUNC_MSG_RET(ret);
}

static int stop_monitor(void)
{
	int ret = 0;
	FUNC_MSG_IN;
	unregister_pm_state_notify(&idle_notify);
	unregister_pm_state_notify_confirm(&idle_confirm);
	if (!ret)
		mon_state = 0;
	FUNC_MSG_RET(ret);
}

static int monitor_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int pos = 0;
	char *s = buf_reg;
	FUNC_MSG_IN;
#ifdef CONFIG_CPU_IDLE
	para = strim(para);

	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret = -EINVAL;
		goto fail;
	}
	pos = ret;

	ret = strncmp(item, "start", sizeof("start"));
	if (0 == ret) {
		if (mon_state) {
			MSG_INFO("Monitor has been started");
			goto end;
		}
		ret = start_monitor();
		if (ret)
			goto fail;
		s += sprintf(s, "Started monitor Idle change");
		goto end;
	}
	ret = strncmp(item, "all", sizeof("all"));
	if (0 == ret) {
		if (mon_state) {
			MSG_INFO("Monitor has been started");
			goto end;
		}
		ret = start_monitor();
		if (ret)
			goto fail;
		mon_all = 1;
		s += sprintf(s, "Started monitor Idle change");
		goto end;
	}
	ret = strncmp(item, "get", sizeof("get"));
	if (0 == ret) {
		if (!mon_state) {
			s += sprintf(s, "Monitor has not been started yet");
			ret = -1;
			goto end;
		}

		get_info(s);
		goto end;
	}
	ret = strncmp(item, "stop", sizeof("stop"));
	if (0 == ret) {
		if (!mon_state) {
			s += sprintf(s, "Monitor has not been started yet");
			ret = -1;
			goto end;
		}
		mon_all = 0;
		ret = stop_monitor();
		if (ret)
			goto fail;
		s += sprintf(s, "Monitor has been stopped");
		goto end;
	}
#else /*!CONFIG_CPU_IDLE*/
	s += sprintf(s, "CONFIG_CPU_IDLE is disable");
#endif /*CONFIG_CPU_IDLE*/
	ret =  -EINVAL;
fail:
	s += sprintf(s, "FAILED");
end:
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(ret);
}

static int suppress_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	char *s = buf_reg;
	FUNC_MSG_IN;
#ifndef CONFIG_HAS_WAKELOCK
	s += sprintf(s, "Not support\n");
	ret =  -ENOTSUPP;
	goto end;
#endif /*CONFIG_HAS_WAKELOCK*/

	para = strim(para);

	ret = get_word(para, size, 0, item, &para_sz);
	if (para_sz == 0) {
		wake_lock(&idle_wakelock);
		s += sprintf(s, "CoreStandby was suppressed\n");
		goto end;
	}
	ret = strncmp(item, "no", sizeof("no"));
	if (0 == ret) {
		wake_unlock(&idle_wakelock);
		s += sprintf(s, "CoreStandby is allowed\n");
		goto end;
	}
	ret =  -EINVAL;
	s += sprintf(s, "FAILED");
end:
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(ret);
}

static void get_info(char *buf)
{

	int i = 0;
	int cur = 0;
	int cpuid = 0;
	char *s = buf;
	struct idle_info *item = NULL;
	FUNC_MSG_IN;

	for (cpuid = 0; cpuid < CONFIG_NR_CPUS; cpuid++) {
		s += sprintf(s, "CPU: %d\n", cpuid);

		cur = cur_last_idx[cpuid];
		cur++;
		for (i = 0; i < NO_LAST_ITEM; i++, cur++) {
			if (cur >= NO_LAST_ITEM)
				cur = 0;

			item = &last_item[cpuid][cur];

			s += sprintf(s, "[%12u] %s in %12u us (%s)\n",
				item->start_us,
				state_name[item->state],
				item->is_active ? item->idle_time : 0,
				item->is_active ? "Actived" : "Still sleep"
				);
		}
	}

	FUNC_MSG_OUT;
}

int wakelock_cmd(char *para, int size)
{
	int ret = 0;
	FUNC_MSG_IN;

#ifdef CONFIG_ARCH_R8A7373
	ret = has_wake_lock_no_expire(WAKE_LOCK_IDLE);
	if (ret == 0)
		MSG_INFO("No active idle wakelock");

#else /*!CONFIG_ARCH_R8A7373*/
	ret = has_wake_lock(WAKE_LOCK_IDLE);
	if (ret == 0)
		MSG_INFO("No active idle wakelock");
	else
		MSG_INFO("Has active idle wakelock");

#endif /*CONFIG_ARCH_R8A7373*/

	FUNC_MSG_RET(0);
}

static void idle_show(char **buf)
{
	FUNC_MSG_IN;
	*buf = buf_reg;

	FUNC_MSG_OUT;
}

static int idle_init(void)
{
	FUNC_MSG_IN;
	ADD_CMD(idle, mon);
	ADD_CMD(idle, sup);
	ADD_CMD(idle, wakelock);
	wake_lock_init(&idle_wakelock, WAKE_LOCK_IDLE, "pmdbg Idle");
	mon_state = 0;
	mon_all = 0;
	FUNC_MSG_RET(0);
}

static void idle_exit(void)
{
	FUNC_MSG_IN;
	wake_lock_destroy(&idle_wakelock);
	DEL_CMD(idle, mon);
	DEL_CMD(idle, sup);
	DEL_CMD(idle, wakelock);
	FUNC_MSG_OUT;
}
