/* sh_resampler.h
 *
 * Copyright (C) 2012 Renesas Mobile Corp.
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

#ifndef SH_RESAMPLER_H_
#define SH_RESAMPLER_H_


/*
 * prototype declaration
 */

/**
 * @brief	initialization function.
 *
 * @param	alsa_buffer_size	ALSA buffer size
 * @param	vcd_buffer_size	    vcd buffer size.
 *
 * @retval	0 if success.
 */
int sh_resampler_init(int alsa_buffer_size, int vcd_buffer_size);

/**
 * @brief	close function.
 *
 * @retval	0 if success.
 */
int sh_resampler_close(void);

/**
 * @brief	resampler work function.
 *
 * @param	output_buffer	pointer to output buffer.
 * @param	output_buffer_size	maximum size allocated to output buffer
 * @param	input_buffer	pointer to input buffer.
 * @param	input_buffer_size	number of sample available in output buffer
 * @param	mode	ALSA to VCD or VCD to alsa.
 *
 * @retval	number of samples available in buffer_out.
 */
int sh_resampler_resample(
		short *output_buffer,
		int output_buffer_size,
		short *input_buffer,
		int input_buffer_size,
		int mode);

/*
 * define macro declaration
 */
/* 16 to 44.1 kHz conversion */
#define VCD2ALSA 0
/* 44.1 to 16 kHz conversion */
#define ALSA2VCD 1
/* 16 to 44.1 kHz conversion */
/* (debug purpose only, memecpy with padding
   audio quality is not maintained) */
#define VCD2ALSA_MEMCPY 2
/* 44.1 to 16 kHz conversion */
#define ALSA2VCD_MEMCPY 3
/* 16 to 48 kHz conversion */
#define VCD2ALSA_48 4
/* 48 to 16 kHz conversion */
#define ALSA_482VCD 5
/* 8 to 16 kHz conversion */
#define  ALSA_8_2_VCD_16 6
/* 16 to 8 kHz conversion */
#define VCD_16_2_ALSA_8 7






/* Error management */
#define SH_RESAMPLER_ERR_BASE 0
#define SH_RESAMPLER_NO_ERR 0
#define SH_RESAMPLER_ERR_1 (SH_RESAMPLER_ERR_BASE-1)
#define SH_RESAMPLER_ERR_2 (SH_RESAMPLER_ERR_BASE-2)
#define SH_RESAMPLER_ERR_3 (SH_RESAMPLER_ERR_BASE-3)
#define SH_RESAMPLER_ERR_4 (SH_RESAMPLER_ERR_BASE-4)
#define SH_RESAMPLER_ERR_5 (SH_RESAMPLER_ERR_BASE-5)
#define SH_RESAMPLER_ERR_6 (SH_RESAMPLER_ERR_BASE-6)
#define SH_RESAMPLER_ERR_7 (SH_RESAMPLER_ERR_BASE-7)
#define SH_RESAMPLER_ERR_8 (SH_RESAMPLER_ERR_BASE-8)
#define SH_RESAMPLER_ERR_9 (SH_RESAMPLER_ERR_BASE-9) /* Error in mem alloc */


/* Other functions */
int sh_resampler_fir_init(
		void
		);
short *sh_resampler_cyclic_add(
		short	*p,
		int offs,
		short	*start,
		int len
		);
void sh_resampler_zeropadd(
		short *sample_in,
		short *sample_out,
		int length_in,
		int length_out,
		int rate
		);
int omxSP_FIROne_Direct_S16_poly(
		short val,
		short *pResult,
		const short	*pTapsQ15,
		int tapsLen,
		short *pDelayLine,
		int *pDelayLineIndex,
		int offset,
		int step
		);
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
		);
long armSatRoundLeftShift_S32(
		long Value,
		int Shift
		);
long armSatAdd_S32(
		long Value1,
		long Value2
		);




#endif /* SH_RESAMPLER_H_ */
