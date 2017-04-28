/* vcd_interface.h
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
#ifndef __VCD_INTERFACE_H__
#define __VCD_INTERFACE_H__

/*
 * constant definition
 */


/*
 * define macro declaration
 */


/*
 * enum declaration
 */


/*
 * structure declaration
 */


/*
 * table declaration
 */


/*
 * prototype declaration
 */
extern void vcd_complete_buffer(void);
extern void vcd_beginning_buffer(void);
extern void vcd_start_fw(void);
extern void vcd_stop_fw(int result);
extern void vcd_stop_fw_only_sound(void);
extern void vcd_codec_type_ind(unsigned int codec_type);
extern void vcd_udata_ind(void);
extern void vcd_start_clkgen(void);
extern void vcd_stop_clkgen(void);
extern void vcd_wait_path(void);
extern void vcd_voip_ul_callback(unsigned int buf_size);
extern void vcd_voip_dl_callback(unsigned int buf_size);
extern void vcd_get_semaphore(void);
extern void vcd_release_semaphore(void);

#endif /* __VCD_INTERFACE_H__ */
