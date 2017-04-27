/* libvcd.h
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

/* for the asynchronous response information */
struct libvcd_status_client_header {
	pid_t tid;			/* thread id of client */
	int fd;				/* file descriptor for ioctl */
	int epoll_fd;			/* file descriptor for epoll */
	void *logical_address;		/* logical address after mmap */
	int cmn_mem_size;
	unsigned long int thread;	/* asynchronous Monitoring thread */
	pid_t monitoring_tid;/* thread id of asynchronous Monitoring thread */
};

struct libvcd_status_async_info {
	unsigned int pre_write;	/* pre-write flag */
	int result;		/* return result asynchronously */
};


/*
 * enum declaration
 */
enum LIBVCD_CB_TYPE {
	LIBVCD_CB_TYPE_SYSTEM_ERROR = 0,
	LIBVCD_CB_TYPE_UDATA,
	LIBVCD_CB_TYPE_VCD_END,
	LIBVCD_CB_TYPE_MAX,
};

/* lib - driver interface parameter */
union vcd_ioctl_param {
	unsigned int addr;	/* asyncronous response address */
	unsigned int file_path;	/* set binary file path address */
	unsigned int write_size;/* set binary write size address */
};


struct libvcd_status_async_map {
	/* header of client */
	struct libvcd_status_client_header client;
	/* asynchronous response information */
	struct libvcd_status_async_info status[LIBVCD_CB_TYPE_MAX];
};

struct vocoder_ioctl_message {
	union vcd_ioctl_param param;	/* API parameter */
	int kind;			/* request or control id */
};

/*
 * define macro declaration
 */
#define LIBVCD_DEVICE_SPECIAL_FILE_NAME	"/dev/vcd_mem"
#define LIBVCD_FALSE	0
#define LIBVCD_TRUE	1
#define ERROR_NONE	0	/* no error occurred */
#define EVENTS_MAX	1
#define NO_ADDRESS	0

#define PROT_READ	0x1	/* page can be read */
#define PROT_WRITE	0x2	/* page can be written */
#define PROT_EXEC	0x4	/* page can be executed */
#define PROT_SEM	0x8	/* page may be used for atomic ops */
#define PROT_NONE	0x0	/* page can not be accessed */

#define MAP_SHARED	0x01	/* Share changes */
#define MAP_PRIVATE	0x02	/* Changes are private */
#define MAP_TYPE	0x0f	/* Mask for type of mapping */
#define MAP_FIXED	0x10	/* Interpret addr exactly */

#define VCD_IOCTL_MAGIC		'P'
#define VCD_IOCTL_OPEN				\
	_IO(VCD_IOCTL_MAGIC, 0)
#define VCD_IOCTL_CLOSE				\
	_IO(VCD_IOCTL_MAGIC, 1)
#define VCD_IOCTL_GET_MSG_BUF			\
	_IOR(VCD_IOCTL_MAGIC, 2, unsigned int)
#define VCD_IOCTL_START_VCD			\
	_IO(VCD_IOCTL_MAGIC, 3)
#define VCD_IOCTL_STOP_VCD			\
	_IO(VCD_IOCTL_MAGIC, 4)
#define VCD_IOCTL_SET_HW_PARAM			\
	_IO(VCD_IOCTL_MAGIC, 5)
#define VCD_IOCTL_START_CALL			\
	_IO(VCD_IOCTL_MAGIC, 6)
#define VCD_IOCTL_STOP_CALL			\
	_IO(VCD_IOCTL_MAGIC, 7)
#define VCD_IOCTL_UDATA				\
	_IO(VCD_IOCTL_MAGIC, 8)
#define VCD_IOCTL_GET_BIN_BUF			\
	_IOR(VCD_IOCTL_MAGIC, 9, unsigned int)
#define VCD_IOCTL_START_SET_BIN			\
	_IOW(VCD_IOCTL_MAGIC, 10, unsigned int)
#define VCD_IOCTL_EXEC_SET_BIN			\
	_IOW(VCD_IOCTL_MAGIC, 11, unsigned int)
#define VCD_IOCTL_END_SET_BIN			\
	_IO(VCD_IOCTL_MAGIC, 12)
#define VCD_IOCTL_GET_ASYNC_MEM			\
	_IOR(VCD_IOCTL_MAGIC, 100, unsigned int)
#define VCD_IOCTL_FREE_ASYNC_MEM		\
	_IOW(VCD_IOCTL_MAGIC, 101, struct libvcd_status_async_map)

