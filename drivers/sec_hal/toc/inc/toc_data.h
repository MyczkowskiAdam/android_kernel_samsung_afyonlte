/*
 * drivers/sec_hal/toc/inc/toc_data.h
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

#ifndef TOC_DATA_H
#define TOC_DATA_H

/// \brief Maximum number of DATA_TOC entries. Must match that of of producer.
#define TOC_MAX_ENTRIES        64 
/// \brief Object id denoting the end of DATA_TOC.
#define TOC_END_MARKER 0xFFFFFFFF

/// Secured Data Table of Contents (DATA_TOC) is an array of structures
/// which describe location of data structures
///
/// \li End of DATA_TOC_ENTRY list is indicated by
/// \li \c object_id == DATA_TOC_END_MARKER (0xFFFFFFFFu)
/// \li Data pointed to by DATA_TOC_ENTRY entry
/// \li starts at 32 bit boundary (which implies unnamed
/// \li padding between data elements).
/// \li DATA_TOC_ENTRY entries are in no particular order.
/// \li Data elements pointed to by DATA_TOC_ENTRY are not
/// \li in any particular order.
///
/// Structure size  16 (0x0010) bytes.
/// 
/// \brief Data Table of contents

typedef struct {
/*   0 (0x0000) */  uint32_t  object_id;
/*   4 (0x0004) */  uint32_t  start;
/*   8 (0x0008) */  uint32_t  length;
/*  12 (0x000C) */  uint32_t  spare;
} DATA_TOC_ENTRY;

// EOF
#endif /* TOC_DATA_H */
