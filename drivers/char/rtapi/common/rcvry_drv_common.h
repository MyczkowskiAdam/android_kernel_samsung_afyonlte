/*
 * rcvry_drv_common.h
 *     Recover Driver Communication common definition header file.
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __RCVRY_DRV_COMMON_H__
#define __RCVRY_DRV_COMMON_H__

/* ******************************* CONSTANTS ******************************** */
#define RTDS_RCVRY_CMD_MAGIC 'O'

/* IOCTL command */
enum {
	RCVRY_CMD_NUM_STANDBY_NG = 10,
	RCVRY_CMD_NUM_STANDBY_NG_CANCEL,
	RCVRY_CMD_NUM_KILL_WAIT,
	RCVRY_CMD_NUM_KILL_WAIT_CANCEL,
	RCVRY_CMD_NUM_GET_PID,

	RCVRY_COMMAND_END
};

#define RCVRY_CMD_STANDBY_NG \
_IO(RTDS_RCVRY_CMD_MAGIC, RCVRY_CMD_NUM_STANDBY_NG)
#define RCVRY_CMD_STANDBY_NG_CANCEL \
_IO(RTDS_RCVRY_CMD_MAGIC, RCVRY_CMD_NUM_STANDBY_NG_CANCEL)
#define RCVRY_CMD_KILL_WAIT \
_IO(RTDS_RCVRY_CMD_MAGIC, RCVRY_CMD_NUM_KILL_WAIT)
#define RCVRY_CMD_KILL_WAIT_CANCEL \
_IO(RTDS_RCVRY_CMD_MAGIC, RCVRY_CMD_NUM_KILL_WAIT_CANCEL)
#define RCVRY_CMD_GET_PID \
_IO(RTDS_RCVRY_CMD_MAGIC, RCVRY_CMD_NUM_GET_PID)

#endif /* __RCVRY_DRV_COMMON_H__ */
