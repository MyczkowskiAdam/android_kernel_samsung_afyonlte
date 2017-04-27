#ifndef __SH_KEYSC_H__
#define __SH_KEYSC_H__

#define SH_KEYSC_MAXKEYS 64
#define WA_EOS_E132_KEYSC	(1 << 0)	

struct sh_keysc_info {
	enum { SH_KEYSC_MODE_1, SH_KEYSC_MODE_2, SH_KEYSC_MODE_3,
	       SH_KEYSC_MODE_4, SH_KEYSC_MODE_5, SH_KEYSC_MODE_6 } mode;
	int scan_timing; /* 0 -> 7, see KYCR1, SCN[2:0] */
	int delay;
	int kycr2_delay;
	int keycodes[SH_KEYSC_MAXKEYS]; /* KEYIN * KEYOUT */
	int automode;
	int scan_timing1; /* 0 -> 15, see A_KYCR2, SCN1[3:0] */
	int scan_timing2; /* 0 -> 11, see A_KYCR2, SCN2[3:0] */

	int wakeup;
	int flags;
};

#endif /* __SH_KEYSC_H__ */
