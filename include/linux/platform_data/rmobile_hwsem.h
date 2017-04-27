/*
 * Renesas SH-/R-Mobile bus semaphore driver
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */
#ifndef __RMOBILE_HWSEM_H
#define __RMOBILE_HWSEM_H

struct hwsem_desc {
	unsigned int offset;
	struct lock_class_key key;
};

struct hwsem_pdata {
	int base_id;
	struct hwsem_desc *descs;
	unsigned int nr_descs;
};

#define HWSEM(_enum_id, _offset)	{ .offset = _offset, }

#endif /* __RMOBILE_HWSEM_H */
