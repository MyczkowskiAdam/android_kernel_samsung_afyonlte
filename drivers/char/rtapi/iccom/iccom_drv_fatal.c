/*
 * iccom_drv_com.c
 *	 Inter Core Communication driver common function file.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "log_kernel.h"

#define KLOG_HEADER "[RTAPI-FATAL]"
#define KLG(...) MSG_ERROR(KLOG_HEADER __VA_ARGS__)

struct fatal_header {
	unsigned int	fatal_type;
	union {
		struct {
			unsigned int	type;
			unsigned int	ercd;
			unsigned int	inf1;
			unsigned int	inf2;
			unsigned int	dummy[3];
			unsigned int	arg[4];
			unsigned int	ctx;
		} case1;
		struct {
			unsigned int	type;
			unsigned int	ercd;
			unsigned int	inf1;
			unsigned int	inf2;
			unsigned short	tid_from;
			unsigned short	tid_to;
			unsigned short	mbxid;
			unsigned short	mplid;
			unsigned short	msgid;
			unsigned short	functionid;
		} case2;
		struct {
			unsigned short	tid;
			unsigned char	kind;
			unsigned char	life;
		} case3;
		struct {
			unsigned int	type;
			unsigned int	ercd;
			unsigned int	inf1;
			unsigned int	inf2;
			unsigned int	TEA;
			unsigned int	PR;
			unsigned int	R[16];
		} case4;
		struct {
			unsigned int	type;
			unsigned int	ercd;
			unsigned int	inf1;
			unsigned int	inf2;
		} case5;
		struct {
			unsigned int	regs[35];
		} case6;
		struct {
			unsigned int	regs[35];
		} case7;
		struct {
			unsigned int	type;
			unsigned int	ercd;
			unsigned int	inf1;
			unsigned int	inf2;
			unsigned short	tid_from;
			unsigned short	tid_to;
			unsigned short	mbxid;
			unsigned short	mplid;
			unsigned short	msgid;
			unsigned short	functionid;
			unsigned int	arg[4];
			unsigned int	ctx;
		} case8;
		struct {
			unsigned int	type;
			unsigned int	ercd;
			unsigned int	inf1;
			unsigned int	inf2;
			struct meram_info {
				unsigned int MEACTSTn[4];
				unsigned int MEnCTRL[32];
				unsigned int MEnBSIZ[32];
				unsigned int MEnMCNF[32];
				unsigned int MEnSSARA[32];
				unsigned int MEnSSARB[32];
				unsigned int MEnSBSIZ[32];
			} meram;
			struct vio6_info {
				unsigned int CMDn[4];
				unsigned int RPFn_Y[4];
				unsigned int RPFn_C0[4];
				unsigned int RPFn_C1[4];
				unsigned int RPFn_AI[4];
				unsigned int WPFn_Y[4];
				unsigned int WPFn_C0[4];
				unsigned int WPFn_C1[4];
			} vi6[2];
			struct tddmac_info {
				unsigned int CHnCTRL[8];
				unsigned int CHnSAR[8];
				unsigned int CHnDAR[8];
			} tdd;
		} case8m;
	} cases;
};


/******************* common ********************/
struct code_string {
	unsigned int	code;
	char *string;
};
static void output_code_string(
	const char *head,
	unsigned int code,
	const struct code_string *table)
{
	while ((table->code != 0) || (table->string != NULL)) {
		if (table->code == code) {
			KLG("%s[0x%x]%s", head, code, table->string);
			break;
		}
		table++;
	}
}

static const struct code_string service_call_retval[] = {
	{0x00000000, "E_OK : Normal end."},
	{0xFFFFFFF7, "E_NOSPT : Unsupported function."},
	{0xFFFFFFF6, "E_RSFN : Service call is not implemented."},
	{0xFFFFFFF5, "E_RSATR : Reserved attribute."},
	{0xFFFFFFEF, "E_PAR : Parameter error."},
	{0xFFFFFFEE, "E_ID : Invalid ID number."},
	{0xFFFFFFE7, "E_CTX : Context error."},
	{0xFFFFFFE6, "E_MACV : Illegal memory access ."},
	{0xFFFFFFE5, "E_OACV : Illegal object access."},
	{0xFFFFFFE4, "E_ILUSE : Invalid service call use."},
	{0xFFFFFFDF, "E_NOMEM : Insufficient memory."},
	{0xFFFFFFDE, "E_NOID : Insufficient ID number."},
	{0xFFFFFFD7, "E_OBJ : Invalid object status."},
	{0xFFFFFFD6, "E_NOEXS : No object exists."},
	{0xFFFFFFD5, "E_QOVER : Queuing or Nesting overflow."},
	{0xFFFFFFCF, "E_RLWAI : Wait state forced release."},
	{0xFFFFFFCE, "E_TMOUT : Polling fail or Timeout."},
	{0xFFFFFFCD, "E_DLT : Waiting object deleted."},
	{0xFFFFFFCC, "E_CLS : Waiting object state change."},
	{0xFFFFFFC7, "E_WBLK : Non blocking."},
	{0xFFFFFFC6, "E_BOVR : Buffer overflow."},
	{0, NULL}
};
static struct code_string task_name[] = {
	{1, "Status Control"},
	{2, "Watchdog"},
	{3, "Timer"},
	{5, "DEBUG"},
	{6, "Memory Control"},
	{13, "Memory Control Sub"},
	{14, "Memory Callback"},
	{7, "LCD"},
	{8, "SLEEP"},
	{18, "ICCOM"},
	{27, "IP Arbitration"},
	{68, "MERAM"},
	{56, "SCI"},
	{57, "serial receive"},
	{15, "Photoimage - SPM_JPU"},
	{16, "Photoimage - SPC"},
	{17, "Photoimage - SPM"},
	{19, "Photoimage - SPT"},
	{22, "Video Decorder Post"},
	{20, "Display"},
	{21, "Display2"},
	{60, "Graphics - DRV"},
	{50, "ICCOM (sysdwn)"},
	{51, "PMBM (sysdwn)"},
	{52, "HPB (sysdwn)"},
	{53, "SHMMU (sysdwn)"},
	{72, "OMXSM"},
	{74, "OMXCH"},
	{85, "OMXST"},
	{86, "OMXSC"},
	{87, "OMXSMC"},
	{88, "OMXSH"},
	{75, "OMX1"},
	{76, "OMX2"},
	{77, "OMX3"},
	{78, "OMX4"},
	{79, "OMX5"},
	{80, "OMX6"},
	{81, "OMX7"},
	{82, "OMX8"},
	{83, "OMX9"},
	{84, "OMX10"},
	{89, "OMX11"},
	{90, "OMX12"},
	{91, "OMX13"},
	{92, "OMX14"},
	{93, "OMX15"},
	{94, "OMX16"},
	{95, "OMX17"},
	{96, "OMX18"},
	{65, "OMX19"},
	{66, "OMX20"},
	{30, "Camera - ISP_STATE"},
	{31, "Camera - ISP_CAM"},
	{32, "Camera - ISP_EC"},
	{33, "Camera - ISP_SCOM"},
	{34, "Camera - ISP_CAM_SUB"},
	{35, "Camera - ISP_IC"},
	{36, "Camera - ISP_PA"},
	{37, "Camera - ISP_FACE"},
	{38, "Camera - ISP_AS"},
	{39, "Camera - ISP_TIMER"},
	{40, "Camera - ISP_IC_SUB"},
	{41, "Camera - ISP_ECRCV"},
	{42, "Camera - ISP_ICRCV"},
	{43, "Camera - ISP_MOCSND"},
	{44, "Camera - ISP_SENSORSND"},
	{45, "Camera - ISP_ICONVC"},
	{46, "Camera - ISP_ICONV"},
	{0, NULL}
};
static struct code_string interrupt_source[] = {
	{0x000, "Power-on reset, H-UDI reset"},
	{0x020, "Manual reset"},
	{0x040, "TLB miss exception (Read)"},
	{0x060, "TLB miss exception (Write)"},
	{0x080, "Initial page write exception"},
	{0x0A0, "TLB protection exception (Read)"},
	{0x0C0, "TLB protection exception (Write)"},
	{0x0E0, "Address error (Read)"},
	{0x100, "Address error (Write)"},
	{0x140, "Instruction TLB multiple hit exception,"
			" Data TLB multiple hit exception"},
	{0x160, "Unconditional trap (TRAPA instruction)"},
	{0x180, "General illegal instruction exception"},
	{0x1A0, "Slot illegal instruction exception"},
	{0x1C0, "NMI"},
	{0x1E0, "User breakpoint trap"},
	/*{0x2200, "IRQ00"},*/
	{0x800, "RTDMAC_DEI0"},
	{0x820, "RTDMAC_DEI1"},
	{0x840, "RTDMAC_DEI2"},
	{0x860, "RTDMAC_DEI3"},
	{0x980, "VCP"},
	{0x9E0, "SGX-3DG"},
	{0xA00, "2DDM0"},
	{0xAE0, "IIC2"},
	{0xBC0, "DADERR"},
	{0xBE0, "KEY"},
	{0xE60, "IIC0"},
	{0xE80, "TMU1_TUNI0"},
	{0xEA0, "TMU1_TUNI1"},
	{0xEC0, "TMU1_TUNI2"},
	{0xF00, "CMT0_0"},
	{0xF20, "TSIF"},
	{0xF40, "CMT0_1"},
	{0xF60, "LMB"},
	{0x400, "CTI"},
	{0x480, "ICB"},
	{0x560, "JPU"},
	{0x580, "LCDC0"},
	{0x5A0, "LCRC"},
	{0x1400, "IIC4"},
	{0x1420, "IIC5"},
	{0x1440, "IICB"},
	{0x1460, "IIC1"},
	{0x1480, "SCIFA0"},
	{0x14A0, "SCIFA1"},
	{0x14C0, "SCIFA2"},
	{0x14E0, "SCIFA3"},
	{0x1500, "SCIFB0"},
	{0x1520, "SCIFB1"},
	{0x1540, "SCIFB2"},
	{0x1560, "SCIFB3"},
	{0x1580, "CMT1_0"},
	{0x15A0, "CMT1_1"},
	{0x15C0, "CMT1_2"},
	{0x15E0, "CMT1_3"},
	{0x1600, "CMT1_4"},
	{0x1620, "CMT1_5"},
	{0x1640, "CMT1_6"},
	{0x1660, "CMT1_7"},
	{0x1680, "MSOF0"},
	{0x16A0, "MSOF1"},
	{0x16C0, "MSOF2"},
	{0x16E0, "MSOF3"},
	{0x1700, "MSOF4"},
	{0x1740, "IIC3"},
	{0x17A0, "SCIRX"},
	{0x17C0, "DSITX0"},
	{0x17E0, "DSITX1"},
	{0x1800, "SPU2A0"},
	{0x1820, "SPU2A1"},
	{0x1840, "FSI"},
	{0x1880, "LCDC1"},
	{0x19A0, "TMU2_TUNI0"},
	{0x19C0, "TMU2_TUNI1"},
	{0x19E0, "TMU2_TUNI2"},
	{0x1A00, "MFIS"},
	{0x1A20, "CPORT2R"},
	{0x1A40, "TMU3_TUNI0"},
	{0x1A60, "TMU3_TUNI1"},
	{0x1A80, "TMU3_TUNI2"},
	{0x1AA0, "Thermal Sensor"},
	{0x1AC0, "CPG"},
	{0x1B40, "SCUW"},
	{0x1B60, "VSP"},
	{0x1BC0, "JPU6E"},
	{0x1BE0, "CSIRX1"},
	{0x1C00, "DSITX1_0"},
	{0x1C20, "DSITX1_1"},
	{0x1D40, "ISP_PRE"},
	{0x1D60, "ISP_POS"},
	{0x1D80, "ISP_FCB1"},
	{0x1DA0, "ISP_FCB0"},
	{0x1DC0, "RCU1"},
	{0x1DE0, "RCU0"},
	{0x1E00, "VINT"},
	{0x1E20, "SPU2V"},
	{0, NULL}
};
static char *get_interrupt_source(unsigned int code)
{
	static char irq_string[6] = "IRQ";
	struct code_string *table = interrupt_source;
	if ((0x2200 <= code) && (code <= 0x29E0)) {
		irq_string[3] = ((code-0x2200)/0x20)/10+'0';
		irq_string[4] = ((code-0x2200)/0x20)%10+'0';
		irq_string[5] = '\0';
		return irq_string;
	}
	while ((table->code != 0) || (table->string != NULL)) {
		if (table->code == code)
			return table->string;
		table++;
	}
	return "!!undefined interrupt source!!";
}
static char *get_task_name(unsigned int code)
{
	struct code_string *table = task_name;
	while ((table->code != 0) || (table->string != NULL)) {
		if (table->code == code) {
			return table->string;
			break;
		}
		table++;
	}
	return "!!undefined task name!!";
}

/******************* case 1: uITRON service call wrapper ********************/
static const struct code_string case1_type[] = {
	{0x1001, "WRAP_acre_alm"},
	{0x1002, "WRAP_acre_flg"},
	{0x1003, "WRAP_acre_sem"},
	{0x1004, "WRAP_act_tsk"},
	{0x1005, "WRAP_chg_ims"},
	{0x1006, "WRAP_clr_flg"},
	{0x1007, "WRAP_cre_mbx"},
	{0x1008, "WRAP_cre_sem"},
	{0x1009, "WRAP_cre_tsk"},
	{0x100a, "WRAP_def_inh"},
	{0x100b, "WRAP_del_alm"},
	{0x100c, "WRAP_del_flg"},
	{0x100d, "WRAP_del_mbx"},
	{0x100e, "WRAP_del_sem"},
	{0x100f, "WRAP_del_tsk"},
	{0x1010, "WRAP_dis_dsp"},
	{0x1011, "WRAP_dly_tsk"},
	{0x1012, "WRAP_ena_dsp"},
	{0x1013, "WRAP_get_ims"},
	{0x1014, "WRAP_get_mpl"},
	{0x1015, "WRAP_get_tid"},
	{0x1016, "WRAP_get_tim"},
	{0x1017, "WRAP_ichg_ims"},
	{0x1018, "WRAP_iclr_flg"},
	{0x1019, "WRAP_iget_ims"},
	{0x101a, "WRAP_ipget_mpl"},
	{0x101b, "WRAP_irel_mpl"},
	{0x101c, "WRAP_iset_flg"},
	{0x101d, "WRAP_isnd_mbx"},
	{0x101e, "WRAP_iwup_tsk"},
	{0x101f, "WRAP_loc_cpu"},
	{0x1020, "WRAP_pget_mpl"},
	{0x1021, "WRAP_pol_flg"},
	{0x1022, "WRAP_prcv_mbx"},
	{0x1023, "WRAP_rcv_mbx"},
	{0x1024, "WRAP_ref_flg"},
	{0x1025, "WRAP_ref_mpl"},
	{0x1026, "WRAP_rel_mpl"},
	{0x1027, "WRAP_set_flg"},
	{0x1028, "WRAP_sig_sem"},
	{0x1029, "WRAP_slp_tsk"},
	{0x102a, "WRAP_snd_mbx"},
	{0x102b, "WRAP_sta_alm"},
	{0x102c, "WRAP_sta_cyc"},
	{0x102d, "WRAP_stp_cyc"},
	{0x102e, "WRAP_ter_tsk"},
	{0x102f, "WRAP_trcv_mbx"},
	{0x1030, "WRAP_tslp_tsk"},
	{0x1031, "WRAP_twai_sem"},
	{0x1032, "WRAP_unl_cpu"},
	{0x1033, "WRAP_unl_cpu"},
	{0x1034, "WRAP_wai_flg"},
	{0x1035, "WRAP_wai_sem"},
	{0x1036, "WRAP_acre_mbx"},
	{0x1037, "WRAP_twai_flg"},
	{0x1038, "WRAP_can_wup"},
	{0x1039, "WRAP_chg_pri"},
	{0x103a, "WRAP_ref_mbx"},
	{0x103b, "WRAP_iref_mpl"},
	{0x103c, "WRAP_rel_mpf"},
	{0x103d, "WRAP_get_pri"},
	{0x103e, "WRAP_rel_wai"},
	{0x103f, "WRAP_wup_tsk"},
	{0x1040, "WRAP_ipol_flg"},
	{0x1041, "WRAP_ipol_sem"},
	{0x1042, "WRAP_isig_sem"},
	{0x1043, "WRAP_ref_tst"},
	{0x1044, "WRAP_iref_tst"},
	{0x1045, "WRAP_iref_flg"},
	{0, NULL}
};
static const char * const case1_context[] = {
	"Task context",
	"Non-task context"
};
static void case_wrap_itron(struct fatal_header *fatal)
{
	output_code_string("", fatal->cases.case1.type, case1_type);
	KLG(" Return value from service call : 0x%08x",
		fatal->cases.case1.ercd);
	output_code_string("  =>",
		fatal->cases.case1.ercd,
		service_call_retval);
	KLG(" Wrapper function's calling program counter(+4) : 0x%08x",
		fatal->cases.case1.inf1);
	KLG(" Wrapper function's calling task ID : 0x%08x",
		fatal->cases.case1.inf2);
	KLG("  => task name : %s", get_task_name(fatal->cases.case1.inf2));
	KLG(" First argument of service call : 0x%08x",
		fatal->cases.case1.arg[0]);
	KLG(" Second argument of service call : 0x%08x",
		fatal->cases.case1.arg[1]);
	KLG(" Third argument of service call : 0x%08x",
		fatal->cases.case1.arg[2]);
	KLG(" Forth argument of service call : 0x%08x",
		fatal->cases.case1.arg[3]);
	if (fatal->cases.case1.ctx <= 1)
		KLG(" Context information : %s",
			case1_context[fatal->cases.case1.ctx]);
}


/******************* case 2: framework function ********************/
static const struct code_string case2_type[] = {
	{0x110, "Generated in smap_SendMessage()"
			" when message transmission error occurs."},
	{0x111, "Generated in smap_SendMessageI()"
			" when message transmission error occurs."},
	{0x112, "Generated in smap_RecvMessage()"
			" when message reception error occurs."},
	{0x113, "Generated in smap_RecvMessageT()"
			" when message reception error occurs."},
	{0x114, "Generated in smap_RecvMessageP()"
			" when message reception error occurs."},
	{0x115, "Generated in smap_SendReply()"
			" when message transmission error occurs."},
	{0x117, "Generated in smap_RecvReplyT() when message"
			" reception error occurs.(except E_TMOUT)"},
	{0x120, "Generated in smap_GetMpl() when error occurs"
			" in allocation of memory for message."},
	{0x121, "Generated in smap_iGetMpl() when error occurs"
			" in allocation of memory for message."},
	{0x122, "Generated in smap_RelMpl()"
			" when error occurs in freeing memory for message."},
	{0x124, "Generated in smap_GetSelfTID()"
			" when Task ID acquisition error occurs."},
	{0x130, "Generated when process for the case Function and Message"
			" have identical ID is not registered to framework."},
	{0x131, "Generated when smapframe_SendFunction()"
			" is called from non-task context."},
	{0x132, "Generated in smapframe_SendFunction()"
			" when wait time for receiving message exceeds 5 seconds, or "
			"Generated in smapframe_SendFunctionT()"
			" when wait time for receiving message exceeds"
			" argument 'timeout' milliseconds"},
	{0, NULL}
};
static const struct code_string case2_type_service_call[] = {
	{0x110, "snd_mbx"},
	{0x111, "isnd_mbx"},
	{0x112, "rcv_mbx"},
	{0x113, "trcv_mbx"},
	{0x114, "prcv_mbx"},
	{0x115, "snd_mbx"},
	{0x117, "trcv_mbx"},
	{0x120, "get_mpf"},
	{0x121, "ipget_mpf"},
	{0x122, "rel_mpf"},
	{0x124, "get_tid"},
	{0, NULL}
};
static void case_framework(struct fatal_header *fatal)
{
	output_code_string("", fatal->cases.case2.type, case2_type);
	output_code_string(" (service call)",
		fatal->cases.case2.type, case2_type_service_call);
	switch (fatal->cases.case2.type) {
	case 0x110:
	case 0x111:
	case 0x112:
	case 0x113:
	case 0x114:
	case 0x122:
		KLG(" Return value from service call : 0x%08x",
			fatal->cases.case2.ercd);
		output_code_string("  =>",
			fatal->cases.case2.ercd, service_call_retval);
		KLG(" Message area pointer : 0x%08x", fatal->cases.case2.inf1);
		break;
	case 0x115:
	case 0x117:
	case 0x124:
		KLG(" Return value from service call : 0x%08x",
			fatal->cases.case2.ercd);
		output_code_string("  =>",
			fatal->cases.case2.ercd, service_call_retval);
		break;
	case 0x120:
		KLG(" Return value from service call : 0x%08x",
			fatal->cases.case2.ercd);
		output_code_string("  =>",
			fatal->cases.case2.ercd, service_call_retval);
		KLG(" Size : %d", fatal->cases.case2.inf1);
		break;
	case 0x121:
		if (fatal->cases.case2.ercd != 0xFFFFFFEF) {
			KLG(" Return value from service call : 0x%08x",
				fatal->cases.case2.ercd);
			output_code_string("  =>",
				fatal->cases.case2.ercd, service_call_retval);
		} else {
			KLG(
				" When value larger than one block of"
				" fixed length memory pool is assigned to uSize,"
				" an argument of smap_iGetMpl()."
			);
		}
		KLG(" Size : %d", fatal->cases.case2.inf1);
		break;
	case 0x130:
		KLG(" Message ID : 0x%08x", fatal->cases.case2.inf1);
		KLG(" Function ID : 0x%08x", fatal->cases.case2.inf2);
		break;
	}

	KLG(" Source Task ID : 0x%04x", fatal->cases.case2.tid_from);
	KLG("  => task name : %s", get_task_name(fatal->cases.case2.tid_from));
	KLG(" Destination Task ID : 0x%04x", fatal->cases.case2.tid_to);
	KLG("  => task name : %s", get_task_name(fatal->cases.case2.tid_to));
	KLG(" Mailbox ID : 0x%04x", fatal->cases.case2.mbxid);
	KLG(" Memory pool ID : 0x%04x", fatal->cases.case2.mplid);
	KLG(" Message ID : 0x%04x", fatal->cases.case2.msgid);
	KLG(" Function ID : 0x%04x", fatal->cases.case2.functionid);
}


/******************* case 3: watchdog timeout ********************/
static const char * const case3_kind[] = {
	"Unregisterd. (WDT_TASK_NOT_ENTRY)",
	"Task being monitored at message waiting status in SMB. (WDT_ACTION_WAIT)",
	"No response to monitor notification event which was sent to task being monitored. (WDT_RESPONSE_WAIT)"
};
static void case_watchdog(struct fatal_header *fatal)
{
	KLG(" Task ID : 0x%04x", fatal->cases.case3.tid);
	KLG("  => task name : %s", get_task_name(fatal->cases.case3.tid));
	if (fatal->cases.case3.kind <= 2)
		KLG(" Task status : %s", case3_kind[fatal->cases.case3.kind]);
	KLG(" LIFE point : %d", fatal->cases.case3.life);
}


/******************* case 4: fatal exception ********************/
static void case_exception(struct fatal_header *fatal)
{
	int i;
	KLG(" Interrupt source[0x%x]%s",
		fatal->cases.case4.ercd,
		get_interrupt_source(fatal->cases.case4.ercd));
	KLG(" Program counter : 0x%08x", fatal->cases.case4.inf1);
	KLG(" SR register : 0x%08x", fatal->cases.case4.inf2);
	KLG(" TEA register : 0x%08x", fatal->cases.case4.TEA);
	KLG(" PR register : 0x%08x", fatal->cases.case4.PR);
	for (i = 0; i < 16; i++)
		KLG(" R%d register : 0x%08x", i, fatal->cases.case4.R[i]);
}


/******************* case 6: manual reset ********************/
/******************* case 7: SDRAM corruption ********************/
static const char * const case6_regs[] = {
	"R0_BANK1",
	"R1_BANK1",
	"R2_BANK1",
	"R3_BANK1",
	"R4_BANK1",
	"R5_BANK1",
	"R6_BANK1",
	"R7_BANK1",
	"R8",
	"R9",
	"R10",
	"R11",
	"R12",
	"R13",
	"R14",
	"R15",
	"R0_BANK0",
	"R1_BANK0",
	"R2_BANK0",
	"R3_BANK0",
	"R4_BANK0",
	"R5_BANK0",
	"R6_BANK0",
	"R7_BANK0",
	"SR",
	"GBR",
	"SSR",
	"SPC",
	"SGR",
	"VBR",
	"PR",
	"MACH",
	"MACL",
	"EXPEVT",
	"INTEVT"
};
static void case_manual_reset(struct fatal_header *fatal)
{
	int i;
	for (i = 0; i < 33; i++) {
		KLG(" register [%s] : 0x%08x",
			case6_regs[i], fatal->cases.case6.regs[i]);
	}
	KLG(" EXPEVT[0x%x]%s",
		fatal->cases.case6.regs[33],
		get_interrupt_source(fatal->cases.case6.regs[33]));
	KLG(" INTEVT[0x%x]%s",
		fatal->cases.case6.regs[34],
		get_interrupt_source(fatal->cases.case6.regs[34]));
}
static void case_sdram_corruption(struct fatal_header *fatal)
{
	int i;
	for (i = 0; i < 33; i++)
		KLG(" register [%s] : 0x%08x",
			case6_regs[i], fatal->cases.case7.regs[i]);
	KLG(" EXPEVT[0x%x]%s",
		fatal->cases.case7.regs[33],
		get_interrupt_source(fatal->cases.case7.regs[33]));
	KLG(" INTEVT[0x%x]%s",
		fatal->cases.case7.regs[34],
		get_interrupt_source(fatal->cases.case7.regs[34]));
}


/******************* case 8: vsys_dwn() is called ********************/
static const struct code_string case8_type[] = {
	{0x2001, "call from Status control task (stcon)"},
	{0x2006, "call from Memory manager task (mem)"},
	{0x2007, "call from LCD task"},
	{0x2008, "call from Sleep task"},
	{0x2012, "call from ICCOM driver task (RT-Domain)"},
	{0x2016, "call from VideoPost task (mpcd)"},
	{0x2032, "call from ICCOM driver (RT-Domain)"},
	{0x2033, "call from PMB space management function"},
	{0x2034, "call from HPB bus semaphore"},
	{0x2035, "call from SHMMU"},
	{0x203C, "call from Graphics task (DRV)"},
	{0, NULL}
};
static const char * const case8_on0off1[] = { "ON", "OFF" };
static const char * const case8_gplane[] = { "Y", "C0", "C1" };
static void case_vsys_dwn(struct fatal_header *fatal)
{
	output_code_string("", fatal->cases.case8.type, case8_type);
	switch (fatal->cases.case8.type) {
	case 0x2001:
		if (fatal->cases.case8.inf2 != 0) { /* case1 */
			KLG(" case1:Register setting failed.");
			KLG("  - Module use request upper limit exceeded.");
			KLG("  - Module stop request at module halt.");
			KLG("  - Wakeup register setting failed.");
			KLG("  - Power-down register setting failed.");
			KLG("  - Clock On/Off setting failed.");
			KLG("  Program counter : 0x%08x",
				fatal->cases.case8.ercd);
			KLG("  Register value  : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  Module/Domain/Clock ID : 0x%08x",
				fatal->cases.case8.inf2);
		} else { /* case2 */
			KLG(" case2:Driver initialization failed.");
			KLG(
				"       or Task initialization by API for Load failed."
			);
			KLG("  Return code : 0x%08x", fatal->cases.case8.ercd);
			KLG("  Program counter : 0x%08x",
				fatal->cases.case8.inf1);
		}
		break;
	case 0x2006:		/* memory manager task */
		switch (fatal->cases.case8.ercd) {
		case 1:
			KLG(" case1:Invalid cache clear address.");
			KLG("  Memory start address : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  Memory end address   : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 2:
			KLG(
				" case2:Header corruption at freeing memory detected."
			);
			KLG("  Memory block requested to be freed : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  Memory block header identifier: 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 3:
			KLG(
				" case3:Corruption of sys shared memory"
				" management information detected."
			);
			KLG(
				"  Start address of corrupted sys shared region: 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  Detail of detected data corruption: 0x%08x",
				fatal->cases.case8.inf2);
			break;
		}
		break;
	case 0x2007:
		KLG(" case1:Abnormal notification type of access termination.");
		KLG(
			"  Specified LCD output buffer access termination notification type"
			" is not LCD_MODE_END (with display update) nor LCD_MODE_DRAW"
			" (without display update).");
		KLG(" case2:Abnormal start address of LCD output buffer.");
		KLG(
			"  Specified LCD output buffer start address is not equal to LCD"
			" output buffer start address of LCD task management.");
		break;
	case 0x2008:		/* sleep task */
		KLG(
			" First message which sleep task receives is"
			" not task initialize message (FUNC_TASK_INIT)");
		KLG("  Message type of received message : 0x%08x",
			fatal->cases.case8.inf1);
		KLG("  Function ID of received message : 0x%08x",
			fatal->cases.case8.inf2);
		KLG("  Source task ID : 0x%04x",
			fatal->cases.case8.tid_from);
		KLG("   => task name : %s",
			get_task_name(fatal->cases.case8.tid_from));
		KLG("  Source task mailbox ID : 0x%04x",
			fatal->cases.case8.mbxid);
		break;
	case 0x2012:
		KLG(
			" In read process from command transfer region,"
			" WRAP_get_mpl return value!=E_OK.");
		KLG("  WRAP_get_mpl return value : 0x%08x",
			fatal->cases.case8.ercd);
		KLG("  function ID : 0x%08x", fatal->cases.case8.inf2);
		break;
	case 0x2016:
		KLG(" Transfer process failed.");
		KLG(
			" Transfer process execution function executed"
			" without transfer request.");
		KLG("  Program counter : 0x%08x",
			fatal->cases.case8.inf1);
		KLG("  Number of execusion lines : 0x%08x",
			fatal->cases.case8.inf2);
		break;
	case 0x2032:
		switch (fatal->cases.case8.inf1) {	/* !!not ercd */
		case 1:
			KLG(
				" case1:In write start process toward command transfer region,"
				" WRAP_wai_sem return value!=E_OK.");
			KLG("  WRAP_wai_sem return value : 0x%08x",
				fatal->cases.case8.ercd);
			KLG("  function id : 0x%08x", fatal->cases.case8.inf2);
			break;
		case 2:
			KLG(
				" case2:In read process from command transfer region,"
				" gSHIccom_f_waitstatus evaluated at more than 2.");
			KLG("  gSHIccom_f_waitstatus : 0x%08x",
				fatal->cases.case8.ercd);
			break;
		case 3:
			KLG(
				" case3:Unexpected behavior of command transfer region"
				" status register");
			KLG(
				"  Command transfer region status register value : 0x%08x",
				fatal->cases.case8.ercd);
			break;
		}
		break;
	case 0x2033:		/* PMB space */
		switch (fatal->cases.case8.ercd) {
		case 1:
			KLG(
				" case1:PMB registraion remains at callback process.");
			KLG("  PMB space virtual address : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  Original address : 0x%08x",
				fatal->cases.case8.inf2);
			KLG("  Chunk ID : 0x%04x", fatal->cases.case8.tid_from);
			KLG("  Entry number counter : 0x%04x",
				fatal->cases.case8.tid_to);
			KLG("  Duplacate open counter : 0x%04x",
				fatal->cases.case8.mbxid);
			KLG("  T/L : %s",
				case8_on0off1[fatal->cases.case8.mplid]);
			KLG("  L/I : %s",
				case8_on0off1[fatal->cases.case8.functionid]);
			KLG("  Callback address 0 : 0x%08x",
				fatal->cases.case8.arg[0]);
			KLG("  Callback address 1 : 0x%08x",
				fatal->cases.case8.arg[1]);
			KLG("  Callback address 2 : 0x%08x",
				fatal->cases.case8.arg[2]);
			KLG("  Callback address 3 : 0x%08x",
				fatal->cases.case8.arg[3]);
			KLG("  Callback address 4 : 0x%08x",
				fatal->cases.case8.ctx);
			break;
		case 2:
			KLG(" case2:Exception occurs at IPMMU.");
			KLG("  IPMMU Exception Address (IPMMUI.IMEAR) : 0x%08x",
				fatal->cases.case8.inf2);
			KLG("  IPMMU Status (IPMMUI.IMSTR) : 0x%08x",
				fatal->cases.case8.inf1);
			/* IMSTR bit status */
			if (fatal->cases.case8.inf1&0x00000001) {
				KLG(
					"   bit TF : a translation falut occurs due to"
					" a hardware page table walk.");
			}
			if (fatal->cases.case8.inf1&0x00000002) {
				KLG(
					"   bit PF : an access right violation occurs.");
			}
			if (fatal->cases.case8.inf1&0x00000004) {
				KLG(
					"   bit ABORT : an error response is returned for"
					" a hardware page table walk.");
			}
			if (fatal->cases.case8.inf1&0x00000010) {
				KLG(
					"   bit MHIT : either of the following exceptions occurs.");
				KLG("            - TLB multiple hits");
				KLG("            - PMB multiple hits");
				KLG("            - PMB miss");
			}
			{
				int i;
				/* MERAM register */
				KLG("  MEACTST  : 0x%08x 0x%08x 0x%08x 0x%08x",
				 fatal->cases.case8m.meram.MEACTSTn[0],
				 fatal->cases.case8m.meram.MEACTSTn[1],
				 fatal->cases.case8m.meram.MEACTSTn[2],
				 fatal->cases.case8m.meram.MEACTSTn[3]);
				for (i = 0; i < 32; i += 4)
					KLG(
					"  ME%02dCTRL : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.meram.MEnCTRL[i],
					fatal->cases.case8m.meram.MEnCTRL[i+1],
					fatal->cases.case8m.meram.MEnCTRL[i+2],
					fatal->cases.case8m.meram.MEnCTRL[i+3]
					);
				for (i = 0; i < 32; i += 4)
					KLG(
					"  ME%02dBSIZE: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.meram.MEnBSIZ[i],
					fatal->cases.case8m.meram.MEnBSIZ[i+1],
					fatal->cases.case8m.meram.MEnBSIZ[i+2],
					fatal->cases.case8m.meram.MEnBSIZ[i+3]
					);
				for (i = 0; i < 32; i += 4)
					KLG(
					"  ME%02dMCNF : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.meram.MEnMCNF[i],
					fatal->cases.case8m.meram.MEnMCNF[i+1],
					fatal->cases.case8m.meram.MEnMCNF[i+2],
					fatal->cases.case8m.meram.MEnMCNF[i+3]
					);
				for (i = 0; i < 32; i += 4)
					KLG(
					"  ME%02dSSARA: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.meram.MEnSSARA[i],
					fatal->cases.case8m.meram.MEnSSARA[i+1],
					fatal->cases.case8m.meram.MEnSSARA[i+2],
					fatal->cases.case8m.meram.MEnSSARA[i+3]
					);
				for (i = 0; i < 32; i += 4)
					KLG(
					"  ME%02dSSARB: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.meram.MEnSSARB[i],
					fatal->cases.case8m.meram.MEnSSARB[i+1],
					fatal->cases.case8m.meram.MEnSSARB[i+2],
					fatal->cases.case8m.meram.MEnSSARB[i+3]
					);
				for (i = 0; i < 32; i += 4)
					KLG(
					"  ME%02dSBSIZ: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.meram.MEnSBSIZ[i],
					fatal->cases.case8m.meram.MEnSBSIZ[i+1],
					fatal->cases.case8m.meram.MEnSBSIZ[i+2],
					fatal->cases.case8m.meram.MEnSBSIZ[i+3]
					);
				/* VSP register */
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_CMD%d   : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].CMDn[i],
					fatal->cases.case8m.vi6[0].CMDn[i+1],
					fatal->cases.case8m.vi6[0].CMDn[i+2],
					fatal->cases.case8m.vi6[0].CMDn[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_RPF%d_Y : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].RPFn_Y[i],
					fatal->cases.case8m.vi6[0].RPFn_Y[i+1],
					fatal->cases.case8m.vi6[0].RPFn_Y[i+2],
					fatal->cases.case8m.vi6[0].RPFn_Y[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_RPF%d_C0: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].RPFn_C0[i],
					fatal->cases.case8m.vi6[0].RPFn_C0[i+1],
					fatal->cases.case8m.vi6[0].RPFn_C0[i+2],
					fatal->cases.case8m.vi6[0].RPFn_C0[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_RPF%d_C1: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].RPFn_C1[i],
					fatal->cases.case8m.vi6[0].RPFn_C1[i+1],
					fatal->cases.case8m.vi6[0].RPFn_C1[i+2],
					fatal->cases.case8m.vi6[0].RPFn_C1[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_RPF%d_AI: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].RPFn_AI[i],
					fatal->cases.case8m.vi6[0].RPFn_AI[i+1],
					fatal->cases.case8m.vi6[0].RPFn_AI[i+2],
					fatal->cases.case8m.vi6[0].RPFn_AI[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_WPF%d_Y : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].WPFn_Y[i],
					fatal->cases.case8m.vi6[0].WPFn_Y[i+1],
					fatal->cases.case8m.vi6[0].WPFn_Y[i+2],
					fatal->cases.case8m.vi6[0].WPFn_Y[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_WPF%d_C0: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].WPFn_C0[i],
					fatal->cases.case8m.vi6[0].WPFn_C0[i+1],
					fatal->cases.case8m.vi6[0].WPFn_C0[i+2],
					fatal->cases.case8m.vi6[0].WPFn_C0[i+3]
					);
				for (i = 0; i < 4; i += 4)
					KLG(
					"  VI6_WPF%d_C1: 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.vi6[0].WPFn_C1[i],
					fatal->cases.case8m.vi6[0].WPFn_C1[i+1],
					fatal->cases.case8m.vi6[0].WPFn_C1[i+2],
					fatal->cases.case8m.vi6[0].WPFn_C1[i+3]
					);
				/* 2D-DMAC register */
				for (i = 0; i < 8; i += 4)
					KLG(
					"  CH%dCTRL : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.tdd.CHnCTRL[i],
					fatal->cases.case8m.tdd.CHnCTRL[i+1],
					fatal->cases.case8m.tdd.CHnCTRL[i+2],
					fatal->cases.case8m.tdd.CHnCTRL[i+3]
					);
				for (i = 0; i < 8; i += 4)
					KLG(
					"  CH%dSAR  : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.tdd.CHnSAR[i],
					fatal->cases.case8m.tdd.CHnSAR[i+1],
					fatal->cases.case8m.tdd.CHnSAR[i+2],
					fatal->cases.case8m.tdd.CHnSAR[i+3]
					);
				for (i = 0; i < 8; i += 4)
					KLG(
					"  CH%dDAR  : 0x%08x 0x%08x 0x%08x 0x%08x",
					i,
					fatal->cases.case8m.tdd.CHnDAR[i],
					fatal->cases.case8m.tdd.CHnDAR[i+1],
					fatal->cases.case8m.tdd.CHnDAR[i+2],
					fatal->cases.case8m.tdd.CHnDAR[i+3]
					);
			}
			break;
		}
		break;
	case 0x2034:		/* HPB bus semaphore */
		switch (fatal->cases.case8.ercd) {
		case 1:
			KLG(" case1:Grabbing HPB bus semaphore failed.");
			KLG(
				"      :More tahn 100usec for grabbing HPB bus semaphore.");
			break;
		case 2:
			KLG(" case2:HPB bus semaphore register write failed.");
			KLG(
				"      :Relevant domain does not grab semaphore at register w"
				"rite and freeing semaphore after grabbing HPB bus semaphore.");
			break;
		}
		KLG("  Register address for semaphore : 0x%08x",
			fatal->cases.case8.inf1);
		KLG("  Semaphore error register value: 0x%08x",
			fatal->cases.case8.inf2);
		break;
	case 0x2035:
		KLG(" Software table walk faild.");
		KLG(
			" At software table walk, conversion information"
			" not stored in descriptor.");
		KLG("  Virtual address : 0x%08x", fatal->cases.case8.inf1);
		KLG("  Program counter : 0x%08x", fatal->cases.case8.inf2);
		break;
	case 0x203C:
		switch (fatal->cases.case8.ercd) {
		case 0x11:
		case 0x12:
		case 0x13:
			KLG(" case1(%s): Fatal error at MERAM(RB) open.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Open return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_Open sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x21:
		case 0x22:
		case 0x23:
			KLG(" case2(%s): Fatal error at MERAM(RB) start.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Start return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_Start sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x41:
		case 0x42:
		case 0x43:
			KLG(" case3(%s): Fatal error at MERAM(RB) read stop.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_ReadStop return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_ReadStop sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x51:
		case 0x52:
		case 0x53:
			KLG(" case4(%s): Fatal error at MERAM(RB) close.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Close return value : 0x%08x",
				fatal->cases.case8.inf1);
			break;
		case 0x111:
		case 0x112:
		case 0x113:
			KLG(" case5(%s): Fatal error at MERAM(WB) open.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Open return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_Open sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x121:
		case 0x122:
		case 0x123:
			KLG(" case6(%s): Fatal error at MERAM(WB) start.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Start return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_Start sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x131:
		case 0x132:
		case 0x133:
			KLG(" case7(%s): Fatal error at MERAM(WB) write stop.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_WriteStop return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_WriteStop sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x151:
		case 0x152:
		case 0x153:
			KLG(" case8(%s): Fatal error at MERAM(WB) close.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Close return value : 0x%08x",
				fatal->cases.case8.inf1);
			break;
		case 0x211:
		case 0x212:
		case 0x213:
			KLG(" case9(%s): Fatal error at MERAM(ICB) open.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Open return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_Open sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x221:
		case 0x222:
		case 0x223:
			KLG(" case6(%s): Fatal error at MERAM(ICB) start.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Start return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_Start sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x231:
		case 0x232:
		case 0x233:
			KLG(" case7(%s): Fatal error at MERAM(ICB) write stop.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_WriteStop return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_WriteStop sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x241:
		case 0x242:
		case 0x243:
			KLG(" case7(%s): Fatal error at MERAM(ICB) read stop.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_ReadStop return value : 0x%08x",
				fatal->cases.case8.inf1);
			KLG("  drv_MERAM_ReadStop sub error code : 0x%08x",
				fatal->cases.case8.inf2);
			break;
		case 0x251:
		case 0x252:
		case 0x253:
			KLG(" case8(%s): Fatal error at MERAM(ICB) close.",
				case8_gplane[(fatal->cases.case8.ercd&0xf)-1]);
			KLG("  drv_MERAM_Close return value : 0x%08x",
				fatal->cases.case8.inf1);
			break;
		}
		break;
	default:
		KLG("Unknown fatal-type..");
		break;
	}
}

/******************* public ********************/
void iccom_debug_output_fatal_info(unsigned char *data_addr, int data_len)
{
	struct fatal_header *fatal = (struct fatal_header *)data_addr;

	switch (fatal->fatal_type) {
	case 1:
		KLG(
			"Fatal error is detected in uITRON service call wrapper function.");
		case_wrap_itron(fatal);
		break;
	case 2:
		KLG(
			"Returned values of service call in framework functions are error."
		);
		case_framework(fatal);
		break;
	case 3:
		KLG("Watchdog task detects abnormality.");
		case_watchdog(fatal);
		break;
	case 4:
		KLG("Fatal exception occurs.");
		case_exception(fatal);
		break;
	case 5:
		KLG(
			"Abort function which is called by assert function in C library.");
		break;
	case 6:
		KLG("Manual reset occurs.");
		case_manual_reset(fatal);
		break;
	case 7:
		KLG(
			"SDRAM corruption is detected at recovery from Standby to Active.");
		case_sdram_corruption(fatal);
		break;
	case 0x88888888:
		KLG("RT-Domain called vsys_dwn().");
		case_vsys_dwn(fatal);
		break;
	default:
		KLG("Unknown fatal-type.");
		break;
	};
}
