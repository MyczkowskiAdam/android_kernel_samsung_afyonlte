#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/pmic/pmic.h>
#include <linux/regulator/consumer.h>

#include <mach/common.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/r8a7373.h>
#include <asm/io.h>

/* #define E3_3_V 3300000 */
#define E1_8_V 1800000

/* Following should come from proper include file from drivers/sec_hal/exports/sec_hal_cmn.h */
extern uint32_t sec_hal_dbg_reg_set(uint32_t *dbgreg1, uint32_t *dbgreg2, uint32_t *dbgreg3);

#define DBGREG9_AID 		0xA5
#define DBGREG9_AID_SHIFT	8
#define DBGREG9_AID_MASK 	(0xFF << DBGREG9_AID_SHIFT)
#define DBGREG9_KEY_SHIFT	0x0
#define DBGREG9_KEY_MASK	0x1

#define SYS_TPIU_BASE		IO_ADDRESS(0xE6F83000)
#define CPU_TPIU_BASE		IO_ADDRESS(0xE6FA3000)
#define CPU_ETR_BASE		IO_ADDRESS(0xE6FA5000)
#define CPU_ETF_BASE		IO_ADDRESS(0xE6FA1000)
#define CPU_TRACE_FUNNEL_BASE	IO_ADDRESS(0xE6FA4000)
#define SYS_TRACE_FUNNEL_BASE	IO_ADDRESS(0xE6F84000)

static int stm_select=-1;

#ifdef CONFIG_U2_STM_ETR_TO_SDRAM
static int wait_for_coresight_access_lock(u32 base)
{
  int retval = -1;
  int timeout = 512;
  int i;
  __raw_writel(0xc5acce55, base + 0xFB0); /* Lock Access */
  for (i = 0; i < timeout && retval; i++) {
    if ((__raw_readl(base + 0xFB4) & 2) == 0) retval = 0;
  }
  printk(KERN_ALERT "wait_for_coresight_access_lock %d\n", retval);
  return retval;
}

static void conf_stm_etr_to_sdram(void)
{
  int i;
  /*
    EOS2 Modem STM Trace to SDRAM through ETR -- Configuration in Short
    ===================================================================
    SUMMARY OF MODEM STM TRACE FLOW, CONFIGURATION IN REVERSE ORDER:
    ----------------------------------------------------------------
    1) Modem   CoreSight / WGEM STM          @ inside WGEM  - Enable traces
    2) System  CoreSight / SYS Funnel STM    @ 0xE6F 8B 000 - Enable Port #1 "From STM-ATB Modem"
    3) System  CoreSight / SYS Trace Funnel  @ 0xE6F 84 000 - Enable Port #2 "From Sys-Funnel-STM"
    4) HostCPU CoreSight / CPU Trace Funnel  @ 0xE6F A4 000 - Enable Port #4 "From Sys-Trace-Funnel"
    5) HostCPU CoreSight / ETF               @ 0xE6F A1 000 - configure FIFO mode
    6) HostCPU CoreSight / ETR configuration @ 0xE6F A5 000 - configure Circular buffer mode, SDRAM write buffer size and start address, etc.
    7) System  CoreSight / SYS-TPIU-STM      @ 0xE6F 8A 000 - set to 32-bit mode to avoid unnecessary stall
    8) HostCPU CoreSight / CPU-TPIU          @ 0xE6F A3 000 - set to 32-bit mode to avoid unnecessary stall
    9) System  CoreSight / SYS-TPIU          @ 0xE6F 83 000 - set to 32-bit mode to avoid unnecessary stall

    DETAILED CONFIGURATION REGISTER WRITES:
    ---------------------------------------
  */

  /* <<<<<< - 9 - System CoreSight  / SYS-TPIU     to 32-bit mode >>>>>> */

  wait_for_coresight_access_lock(SYS_TPIU_BASE);
#if 1
  __raw_writel((1<<(16-1)), SYS_TPIU_BASE + 0x004);               /* Current Port Size 4-bits wide to avoid stall */
#else
  __raw_writel((1<<(32-1)), SYS_TPIU_BASE + 0x004);               /* Current Port Size 32-bits wide to avoid stall */
#endif

  /* <<<<<< - 8 - HostCPU CoreSight / CPU-TPIU     to 32-bit mode >>>>>> */

  wait_for_coresight_access_lock(CPU_TPIU_BASE);
#if 1
  __raw_writel((1<<(16-1)), CPU_TPIU_BASE + 0x004);               /* Current Port Size 16-bits wide to avoid stall */
#else
  __raw_writel((1<<(32-1)), CPU_TPIU_BASE + 0x004);               /* Current Port Size 32-bits wide to avoid stall */
#endif
  /* <<<<<< - 7 - System CoreSight  / SYS-TPIU-STM to 32-bit mode >>>>>> */

  wait_for_coresight_access_lock(SYS_TPIU_STM_BASE);
#if 1
  __raw_writel((1<<(4-1)), SYS_TPIU_STM_BASE + 0x004);    /* Current Port Size 16-bits wide to avoid stall */
#else
  __raw_writel((1<<(32-1)), SYS_TPIU_STM_BASE + 0x004);   /* Current Port Size 32-bits wide to avoid stall */
#endif

  /* <<<<<< - 6 - HostCPU CoreSight / ETR configuration >>>>>>
     For ARM Specification of this HW block, see CoreSight Trace Memory Controller Technical Reference Manual
     SW Registers of ETR are same as ETF in different HW configuration
  */

  wait_for_coresight_access_lock(CPU_ETR_BASE);
  __raw_writel(0, CPU_ETR_BASE + 0x020);                  /* CTL Control: 0 */
  __raw_writel(0, CPU_ETR_BASE + 0x028);                  /* MODE: Circular buffer */
  __raw_writel(3, CPU_ETR_BASE + 0x304);                  /* FFCR: Formatting enabled */

  __raw_writel(
	       (       (3 << 8) |              /*    WrBurstLen, 0 = 1, 1 = 2, ..., 15 = 16     */
		       (0 << 7) |              /*    0 = Single buffer, 1 = ScatterGather       */
		       (0 << 6) |              /*    Reserved                                   */
		       (0 << 5) |              /*    CacheCtrlBit3 No write alloc / write alloc */
		       (0 << 4) |              /*    CacheCtrlBit2 No read alloc / read alloc   */
		       (1 << 3) |              /*    CacheCtrlBit1 Non-cacheable  / Cacheable   */
		       (1 << 2) |              /*    CacheCtrlBit0 Non-bufferable / Bufferable  */
		       (1 << 1) |              /*    ProtCtrlBit1  Secure / Non-secure          */
		       (1 << 0)                /*    ProtCtrlBit0  Normal / Privileged          */
		       ),
	       CPU_ETR_BASE + 0x110); /* AXICTL: Set as commented above */

  __raw_writel(0, CPU_ETR_BASE + 0x034);                  /* BUFWM Buffer Level Water Mark: 0 */
  __raw_writel(0, CPU_ETR_BASE + 0x018);                  /* RWP RAM Writer Pointer: 0 */
  __raw_writel(0, CPU_ETR_BASE + 0x03C);                  /* RWP RAM Writer Pointer High: 0 */
  __raw_writel(0x45801000, CPU_ETR_BASE + 0x118);         /* DBALO Data Buffer Address Low: 0x 4580 10000 */
  __raw_writel(0, CPU_ETR_BASE + 0x11C);                  /* DBAHI Data Buffer Address High: 0 */
  __raw_writel(((39*1024*1024  + 764*1024)/ 4), CPU_ETR_BASE + 0x004); /* RSZ RAM Size Register: 39MB + 764 kB */
  __raw_writel(1, CPU_ETR_BASE + 0x020);                  /* CTL Control: 1 */

  /* <<<<<< - 5 - HostCPU CoreSight / ETF - configuration to FIFO mode >>>>>>
     For ARM Specification of this HW block, see CoreSight Trace Memory Controller Technical Reference Manual
  */

  wait_for_coresight_access_lock(CPU_ETF_BASE);
  __raw_writel(0, CPU_ETF_BASE + 0x020);                  /* CTL Control: TraceCaptEn OFF ==> Disabled */
  __raw_writel(2, CPU_ETF_BASE + 0x028);                  /* MODE: FIFO */
  __raw_writel(3, CPU_ETF_BASE + 0x304);                  /* FFCR Formatter and Flush Control Register: Formatting enabled */
  __raw_writel(0, CPU_ETF_BASE + 0x034);                  /* BUFWM Buffer Level Water Mark: 0 */
  __raw_writel(1, CPU_ETF_BASE + 0x020);                  /* CTL Control: TraceCaptEn ON ==> Running */

  /* <<<<<< - 4 - HostCPU CoreSight / CPU Trace Funnel - Enable Port #3 "From Sys-Trace-Funnel" >>>>>> */

  wait_for_coresight_access_lock(CPU_TRACE_FUNNEL_BASE);
  __raw_writel((0x300 | (1<<4)), CPU_TRACE_FUNNEL_BASE + 0x000);  /* Enable only Slave port 4, i.e. From Sys-Trace-Funnel */

  /* <<<<<< - 3 - System CoreSight / SYS Trace Funnel - Enable Port #2 "From Sys-Funnel-STM" >>>>>> */

  wait_for_coresight_access_lock(SYS_TRACE_FUNNEL_BASE);
  __raw_writel((0x300 | (1<<2)), SYS_TRACE_FUNNEL_BASE + 0x000);  // Enable only Slave port 2, i.e. From Sys-Funnel-STM

  /* <<<<<< - 2 - System CoreSight / SYS Funnel STM - Enable Port #1 "From STM-ATB Modem" >>>>>> */

  wait_for_coresight_access_lock(SYS_TRACE_FUNNEL_STM_BASE);
  __raw_writel((0x300 | (1<<1)), SYS_TRACE_FUNNEL_STM_BASE + 0x000);      /* Enable only Slave port 1, i.e. Modem top-level funnel for STM */

  /* <<<<<< - 1 - Modem CoreSight / WGEM STM - Enable traces >>>>>>
     This happens inside WGEM L2 TCM vector boot code
  */
}
#endif

int u2evm_get_stm_select(void)
{
  return stm_select;
}
// EXPORT_SYMBOL(u2evm_get_stm_select);

int u2evm_init_stm_select(void)
{
  char *cp = &boot_command_line[0];
  int ci;
  int stm_boot_arg = -1;
  uint32_t g_dbgreg1 = 0x20000000;
#ifdef CONFIG_ARM_TZ
  uint32_t ret;
  uint32_t g_dbgreg2 = 0x00;
  uint32_t g_dbgreg3 = 0x00078077;
#else
  uint32_t reg, key;
#endif
  uint32_t val;
  int i;
  int sec_stm_select = -1;

  if (cp[0] && cp[1] && cp[2] && cp[3] && cp[4]) {
    for (ci=4; cp[ci]; ci++) {
      if (cp[ci-4] == 's' &&
	  cp[ci-3] == 't' &&
	  cp[ci-2] == 'm' &&
	  cp[ci-1] == '=') {
	switch (cp[ci]) {
	case '0': stm_boot_arg =  0; break;
	case '1': stm_boot_arg =  1; g_dbgreg1 |= (1<<20); break;
	default:  stm_boot_arg = -1; break;
	}
	break;
      }
    }
  }

  printk(KERN_ALERT "stm_boot_arg=%d\n", stm_boot_arg);

#ifdef CONFIG_ARM_TZ
  // Try to set debug mode according to boot argument, if secure side allows for it.
  val = __raw_readl(DBGREG1);
  g_dbgreg1 = (g_dbgreg1 & 0xFFFF0000) | (val & 0x0000FFFF); /* Keep TRC[2:0] and MDA[7:0] but check-modify MD... and STMSEL[:] */
  if ((stm_boot_arg >= 0) && (val != g_dbgreg1)) {
    ret = sec_hal_dbg_reg_set(&g_dbgreg1, &g_dbgreg2, &g_dbgreg3);
    printk(KERN_ALERT "TZ: ret=%x,g_dbgreg1/2/3=0x%08x, 0x%08x, 0x%08x\n", ret, g_dbgreg1, g_dbgreg2, g_dbgreg3);
  }
#else
  val = __raw_readl(DBGREG1);
  g_dbgreg1 = (g_dbgreg1 & 0xFFFF0000) | (val & 0x0000FFFF); /* Keep TRC[2:0] and MDA[7:0] but check-modify MD... and STMSEL[:] */
  if (stm_boot_arg >= 0) {
      if (val == (g_dbgreg1^(1<<20))) {
	// Try to mux stm output mode according to boot argument
	// if R-Loader has enabled debug mode, but to wrong SDHI port.
	__raw_writel(val, DBGREG1);
	printk(KERN_ALERT "Non-TZ: overrode stm_sel bit\n");
      } else if (val != g_dbgreg1) {
	// Try to enable debug mode and set everything, since R-Loader did not enable it.
	// Implementation idea copied from drivers/pmdbg/pmdbg_dbgpin.c, to be verified!
	printk(KERN_ALERT "Non-TZ: enter debug mode for stm_sel>>\n");
	reg = __raw_readl(DBGREG9);
	key = (reg & DBGREG9_KEY_MASK) >> DBGREG9_KEY_SHIFT;
	__raw_writel((DBGREG9_AID_MASK & (DBGREG9_AID << DBGREG9_AID_SHIFT)), DBGREG9);
	val = __raw_readl(DBGREG9);
	__raw_writel((val | (DBGREG9_AID_MASK & (DBGREG9_AID << DBGREG9_AID_SHIFT)) | DBGREG9_KEY_MASK), DBGREG9);

	__raw_writel(0x000780ff, DBGREG3);
        __raw_writel(g_dbgreg1, DBGREG1);
	__raw_writel(0x00000002, SWUCR);
	while((__raw_readl(PSTR) & 0x00000002) == 0);
	__raw_writel((DBGREG9_AID_MASK & (DBGREG9_AID << DBGREG9_AID_SHIFT)), DBGREG9);
	val = __raw_readl(DBGREG9);
	__raw_writel((val | (DBGREG9_AID_MASK & (DBGREG9_AID << DBGREG9_AID_SHIFT)) | (key << DBGREG9_KEY_SHIFT)), DBGREG9);
	val = __raw_readl(DBGREG9);
	__raw_writel((val & ~(DBGREG9_AID_MASK & (DBGREG9_AID << DBGREG9_AID_SHIFT))), DBGREG9);
	printk(KERN_ALERT "Non-TZ: enter debug mode for stm_sel<<\n");
      }
  }
#endif

/* For case that TZ:Secure ISSW or Non-TZ:R-Loader has selected debug mode already! */

  val = __raw_readl(DBGREG1);
  if ((val & (1 << 29)) == 0) {
    sec_stm_select = -1;
  } else {
    if ((val & (1 << 20)) == 0) {
      sec_stm_select = 0;
    } else {
      sec_stm_select = 1;
    }
  }

  printk(KERN_ALERT "sec_stm_select=%d\n", sec_stm_select);

  if (sec_stm_select >= 0) { /* Only if Secure side allows debugging */
    stm_select = stm_boot_arg;
  } else {
    stm_select = -1;
  }

  printk(KERN_ALERT "final stm_select=%d\n", stm_select);

  /* FIRST, CONFIGURE STM CLK AND DATA PINMUX */
  if (1 == stm_select) {
    /* SDHI1 used for STM Data, STM Clock */
    gpio_request(GPIO_FN_STMCLK_2, NULL);   /* PORT 288 */
    gpio_request(GPIO_FN_STMDATA0_2, NULL); /* PORT 289 */
    gpio_request(GPIO_FN_STMDATA1_2, NULL); /* PORT 290 */
    gpio_request(GPIO_FN_STMDATA2_2, NULL); /* PORT 291 */
    gpio_request(GPIO_FN_STMDATA3_2, NULL); /* PORT 292 */
  } else if (0 == stm_select) {
    /* SDHI0 used for STM Data, STM Clock */
    gpio_request(GPIO_FN_STMCLK_1, NULL);   /* PORT 326 */
    gpio_request(GPIO_FN_STMDATA0_1, NULL); /* PORT 320 */
    gpio_request(GPIO_FN_STMDATA1_1, NULL); /* PORT 321 */
    gpio_request(GPIO_FN_STMDATA2_1, NULL); /* PORT 322 */
    gpio_request(GPIO_FN_STMDATA3_1, NULL); /* PORT 323 */
  }

  /* SECOND, ENABLE TERMINAL POWER FOR STM CLK AND DATA PINS */
  if (stm_select >= 0) {
    __raw_writel(__raw_readl(MSEL3CR) | (1<<28), MSEL3CR);
  }

  /* THIRD, PINMUX STM SIDI (i,e, return channel) MUX FOR BB/MODEM */
  /* ALSO, CONFIGURE SYS-(TRACE) FUNNEL-STM, and SYS-TPIU-STM */

  if (1 == stm_select) {
    /* SDHI1 used for STMSIDI */
    gpio_request(GPIO_FN_STMSIDI_2, NULL); /* PORT 293 */
    gpio_pull_up_port(GPIO_PORT293);
  }

  if (0 == stm_select) {
    /* SDHI0 used for STMSIDI */
    gpio_request(GPIO_FN_STMSIDI_1, NULL); /* PORT 324 */
    gpio_pull_up_port(GPIO_PORT324);
  }

  if (stm_select >= 0) {
    /* Configure SYS-(Trace) Funnel-STM @ 0xE6F8B000 */
    __raw_writel(0xc5acce55, SYS_TRACE_FUNNEL_STM_BASE + 0xFB0); // Lock Access
    for(i=0; i<0xF0; i++);
    val = __raw_readl(SYS_TRACE_FUNNEL_STM_BASE + 0x000);
    val = 0x302 | (val & 0x0FF); // Open Modem port. Keep other ports open if previously opened.
    for(i=0; i<0xF0; i++);
    __raw_writel(0xc5acce55, SYS_TRACE_FUNNEL_STM_BASE + 0xFB0);
    for(i=0; i<0x10; i++);
    __raw_writel(       val, SYS_TRACE_FUNNEL_STM_BASE + 0x000);
    for(i=0; i<0xF0; i++);

    /* Configure SYS-TPIU-STM @ 0xE6F8A000 */
    __raw_writel(0xc5acce55, SYS_TPIU_STM_BASE + 0xFB0); // Lock Access
    __raw_writel(       0x8, SYS_TPIU_STM_BASE + 0x004); // 0x8 means Current Port Size 4-bits wide (TRACEDATA0-3 all set)
    __raw_writel(     0x112, SYS_TPIU_STM_BASE + 0x304); // Formatter and Flush control
    val = __raw_readl(SYS_TPIU_STM_BASE + 0x304); // Formatter and Flush control
    __raw_writel(     0x162, SYS_TPIU_STM_BASE + 0x304); // Formatter and Flush control
    val = __raw_readl(SYS_TPIU_STM_BASE + 0x304); // Formatter and Flush control
#if 0 // STM Walking ones test mode, only for testing timing, not for normal trace operation!
    __raw_writel(0x00020001, SYS_TPIU_STM_BASE + 0x204); // STM Walking ones test mode
#endif
  }

#ifdef CONFIG_U2_STM_ETR_TO_SDRAM
  if (stm_select >= 0 /* Could just check if SYS & CPU CoreSight is enabled */ ) {
    conf_stm_etr_to_sdram();
  }
#endif /* CONFIG_U2_STM_ETR_TO_SDRAM */

  return stm_select;
}
// EXPORT_SYMBOL(u2evm_get_stm_select);

static int __init stm_sdhi0_regulator_late_init(void)
{
        int ret = 0;
	struct regulator *regulator;

	if (0 != u2evm_get_stm_select()) {
	  /* STM traces not enabled on SDHI0 interface, */
	  /* SD-Card driver takes care of SDHI0 regualtor. */
	  return ret;
	}

	printk(KERN_ALERT "%s 1.8V >>", __func__);

	/* POWER ON */

	regulator = regulator_get(NULL, "vio_sd");
	if (IS_ERR(regulator)) {
	  printk(KERN_ALERT "%s:err reg_get1 vio_sd\n", __func__);
	  return ret;
	}

	ret = regulator_enable(regulator);
	if (ret)
	  printk(KERN_ALERT "%s:err reg_ena1 vio_sd %d\n",
		 __func__ , ret);

	regulator_put(regulator);

	regulator = regulator_get(NULL, "vsd");
	if (IS_ERR(regulator)) {
	  printk(KERN_ALERT "%s:err reg_get1 vsd\n", __func__);
	  return ret;
	}

	ret = regulator_enable(regulator);
	if (ret)
	  printk(KERN_ALERT "%s:err reg_ena1 vsd %d\n",
		 __func__ , ret);

	regulator_put(regulator);

	/* SET 1.8V */

	regulator = regulator_get(NULL, "vio_sd");
	if (IS_ERR(regulator)) {
	  printk(KERN_ALERT "%s:err reg_get2 vio_sd\n", __func__);
	  return ret;
	}

	if (regulator_is_enabled(regulator)) {
	  ret = regulator_disable(regulator);
	  if (ret)
	    printk(KERN_ALERT "%s:err reg_dis vio_sd %d\n",
		   __func__ , ret);
	}

	ret = regulator_set_voltage(regulator, E1_8_V, E1_8_V);
	if (ret)
	  printk(KERN_ALERT "%s:err reg_set_v vio_sd %d\n",
		 __func__, ret);

	ret = regulator_enable(regulator);
	if (ret)
	  printk(KERN_ALERT "%s:err reg_ena2 vio_sd %d\n",
		 __func__, ret);

	regulator_put(regulator);

	regulator = regulator_get(NULL, "vsd");
	if (IS_ERR(regulator)) {
	  printk(KERN_ALERT "%s:err reg_get2 vsd\n", __func__);
	  return ret;
	}

	if (regulator_is_enabled(regulator)) {
	  ret = regulator_disable(regulator);
	  if (ret)
	    printk(KERN_ALERT "%s:err reg_dis vsd %d\n",
		   __func__ , ret);
	}

	ret = regulator_set_voltage(regulator, E1_8_V, E1_8_V);
	if (ret)
	  printk(KERN_ALERT "%s:err reg_set_v vsd %d\n",
		 __func__, ret);

	ret = regulator_enable(regulator);
	if (ret)
	  printk(KERN_ALERT "%s:err reg_ena2 vsd %d\n",
		 __func__, ret);

	regulator_put(regulator);

	printk(KERN_ALERT "%s <<", __func__);
	return ret;
}
late_initcall(stm_sdhi0_regulator_late_init);
