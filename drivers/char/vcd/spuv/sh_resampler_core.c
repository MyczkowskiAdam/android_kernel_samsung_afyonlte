/* sh_resampler_core.c
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

#ifdef RESAMP_WINDOWS
#include <stdlib.h>
#include "main.h"
#else
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#endif

#include <linux/sh_dma.h>
#include <linux/dma-mapping.h>

#include "sh_resampler.h"


#define FILTER_LG 314
#define CONV_FILTER_PREC 17
#define UPSAMPLE_44 441
#define DOWNSAMPLE_44 160
#define UPSAMPLE_16 160
#define DOWNSAMPLE_16 441


#define SH_RESAMPLER_MAX_S32 (2147483647)
#define SH_RESAMPLER_MIN_S32 (-2147483647-1)
#define SH_RESAMPLER_MAX_S16 (32767)
#define SH_RESAMPLER_MIN_S16 (-32767-1)



#define DOWNSAMPLE_2 2
#define DOWNSAMPLE_4 4
#define UPSAMPLE_3 3
#define UPSAMPLE_7 7
#define UPSAMPLE_4 4
#define UPSAMPLE_2 2
#define UPSAMPLE_5 5

/* Filter size definition:
!!!! Update the FILTER_MAX_LG accordingly !!! */
#define FILTERx2_LG 66
#define FILTERx2b_LG 292
#define FILTERx3_LG 162
#define FILTERx3b_LG 26
#define FILTERx3c_LG 300
#define FILTERx3d_LG 121
#define FILTERx3e_LG 59
#define FILTERx3f_LG 27
#define FILTERx4_LG 53
#define FILTERx4b_LG 39
#define FILTERx5_LG 41
#define FILTERx7_LG 70
#define FILTERx7b_LG 24


#define FILTER_MAX_LG FILTERx3c_LG


int m_ALSA_sampling_rate;
int m_VCD_sampling_rate;


short x2[FILTERx2_LG] = {
	-3, 0, 7, 17, 24, 20, -1, -35, -66,
	-71, -32, 46, 132, 173, 126, -18, -207,
	-343, -323, -104, 255, 590, 693, 421,
	-204, -950, -1430, -1247, -171, 1716,
	4006, 6074, 7301, 7301, 6074, 4006, 1716,
	-171, -1247, -1430, -950, -204, 421, 693,
	590, 255, -104, -323, -343, -207, -18,
	126, 173, 132, 46, -32, -71, -66, -35,
	-1, 20, 24, 17, 7, 0, -3};

short x2b[FILTERx2b_LG] = {
	6, 23, 39, 36, 8, -17, -14,
	8, 13, -3, -12, 0, 11, 1, -11, -3, 11, 4,
	-11, -6, 11, 7, -10, -9, 10, 11, -10, -13,
	9, 15, -8, -17, 6, 19, -4, -21, 2, 23, 1,
	-25, -3, 26, 7, -28, -10, 28, 14, -29, -19,
	29, 23, -28, -28, 27, 33, -25, -38, 23, 43,
	-20, -49, 16, 54, -11, -58, 5, 63, 1, -67,
	-8, 70, 16, -73, -25, 75, 35, -76, -45, 76,
	56, -75, -67, 73, 79, -69, -91, 64, 103,
	-57, -115, 49, 128, -39, -140, 26, 152,
	-13, -163, -3, 173, 21, -182, -42, 190,
	64, -197, -88, 202, 115, -204, -145,
	205, 177, -203, -212, 197, 249, -188,
	-290, 175, 334, -157, -383, 134, 436,
	-103, -496, 63, 564, -13, -642, -53,
	737, 139, -855, -259, 1011, 431, -1238,
	-706, 1611, 1214, -2383, -2516, 5157,
	14098, 14098, 5157, -2516, -2383, 1214,
	1611, -706, -1238, 431, 1011, -259, -855,
	139, 737, -53, -642, -13, 564, 63, -496,
	-103, 436, 134, -383, -157, 334, 175, -290,
	-188, 249, 197, -212, -203, 177, 205, -145,
	-204, 115, 202, -88, -197, 64, 190, -42,
	-182, 21, 173, -3, -163, -13, 152, 26, -140,
	-39, 128, 49, -115, -57, 103, 64, -91, -69,
	79, 73, -67, -75, 56, 76, -45, -76, 35, 75,
	-25, -73, 16, 70, -8, -67, 1, 63, 5, -58,
	-11, 54, 16, -49, -20, 43, 23, -38, -25, 33,
	27, -28, -28, 23, 29, -19, -29, 14, 28, -10,
	-28, 7, 26, -3, -25, 1, 23, 2, -21, -4, 19,
	6, -17, -8, 15, 9, -13, -10, 11, 10, -9,
	-10, 7, 11, -6, -11, 4, 11, -3, -11, 1,
	11, 0, -12, -3, 13, 8, -14, -17, 8, 36,
	39, 23, 6};


short x3[FILTERx3_LG] = {
		-27, -71, -116, -117, -30, 154,
		381, 553, 573, 415, 154, -73,
		-151, -63, 97, 192, 148, 1,
		-128, -136, -23, 111, 149, 58,
		-84, -151, -82, 63, 155, 109,
		-38, -156, -136, 11, 155, 163, 20,
		-150, -191, -56, 139, 218, 99, -121,
		-243, -146, 94, 265, 200, -58, -283,
		-260, 10, 293, 325, 52, -296, -398,
		-132, 288, 479, 234, -264, -571,
		-368, 220, 680, 549, -143, -820, -815,
		8, 1022, 1261, 254, -1395, -2239, -958,
		2567, 6927, 9921, 9921, 6927, 2567,
		-958, -2239, -1395, 254, 1261, 1022,
		8, -815, -820, -143, 549, 680, 220,
		-368, -571, -264, 234, 479, 288, -132,
		-398, -296, 52, 325, 293, 10, -260, -283,
		-58, 200, 265, 94, -146, -243, -121, 99,
		218, 139, -56, -191, -150, 20, 163, 155,
		11, -136, -156, -38, 109, 155, 63, -82,
		-151, -84, 58, 149, 111, -23, -136, -128,
		1, 148, 192, 97, -63, -151, -73, 154,
		415, 573, 553, 381, 154, -30, -117,
		-116, -71, -27};

short x3b[FILTERx3b_LG] = {
		-48, -115, -94, 132, 476, 559, -8,
		-1074, -1686, -613, 2565, 6727, 9685,
		9685, 6727, 2565, -613, -1686, -1074,
		-8, 559, 476, 132, -94, -115, -48};


short x3c[FILTERx3c_LG] = {
		1, -24, -61, -109, -142, -140, -93, -21,
		43, 66, 41, -8, -44, -42, -7, 30, 39, 14,
		-21, -37, -19, 15, 35, 24, -9, -34, -28, 4,
		33, 32, 1, -31, -35, -7, 29, 39, 13, -26, -43,
		-20, 22, 45, 27, -17, -48, -35, 11, 49, 42,
		-4, -49, -50, -4, 47, 57, 13, -45, -64, -24,
		40, 69, 35, -34, -74, -47, 26, 78, 59, -16,
		-79, -71, 4, 79, 83, 9, -77, -95, -24, 72,
		106, 41, -65, -115, -60, 55, 123, 79, -42,
		-129, -99, 26, 133, 120, -6, -133, -141,
		-17, 130, 162, 43, -124, -182, -72, 113,
		201, 104, -97, -218, -140, 75, 232, 179,
		-47, -244, -221, 13, 251, 267, 29, -254,
		-316, -81, 250, 370, 144, -239, -428, -222,
		219, 494, 319, -185, -569, -445, 133, 662,
		617, -51, -785, -870, -86, 973, 1301, 349,
		-1332, -2264, -1049, 2492, 6936, 10005, 10005,
		6936, 2492, -1049, -2264, -1332, 349, 1301,
		973, -86, -870, -785, -51, 617, 662, 133,
		-445, -569, -185, 319, 494, 219, -222, -428,
		-239, 144, 370, 250, -81, -316, -254, 29, 267,
		251, 13, -221, -244, -47, 179, 232, 75, -140,
		-218, -97, 104, 201, 113, -72, -182, -124, 43,
		162, 130, -17, -141, -133, -6, 120, 133, 26,
		-99, -129, -42, 79, 123, 55, -60, -115, -65,
		41, 106, 72, -24, -95, -77, 9, 83, 79, 4, -71,
		-79, -16, 59, 78, 26, -47, -74, -34, 35, 69,
		40, -24, -64, -45, 13, 57, 47, -4, -50, -49,
		-4, 42, 49, 11, -35, -48, -17, 27, 45, 22, -20,
		-43, -26, 13, 39, 29, -7, -35, -31, 1, 32, 33,
		4, -28, -34, -9, 24, 35, 15, -19, -37, -21, 14,
		39, 30, -7, -42, -44, -8, 41, 66, 43, -21, -93,
		-140, -142, -109, -61, -24, 1};

short x3d[FILTERx3d_LG] = {
		-10, -1, 28, 82, 142, 172, 145, 62, -35, -91,
		-70, 7, 80, 87, 18, -72, -107, -50, 58, 126,
		89, -33, -139, -133, -6, 141, 179, 60, -128,
		-223, -128, 95, 260, 209, -37, -281, -301,
		-49, 281, 399, 168, -250, -499, -326, 175,
		595, 533, -39, -683, -814, -197, 758, 1231,
		631, -815, -2014, -1655, 850, 4785, 8387,
		9842, 8387, 4785, 850, -1655, -2014, -815,
		631, 1231, 758, -197, -814, -683, -39, 533,
		595, 175, -326, -499, -250, 168, 399, 281,
		-49, -301, -281, -37, 209, 260, 95, -128,
		-223, -128, 60, 179, 141, -6, -133, -139,
		-33, 89, 126, 58, -50, -107, -72, 18, 87,
		80, 7, -70, -91, -35, 62, 145, 172, 142,
		82, 28, -1, -10};

short x3e[FILTERx3e_LG] = {
		40, -80, -178, -249, -211, -51, 145, 237,
		133, -112, -314, -282, 7, 359, 474, 194,
		-328, -688, -521, 163, 896, 1031, 247,
		-1072, -1931, -1293, 1189, 4790, 7982,
		9255, 7982, 4790, 1189, -1293, -1931,
		-1072, 247, 1031, 896, 163, -521, -688,
		-328, 194, 474, 359, 7, -282, -314, -112,
		133, 237, 145, -51, -211, -249, -178,
		-80, 40};

short x3f[FILTERx3f_LG] = {
		-333, -79, 484, 613, -177, -837, -30,
		1390, 885, -1760, -2590, 2091, 9949,
		13880, 9949, 2091, -2590, -1760, 885,
		1390, -30, -837, -177, 613, 484, -79,
		-333};


short x4[FILTERx4_LG] = {
		81, 44, 23, -24, -90, -155, -192, -175, -90,
		56, 231, 381, 443, 368, 137, -218, -615,
		-931, -1031, -800, -182, 801, 2034, 3334,
		4481, 5269, 5549, 5269, 4481, 3334, 2034,
		801, -182, -800, -1031, -931, -615, -218,
		137, 368, 443, 381, 231, 56, -90, -175,
		-192, -155, -90, -24, 23, 44, 81};

short x4b[FILTERx4b_LG] = {
		71, -49, -205, -414, -569, -546, -276,
		180, 625, 794, 498, -233, -1091, -1574,
		-1198, 258, 2583, 5158, 7167, 7926, 7167,
		5158, 2583, 258, -1198, -1574, -1091,
		-233, 498, 794, 625, 180, -276, -546,
		-569, -414, -205, -49, 71};

short x5[FILTERx5_LG] = {
		126, 180, 264, 327, 339, 275, 120, -119,
		-411, -697, -905, -952, -773, -328, 378,
		1292, 2315, 3318, 4164, 4728, 4926, 4728,
		4164, 3318, 2315, 1292, 378, -328, -773,
		-952, -905, -697, -411, -119, 120, 275,
		339, 327, 264, 180, 126};

short x7[FILTERx7_LG] = {
		-58, -132, -126, -200, -222, -255, -251,
		-228, -171, -88, 17, 131, 241, 328, 377,
		374, 311, 187, 10, -200, -418, -611, -744,
		-784, -703, -485, -127, 360, 949, 1598, 2259,
		2875, 3394, 3769, 3966, 3966, 3769, 3394, 2875,
		2259, 1598, 949, 360, -127, -485, -703, -784,
		-744, -611, -418, -200, 10, 187, 311, 374, 377,
		328, 241, 131, 17, -88, -171, -228, -251,
		-255, -222, -200, -126, -132, -58};

short x7b[FILTERx7b_LG] = {
		-197, -214, -235, -151, 90, 518, 1131,
		1884, 2692, 3445, 4028, 4347, 4347,
		4028, 3445, 2692, 1884, 1131, 518, 90,
		-151, -235, -214, -197};

int		alsa2vcd_ptrdelay1_p;
short  *alsa2vcd_pdelaystart1_p;
int		alsa2vcd_ptrdelay2_p;
short  *alsa2vcd_pdelaystart2_p;
int		alsa2vcd_ptrdelay3_p;
short  *alsa2vcd_pdelaystart3_p;
int		alsa2vcd_ptrdelay4_p;
short  *alsa2vcd_pdelaystart4_p;

int		vcd2alsa_ptrdelay1_p;
short  *vcd2alsa_pdelaystart1_p;
int		vcd2alsa_ptrdelay2_p;
short  *vcd2alsa_pdelaystart2_p;
int		vcd2alsa_ptrdelay3_p;
short  *vcd2alsa_pdelaystart3_p;
int		vcd2alsa_ptrdelay4_p;
short  *vcd2alsa_pdelaystart4_p;

short	alsa2vcd_delay1_ap[2*(FILTER_MAX_LG+1)];
short	alsa2vcd_delay2_ap[2*(FILTER_MAX_LG+1)];
short	alsa2vcd_delay3_ap[2*(FILTER_MAX_LG+1)];
short	alsa2vcd_delay4_ap[2*(FILTER_MAX_LG+1)];

short	vcd2alsa_delay1_ap[2*(FILTER_MAX_LG+1)];
short	vcd2alsa_delay2_ap[2*(FILTER_MAX_LG+1)];
short	vcd2alsa_delay3_ap[2*(FILTER_MAX_LG+1)];
short	vcd2alsa_delay4_ap[2*(FILTER_MAX_LG+1)];


short *sh_resampler_in_stage_1;
short *sh_resampler_in_stage_2;
short *sh_resampler_in_stage_3;
short *sh_resampler_in_stage_4;
short *sh_resampler_in_stage_5;

short *sh_resampler_out_stage_1;
short *sh_resampler_out_stage_2;
short *sh_resampler_out_stage_3;
short *sh_resampler_out_stage_4;
short *sh_resampler_out_stage_5;

dma_addr_t phys_in_stage_1;
dma_addr_t phys_in_stage_2;
dma_addr_t phys_in_stage_3;
dma_addr_t phys_in_stage_4;
dma_addr_t phys_in_stage_5;

dma_addr_t phys_out_stage_1;
dma_addr_t phys_out_stage_2;
dma_addr_t phys_out_stage_3;
dma_addr_t phys_out_stage_4;
dma_addr_t phys_out_stage_5;

short *sh_resampler_d_in_stage_1;
short *sh_resampler_d_in_stage_2;
short *sh_resampler_d_in_stage_3;
short *sh_resampler_d_in_stage_4;
short *sh_resampler_d_in_stage_5;

short *sh_resampler_d_out_stage_1;
short *sh_resampler_d_out_stage_2;
short *sh_resampler_d_out_stage_3;
short *sh_resampler_d_out_stage_4;
short *sh_resampler_d_out_stage_5;

dma_addr_t phys_d_in_stage_1;
dma_addr_t phys_d_in_stage_2;
dma_addr_t phys_d_in_stage_3;
dma_addr_t phys_d_in_stage_4;
dma_addr_t phys_d_in_stage_5;

dma_addr_t phys_d_out_stage_1;
dma_addr_t phys_d_out_stage_2;
dma_addr_t phys_d_out_stage_3;
dma_addr_t phys_d_out_stage_4;
dma_addr_t phys_d_out_stage_5;

int sh_resampler_size_d_in1;
int sh_resampler_size_d_out1;

int sh_resampler_size_d_in2;
int sh_resampler_size_d_out2;

int sh_resampler_size_d_in3;
int sh_resampler_size_d_out3;

int sh_resampler_size_d_in4;
int sh_resampler_size_d_out4;

int sh_resampler_size_in1;
int sh_resampler_size_out1;

int sh_resampler_size_in2;
int sh_resampler_size_out2;

int sh_resampler_size_in3;
int sh_resampler_size_out3;

int sh_resampler_size_in4;
int sh_resampler_size_out4;



int sh_resampler_init(
		int input_buffer_alsa_size,
		int input_buffer_vcd_size
		)
{
	/* Init the FIR stuctures */
	sh_resampler_fir_init();

	/* Upsampling */
	sh_resampler_size_in1	= input_buffer_vcd_size*UPSAMPLE_3;
	sh_resampler_size_out1	= input_buffer_vcd_size*UPSAMPLE_3;

	sh_resampler_size_in2	= sh_resampler_size_out1/UPSAMPLE_2;
	sh_resampler_size_out2	= sh_resampler_size_in2*UPSAMPLE_3;

	sh_resampler_size_in3	= sh_resampler_size_out2/UPSAMPLE_4;
	sh_resampler_size_out3	= sh_resampler_size_in3*UPSAMPLE_7;

	sh_resampler_size_in4	= sh_resampler_size_out3/UPSAMPLE_5;
	sh_resampler_size_out4	= sh_resampler_size_in4*UPSAMPLE_7;


	/* Before allocating : Set all to NULL */
	sh_resampler_in_stage_1 = NULL;
	sh_resampler_in_stage_2 = NULL;
	sh_resampler_in_stage_3 = NULL;
	sh_resampler_in_stage_4 = NULL;
	sh_resampler_out_stage_1 = NULL;
	sh_resampler_out_stage_2 = NULL;
	sh_resampler_out_stage_3 = NULL;
	sh_resampler_out_stage_4 = NULL;
	sh_resampler_d_in_stage_1 = NULL;
	sh_resampler_d_in_stage_2 = NULL;
	sh_resampler_d_in_stage_3 = NULL;
	sh_resampler_d_in_stage_4 = NULL;
	sh_resampler_d_out_stage_1 = NULL;
	sh_resampler_d_out_stage_2 = NULL;
	sh_resampler_d_out_stage_3 = NULL;
	sh_resampler_d_out_stage_4 = NULL;

	/* Allocation of the intermediate buffers */

	sh_resampler_in_stage_1 = dma_alloc_coherent(NULL,
					sh_resampler_size_out1*sizeof(short),
					&phys_in_stage_1, GFP_ATOMIC);
	sh_resampler_in_stage_2 = dma_alloc_coherent(NULL,
					sh_resampler_size_out2*sizeof(short),
					&phys_in_stage_2, GFP_ATOMIC);
	sh_resampler_in_stage_3 = dma_alloc_coherent(NULL,
					sh_resampler_size_out3*sizeof(short),
					&phys_in_stage_3, GFP_ATOMIC);
	sh_resampler_in_stage_4 = dma_alloc_coherent(NULL,
					sh_resampler_size_out4*sizeof(short),
					&phys_in_stage_4, GFP_ATOMIC);


	sh_resampler_out_stage_1 = dma_alloc_coherent(NULL,
					sh_resampler_size_out1*sizeof(short),
					&phys_out_stage_1, GFP_ATOMIC);
	sh_resampler_out_stage_2 = dma_alloc_coherent(NULL,
					sh_resampler_size_out2*sizeof(short),
					&phys_out_stage_2, GFP_ATOMIC);
	sh_resampler_out_stage_3 = dma_alloc_coherent(NULL,
					sh_resampler_size_out3*sizeof(short),
					&phys_out_stage_3, GFP_ATOMIC);
	sh_resampler_out_stage_4 = dma_alloc_coherent(NULL,
					sh_resampler_size_out4*sizeof(short),
					&phys_out_stage_4, GFP_ATOMIC);

	/* Downsampling */
	sh_resampler_size_d_in1		= input_buffer_alsa_size*UPSAMPLE_2;
	sh_resampler_size_d_out1	= input_buffer_alsa_size*UPSAMPLE_2;

	sh_resampler_size_d_in2		= sh_resampler_size_d_out1/UPSAMPLE_3;
	sh_resampler_size_d_out2	= sh_resampler_size_d_in2*UPSAMPLE_4;

	sh_resampler_size_d_in3		= sh_resampler_size_d_out2/UPSAMPLE_7;
	sh_resampler_size_d_out3	= sh_resampler_size_d_in3*UPSAMPLE_4;

	sh_resampler_size_d_in4		= sh_resampler_size_d_out3/UPSAMPLE_3;
	sh_resampler_size_d_out4	= sh_resampler_size_d_in4*UPSAMPLE_5;


	sh_resampler_d_in_stage_1 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out1*sizeof(short),
					&phys_d_in_stage_1, GFP_ATOMIC);
	sh_resampler_d_in_stage_2 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out2*sizeof(short),
					&phys_d_in_stage_2, GFP_ATOMIC);
	sh_resampler_d_in_stage_3 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out3*sizeof(short),
					&phys_d_in_stage_3, GFP_ATOMIC);
	sh_resampler_d_in_stage_4 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out4*sizeof(short),
					&phys_d_in_stage_4, GFP_ATOMIC);


	sh_resampler_d_out_stage_1 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out1*sizeof(short),
					&phys_d_out_stage_1, GFP_ATOMIC);
	sh_resampler_d_out_stage_2 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out2*sizeof(short),
					&phys_d_out_stage_2, GFP_ATOMIC);
	sh_resampler_d_out_stage_3 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out3*sizeof(short),
					&phys_d_out_stage_3, GFP_ATOMIC);
	sh_resampler_d_out_stage_4 = dma_alloc_coherent(NULL,
					sh_resampler_size_d_out4*sizeof(short),
					&phys_d_out_stage_4, GFP_ATOMIC);

	if ((sh_resampler_in_stage_1 == NULL)
		|| (sh_resampler_in_stage_2 == NULL)
		|| (sh_resampler_in_stage_3 == NULL)
		|| (sh_resampler_in_stage_4 == NULL)
		|| (sh_resampler_out_stage_1 == NULL)
		|| (sh_resampler_out_stage_2 == NULL)
		|| (sh_resampler_out_stage_3 == NULL)
		|| (sh_resampler_out_stage_4 == NULL)
		|| (sh_resampler_d_in_stage_1 == NULL)
		|| (sh_resampler_d_in_stage_2 == NULL)
		|| (sh_resampler_d_in_stage_3 == NULL)
		|| (sh_resampler_d_in_stage_4 == NULL)
		|| (sh_resampler_d_out_stage_1 == NULL)
		|| (sh_resampler_d_out_stage_2 == NULL)
		|| (sh_resampler_d_out_stage_3 == NULL)
		|| (sh_resampler_d_out_stage_4 == NULL)
		)	{
		return SH_RESAMPLER_ERR_9;
	}



	return SH_RESAMPLER_NO_ERR;
}

int sh_resampler_close(void)
{

	 m_VCD_sampling_rate = 0;
	 m_ALSA_sampling_rate = 0;

	/* Upsampling */
	if (sh_resampler_in_stage_1 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out1*sizeof(short),
			sh_resampler_in_stage_1, phys_in_stage_1);

	if (sh_resampler_in_stage_2 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out2*sizeof(short),
			sh_resampler_in_stage_2, phys_in_stage_2);

	if (sh_resampler_in_stage_3 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out3*sizeof(short),
			sh_resampler_in_stage_3, phys_in_stage_3);

	if (sh_resampler_in_stage_4 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out4*sizeof(short),
			sh_resampler_in_stage_4, phys_in_stage_4);

	if (sh_resampler_out_stage_1 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out1*sizeof(short),
			sh_resampler_out_stage_1, phys_out_stage_1);

	if (sh_resampler_out_stage_2 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out2*sizeof(short),
			sh_resampler_out_stage_2, phys_out_stage_2);

	if (sh_resampler_out_stage_3 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out3*sizeof(short),
			sh_resampler_out_stage_3, phys_out_stage_3);

	if (sh_resampler_out_stage_4 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_out4*sizeof(short),
			sh_resampler_out_stage_4, phys_out_stage_4);

	if (sh_resampler_d_in_stage_1 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out1*sizeof(short),
			sh_resampler_d_in_stage_1, phys_d_in_stage_1);

	if (sh_resampler_d_in_stage_2 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out2*sizeof(short),
			sh_resampler_d_in_stage_2, phys_d_in_stage_2);

	if (sh_resampler_d_in_stage_3 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out3*sizeof(short),
			sh_resampler_d_in_stage_3, phys_d_in_stage_3);

	if (sh_resampler_d_in_stage_4 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out4*sizeof(short),
			sh_resampler_d_in_stage_4, phys_d_in_stage_4);

	if (sh_resampler_d_out_stage_1 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out1*sizeof(short),
			sh_resampler_d_out_stage_1, phys_d_out_stage_1);

	if (sh_resampler_d_out_stage_2 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out2*sizeof(short),
			sh_resampler_d_out_stage_2, phys_d_out_stage_2);

	if (sh_resampler_d_out_stage_3 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out3*sizeof(short),
			sh_resampler_d_out_stage_3, phys_d_out_stage_3);

	if (sh_resampler_d_out_stage_4 != NULL)
		dma_free_coherent(NULL, sh_resampler_size_d_out4*sizeof(short),
			sh_resampler_d_out_stage_4, phys_d_out_stage_4);

	return SH_RESAMPLER_NO_ERR;
}

int sh_resampler_resample(
		short *output_buffer,
		int output_buffer_size,
		short *input_buffer,
		int input_buffer_size,
		int mode
		)
{

	int i;
	long accum;
	int error = output_buffer_size;


	switch (mode) {
	case ALSA2VCD:		/* 44.1k to 16k */
		/* Stage1 */
		sh_resampler_zeropadd(
				input_buffer,
				sh_resampler_d_in_stage_1,
				input_buffer_size,
				sh_resampler_size_d_out1,
				UPSAMPLE_2);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_d_in_stage_1,
				sh_resampler_d_out_stage_1,
				sh_resampler_size_d_out1,
				&x2[0],
				FILTERx2_LG,
				alsa2vcd_pdelaystart1_p,
				&alsa2vcd_ptrdelay1_p, 0, UPSAMPLE_2) != 0) {
			error = SH_RESAMPLER_ERR_1;
		}
		for (i = 0; i < sh_resampler_size_d_in2; i++)	{
			accum = (long)sh_resampler_d_out_stage_1[i*UPSAMPLE_3]
				* (long)UPSAMPLE_2;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			sh_resampler_d_in_stage_1[i] = (short)accum;
		}




		/*  Stage2 */
		sh_resampler_zeropadd(
				sh_resampler_d_in_stage_1,
				sh_resampler_d_in_stage_2,
				sh_resampler_size_d_in2,
				sh_resampler_size_d_out2,
				UPSAMPLE_4);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_d_in_stage_2,
				sh_resampler_d_out_stage_2,
				sh_resampler_size_d_out2,
				&x4[0],
				FILTERx4_LG,
				alsa2vcd_pdelaystart2_p,
				&alsa2vcd_ptrdelay2_p, 0, UPSAMPLE_4) != 0) {
				error = SH_RESAMPLER_ERR_2;
		}
		for (i = 0; i < sh_resampler_size_d_in3; i++)	{
			accum = (long)sh_resampler_d_out_stage_2[i*UPSAMPLE_7]
				* (long)UPSAMPLE_4;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			sh_resampler_d_in_stage_2[i] = (short)accum;
		}

		/* Stage3 */
		sh_resampler_zeropadd(
				sh_resampler_d_in_stage_2,
				sh_resampler_d_in_stage_3,
				sh_resampler_size_d_in3,
				sh_resampler_size_d_out3,
				UPSAMPLE_4);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_d_in_stage_3,
				sh_resampler_d_out_stage_3,
				sh_resampler_size_d_out3,
				&x4[0],
				FILTERx4_LG,
				alsa2vcd_pdelaystart3_p,
				&alsa2vcd_ptrdelay3_p, 0, UPSAMPLE_4) != 0) {
			error = SH_RESAMPLER_ERR_3;
		}
		for (i = 0; i < sh_resampler_size_d_in4; i++)	{
			accum = (long)sh_resampler_d_out_stage_3[i*UPSAMPLE_3]
				* (long)UPSAMPLE_4;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			sh_resampler_d_in_stage_3[i] = (short)accum;
		}

		/* Stage4 */
		sh_resampler_zeropadd(
				sh_resampler_d_in_stage_3,
				sh_resampler_d_in_stage_4,
				sh_resampler_size_d_in4,
				sh_resampler_size_d_out4,
				UPSAMPLE_5);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_d_in_stage_4,
				sh_resampler_d_out_stage_4,
				sh_resampler_size_d_out4,
				&x5[0],
				FILTERx5_LG,
				alsa2vcd_pdelaystart4_p,
				&alsa2vcd_ptrdelay4_p , 0, UPSAMPLE_5) != 0) {
			error = SH_RESAMPLER_ERR_4;
		}

		/* Dowsample */
		for (i = 0; i < output_buffer_size; i++)	{
			accum = (long)sh_resampler_d_out_stage_4[i*UPSAMPLE_7]
				* (long)UPSAMPLE_5;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			output_buffer[i] = (short)accum;
		}

		break;


	case VCD2ALSA:
		/*  Stage1 */
		sh_resampler_zeropadd(
				input_buffer,
				sh_resampler_in_stage_1,
				input_buffer_size,
				sh_resampler_size_out1,
				UPSAMPLE_3);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_in_stage_1,
				sh_resampler_out_stage_1,
				sh_resampler_size_out1,
				&x3f[0],
				FILTERx3f_LG,
				vcd2alsa_pdelaystart1_p,
				&vcd2alsa_ptrdelay1_p, 0, UPSAMPLE_3) != 0)
			error = SH_RESAMPLER_ERR_5;


		for (i = 0; i < sh_resampler_size_in2; i++)	{
			accum = (long)sh_resampler_out_stage_1[i*UPSAMPLE_2]
				* (long)UPSAMPLE_3;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			sh_resampler_in_stage_1[i] = (short)accum;
		}




		/* Stage2 */
		sh_resampler_zeropadd(
				sh_resampler_in_stage_1,
				sh_resampler_in_stage_2,
				sh_resampler_size_in2,
				sh_resampler_size_out2,
				UPSAMPLE_3);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_in_stage_2,
				sh_resampler_out_stage_2,
				sh_resampler_size_out2,
				&x3b[0],
				FILTERx3b_LG,
				vcd2alsa_pdelaystart2_p,
				&vcd2alsa_ptrdelay2_p, 0, UPSAMPLE_3) != 0)
			error = SH_RESAMPLER_ERR_6;


		for (i = 0; i < sh_resampler_size_in3; i++)	{
			accum = (long)sh_resampler_out_stage_2[i*UPSAMPLE_4]
				* (long)UPSAMPLE_3;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			sh_resampler_in_stage_2[i] = (short)accum;
		}


		/* Stage3 */
		sh_resampler_zeropadd(
				sh_resampler_in_stage_2,
				sh_resampler_in_stage_3,
				sh_resampler_size_in3,
				sh_resampler_size_out3,
				UPSAMPLE_7);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_in_stage_3,
				sh_resampler_out_stage_3,
				sh_resampler_size_out3,
				&x7b[0],
				FILTERx7b_LG,
				vcd2alsa_pdelaystart3_p,
				&vcd2alsa_ptrdelay3_p, 0, UPSAMPLE_7) != 0)
			error = SH_RESAMPLER_ERR_7;


		for (i = 0; i < sh_resampler_size_in4; i++)	{
			accum = (long)sh_resampler_out_stage_3[i*UPSAMPLE_5]
				* (long)UPSAMPLE_7;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			sh_resampler_in_stage_3[i] = (short)accum;
		}

		/* Stage4 */
		sh_resampler_zeropadd(
				sh_resampler_in_stage_3,
				sh_resampler_in_stage_4,
				sh_resampler_size_in4,
				sh_resampler_size_out4,
				UPSAMPLE_7);
		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_in_stage_4,
				sh_resampler_out_stage_4,
				sh_resampler_size_out4,
				&x7b[0],
				FILTERx7b_LG,
				vcd2alsa_pdelaystart4_p,
				&vcd2alsa_ptrdelay4_p, 0, UPSAMPLE_7) != 0)
			error = SH_RESAMPLER_ERR_8;


		/* Dowsample */
		for (i = 0; i < output_buffer_size; i++)	{
			accum = (long)sh_resampler_out_stage_4[i*UPSAMPLE_4]
				* (long)UPSAMPLE_7;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			output_buffer[i] = (short)accum;
		}

		break;
	case ALSA2VCD_MEMCPY:
		for (i = 0; i < output_buffer_size; i++)
			output_buffer[i] = input_buffer[i];

		break;
	case VCD2ALSA_MEMCPY:
		for (i = 0; i < input_buffer_size; i++)
			output_buffer[i] = input_buffer[i];

		for (i = input_buffer_size; i < output_buffer_size; i++)
			output_buffer[i] = 0;

		break;
	case ALSA_482VCD:		/* 48k to 16k */
		if (omxSP_FIR_Direct_S16_poly(
				input_buffer,
				sh_resampler_out_stage_1,
				sh_resampler_size_out1,
				&x3f[0],
				FILTERx3f_LG,
				alsa2vcd_pdelaystart1_p,
				&alsa2vcd_ptrdelay1_p, 0, 1) != 0) {
			error = SH_RESAMPLER_ERR_1;
		}
		/* Downsampling of 3 */
		for (i = 0; i < sh_resampler_size_out1/UPSAMPLE_3; i++)
			output_buffer[i] =
				sh_resampler_out_stage_1[i*UPSAMPLE_3];

		break;

	case VCD2ALSA_48:		/* 16k to 48k */
		sh_resampler_zeropadd(
				input_buffer,
				sh_resampler_in_stage_1,
				input_buffer_size,
				sh_resampler_size_out1,
				UPSAMPLE_3);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_in_stage_1,
				output_buffer,
				sh_resampler_size_out1,
				&x3f[0],
				FILTERx3f_LG,
				vcd2alsa_pdelaystart1_p,
				&vcd2alsa_ptrdelay1_p, 0, 1) != 0)
			error = SH_RESAMPLER_ERR_1;

		for (i = 0; i < sh_resampler_size_out1; i++)	{
			accum = (long)output_buffer[i]
				* (long)UPSAMPLE_3;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			output_buffer[i] = (short)accum;
		}

		break;

	case ALSA_8_2_VCD_16:		/* 8k to 16k */
		sh_resampler_zeropadd(
				input_buffer,
				sh_resampler_in_stage_1, /* oversdimensioned*/
				input_buffer_size,
				(int)input_buffer_size*UPSAMPLE_2,
				UPSAMPLE_2);

		if (omxSP_FIR_Direct_S16_poly(
				sh_resampler_in_stage_1,
				output_buffer,
				(int)input_buffer_size*UPSAMPLE_2,
				&x2b[0],
				FILTERx2b_LG,
				alsa2vcd_pdelaystart1_p,
				&alsa2vcd_ptrdelay1_p, 0, 1) != 0)
			error = SH_RESAMPLER_ERR_1;

		for (i = 0; i < (int)input_buffer_size*UPSAMPLE_2; i++)	{
			accum = (long)output_buffer[i]
				* (long)UPSAMPLE_2;
			if (accum > SH_RESAMPLER_MAX_S16)
				accum = (long)SH_RESAMPLER_MAX_S16;
			if (accum < SH_RESAMPLER_MIN_S16)
				accum = SH_RESAMPLER_MIN_S16;
			output_buffer[i] = (short)accum;
		}

		break;
	case VCD_16_2_ALSA_8:		/* 16k to 8k */
		if (omxSP_FIR_Direct_S16_poly(
				input_buffer,
				sh_resampler_out_stage_1,
				input_buffer_size,
				&x2b[0],
				FILTERx2b_LG,
				vcd2alsa_pdelaystart1_p,
				&vcd2alsa_ptrdelay1_p, 0, 1) != 0) {
			error = SH_RESAMPLER_ERR_1;
		}
		/* Downsampling of 2 */
		for (i = 0; i < input_buffer_size/UPSAMPLE_2; i++)
			output_buffer[i] =
					sh_resampler_out_stage_1[i*UPSAMPLE_2];

		break;


	default:
		/* Exit code */
			break;
	}

	return error;
}



int sh_resampler_fir_init(void)
{
	int i;

	alsa2vcd_ptrdelay1_p		= 0;
	alsa2vcd_pdelaystart1_p	= &alsa2vcd_delay1_ap[0];

	alsa2vcd_ptrdelay2_p		= 0;
	alsa2vcd_pdelaystart2_p	= &alsa2vcd_delay2_ap[0];

	alsa2vcd_ptrdelay3_p		= 0;
	alsa2vcd_pdelaystart3_p	= &alsa2vcd_delay3_ap[0];

	alsa2vcd_ptrdelay4_p		= 0;
	alsa2vcd_pdelaystart4_p	= &alsa2vcd_delay4_ap[0];

	vcd2alsa_ptrdelay1_p		= 0;
	vcd2alsa_pdelaystart1_p	= &vcd2alsa_delay1_ap[0];

	vcd2alsa_ptrdelay2_p		= 0;
	vcd2alsa_pdelaystart2_p	= &vcd2alsa_delay2_ap[0];

	vcd2alsa_ptrdelay3_p		= 0;
	vcd2alsa_pdelaystart3_p	= &vcd2alsa_delay3_ap[0];

	vcd2alsa_ptrdelay4_p		= 0;
	vcd2alsa_pdelaystart4_p	= &vcd2alsa_delay4_ap[0];

	for (i = 0; i < 2*(FILTER_MAX_LG+1); i++)	{
		alsa2vcd_pdelaystart1_p[i] = 0;
		alsa2vcd_pdelaystart2_p[i] = 0;
		alsa2vcd_pdelaystart3_p[i] = 0;
		alsa2vcd_pdelaystart4_p[i] = 0;

		vcd2alsa_pdelaystart1_p[i] = 0;
		vcd2alsa_pdelaystart2_p[i] = 0;
		vcd2alsa_pdelaystart3_p[i] = 0;
		vcd2alsa_pdelaystart4_p[i] = 0;
	}


	return 0;
}



void sh_resampler_zeropadd(
		short *sample_in,
		short *sample_out,
		int length_in, int
		length_out,
		int rate)
{
	int i;

	/* Oversample the input */
	memset(sample_out, 0, length_out*sizeof(short));
	for (i = 0 ; i < length_in ; i++)
		sample_out[i*rate] = sample_in[i];

}

int omxSP_FIR_Direct_S16_poly(
	const short *pSrc,
	short *pDst,
	int sampLen,
	const short *pTapsQ15,
	int tapsLen,
	short *pDelayLine,
	int *pDelayLineIndex,
	int offset,
	int step
)
{
	int  Count;

	int index;
	unsigned long offset_loc;
	long accum;
	short *pDelayCurrent;


	for (Count = 0; Count < (int)sampLen; Count++)	{
		/* Input parameter check */
		/* Update local pos of the offset */
		offset_loc = Count%step;

		/* Update the delay state */
		pDelayCurrent = &pDelayLine[*pDelayLineIndex];

		/* Copy input to current delay line position */
		pDelayCurrent[0] = pDelayCurrent[tapsLen] = pSrc[Count];

		accum = 0;
		for (index = offset_loc; index < (int) tapsLen; index += step)
			accum += (long)pTapsQ15[index] * pDelayCurrent[index];

		if (--(*pDelayLineIndex) < 0)
			*pDelayLineIndex = tapsLen - 1;


		/* Store the result */
		accum = armSatAdd_S32(accum, 16384);
		*(pDst+Count) = (short)(accum >> 15);

	}

	return 0;
}


/**
 * Function:  omxSP_FIROne_Direct_S16   (2.2.3.1.2)
 *
 * Description:
 * Single-sample FIR filtering for 16-bit data type. These functions apply
 * the FIR filter defined by the coefficient vector pTapsQ15 to a single
 * sample of input data. The result is saturated with rounding if the
 * operation produces a value outside the range of a signed 16-bit integer.
 * Rounding behavior is defined in:
 *       section 1.6.7 "Integer Scaling and Rounding Conventions".
 * The internal accumulator width must be at least 32 bits.  The result is
 * undefined if any of the partially accumulated values exceeds the range of a
 * signed 32-bit integer.
 *
 * Input Arguments:
 *
 *   val      - the single input sample to which the filter is
 *            applied.
 *   pTapsQ15 - pointer to the vector that contains the filter coefficients,
 *            represented in Q0.15 format (as defined in section 1.6.5). Given
 *            that:
 *                    -32768 = pTapsQ15(k) < 32768,
 *                         0 = k < tapsLen,
 *            the range on the actual filter coefficients is -1 = bK <1, and
 *            therefore coefficient normalization may be required during the
 *            filter design process.
 *   tapsLen - the number of taps, or, equivalently, the filter order + 1
 *   pDelayLine - pointer to the 2.tapsLen -element filter memory buffer
 *            (state). The user is responsible for allocation, initialization,
 *            and de-allocation. The filter memory elements are initialized to
 *            zero in most applications.
 *   pDelayLineIndex - pointer to the filter memory index that is maintained
 *            internally by the function. The user should initialize the value
 *            of this index to zero.
 *
 * Output Arguments:
 *
 *   pResult - pointer to the filtered output sample
 *
 * Return Value:
 *
 *    OMX_Sts_NoErr - no error
 *    OMX_Sts_BadArgErr - bad arguments; returned if one or more of the
 *              following is true:
 *    -    One or more of the following pointers is NULL:
 *            -  pResult,
 *            -  pTapsQ15,
 *            -  pDelayLine, or
 *            -  pDelayLineIndex
 *    -    tapslen < 1
 *    -    *pDelayLineIndex < 0 or *pDelayLineIndex >= (2 * tapslen)
 *
 */

int omxSP_FIROne_Direct_S16_poly(
		short val,
		short *pResult,
		const short *pTapsQ15,
		int tapsLen,
		short *pDelayLine,
		int *pDelayLineIndex,
		int offset,
		int step
	)
	{
	unsigned long index;
	unsigned long offset_loc;
	long accum;
	short *pDelayCurrent;

	/* Input parameter check */
	/* Update local pos of the offset */
	offset_loc = offset%step;

	/* Update the delay state */
	pDelayCurrent = &pDelayLine[*pDelayLineIndex];

	/* Copy input to current delay line position */
	pDelayCurrent[0] = pDelayCurrent[tapsLen] = val;

	accum = 0;
	for (index = offset_loc; index < (unsigned long)tapsLen; index += step)
		accum += (long)pTapsQ15[index] * (long)pDelayCurrent[index];

	if (--(*pDelayLineIndex) < 0)
		*pDelayLineIndex = tapsLen - 1;


	/* Store the result */
	*pResult = (short)armSatRoundLeftShift_S32(accum, -15);
	return 0;
}



/**
 * Function :armSatRoundLeftShift_S32()
 *
 * Description :
 *     Returns the result of saturating left-shift operation on input
 *     Or rounded Right shift if the input Shift is negative.
 *
 * Parametrs:
 * [in] Value        Operand
 * [in] Shift        Operand for shift operation
 *
 * Return:
 * [out]             Result of operation
 *
 **/

long armSatRoundLeftShift_S32(long Value, int Shift)
{
	int i;

	if (Shift < 0)    {
		Shift = -Shift;
		Value = armSatAdd_S32(Value, (1 << (Shift - 1)));
		Value = Value >> Shift;
	}	else	{
	for (i = 0; i < Shift; i++)
		Value = armSatAdd_S32(Value, Value);
	}
	return Value;
}

/***********************************************************************/
	/* Saturating Arithmetic operations */

/**
 * Function :armSatAdd_S32()
 *
 * Description :
 *   Returns the result of saturated addition of the two inputs Value1, Value2
 *
 * Parametrs:
 * [in] Value1       First Operand
 * [in] Value2       Second Operand
 *
 * Return:
 * [out]             Result of operation
 *
 *
 **/

long armSatAdd_S32(long Value1, long Value2)
{
	long Result;

	Result = Value1 + Value2;

	if ((Value1^Value2) >= 0)	{
		/*Same sign*/
	if ((Result^Value1) >= 0)	{
		/*Result has not saturated*/
		return	Result;
	}	else	{
	if (Value1 >= 0)	{
		/*Result has saturated in positive side*/
		return	SH_RESAMPLER_MAX_S32;
	}	else	{
	/*Result has saturated in negative side*/
	return	SH_RESAMPLER_MIN_S32;
	}
	}
	}	else	{
	return	Result;
	}

}
