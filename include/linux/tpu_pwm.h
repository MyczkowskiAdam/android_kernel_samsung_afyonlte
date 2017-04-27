/*
 * /include/linux/tpu_pwm.h
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

#ifndef __TPU_PWM_H
#define __TPU_PWM_H

#ifdef __KERNEL__


/* Enumerating value of control switch of PWM. */
enum tpu_pwm_state {
	TPU_PWM_STOP,
	TPU_PWM_START
};

extern int tpu_pwm_open(const char *channel, int prescaler, void **handle);

extern int tpu_pwm_close(void *handle);

extern int tpu_pwm_enable(void *handle, enum tpu_pwm_state state,
							int duty, int cycle);

#endif	/* __KERNEL__ */

#endif	/* __TPU_PWM_H */
