/***************************************************************************
 * * *
 * * *   Driver to read and parse EDID values from SII8332 driver
 * * *
 * * *
 * * * This program is free software; you can redistribute it and/or modify
 * * * it under the terms of the GNU General Public License as published by
 * * * the Free Software Foundation version 2.
 * * *
 * * * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * * * kind, whether express or implied; without even the implied warranty
 * * * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * * * GNU General Public License for more details.
 * * *
 * * ***********************************************************************/
#ifndef __EDID2__H__
#define __EDID2__H__

#include <linux/edid_platform.h>
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

#define EDID_SIZE 128

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
/*Offset of Established Timing in EDID block*/
#define ESTABLISHED_TIMING_INDEX        35
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

extern int outputformat;/*edid*/
extern int mhl_plugedin_state;

struct EDID_Descriptors {	/* for storing EDID parsed data*/
	/* maximum number of video descriptors*/
	u8 VideoDescriptor[MAX_V_DESCRIPTORS];
	/* maximum number of audio descriptors*/
	u8 AudioDescriptor[MAX_A_DESCRIPTORS][3];
	/* maximum number of speaker configurations*/
	u8 SpkrAlloc[MAX_SPEAKER_CONFIGURATIONS];
	/* "1" if DTV monitor underscans IT video formats by default*/
	bool UnderScan;
	/* Sink supports Basic Audio*/
	bool BasicAudio;
	/* Sink supports YCbCr 4:4:4*/
	bool YCbCr_4_4_4;
	/* Sink supports YCbCr 4:2:2*/
	bool YCbCr_4_2_2;
	/* "1" if HDMI signature found*/
	bool HDMI_Sink;
	/* CEC Physical address. See HDMI 1.3 Table 8-6*/
	u8 CEC_A_B;
	u8 CEC_C_D;
	/* IEC 61966-2-4 colorimetry support: 1 - xvYCC601; 2 - xvYCC709 */
	u8 ColorimetrySupportFlags;
	u8 MetadataProfile;
	bool _3D_Supported;
};

struct edidtx_0x60 {
	struct edid_platform_data	*pdata;
};

struct edidtx_0xA0 {
	struct edid_platform_data	*pdata;
};

struct edid_tx {
	bool hdmi_sink;
	struct EDID_Descriptors EDID_Data;
	u8 edid_block1[EDID_SIZE];
	u8 edid_block2[EDID_SIZE];
	u8 edid_block3[EDID_SIZE];
	u8 edid_block0[EDID_SIZE];

	struct edid_platform_data	*pdata;
	struct mutex			i2c_lock;

	struct i2c_client *client;
};

int edid_set_output_format(void);
int edid_read(void);

struct dtd {
	unsigned long pixelFrequency;
	unsigned short hActive;
	unsigned short hBlank;
	unsigned short vActive;
	unsigned short vBlank;
	unsigned short outputformat;
};

enum edid_output_CEA_VESA {
	edid_CUSTOM,
	edid_CEA1_640X480P_59_94HZ,
	edid_CEA2_3_720X480P_59_94HZ,
	edid_CEA4_1280X720P_60HZ,
	edid_CEA5_1920X1080I_60HZ,
	edid_CEA6_7_NTSC_60HZ,
	edid_CEA14_15_480p_60HZ,
	edid_CEA16_1920X1080P_60HZ,
	edid_CEA17_18_720X576P_50HZ,
	edid_CEA19_1280X720P_50HZ,
	edid_CEA20_1920X1080I_50HZ,
	edid_CEA21_22_576I_PAL_50HZ,
	edid_CEA29_30_576P_50HZ,
	edid_CEA31_1920x1080P_50Hz,
	edid_CEA32_1920X1080P_24HZ,
	edid_CEA33_1920X1080P_25HZ,
	edid_CEA34_1920X1080P_30HZ,
	edid_CEA60_1280X720P_24HZ,
	edid_CEA61_1280X720P_25HZ,
	edid_CEA62_1280X720P_30HZ,
	edid_VESA9_800X600P_60_32HZ,
	edid_VESA14_848X480P_60HZ,
	edid_VESA16_1024X768P_60HZ,
	edid_VESA22_1280X768P_59_99HZ,
	edid_VESA23_1280X768P_59_87HZ,
	edid_VESA27_1280X800P_59_91HZ,
	edid_VESA28_1280X800P_59_81HZ,
	edid_VESA39_1360X768P_60_02HZ,
	edid_VESA81_1366X768P_59_79HZ,
	edid_VIDEO_OUTPUT_CEA_VESA_MAX
};

#endif
