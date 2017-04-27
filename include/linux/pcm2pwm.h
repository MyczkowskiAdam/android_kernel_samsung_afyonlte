/*
 * /include/linux/pcm2pwm.h
 *
 * Copyright (C) 2011-2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef __PCM2PWM_H
#define __PCM2PWM_H

#ifdef __KERNEL__

/* Enumerating value of control switch of PCM2PWM */
enum pcm2pwm_request_state {
	STOP_PCM2PWM,
	START_PCM2PWM,
};

/* Port information */
struct pcm2pwm_port_info {
	const int port_func;
	const char *func_name;
};


extern int pcm2pwm_open(void);
extern int pcm2pwm_close(void);
extern int pcm2pwm_enable(enum pcm2pwm_request_state state,
							const void *src, unsigned int data_sz, u16 cnt);

#endif /* __KERNEL__ */
#endif /* __PCM2PWM_H */
