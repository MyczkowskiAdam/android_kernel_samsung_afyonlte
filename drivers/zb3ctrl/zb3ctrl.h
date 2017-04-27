#ifndef __SBSC_H__
#define __SBSC_H__

#include <linux/ioctl.h>

#define ZB3_CTRL_MAGIC		'Z'
#define ZB3_CTRL_REQ _IOC(_IOC_WRITE, ZB3_CTRL_MAGIC, 1, sizeof(int))

#define ZB3_STATE_NONE  0
#define ZB3_STATE_LOW   1
#define ZB3_STATE_HIGH  2

#define ZB3_COUNT_MAX   10

#define ZB3_DEV_NAME "zb3ctrl"

#endif
