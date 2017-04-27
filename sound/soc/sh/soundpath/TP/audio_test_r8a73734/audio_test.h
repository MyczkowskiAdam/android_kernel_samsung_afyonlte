/* audio_test.h
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*!
  @file		audio_test.h

  @brief	Local definition Audio test command header file.
*/

#ifndef __AUDIO_TEST_H__
#define __AUDIO_TEST_H__

/*---------------------------------------------------------------------------*/
/* include files                                                             */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* typedef declaration                                                       */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* define macro declaration                                                  */
/*---------------------------------------------------------------------------*/
/***********************************/
/* Compile switch                  */
/***********************************/
/* #define AUDIO_TEST_DBG */

/***********************************/
/* log level                       */
/***********************************/
#define AUDIO_TEST_DRV_NAME		"audio_test"
#define AUDIO_TEST_LOG_LEVEL		"log_level"
#define AUDIO_TEST_LOG_NO_PRINT		(0x00)
#define AUDIO_TEST_LOG_ERR_PRINT	(0x01)
#define AUDIO_TEST_LOG_PROC_PRINT	(0x02)
#define AUDIO_TEST_LOG_FUNC_PRINT	(0x03)
#define AUDIO_TEST_LOG_DEBUG_PRINT	(0x04)

#define AUDIO_TEST_LOG_BIT_REG_DUMP	(0x10)
#define AUDIO_TEST_LOG_BIT_DMESG	(0x80)

#define AUDIO_TEST_LOG_LEVEL_MAX	(0xffffffff)
#define AUDIO_TEST_LOG_BYTE_LOW(sw)	((sw) & 0x0000000f)

#define AUDIO_TEST_GET_PROCESS_TIME(tv)	do_gettimeofday(&tv)
#define __PRN_AUDIO_TEST__
#define __PRN_AUDIO_TEST_ADD_TIME__

/***********************************/
/* pcm dump                        */
/***********************************/
#define AUDIO_TEST_PCMNAME		"pcmname"


/*!
  @brief	Device file name.
*/
#define AUDIO_TEST_DEVICE_NAME		"audio_test"

/***********************************/
/* use power domain		   */
/***********************************/
#define AUDIO_TEST_POWER_DOMAIN_MAX          8

/*---------------------------------------------------------------------------*/
/* structure declaration                                                     */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* define function macro declaration                                         */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* enum declaration                                                          */
/*---------------------------------------------------------------------------*/
/*!
  @brief	HW ID value.
*/
enum audio_test_hw_val {
	AUDIO_TEST_HW_CLKGEN,		/**< CLKGEN. */
	AUDIO_TEST_HW_FSI,		/**< FSI. */
	AUDIO_TEST_HW_SCUW,		/**< SCUW. */
	AUDIO_TEST_HW_MAX
};

/*!
  @brief	VCD start status.
*/
enum audio_test_vcd_stat {
	AUDIO_TEST_VCD_NONE,
	AUDIO_TEST_VCD_OK,		/**< Start OK. */
	AUDIO_TEST_VCD_NG,		/**< Start NG. */
	AUDIO_TEST_VCD_MAX
};

/*---------------------------------------------------------------------------*/
/* extern variable declaration                                               */
/*---------------------------------------------------------------------------*/
/*!
  @brief	Audio Test driver log level.
*/
extern u_int audio_test_log_level;

/*---------------------------------------------------------------------------*/
/* extern function declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* prototype declaration                                                     */
/*---------------------------------------------------------------------------*/
int audio_test_ic_read(const u_short addr, u_short *value);
int audio_test_ic_write(const u_short addr, const u_short value);
int audio_test_ic_set_device(const u_long device, const u_int mode);
int audio_test_ic_clear_device(void);
int audio_test_ic_get_device(u_long *device);
int audio_test_ic_set_volume(const u_long device, const u_int volume);
int audio_test_ic_get_volume(const u_long device, u_int *volume);
void audio_test_cnv_input_device(u_int device_type, u_long *device);
void audio_test_cnv_output_device(u_int device_type, u_long *device);
void audio_test_cnv_volume(u_int volume_type, u_int *volume);
void audio_test_cnv_oe(u_int device_type, u_int LR_type, u_short *oe);

/*---------------------------------------------------------------------------*/
/* inline function implementation                                            */
/*---------------------------------------------------------------------------*/
#ifdef __PRN_AUDIO_TEST__

#define audio_test_log_reg_dump(fmt, ...)                                    \
do {                                                                         \
	if (audio_test_log_level & AUDIO_TEST_LOG_BIT_REG_DUMP) {            \
		(audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) ?          \
			pr_err(fmt, ##__VA_ARGS__) :                         \
			pr_alert(fmt, ##__VA_ARGS__);                        \
	}                                                                    \
} while (0)


#ifdef __PRN_AUDIO_TEST_ADD_TIME__

#define audio_test_log_err(fmt, ...)                                         \
do {                                                                         \
	struct timeval tv;                                                   \
	if (AUDIO_TEST_LOG_ERR_PRINT <=                                      \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		AUDIO_TEST_GET_PROCESS_TIME(tv);                             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME           \
				" : %s():[ERR]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__,  ##__VA_ARGS__);                   \
		} else {                                                     \
			pr_alert("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME         \
				" : %s():[ERR]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_info(fmt, ...)                                        \
do {                                                                         \
	struct timeval tv;                                                   \
	if (AUDIO_TEST_LOG_PROC_PRINT <=                                     \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		AUDIO_TEST_GET_PROCESS_TIME(tv);                             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME           \
				" : %s():[INF]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		} else {                                                     \
			pr_alert("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME         \
				" : %s():[INF]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_debug(fmt, ...)                                       \
do {                                                                         \
	struct timeval tv;                                                   \
	if (AUDIO_TEST_LOG_DEBUG_PRINT <=                                    \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		AUDIO_TEST_GET_PROCESS_TIME(tv);                             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME           \
				" : %s():[DBG]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		} else {                                                     \
			pr_alert("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME         \
				" : %s():[DBG]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_efunc(fmt, ...)                                       \
do {                                                                         \
	struct timeval tv;                                                   \
	if (AUDIO_TEST_LOG_FUNC_PRINT <=                                     \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		AUDIO_TEST_GET_PROCESS_TIME(tv);                             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME           \
				" : %s():[ENT]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		} else {                                                     \
			pr_alert("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME         \
				" : %s():[ENT]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_rfunc(fmt, ...)                                       \
do {                                                                         \
	struct timeval tv;                                                   \
	if (AUDIO_TEST_LOG_FUNC_PRINT <=                                     \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		AUDIO_TEST_GET_PROCESS_TIME(tv);                             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME           \
				" : %s():[RTN]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		} else {                                                     \
			pr_alert("[%5ld.%06ld] " AUDIO_TEST_DRV_NAME         \
				" : %s():[RTN]:" fmt, tv.tv_sec, tv.tv_usec, \
				__func__, ##__VA_ARGS__);                    \
		}                                                            \
	}                                                                    \
} while (0)


#else /* != __PRN_AUDIO_TEST_ADD_TIME__ */


#define audio_test_log_err(fmt, ...)                                         \
do {                                                                         \
	if (AUDIO_TEST_LOG_ERR_PRINT <=                                      \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err(AUDIO_TEST_DRV_NAME " : %s():[ERR]:"          \
				fmt, __func__, ##__VA_ARGS__);               \
		} else {                                                     \
			pr_alert(AUDIO_TEST_DRV_NAME " : %s():[ERR]:"        \
				fmt, __func__, ##__VA_ARGS__);               \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_info(fmt, ...)                                        \
do {                                                                         \
	if (AUDIO_TEST_LOG_PROC_PRINT <=                                     \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err(AUDIO_TEST_DRV_NAME " : %s():[INF]:"          \
				fmt, __func__, ##__VA_ARGS__);               \
		} else {                                                     \
			pr_alert(AUDIO_TEST_DRV_NAME " : %s():[INF]:"        \
				fmt, __func__, ##__VA_ARGS__);               \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_debug(fmt, ...)                                       \
do {                                                                         \
	if (AUDIO_TEST_LOG_DEBUG_PRINT <=                                    \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err(AUDIO_TEST_DRV_NAME " : %s():[DBG]:"          \
				fmt, __func__, ##__VA_ARGS__);               \
		} else {                                                     \
			pr_alert(AUDIO_TEST_DRV_NAME " : %s():[DBG]:"        \
				fmt, __func__, ##__VA_ARGS__);               \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_efunc(fmt, ...)                                       \
do {                                                                         \
	if (AUDIO_TEST_LOG_FUNC_PRINT <=                                     \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err(AUDIO_TEST_DRV_NAME " : %s():[ENT]:"          \
				fmt, __func__, ##__VA_ARGS__);               \
		} else {                                                     \
			pr_alert(AUDIO_TEST_DRV_NAME " : %s():[ENT]:"        \
				fmt, __func__, ##__VA_ARGS__);               \
		}                                                            \
	}                                                                    \
} while (0)

#define audio_test_log_rfunc(fmt, ...)                                       \
do {                                                                         \
	if (AUDIO_TEST_LOG_FUNC_PRINT <=                                     \
		AUDIO_TEST_LOG_BYTE_LOW(audio_test_log_level)) {             \
		if (audio_test_log_level & AUDIO_TEST_LOG_BIT_DMESG) {       \
			pr_err(AUDIO_TEST_DRV_NAME " : %s():[RTN]:"          \
				fmt, __func__, ##__VA_ARGS__);               \
		} else {                                                     \
			pr_alert(AUDIO_TEST_DRV_NAME " : %s():[RTN]:"        \
				fmt, __func__, ##__VA_ARGS__);               \
		}                                                            \
	}                                                                    \
} while (0)

#endif  /* __PRN_AUDIO_TEST_ADD_TIME__ */
#else   /* != __PRN_AUDIO_TEST__ */

#define audio_test_log_reg_dump(fmt, ...)       do { } while (0)
#define audio_test_log_err(fmt, ...)            do { } while (0)
#define audio_test_log_info(fmt, ...)           do { } while (0)
#define audio_test_log_debug(fmt, ...)          do { } while (0)
#define audio_test_log_efunc(fmt, ...)          do { } while (0)
#define audio_test_log_rfunc(fmt, ...)          do { } while (0)

#endif	/* __PRN_AUDIO_TEST__ */

#endif  /* __AUDIO_TEST_H__ */
