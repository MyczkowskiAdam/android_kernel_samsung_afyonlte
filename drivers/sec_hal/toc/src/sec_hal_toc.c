/*
 * drivers/sec_hal/toc/src/sec_hal_toc.c
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "sec_hal_rt.h"
#include "sec_hal_toc.h"
#include "toc_data.h"
#include "sec_hal_rt_trace.h"

#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/completion.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>

#include <asm/io.h>


void * toc_get_payload(DATA_TOC_ENTRY *toc_root,
		const uint32_t obj_id,
		uint32_t * const p_len)
{
	void *rv = 0;
	uint32_t idx;

	SEC_HAL_TRACE_ENTRY();

	for (idx = 0; idx < TOC_MAX_ENTRIES && toc_root; idx++) {
		if (toc_root[idx].object_id == obj_id) {
			rv = (void*)(((ptrdiff_t)toc_root) + toc_root[idx].start);
			if (p_len)
				*p_len = toc_root[idx].length;
			break;
		}
		if (toc_root[idx].object_id == TOC_END_MARKER)
			break;
	}

	SEC_HAL_TRACE_EXIT();
	return rv;
}


const void * toc_put_payload(DATA_TOC_ENTRY *toc_root,
		const uint32_t obj_id,
		const void * const data,
		const uint32_t length,
		const void ** const p_handle,
		const uint32_t reserve,
		const uint32_t offset)
{
	DATA_TOC_ENTRY *item = 0;
	void *slot = 0;

	SEC_HAL_TRACE_ENTRY();

	item = get_new_toc_entry(toc_root, obj_id);
	if (!item)
		return 0;

	if (item->object_id == TOC_END_MARKER) {
		slot = toc_get_free_slot(toc_root, length);
		memcpy(slot, data, length);
		item->object_id = obj_id;
		item->start = (uint32_t)((ptrdiff_t)slot - (ptrdiff_t)toc_root);
		item->length = length;
		item->spare = 0;
	}

	if (p_handle)
		*p_handle = item;

	SEC_HAL_TRACE_EXIT();
	return 0;
}


DATA_TOC_ENTRY * get_new_toc_entry(DATA_TOC_ENTRY *toc_root, uint32_t obj_id)
{
	uint32_t idx;

	SEC_HAL_TRACE_ENTRY();

	for (idx = 0; idx < TOC_MAX_ENTRIES; idx++) {
		if ((toc_root[idx].object_id == TOC_END_MARKER)
				|| (toc_root[idx].object_id == obj_id)) {
			// Note: data areas of duplicates are lost. This is to keep the implementation simple
			return toc_root + idx;
		}
	}

	SEC_HAL_TRACE_EXIT();
	return 0;
}


void * toc_get_free_slot(DATA_TOC_ENTRY * toc_root, uint32_t size)
{
	uint32_t idx;
	uint32_t max_offset = TOC_MAX_ENTRIES * 16;

	SEC_HAL_TRACE_ENTRY();

	for (idx = 0; idx < TOC_MAX_ENTRIES; idx++) {
		if (toc_root[idx].object_id == TOC_END_MARKER)
			break;
		if (max_offset <= toc_root[idx].start)
			max_offset = toc_root[idx].start + toc_root[idx].length;
	}

	SEC_HAL_TRACE_EXIT();
	return ((void*)toc_root + max_offset);
}


int toc_initialize(DATA_TOC_ENTRY * toc_root, uint32_t size)
{
	memset(toc_root, TOC_END_MARKER, size);
	return 0;
}

