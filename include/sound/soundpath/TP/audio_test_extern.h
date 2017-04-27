/* audio_test_extern.h
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/*!
  @file		audio_test_extern.h

  @brief	Public definition Audio test command header file.
*/

#ifndef __AUDIO_TEST_EXTERN_H__
#define __AUDIO_TEST_EXTERN_H__

/*---------------------------------------------------------------------------*/
/* include files                                                             */
/*---------------------------------------------------------------------------*/
#include <linux/ioctl.h>

/*---------------------------------------------------------------------------*/
/* typedef declaration                                                       */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* define macro declaration                                                  */
/*---------------------------------------------------------------------------*/
/***********************************/
/* ioctl                           */
/***********************************/
#define AUDIO_TEST_IOC_MAGIC		'a'

/***********************************/
/* ioctl                           */
/***********************************/
#define AUDIO_TEST_PCMNAME_MAX_LEN	(100)

/*---------------------------------------------------------------------------*/
/* structure declaration                                                     */
/*---------------------------------------------------------------------------*/
/*!
  @brief	ioctl command.
*/
struct audio_test_ioctl_cmd {
	u_int in_device_type;	/**< Intput Device type */
	u_int out_device_type;	/**< Output Device type */
	u_int out_LR_type;	/**< LR type */
	u_int out_volume;	/**< Volume */
	u_int fsi_port;		/**< FSI Port */
	u_int *detect_jack;	/**< Jack detect */
	u_int *detect_key;	/**< Key detect */
	u_int vqa_val;		/**< VQA Valid */
	u_int delay_val;	/**< Delay Valid */
	u_int pt_state;		/**< PT state */
	u_int call_kind;	/**< Call kind */
};
/*!
  @brief	ioctl command.
*/
struct audio_test_ioctl_pcmname_cmd {
	u_int pcmdirection;		/**< Direction(Playback/Capture) */
	u_int pcmstate;			/**< PCM status(Open/Close) */
	u_int pcmtype;			/**< PCM type(Normal/PT) */
	char pcmname[AUDIO_TEST_PCMNAME_MAX_LEN];	/**< PCM name */
};

/*---------------------------------------------------------------------------*/
/* define function macro declaration                                         */
/*---------------------------------------------------------------------------*/
#define AUDIO_TEST_IOCTL_SETDEVICE	_IOW(AUDIO_TEST_IOC_MAGIC, 1, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STARTSCUWLOOP	_IOW(AUDIO_TEST_IOC_MAGIC, 2, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STOPSCUWLOOP	_IOW(AUDIO_TEST_IOC_MAGIC, 3, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_DETECTJACK	_IOR(AUDIO_TEST_IOC_MAGIC, 4, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_DETECTKEY	_IOR(AUDIO_TEST_IOC_MAGIC, 5, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STARTTONE	_IOW(AUDIO_TEST_IOC_MAGIC, 6, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STOPTONE	_IOW(AUDIO_TEST_IOC_MAGIC, 7, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STARTSPUVLOOP	_IOW(AUDIO_TEST_IOC_MAGIC, 8, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STOPSPUVLOOP	_IOW(AUDIO_TEST_IOC_MAGIC, 9, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_SETCALLMODE	_IOW(AUDIO_TEST_IOC_MAGIC, 10, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_GETLBSTATE	_IOR(AUDIO_TEST_IOC_MAGIC, 11, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STARTSOUNDPLAY	_IOR(AUDIO_TEST_IOC_MAGIC, 12, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_STOPSOUNDPLAY	_IOR(AUDIO_TEST_IOC_MAGIC, 13, \
					struct audio_test_ioctl_cmd)
#define AUDIO_TEST_IOCTL_GET_PCMNAME	_IOR(AUDIO_TEST_IOC_MAGIC, 14, \
					struct audio_test_ioctl_pcmname_cmd)
#define AUDIO_TEST_IOCTL_SET_PCMNAME	_IOW(AUDIO_TEST_IOC_MAGIC, 15, \
					struct audio_test_ioctl_pcmname_cmd)

#ifndef __AUDIO_TEST_LIB_H__
/*---------------------------------------------------------------------------*/
/* enum declaration                                                          */
/*---------------------------------------------------------------------------*/
/*!
  @brief	ON/OFF type.
*/
enum audio_test_state_type {
	AUDIO_TEST_DRV_STATE_OFF,	/**< 0x00: OFF. */
	AUDIO_TEST_DRV_STATE_ON,	/**< 1x01: ON. */
	AUDIO_TEST_DRV_STATE_MAX
};

/*!
  @brief	Input device type.
*/
enum audio_test_in_device_type {
	AUDIO_TEST_DRV_IN_MIC,		/**< 0x00: Mic. */
	AUDIO_TEST_DRV_IN_MAINMIC,	/**< 0x01: Main mic. */
	AUDIO_TEST_DRV_IN_SUBMIC,	/**< 0x02: Sub mic. */
	AUDIO_TEST_DRV_IN_HEADSETMIC,	/**< 0x03: Headset mic. */
	AUDIO_TEST_DRV_IN_MAX,
	AUDIO_TEST_DRV_IN_DEV_NONE = 0xFFFFFFFF
};

/*!
  @brief	Output device type.
*/
enum audio_test_out_device_type {
	AUDIO_TEST_DRV_OUT_SPEAKER,	/**< 0x00: Speaker. */
	AUDIO_TEST_DRV_OUT_HEADPHONE,	/**< 0x01: Headphone. */
	AUDIO_TEST_DRV_OUT_EARPIECE,	/**< 0x02: Earpiece. */
	AUDIO_TEST_DRV_OUT_MAX,
	AUDIO_TEST_DRV_OUT_DEV_NONE = 0xFFFFFFFF
};

/*!
  @brief	Output LR type.
*/
enum audio_test_out_LR_type {
	AUDIO_TEST_DRV_LR_L,		/**< 0x00: Use L. */
	AUDIO_TEST_DRV_LR_R,		/**< 0x01: Use R. */
	AUDIO_TEST_DRV_LR_LR,		/**< 0x02: Use LR. */
	AUDIO_TEST_DRV_LR_MAX
};

/*!
  @brief	FSI Port.
*/
enum audio_test_fsi_port {
	AUDIO_TEST_DRV_FSI_PORTA,	/**< 0x00: Port A. */
	AUDIO_TEST_DRV_FSI_PORTB,	/**< 0x01: Port B. */
	AUDIO_TEST_DRV_FSI_MAX
};

/*!
  @brief	direction.
*/
enum audio_test_pcm_direction {
	AUDIO_TEST_DRV_PCMDIR_PLAYBACK,	/**< 0x00: Playback. */
	AUDIO_TEST_DRV_PCMDIR_CAPTURE,	/**< 0x01: Capture. */
	AUDIO_TEST_DRV_PCMDIR_MAX
};

/*!
  @brief	PCM status.
*/
enum audio_test_pcm_state {
	AUDIO_TEST_DRV_PCMSTATE_OPEN,	/**< 0x00: PCM open. */
	AUDIO_TEST_DRV_PCMSTATE_CLOSE,	/**< 0x01: PCM close. */
	AUDIO_TEST_DRV_PCMSTATE_MAX
};
/*!
  @brief	PCM type.
*/
enum audio_test_pcm_type {
	AUDIO_TEST_DRV_PCMTYPE_NORMAL,	/**< 0x00: Normal */
	AUDIO_TEST_DRV_PCMTYPE_PT,	/**< 0x01: PT */
	AUDIO_TEST_DRV_PCMTYPE_MAX
};
#endif

/*!
  @brief	VCD call kind.
*/
enum audio_test_vcd_call_kind {
	AUDIO_TEST_DRV_KIND_CALL,	/**< 0x00: Call. */
	AUDIO_TEST_DRV_KIND_KIND_PCM_LB,/**< 0x01: PCM Loopback. */
	AUDIO_TEST_DRV_KIND_1KHZ,	/**< 0x02: 1kHzTone. */
	AUDIO_TEST_DRV_KIND_MAX
};

/*---------------------------------------------------------------------------*/
/* extern variable declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* extern function declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* prototype declaration                                                     */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* inline function implementation                                            */
/*---------------------------------------------------------------------------*/
/* none */

#endif  /* __AUDIO_TEST_EXTERN_H__ */
