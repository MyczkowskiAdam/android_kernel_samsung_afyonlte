/*
 * arch/arm/mach-shmobile/include/mach/pm.h
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
#ifndef __ASM_ARCH_PM_H
#define __ASM_ARCH_PM_H __FILE__
#include <linux/suspend.h>
#include <linux/version.h>
#include <mach/r8a7373.h>

#include <linux/io.h>
#include <linux/spinlock_types.h>
#include <linux/atomic.h>
#include <linux/hwspinlock.h>

#ifdef CONFIG_PM_HAS_SECURE
#include "../../../../../drivers/sec_hal/rt/inc/sec_hal_rt.h"
#include "../../../../../drivers/sec_hal/exports/sec_hal_pm.h"
#endif

/*
 * PM state interface
 */
/*
 * @notify the AP from the BB to the callback function, system
 * sleep/CoreStandby/suspend and restore it to the state transition is used to
 * notify the AP.
 * Return 0 if it succeeds, nonzero otherwise.
 */
#define PM_STATE_NOTIFY_SLEEP			1
#define PM_STATE_NOTIFY_SLEEP_LOWFREQ	2
#define PM_STATE_NOTIFY_CORESTANDBY		3
#define PM_STATE_NOTIFY_WAKEUP			4
#define PM_STATE_NOTIFY_SUSPEND			5
#define PM_STATE_NOTIFY_RESUME			6
#define PM_STATE_NOTIFY_CORESTANDBY_2	7
struct pm_state_notify {
	struct list_head link;
	const char *name;
	unsigned int (*notify)(int state);
};

/*
 * @confirm the AP from the BB to the callback function, the system
 * sleep/CoreStandby/suspend state whether the transition can be used to
 * confirm the AP.
 * It returns 0 if possible, otherwise nonzero.
 */
struct pm_state_notify_confirm {
	struct list_head link;
	const char *name;
	unsigned int (*confirm)(void);
};

/* Need to be used by others of PM */

extern int core_wait_kick(int time);
extern int core_set_kick(int time);
extern int clock_update(unsigned int freqA, unsigned int freqA_mask,
				unsigned int freqB, unsigned int freqB_mask);
extern unsigned int suspend_ZB3_backup(void);
extern int shmobile_init_pm(void);
extern struct hwspinlock *gen_sem1;
extern struct hwspinlock *sw_cpg_lock;
extern unsigned int is_suspend_request;
extern int start_corestandby(void);
extern int start_corestandby_2(void);
extern void ArmVector(void);
extern void corestandby(void);
extern void corestandby_2(void);
extern void systemsuspend(void);
extern void save_arm_register(void);
extern void restore_arm_register_pa(void);
extern void restore_arm_register_va(void);
extern void save_arm_common_register(void);
extern void restore_arm_common_register(void);
extern void save_common_register(void);
extern void restore_common_register(void);
extern void sys_powerdown(void);
extern void sys_powerup(void);

extern void start_wfi(void);
extern void start_wfi2(void);
extern void disablemmu(void);
extern void systemsuspend_cpu0_pa(void);
extern void systemsuspend_cpu1_pa(void);
extern void corestandby_pa(void);
extern void corestandby_pa_2(void);

extern void corestandby_down_status(void);
extern void corestandby_up_status(void);
extern void xtal_though(void);
extern void PM_Spin_Lock(void);
extern void PM_Spin_Unlock(void);

extern int copy_functions(void);

extern void jump_systemsuspend(void);
extern int has_wake_lock_no_expire(int type);
extern void request_suspend_state(suspend_state_t state);
extern suspend_state_t get_suspend_state(void);

#ifdef CONFIG_CPU_IDLE
void register_pm_state_notify(struct pm_state_notify *h);
void unregister_pm_state_notify(struct pm_state_notify *h);
void register_pm_state_notify_confirm(struct pm_state_notify_confirm *h);
void unregister_pm_state_notify_confirm(struct pm_state_notify_confirm *h);
unsigned int state_notify_confirm(void);
unsigned int state_notify(int state);
#ifdef CONFIG_PM_DEBUG
extern int control_cpuidle(int is_enable);
extern int is_cpuidle_enable(void);
#endif /* CONFIG_PM_DEBUG */
#else /*!CONFIG_CPU_IDLE*/
static inline void register_pm_state_notify(struct pm_state_notify *h) {}
static inline void unregister_pm_state_notify(struct pm_state_notify *h) {}
static inline void register_pm_state_notify_confirm(
	struct pm_state_notify_confirm *h) {}
static inline void unregister_pm_state_notify_confirm(
	struct pm_state_notify_confirm *h) {}
static inline unsigned int state_notify_confirm(void) { return 0; }
static inline unsigned int state_notify(int state) { return 0; }
#ifdef CONFIG_PM_DEBUG
static inline int control_cpuidle(int is_enable) { return 0; }
static inline int is_cpuidle_enable(void)  { return 0; }
#endif /* CONFIG_PM_DEBUG */
#endif /*CONFIG_CPU_IDLE*/


#define POWER_DOMAIN_COUNT_MAX	3
#define CONFIG_PM_RUNTIME_A3SG
#define CONFIG_PM_RUNTIME_A3SP
#define CONFIG_PM_RUNTIME_A3R
#define CONFIG_PM_RUNTIME_A4RM
#define CONFIG_PM_RUNTIME_A4MP

/*Value of power area (value is appropriate with SWUCR, SPDCR, PSTR registers)*/
#define POWER_A2SL					BIT(20)
#define POWER_A3SM					BIT(19)
#define POWER_A3SG					BIT(18)
#define POWER_A3SP					BIT(17)
#define POWER_C4					BIT(16)
#define POWER_A2RI					BIT(15)
#define POWER_A2RV					BIT(14)
#define POWER_A3R					BIT(13)
#define POWER_A4RM					BIT(12)
#define POWER_A4MP					BIT(8)
#define POWER_A4LC					BIT(6)
#define POWER_D4					BIT(1)
#define POWER_ALL					0x001FF142
#define POWER_NONE					0

/* Power ID (be appropriate with bits order on SWUCR, SPDCR, PSTR registers) */
#define ID_A2SL					20
#define ID_A3SM					19
#define ID_A3SG					18
#define ID_A3SP					17
#define ID_C4					16
#define ID_A2RI					15
#define ID_A2RV					14
#define ID_A3R					13
#define ID_A4RM					12
#define ID_A4MP					8
#define ID_A4LC					6
#define ID_D4					1

extern atomic_t	pdwait_flag;
void pdwait_judge(void);

#ifdef CONFIG_PDC
struct power_domain_info {
	struct device *devs[POWER_DOMAIN_COUNT_MAX];
	size_t cnt;
};
int power_domain_devices(const char *drv_name,
		struct device **dev, size_t *dev_cnt);

u64 power_down_count(unsigned int powerdomain);

void for_each_power_device(const struct device *dev,
		int (*iterator)(struct device *));
void power_domains_get_sync(const struct device *dev);
void power_domains_put_noidle(const struct device *dev);
struct power_domain_info *__to_pdi(const struct device *dev);
struct power_domain_info *get_pdi(const char *name);
void power_domain_set_status(unsigned int area, bool on);
#ifdef CONFIG_PM_DEBUG
int control_pdc(int is_enable);
int is_pdc_enable(void);
#endif
#else /*!CONFIG_PDC*/
static inline int power_domain_devices(const char *drv_name,
		struct device **dev, size_t *dev_cnt) { return 0; }

static inline u64 power_down_count(unsigned int powerdomain) { return 0; }

static inline void for_each_power_device(const struct device *dev,
		int (*iterator)(struct device *)) {}
static inline void power_domains_get_sync(const struct device *dev) {}
static inline void power_domains_put_noidle(const struct device *dev) {}
#ifdef CONFIG_PM_DEBUG
static inline int control_pdc(int is_enable) { return 0; }
static inline int is_pdc_enable(void) { return 0; }
#endif
inline void pdwait_judge(void) {}
#endif  /*CONFIG_PDC*/

#ifdef CONFIG_SUSPEND
suspend_state_t get_shmobile_suspend_state(void);
void shwystatdm_regs_save(void);
void shwystatdm_regs_restore(void);
#ifdef CONFIG_PM_DEBUG
int control_systemsuspend(int is_enabled);
int is_systemsuspend_enable(void);
#else
static inline int control_systemsuspend(int is_enabled) { return 0; }
static inline int is_systemsuspend_enable(void) { return 0; }
#endif
#else /*!CONFIG_SUSPEND*/
static inline suspend_state_t get_shmobile_suspend_state(void) { return 0; }
static inline int control_systemsuspend(int is_enabled) { return 0; }
static inline int is_systemsuspend_enable(void) { return 0; }
static inline void shwystatdm_regs_save(void) { }
static inline void shwystatdm_regs_restore(void) { }
#endif /*CONFIG_SUSPEND*/


#ifdef CONFIG_PM_HAS_SECURE
#if 0
extern uint32_t sec_hal_pm_poweroff(void);
extern uint32_t sec_hal_coma_entry(uint32_t mode, uint32_t freq,
	uint32_t wakeup_address, uint32_t context_save_address);
#endif
#else /* !CONFIG_PM_HAS_SECURE*/
static inline uint32_t sec_hal_pm_poweroff(void) { return 0; }
static inline uint32_t sec_hal_coma_entry(uint32_t mode, uint32_t freq,
	uint32_t wakeup_address, uint32_t context_save_address) { return 0; }
#endif /*CONFIG_PM_HAS_SECURE*/

/* HPB Phys:0xE6000000 + 0x101C */
#define CHIP_VERSION_ES2_0	0x00003E10
#define CHIP_VERSION_ES2_1	0x00003E11
#define CHIP_VERSION_ES2_2	0x00003E12

#define CHIP_VERSION_MASK	0x0000FFFF
#define ES_REV_2_0	(1 << 2)
#define ES_REV_2_1	((1 << 2) | (1 << 0))
#define ES_REV_2_2	((1 << 2) | (1 << 1))

#define ES_REV_2X	(ES_REV_2_0 | ES_REV_2_1 | ES_REV_2_2)
#define ES_REV_ALL	(ES_REV_2X)
extern unsigned int system_rev;

/*
 * Helper functions for getting chip revision
 */
static inline int shmobile_chip_rev(void)
{
	switch (system_rev & CHIP_VERSION_MASK) {
	case CHIP_VERSION_ES2_0:
		return ES_REV_2_0;
	case CHIP_VERSION_ES2_1:
		return ES_REV_2_1;
	case CHIP_VERSION_ES2_2:
		return ES_REV_2_2;
	default:
		break;
	}
	return ES_REV_2X;
}

/* SGX flags */
enum sgx_flg_type {
	CPUFREQ_SGXON  = 0,
	CPUFREQ_SGXOFF,
	CPUFREQ_SGXNUM
};

enum freq_mode {
	MODE_1 = 0,
	MODE_2,
	MODE_3,
	MODE_4,
	MODE_5,
	MODE_6,
	MODE_7,
	MODE_8,
	MODE_9,
	MODE_10,
	MODE_11,
	MODE_12,
	MODE_13,
	MODE_14,
	MODE_15
};

enum clk_type {
	I_CLK = 0,
	ZG_CLK,
	B_CLK,
	M1_CLK,
	M3_CLK,
	M5_CLK,
	Z_CLK,
	ZTR_CLK,
	ZT_CLK,
	ZX_CLK,
	HP_CLK,
	ZS_CLK,
	ZB_CLK,
	ZB3_CLK
};

enum clk_div {
	DIV1_1 = 0x00,
	DIV1_2,
	DIV1_3,
	DIV1_4,
	DIV1_5,
	DIV1_6,
	DIV1_7,
	DIV1_8,
	DIV1_12,
	DIV1_16,
	DIV1_18,
	DIV1_24,
	DIV1_32,
	DIV1_36,
	DIV1_48,
	DIV1_96
};

enum pll_type {
	PLL0 = 0x00,
	PLL1,
	PLL2,
	PLL3
};

enum pll_ratio {
	PLLx35 = 35,
	PLLx38 = 38,
	PLLx42 = 42,
	PLLx46 = 46,
	PLLx49 = 49,
	PLLx56 = 56
};

enum {
	LLx1_16 = 1,
	LLx2_16,
	LLx3_16,
	LLx4_16,
	LLx5_16,
	LLx6_16,
	LLx7_16,
	LLx8_16,
	LLx9_16,
	LLx10_16,
	LLx11_16,
	LLx12_16,
	LLx13_16,
	LLx14_16,
	LLx15_16,
	LLx16_16,
};

struct clk_rate {
	enum clk_div i_clk;
	enum clk_div zg_clk;
	enum clk_div b_clk;
	enum clk_div m1_clk;
	enum clk_div m3_clk;
	enum clk_div m5_clk;
	enum clk_div z_clk;
	enum clk_div ztr_clk;
	enum clk_div zt_clk;
	enum clk_div zx_clk;
	enum clk_div hp_clk;
	enum clk_div zs_clk;
	enum clk_div zb_clk;
	enum clk_div zb3_clk;
	unsigned int zb3_freq;
	enum pll_ratio pll0;
};

enum limit_freq {
	LIMIT_NONE,		/* no limit		*/
	LIMIT_MID,		/* limit mid	*/
	LIMIT_LOW,		/* limit low	*/
};
enum {
	ZSCLK = BIT(1),
	HPCLK = BIT(2)
};
/* API use for Power Off module */
extern void arm_machine_flush_console(void);
#ifdef CONFIG_CPU_FREQ
#define ZB3_CLK_DFS_ENABLE
#define ZB3_CLK_SUSPEND_ENABLE
#define ZB3_CLK_IDLE_ENABLE
#ifdef DVFS_DEBUG_MODE
#define pr_log	pr_alert
#else
#define pr_log(...)
#endif /* DVFS_DEBUG_MODE */
#define ZFREQ_MODE 1
#ifndef valid_revision
#define valid_revision()	(ES_REV_2X & shmobile_chip_rev())
#endif /* valid_revision */
#ifndef validate
#ifdef CONFIG_PM_DEBUG
#define validate()		(valid_revision() && is_cpufreq_enable())
#else /* CONFIG_PM_DEBUG */
#define validate()		valid_revision()
#endif /* CONFIG_PM_DEBUG */
#endif /* validate */
#define DYNAMIC_HOTPLUG_CPU	1

#ifdef CONFIG_CPU_FREQ_GOV_ONDEMAND
extern int samplrate_downfact_change(unsigned int sampl_rate,
			unsigned int down_factor, unsigned int flag);
extern void samplrate_downfact_get(unsigned int *sampl_rate,
				unsigned int *down_factor);
#else
static inline int samplrate_downfact_change(unsigned int sampl_rate,
			unsigned int down_factor, unsigned int flag)
{ return 0; }
static inline void samplrate_downfact_get(unsigned int *sampl_rate,
				unsigned int *down_factor) {}
#endif /* CONFIG_CPU_FREQ_GOV_ONDEMAND */

/* verylow mode enable flag */
/* #define SH_CPUFREQ_VERYLOW	1 */
extern int disable_early_suspend_clock(void);
extern void enable_early_suspend_clock(void);
extern int is_cpufreq_clk_state_earlysuspend(void);
extern void start_cpufreq(void);
extern int stop_cpufreq(void);
extern bool cpufreq_compulsive_exec_get(void);
extern void cpufreq_compulsive_exec_clear(void);
extern void disable_dfs_mode_min(void);
extern void enable_dfs_mode_min(void);
extern void set_limit_min_cpufreq(void);
extern void unset_limit_min_cpufreq(void);
extern int movie_cpufreq(int sw720p);
extern int corestandby_cpufreq(void);
extern int suspend_cpufreq(void);
extern int resume_cpufreq(void);
extern int sgx_cpufreq(int flag);
extern void control_dfs_scaling(bool enabled);
extern int limit_max_cpufreq(int max);
extern int suppress_clocks_change(bool set_max);
extern void unsuppress_clocks_change(void);
#ifdef CONFIG_PM_DEBUG
extern int control_cpufreq(int is_enable);
extern int is_cpufreq_enable(void);
#endif /* CONFIG_PM_DEBUG */
/* Internal API for CPUFreq driver only */
/* for corestandby */
extern int cpg_get_freq(struct clk_rate *rates);
extern int cpg_set_sbsc_freq(unsigned int new_ape_freq);
extern int pm_set_clocks(const struct clk_rate clk_div);
extern int pm_set_clock_mode(const int mode);
extern int pm_get_clock_mode(const int mode, struct clk_rate *rate);
extern int pm_set_syscpu_frequency(int div);
extern unsigned int pm_get_syscpu_frequency(void);
extern int pm_set_pll_ratio(int pll, unsigned int val);
extern int pm_get_pll_ratio(int pll);
extern int pm_setup_clock(void);
extern void shmobile_sbsc_init(void);
extern int pm_enable_clock_change(int clk);
extern int pm_disable_clock_change(int clk);
extern unsigned long pm_get_spinlock(void);
extern void pm_release_spinlock(unsigned long flag);
#else
static inline void start_cpufreq(void) {}
static inline int stop_cpufreq(void) { return 0; }
static inline void disable_dfs_mode_min(void) {}
static inline void enable_dfs_mode_min(void) {}
static inline int movie_cpufreq(int sw720p) { return 0; }
static inline int corestandby_cpufreq(void) { return 0; }
static inline int suspend_cpufreq(void) { return 0; }
static inline int resume_cpufreq(void) { return 0; }
static inline int sgx_cpufreq(int flag) { return 0; }
static inline void control_dfs_scaling(bool enabled) {}
static inline int limit_max_cpufreq(int max) { return 0; }
static inline int suppress_clocks_change(bool set_max) { return 0; }
static inline void unsuppress_clocks_change(void) {}
#ifdef CONFIG_PM_DEBUG
static inline int control_cpufreq(int is_enable) { return 0; }
static inline int is_cpufreq_enable(void) { return 0; }
#endif
/* Internal API for CPUFreq driver only */
static inline int cpg_set_sbsc_freq(unsigned int new_ape_freq) { return 0; }
static inline int pm_set_clocks(const struct clk_rate clk_div) { return 0; }
static inline int pm_set_clock_mode(const int mode) { return 0; }
static inline int pm_get_clock_mode(const int mode, struct clk_rate *rate)
{return 0; }
static inline int pm_set_syscpu_frequency(int div) { return 0; }
static inline unsigned int pm_get_syscpu_frequency(void) { return 0; }
static inline int pm_set_pll_ratio(int pll, unsigned int val)
{return -EINVAL; }
static inline int pm_get_pll_ratio(int pll) { return -EINVAL; }
static inline int pm_setup_clock(void) { return 0; }
static inline int pm_enable_clock_change(int clk) { return 0; }
static inline int pm_disable_clock_change(int clk) { return 0; }
static inline unsigned long pm_get_spinlock(void) { return 0; }
static inline void pm_release_spinlock(unsigned long flag) { }
#endif /* CONFIG_CPU_FREQ */

#if (defined CONFIG_HOTPLUG_CPU_MGR) && (defined CONFIG_ARCH_R8A7373)
#define THS_HOTPLUG_ID		(1 << 4)
#define DFS_HOTPLUG_ID		(1 << 8)
#define SYSFS_HOTPLUG_ID	(1 << 12)
#endif /*CONFIG_HOTPLUG_CPU_MGR && CONFIG_ARCH_R8A7373*/

#ifdef CONFIG_ARCH_R8A7373
/* Common functions of the PM debug */
#define PMDBG_MAX_CPUS		2
extern void pmdbg_mon(int cpum, unsigned int max_load,
		unsigned int load0, unsigned int load1,
		unsigned int cur, unsigned int req);
extern char pmdbg_get_enable_cpu_profile(void);

/* Common functions of the PM debug */
extern void pmdbg_dump_suspend(void);
extern void pmdbg_pmic_dump_suspend(void *pmic_data);
extern char pmdbg_get_enable_dump_suspend(void);
#endif /* CONFIG_ARCH_R8A7373 */

#endif /* __ASM_ARCH_PM_H */
