/***************************************************************************
*
*   Silicon Image MHL Transmitter Driver
*
* Copyright (C) (2011, Silicon Image Inc)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*****************************************************************************/
#ifndef __SII8332__H__
#define __SII8332__H__

#define CONFIG_SII9234_RCP 1

#include <linux/sii8332_platform.h>
#include <linux/spinlock.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define BIT0                    0x01
#define BIT1                    0x02
#define BIT2                    0x04
#define BIT3                    0x08
#define BIT4                    0x10
#define BIT5                    0x20
#define BIT6                    0x40
#define BIT7                    0x80

#define SET_BITS    0xFF
#define CLEAR_BITS  0x00

#define	PAGE_0			0x72
#define	PAGE_1			0x7A
#define	PAGE_2			0x92
#define	PAGE_3			0x9A
#define	PAGE_CBUS		0xC8
#define DDC_0x60    0x60
#define DDC_0xA0    0xA0

#define MSC_SEND  0x00
#define MSC_DONE_ACK  0x01
#define MSC_DONE_NACK 0x02


#define BIT_CBUS_MSC_PEER_CMD              0x01
#define BIT_CBUS_MSC_MSG                   0x02
#define BIT_CBUS_MSC_READ_DEVCAP           0x04
#define BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT 0x08
#define BIT_CBUS_MSC_WRITE_BURST           0x10

/* Device Power State*/
#define MHL_DEV_UNPOWERED		0x00
#define MHL_DEV_INACTIVE		0x01
#define MHL_DEV_QUIET			0x03
#define MHL_DEV_ACTIVE			0x04

/*  Version that this chip supports*/
#define	MHL_VER_MAJOR		(0x01 << 4)	/*  bits 4..7*/
#define	MHL_VER_MINOR		0x01		/*  bits 0..3*/
#define MHL_VERSION						(MHL_VER_MAJOR | MHL_VER_MINOR)

/*Device Category*/
#define	MHL_DEV_CATEGORY_POW_BIT			(BIT4)

#define	MHL_DEV_CAT_SINK					0x01
#define	MHL_DEV_CAT_SOURCE					0x02
#define	MHL_DEV_CAT_DONGLE					0x03
#define	MHL_DEV_CAT_SELF_POWERED_DONGLE		0x13

/*Video Link Mode*/
#define	MHL_DEV_VID_LINK_SUPPRGB444			0x01
#define	MHL_DEV_VID_LINK_SUPPYCBCR444		0x02
#define	MHL_DEV_VID_LINK_SUPPYCBCR422		0x04
#define	MHL_DEV_VID_LINK_SUPP_PPIXEL		0x08
#define	MHL_DEV_VID_LINK_SUPP_ISLANDS		0x10

/*Audio Link Mode Support*/
#define	MHL_DEV_AUD_LINK_2CH				0x01
#define	MHL_DEV_AUD_LINK_8CH				0x02


/*Feature Flag in the devcap*/
#define	MHL_FEATURE_RCP_SUPPORT				BIT0	/* Dongles have freedom to not support RCP*/
#define	MHL_FEATURE_RAP_SUPPORT				BIT1	/* Dongles have freedom to not support RAP*/
#define	MHL_FEATURE_SP_SUPPORT				BIT2	/* Dongles have freedom to not support SCRATCHPAD*/

/*#define		MHL_POWER_SUPPLY_CAPACITY		16		/* 160 mA current*/
/*#define		MHL_POWER_SUPPLY_PROVIDED		16		/* 160mA 0r 0 for Wolverine.*/
/*#define		MHL_HDCP_STATUS					0		/* Bits set dynamically*/


/* VIDEO TYPES*/
#define		MHL_VT_GRAPHICS					0x00
#define		MHL_VT_PHOTO					0x02
#define		MHL_VT_CINEMA					0x04
#define		MHL_VT_GAMES					0x08
#define		MHL_SUPP_VT						0x80

/*Logical Dev Map*/
#define	MHL_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHL_DEV_LD_VIDEO					(0x01 << 1)
#define	MHL_DEV_LD_AUDIO					(0x01 << 2)
#define	MHL_DEV_LD_MEDIA					(0x01 << 3)
#define	MHL_DEV_LD_TUNER					(0x01 << 4)
#define	MHL_DEV_LD_RECORD					(0x01 << 5)
#define	MHL_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHL_DEV_LD_GUI						(0x01 << 7)

/*Bandwidth*/
#define	MHL_BANDWIDTH_LIMIT					22		/* 225 MHz*/


#define MHL_STATUS_REG_CONNECTED_RDY        0x30
#define MHL_STATUS_REG_LINK_MODE            0x31

#define	MHL_STATUS_DCAP_RDY					BIT0

#define MHL_STATUS_CLK_MODE_MASK            0x07
#define MHL_STATUS_CLK_MODE_PACKED_PIXEL    0x02
#define MHL_STATUS_CLK_MODE_NORMAL          0x03
#define MHL_STATUS_PATH_EN_MASK             0x08
#define MHL_STATUS_PATH_ENABLED             0x08
#define MHL_STATUS_PATH_DISABLED            0x00
#define MHL_STATUS_MUTED_MASK               0x10

#define MHL_RCHANGE_INT                     0x20
#define MHL_DCHANGE_INT                     0x21

#define	MHL_INT_DCAP_CHG					BIT0
#define MHL_INT_DSCR_CHG                    BIT1
#define MHL_INT_REQ_WRT                     BIT2
#define MHL_INT_GRT_WRT                     BIT3

/* On INTR_1 the EDID_CHG is located at BIT 0*/
#define	MHL_INT_EDID_CHG					BIT1

#define		MHL_INT_AND_STATUS_SIZE			0x33		/* This contains one nibble each - max offset*/
#define		MHL_SCRATCHPAD_SIZE				16
#define		MHL_MAX_BUFFER_SIZE				MHL_SCRATCHPAD_SIZE	/* manually define highest number*/


#define MHL_CAP_DEV_STATE 0x00
#define MHL_CAP_MHL_VERSION 0x01
#define	MHL_DEV_CATEGORY_OFFSET				0x02
#define MHL_CAP_ADOPTER_ID_H  0x03
#define MHL_CAP_ADOPTER_ID_L  0x04
#define MHL_CAP_VID_LINK_MODE 0x05
#define MHL_CAP_AUD_LINK_MODE 0x06
#define MHL_CAP_VIDEO_TYPE 0x07
#define MHL_CAP_LOG_DEV_MAP 0x08
#define MHL_CAP_BANDWIDTH 0x09
#define MHL_CAP_FEATURE_FLAG 0x0A
#define MHL_CAP_DEVICE_ID_H 0x0B
#define MHL_CAP_DEVICE_ID_L 0x0C
#define MHL_CAP_SCRATCHPAD_SIZE 0x0D
#define MHL_CAP_INT_STAT_SIZE 0x0E
#define MHL_CAP_RESERVED  0x0F

#define MHL_SINK_W_POW  0x11
#define MHL_SINK_WO_POW 0x01
#define MHL_DONGLE_W_POW 0x13
#define MHL_DONGLE_WO_POW 0x03

#define BIT_DDC_ABORT                   (BIT2)    /* Responder aborted DDC command at translation layer */
#define BIT_MSC_MSG_RCV                 (BIT3)    /* Responder sent a VS_MSG packet (response data or command.) */
#define BIT_MSC_XFR_DONE                (BIT4)    /* Responder sent ACK packet (not VS_MSG) */
#define BIT_MSC_XFR_ABORT               (BIT5)    /* Command send aborted on TX side */
#define BIT_MSC_ABORT                   (BIT6)    /* Responder aborted MSC command at translation layer */

#define OUTPUT_MODE_MASK					(BIT0)
#define OUTPUT_MODE_DVI						(0x00)
#define OUTPUT_MODE_HDMI					(0x01)

#define TMDS_OUTPUT_CONTROL_MASK			(BIT4)
#define TMDS_OUTPUT_CONTROL_ACTIVE			(0x00)
#define TMDS_OUTPUT_CONTROL_POWER_DOWN		(0x10)

#define AV_MUTE_MASK						(BIT3)
#define AV_MUTE_NORMAL						(0x00)
#define AV_MUTE_MUTED						(0x08)

#define	CBUSABORT_BIT_REQ_MAXFAIL			(0x01 << 0)
#define	CBUSABORT_BIT_PROTOCOL_ERROR		(0x01 << 1)
#define	CBUSABORT_BIT_REQ_TIMEOUT			(0x01 << 2)
#define	CBUSABORT_BIT_UNDEFINED_OPCODE		(0x01 << 3)
#define	CBUSSTATUS_BIT_CONNECTED			(0x01 << 6)
#define	CBUSABORT_BIT_PEER_ABORTED			(0x01 << 7)

enum
{
    MHL_MSC_MSG_RCP             = 0x10,     /* RCP sub-command*/
    MHL_MSC_MSG_RCPK            = 0x11,     /* RCP Acknowledge sub-command*/
    MHL_MSC_MSG_RCPE            = 0x12,     /* RCP Error sub-command*/
    MHL_MSC_MSG_RAP             = 0x20,     /* Mode Change Warning sub-command*/
    MHL_MSC_MSG_RAPK            = 0x21     /* MCW Acknowledge sub-command*/
};

#define	RCPE_NO_ERROR				0x00
#define	RCPE_INEEFECTIVE_KEY_CODE	0x01
#define	RCPE_BUSY					0x02

/* MHL spec related defines*/

enum
{
	MHL_ACK						= 0x33,	/* Command or Data byte acknowledge*/
	MHL_NACK					= 0x34,	/* Command or Data byte not acknowledge*/
	MHL_ABORT					= 0x35,	/* Transaction abort*/
	MHL_WRITE_STAT				= 0x60 | 0x80,	/* 0xE0 - Write one status register strip top bit*/
	MHL_SET_INT					= 0x60,	/* Write one interrupt register*/
	MHL_READ_DEVCAP				= 0x61,	/* Read one register*/
	MHL_GET_STATE				= 0x62,	/* Read CBUS revision level from follower*/
	MHL_GET_VENDOR_ID			= 0x63,	/* Read vendor ID value from follower.*/
	MHL_SET_HPD					= 0x64,	/* Set Hot Plug Detect in follower*/
	MHL_CLR_HPD					= 0x65,	/* Clear Hot Plug Detect in follower*/
	MHL_SET_CAP_ID				= 0x66,	/* Set Capture ID for downstream device.*/
	MHL_GET_CAP_ID				= 0x67,	/* Get Capture ID from downstream device.*/
	MHL_MSC_MSG					= 0x68,	/* VS command to send RCP sub-commands*/
	MHL_GET_SC1_ERRORCODE		= 0x69,	/* Get Vendor-Specific command error code.*/
	MHL_GET_DDC_ERRORCODE		= 0x6A,	/* Get DDC channel command error code.*/
	MHL_GET_MSC_ERRORCODE		= 0x6B,	/* Get MSC command error code.*/
	MHL_WRITE_BURST				= 0x6C,	/* Write 1-16 bytes to responder's scratchpad.*/
	MHL_GET_SC3_ERRORCODE		= 0x6D	/* Get channel 3 command error code.*/
};

#define	MHL_RAP_CONTENT_ON		0x10	/* Turn content streaming ON.*/
#define	MHL_RAP_CONTENT_OFF		0x11	/* Turn content streaming OFF.*/


/* MHL Timings applicable to this driver.*/

#define	T_SRC_VBUS_CBUS_TO_STABLE	(200)	/* 100 - 1000 milliseconds. Per MHL 1.0 Specs*/
#define	T_SRC_WAKE_PULSE_WIDTH_1	(20)	/* 20 milliseconds. Per MHL 1.0 Specs*/
#define	T_SRC_WAKE_PULSE_WIDTH_2	(60)	/* 60 milliseconds. Per MHL 1.0 Specs*/

#define	T_SRC_WAKE_TO_DISCOVER		(200)	/* 100 - 1000 milliseconds. Per MHL 1.0 Specs*/

#define T_SRC_VBUS_CBUS_T0_STABLE	(500)

/* Allow RSEN to stay low this much before reacting.*/
/* Per specs between 100 to 200 ms*/
#define	T_SRC_RSEN_DEGLITCH			(100)	/* (150)*/

/* Wait this much after connection before reacting to RSEN (300-500ms)*/
/* Per specs between 300 to 500 ms*/
#define	T_SRC_RXSENSE_CHK			(400)

#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI )

#define DEVCAP_VAL_DEV_STATE       0
#define DEVCAP_VAL_MHL_VERSION     MHL_VERSION
#define DEVCAP_VAL_DEV_CAT         (MHL_DEV_CAT_SOURCE | MHL_DEV_CATEGORY_POW_BIT)
#define DEVCAP_VAL_ADOPTER_ID_H    0x00
#define DEVCAP_VAL_ADOPTER_ID_L    0x00
#define DEVCAP_VAL_VID_LINK_MODE   MHL_DEV_VID_LINK_SUPPRGB444
#define DEVCAP_VAL_AUD_LINK_MODE   MHL_DEV_AUD_LINK_2CH
#define DEVCAP_VAL_VIDEO_TYPE      0
#define DEVCAP_VAL_LOG_DEV_MAP     MHL_LOGICAL_DEVICE_MAP
#define DEVCAP_VAL_BANDWIDTH       0
#define DEVCAP_VAL_FEATURE_FLAG    (MHL_FEATURE_RCP_SUPPORT | MHL_FEATURE_RAP_SUPPORT |MHL_FEATURE_SP_SUPPORT)
#define DEVCAP_VAL_DEVICE_ID_H     0x00
#define DEVCAP_VAL_DEVICE_ID_L     0x00
#define DEVCAP_VAL_SCRATCHPAD_SIZE MHL_SCRATCHPAD_SIZE
#define DEVCAP_VAL_INT_STAT_SIZE   MHL_INT_AND_STATUS_SIZE
#define DEVCAP_VAL_RESERVED        0

#define EDID_SIZE 128
#define DDC_BUS_REQUEST_MASK				(BIT2)
#define DDC_BUS_REQUEST_NOT_USING			(0x00)
#define DDC_BUS_REQUEST_REQUESTED			(0x04)

#define DDC_BUS_GRANT_MASK					(BIT1)
#define DDC_BUS_GRANT_NOT_AVAILABLE			(0x00)
#define DDC_BUS_GRANT_GRANTED				(0x02)

#define TWO_LSBITS              0x03
#define THREE_LSBITS            0x07
#define FOUR_LSBITS             0x0F
#define FIVE_LSBITS             0x1F
#define TWO_MSBITS              0xC0

#define T_DDC_ACCESS    50

#define EDID_BLOCK_0_OFFSET 0x0000
#define EDID_BLOCK_1_OFFSET 0x0080

#define EDID_BLOCK_SIZE      128
#define NUM_OF_EXTEN_ADDR   0x7E

#define EDID_TAG_ADDR       0x00
#define EDID_REV_ADDR       0x01
#define EDID_TAG_IDX        0x02
#define LONG_DESCR_PTR_IDX  0x02
#define MISC_SUPPORT_IDX    0x03

#define ESTABLISHED_TIMING_INDEX        35      /* Offset of Established Timing in EDID block*/
#define NUM_OF_STANDARD_TIMINGS          8
#define STANDARD_TIMING_OFFSET          38
#define LONG_DESCR_LEN                  18
#define NUM_OF_DETAILED_DESCRIPTORS      4

#define DETAILED_TIMING_OFFSET        0x36

/* Offsets within a Long Descriptors Block*/
/*========================================*/
#define PIX_CLK_OFFSET                   0
#define H_ACTIVE_OFFSET                  2
#define H_BLANKING_OFFSET                3
#define V_ACTIVE_OFFSET                  5
#define V_BLANKING_OFFSET                6
#define H_SYNC_OFFSET                    8
#define H_SYNC_PW_OFFSET                 9
#define V_SYNC_OFFSET                   10
#define V_SYNC_PW_OFFSET                10
#define H_IMAGE_SIZE_OFFSET             12
#define V_IMAGE_SIZE_OFFSET             13
#define H_BORDER_OFFSET                 15
#define V_BORDER_OFFSET                 16
#define FLAGS_OFFSET                    17

#define AR16_10                          0
#define AR4_3                            1
#define AR5_4                            2
#define AR16_9                           3

/* Data Block Tag Codes*/
/*=====================*/
#define AUDIO_D_BLOCK       0x01
#define VIDEO_D_BLOCK       0x02
#define VENDOR_SPEC_D_BLOCK 0x03
#define SPKR_ALLOC_D_BLOCK  0x04
#define USE_EXTENDED_TAG    0x07

/* Extended Data Block Tag Codes*/
/*==============================*/
#define COLORIMETRY_D_BLOCK 0x05

#define HDMI_SIGNATURE_LEN  0x03

#define CEC_PHYS_ADDR_LEN   0x02
#define EDID_EXTENSION_TAG  0x02
#define EDID_REV_THREE      0x03
#define EDID_DATA_START     0x04

#define EDID_BLOCK_0        0x00
#define EDID_BLOCK_2_3      0x01

#define VIDEO_CAPABILITY_D_BLOCK 0x00
#define MAX_V_DESCRIPTORS			20
#define MAX_A_DESCRIPTORS			10
#define MAX_SPEAKER_CONFIGURATIONS	 4
#define AUDIO_DESCR_SIZE			 3

#define EXTENDED_LINK_PROTECTION_MASK		(BIT7)
#define EXTENDED_LINK_PROTECTION_NONE		(0x00)
#define EXTENDED_LINK_PROTECTION_SECURE		(0x80)

#define LOCAL_LINK_PROTECTION_MASK			(BIT6)
#define LOCAL_LINK_PROTECTION_NONE			(0x00)
#define LOCAL_LINK_PROTECTION_SECURE		(0x40)

#define LINK_STATUS_MASK					(BIT5 | BIT4)
#define LINK_STATUS_NORMAL					(0x00)
#define LINK_STATUS_LINK_LOST				(0x10)
#define LINK_STATUS_RENEGOTIATION_REQ		(0x20)
#define LINK_STATUS_LINK_SUSPENDED			(0x30)

#define HDCP_REPEATER_MASK					(BIT3)
#define HDCP_REPEATER_NO					(0x00)
#define HDCP_REPEATER_YES					(0x08)

#define CONNECTOR_TYPE_MASK					(BIT2 | BIT0)
#define CONNECTOR_TYPE_DVI					(0x00)
#define CONNECTOR_TYPE_RSVD					(0x01)
#define CONNECTOR_TYPE_HDMI					(0x04)
#define CONNECTOR_TYPE_FUTURE				(0x05)

#define PROTECTION_TYPE_MASK				(BIT1)
#define PROTECTION_TYPE_NONE				(0x00)
#define PROTECTION_TYPE_HDCP				(0x02)

#define PROTECTION_LEVEL_MASK				(BIT0)
#define PROTECTION_LEVEL_MIN				(0x00)
#define PROTECTION_LEVEL_MAX				(0x01)

#define KSV_FORWARD_MASK					(BIT4)
#define KSV_FORWARD_ENABLE					(0x10)
#define KSV_FORWARD_DISABLE					(0x00)

#define SIZE_AUDIO_INFOFRAME 14
#define DDC_XLTN_TIMEOUT_MAX_VAL              0x30

#define BIT_TPI_OUTPUT_FORMAT_MASK          0x03
#define BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB   0x00
#define BIT_TPI_OUTPUT_FORMAT_YCbCr444      0x01
#define BIT_TPI_OUTPUT_FORMAT_YCbCr422      0x02
#define BIT_TPI_OUTPUT_FORMAT_DVI_TO_RGB    0x03

#define INPUT_COLOR_SPACE_RGB 0x00
#define INPUT_COLOR_SPACE_YCBCR444 0x01
#define INPUT_COLOR_SPACE_YCBCR422 0x02

#define MIPI_DSI_FORMAT_RESOLUTION_CLEAR  0x40

#define SIZE_AUDIO_INFOFRAME 14
#define DDC_XLTN_TIMEOUT_MAX_VAL              0x30

#define BIT_TPI_OUTPUT_FORMAT_MASK          0x03
#define BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB   0x00
#define BIT_TPI_OUTPUT_FORMAT_YCbCr444      0x01
#define BIT_TPI_OUTPUT_FORMAT_YCbCr422      0x02
#define BIT_TPI_OUTPUT_FORMAT_DVI_TO_RGB    0x03

#define INPUT_COLOR_SPACE_RGB 0x00
#define INPUT_COLOR_SPACE_YCBCR444 0x01
#define INPUT_COLOR_SPACE_YCBCR422 0x02

#define MIPI_DSI_FORMAT_RESOLUTION_CLEAR  0x40

struct dtd {
   unsigned long pixelFrequency;
   unsigned short hActive;
   unsigned short hBlank;
   unsigned short vActive;
   unsigned short vBlank;
   unsigned short outputformat;
};

enum mhl_output_CEA_VESA {
	MHL_CUSTOM,
	MHL_CEA1_640X480P_59_94HZ,
	MHL_CEA2_3_720X480P_59_94HZ,
	MHL_CEA4_1280X720P_60HZ,
	MHL_CEA5_1920X1080I_60HZ,
	MHL_CEA6_7_NTSC_60HZ,
	MHL_CEA14_15_480p_60HZ,
	MHL_CEA16_1920X1080P_60HZ,
	MHL_CEA17_18_720X576P_50HZ,
	MHL_CEA19_1280X720P_50HZ,
	MHL_CEA20_1920X1080I_50HZ,
	MHL_CEA21_22_576I_PAL_50HZ,
	MHL_CEA29_30_576P_50HZ,
	MHL_CEA31_1920x1080P_50Hz,
	MHL_CEA32_1920X1080P_24HZ,
	MHL_CEA33_1920X1080P_25HZ,
	MHL_CEA34_1920X1080P_30HZ,
	MHL_CEA60_1280X720P_24HZ,
	MHL_CEA61_1280X720P_25HZ,
	MHL_CEA62_1280X720P_30HZ,
	MHL_VESA9_800X600P_60_32HZ,
	MHL_VESA14_848X480P_60HZ,
	MHL_VESA16_1024X768P_60HZ,
	MHL_VESA22_1280X768P_59_99HZ,
	MHL_VESA23_1280X768P_59_87HZ,
	MHL_VESA27_1280X800P_59_91HZ,
	MHL_VESA28_1280X800P_59_81HZ,
	MHL_VESA39_1360X768P_60_02HZ,
	MHL_VESA81_1366X768P_59_79HZ,
	MHL_VIDEO_OUTPUT_CEA_VESA_MAX
};
enum{
    BIT_INTR4_SCDT_CHANGE                   = 0x01
    ,BIT_INTR4_RPWR5V_CHANGE                = 0x02
    ,BIT_INTR4_MHL_EST                      = 0x04
    ,BIT_INTR4_USB_EST                      = 0x08
    ,BIT_INTR4_CBUS_LKOUT                   = 0x10
    ,BIT_INTR4_CBUS_DISCONNECT              = 0x20
    ,BIT_INTR4_RGND_RDY                     = 0x40
};

enum{
     BIT_INTR5_CKDT                 = 0x01
    ,BIT_INTR5_VBUS_CHG             = 0x02
    ,BIT_INTR5_PXL_FORMAT_CHG       = 0x04
    ,BIT_INTR5_MHL_FIFO_OVERFLOW    = 0x08
    ,BIT_INTR5_MHL_FIFO_UNDERFLOW   = 0x10
};

enum{
	MHL_STATE_OFF,
	MHL_STATE_ON
};
enum{
     BIT_TPI_INTR_ST0_HOT_PLUG_EVENT                = 0x01,
	BIT_TPI_INTR_ST0_BKSV_ERR                      = 0x02,
    BIT_TPI_INTR_ST0_BKSV_DONE                     = 0x04,
    BIT_TPI_INTR_ST0_KSV_FIFO_FIRST                = 0x08,
    BIT_TPI_INTR_ST0_AUDIO_ERROR_EVENT             = 0x10,
    BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT    = 0x20,
    BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT = 0x40,
    BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT = 0x80
};

typedef struct {								/* for storing EDID parsed data*/
	u8 VideoDescriptor[MAX_V_DESCRIPTORS];	/* maximum number of video descriptors*/
	u8 AudioDescriptor[MAX_A_DESCRIPTORS][3];	/* maximum number of audio descriptors*/
	u8 SpkrAlloc[MAX_SPEAKER_CONFIGURATIONS];	/* maximum number of speaker configurations*/
	bool UnderScan;								/* "1" if DTV monitor underscans IT video formats by default*/
	bool BasicAudio;							/* Sink supports Basic Audio*/
	bool YCbCr_4_4_4;							/* Sink supports YCbCr 4:4:4*/
	bool YCbCr_4_2_2;							/* Sink supports YCbCr 4:2:2*/
	bool HDMI_Sink;								/* "1" if HDMI signature found*/
	u8 CEC_A_B;								/* CEC Physical address. See HDMI 1.3 Table 8-6*/
	u8 CEC_C_D;
	u8 ColorimetrySupportFlags;				/* IEC 61966-2-4 colorimetry support: 1 - xvYCC601; 2 - xvYCC709 */
	u8 MetadataProfile;
	bool _3D_Supported;
} EDID_Descriptors;

typedef struct {
	/*bool cmd_on;*/
	u8 command;
	u8 offset;
	u8 lenght;
	u8 buff[16];
}cbus_pkt;

typedef enum
{
	AVI_CMD_NONE = 0x00,
	HPD_HIGH_EVENT = 0x01,
	HPD_LOW_EVENT,
	MIPI_INPUT_EVENT,

	AVI_CMD_MAX
}avi_cmd_type;


struct tx_page0 {
	struct mhl_platform_data	*pdata;
};

struct tx_page1 {
	struct mhl_platform_data	*pdata;
};

struct tx_page2 {
	struct mhl_platform_data	*pdata;
};

struct tx_page3 {
	struct mhl_platform_data	*pdata;
};


struct mhl_tx {
	u8 cmd_rx_cnt;
	u8 cmd_tx_cnt;
	u8 msc_cmd_done_intr;
	u8 intr5_mask;
	u8 intr4_mask;
	u8 intr_tpi_mask;
	u8 mipi_input_colorspace;
	u8 mhl_output_colorspace;
	u8 colorimetryAspectRatio;
	u8 interlaced;
	u8 inputVideoCode;

	bool mhl_onoff;
	bool msc_cmd_abord;
	bool avi_work;
	bool edid_access_done;
	bool aksv_available;
	bool video_out_setting;
	bool hdcp_started;
	bool hdcp_on_ready;
	cbus_pkt msc_cmd_q[10];
	avi_cmd_type avi_cmd;
	bool hdmi_sink;
	EDID_Descriptors EDID_Data;
	u8 edid_block1[EDID_SIZE];
	u8 edid_block2[EDID_SIZE];
	u8 edid_block3[EDID_SIZE];
	u8 edid_block0[EDID_SIZE];

#ifdef CONFIG_SII9234_RCP
	u8 error_key;
	struct input_dev		*input_dev;
#endif

	struct mhl_platform_data	*pdata;
	struct mutex			i2c_lock;
	struct mutex  cbus_cmd_lock;

	struct work_struct mhl_power_on;
	struct work_struct cbus_cmd_work;
	struct workqueue_struct *cbus_cmd_wqs;
	struct work_struct avi_control_work;
	struct workqueue_struct *avi_control_wqs;
	struct early_suspend early_suspend;
	struct i2c_client *client;
	wait_queue_head_t		cbus_hanler_wq;
	wait_queue_head_t		avi_control_wq;

};

enum{
     tsfDDROver2_1Lane    = 0x01,
	 tsfDDROver2_2Lanes   = 0x02,
	 tsfDDROver2_3Lanes   = 0x04
};

typedef struct {
	u16 hRes;
	u16 vRes;
	u8 colorimetryAspectRatio;
	u8 inputVideoCode;
	u8 interlaced;
	u8 flags;
} video_mode_h;

typedef struct{
  u8 device_id;
  u8 offset;
  u8 value;
} video_mode_reg;

typedef struct{
    video_mode_h header;
    video_mode_reg regVals[20];
} video_mode_info;

enum{
     BIT_MIPI_W_VC_MASK                     = 0x03
    , BIT_MIPI_W_USE_VC                      = 0x04
    , BIT_MIPI_W_ECC_USE                     = 0x08
    , BIT_MIPI_W_RES_UPDATE                  = 0x10
    , BIT_MIPI_W_DSI_FORMATE_USE             = 0x20
    , BIT_MIPI_W_DSI_PXL_FORMAT_CHANGED_IN   = 0x40
    , BIT_MIPI_R_DSI_PXL_FORMAT_CHANGED      = 0x80
};

enum{
     BIT_MIPI_W_NUM_USED_LANE_MASK  =  0x07
    , BIT_MIPI_W_PD_CK               = 0x08
    , BIT_MIPI_W_PD_MASK             = 0x70
    , BIT_MIP_DDR_OVER_2             = 0x80
};


#define TX_PAGE_TPI         0x0000		    /* TPI*/

#define REG_TPI_CONFIG2                     (TX_PAGE_TPI | 0x0025)
typedef enum{
     BIT_TPI_AUDIO_HANDLING_MASK                         = 0x03
    , BIT_TPI_AUDIO_HANDLING_PASS_BASIC_AUDIO_ONLY        = 0x00
    , BIT_TPI_AUDIO_HANDLING_PASS_ALL_AUDIO_MODES         = 0x01
    , BIT_TPI_AUDIO_HANDLING_DOWNSAMPLE_INCOMING_AS_NEEDED = 0x02
    , BIT_TPI_AUDIO_HANDLING_DO_NOT_CHECK_AUDIO_STREAM    = 0x03
} TpiConfig2Bits_e;
#define REG_TPI_CONFIG3                     (TX_PAGE_TPI | 0x0026)
typedef enum{
     BIT_TPI_AUDIO_CODING_TYPE_MASK             = 0x0F
    , BIT_TPI_AUDIO_CODING_TYPE_STREAM_HEADER    = 0x00
    , BIT_TPI_AUDIO_CODING_TYPE_PCM              = 0x01
    , BIT_TPI_AUDIO_CODING_TYPE_AC3              = 0x02
    , BIT_TPI_AUDIO_CODING_TYPE_MPEG1            = 0x03
    , BIT_TPI_AUDIO_CODING_TYPE_MP3              = 0x04
    , BIT_TPI_AUDIO_CODING_TYPE_MPEG2            = 0x05
    , BIT_TPI_AUDIO_CODING_TYPE_AAC              = 0x06
    , BIT_TPI_AUDIO_CODING_TYPE_DTS              = 0x07
    , BIT_TPI_AUDIO_CODING_TYPE_ATRAC            = 0x08

    , BIT_TPI_CONFIG3_MUTE_MASK                  = 0x10
    , BIT_TPI_CONFIG3_MUTE_NORMAL                = 0x00
    , BIT_TPI_CONFIG3_MUTE_MUTED                 = 0x10

    , BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_MASK    = 0x20
    , BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH     = 0x10
    , BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX = 0x20

    , BIT_TPI_CONFIG_3_AUDIO_INTERFACE_MASK      = 0xC0
    , BIT_TPI_CONFIG_3_AUDIO_INTERFACE_DISABLED  = 0x00
    , BIT_TPI_CONFIG_3_AUDIO_INTERFACE_SPDIF     = 0x40
    , BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S       = 0x80
    , BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO  = 0xC0
} TpiConfig3Bits_e;

enum{
     BIT_TPI_INFO_SEL_MASK                  = 0x07
    , BIT_TPI_INFO_SEL_AVI                   = 0x00
    , BIT_TPI_INFO_SEL_SPD                   = 0x01
    , BIT_TPI_INFO_SEL_Audio                 = 0x02
    , BIT_TPI_INFO_SEL_MPEG                  = 0x03
    , BIT_TPI_INFO_SEL_GENERIC               = 0x04
    , BIT_TPI_INFO_SEL_GENERIC2              = 0x05

    , BIT_TPI_INFO_READ_FLAG_MASK            = 0x20
    , BIT_TPI_INFO_READ_FLAG_NO_READ         = 0x00
    , BIT_TPI_INFO_READ_FLAG_READ            = 0x20
    , BIT_TPI_INFO_RPT                       = 0x40
    , BIT_TPI_INFO_EN                        = 0x80
};

static int hdmi_open(struct inode *inode, struct file *filp);
static int hdmi_release(struct inode *inode, struct file *filp);
/*static unsigned int hdmi_poll(struct file *filp, poll_table *wait);*/
static int set_audio_mode(struct mhl_tx *mhl);
static int set_audio_infoframe(struct mhl_tx *mhl);
static int set_ouptput_timings(struct mhl_tx *mhl, bool *input_check);
static int color_format_set(struct mhl_tx *mhl);
static int set_avi_infoframe(struct mhl_tx *mhl);

static int MipiIsr(struct mhl_tx *mhl);

static ssize_t show_state_hdmi(struct device *dev,
		struct device_attribute *attr, char *buf);
static int show_info(struct device *dev, struct device_attribute *attr, char *buf);
int hdmi_get_hpd_state(void);
static int mhl_suspend(struct device *dev);
static int mhl_resume(struct device *dev);
static irqreturn_t mhl_irq_thread(int irq, void *data);
static int ldo2_onoff(bool);
#if 0
static void irq_status(struct mhl_tx *mhl);
#endif
#endif /* __SII8332__H__ */
