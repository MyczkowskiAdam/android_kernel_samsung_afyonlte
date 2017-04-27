/*
 * screen_id.h
 *     function id definition header file.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
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

#ifndef __SCREEN_ID_H__
#define __SCREEN_ID_H__

/*
 * Task ID
 */

/* Display */
/* Display Task */
/* Task ID */
#define TASK_DISPLAY (20)
/* SMB ID */
#define SMB_DISPLAY (TASK_DISPLAY*2)
/* Function ID Base */
#define FUNCTIONID_DISPLAY_BASE (TASK_DISPLAY*256+3)

/* Display2 */
/* Display2 Task */
/* Task ID */
#define TASK_DISPLAY2 (21)
/* SMB ID */
#define SMB_DISPLAY2 (TASK_DISPLAY2*2)

/* Graphics */
/* Graphics Task */
/* Task ID */
#define TASK_GRAPHICS (60)
/* SMB ID */
#define SMB_GRAPHICS (TASK_GRAPHICS*2)
/* Function ID Base */
#define FUNCTIONID_GRAPHICS_BASE (TASK_GRAPHICS*256+3)

/*
 * Function ID
 */

/*  Display  */
#define EVENT_DISPLAY_DRAW (FUNCTIONID_DISPLAY_BASE+2)
#define EVENT_DISPLAY_STARTLCD (FUNCTIONID_DISPLAY_BASE+3)
#define EVENT_DISPLAY_STOPLCD (FUNCTIONID_DISPLAY_BASE+4)
#define EVENT_DISPLAY_SETLCDREFRESH (FUNCTIONID_DISPLAY_BASE+5)
#define EVENT_DISPLAY_STARTHDMI (FUNCTIONID_DISPLAY_BASE+6)
#define EVENT_DISPLAY_STOPHDMI (FUNCTIONID_DISPLAY_BASE+7)
#define EVENT_DISPLAY_WRITEDSISHORTPACKET (FUNCTIONID_DISPLAY_BASE+8)
#define EVENT_DISPLAY_WRITEDSILONGPACKET (FUNCTIONID_DISPLAY_BASE+9)
#define EVENT_DISPLAY_SETLCDIFPARAMETERS (FUNCTIONID_DISPLAY_BASE+10)
#define EVENT_DISPLAY_SETHDMIIFPARAMETERS (FUNCTIONID_DISPLAY_BASE+11)
#define EVENT_DISPLAY_SETADDRESS (FUNCTIONID_DISPLAY_BASE+12)
/* #define EVENT_DISPLAY_READDSISHORTPACKET (FUNCTIONID_DISPLAY_BASE+38) */
#define EVENT_DISPLAY_READDSISHORTPACKET (FUNCTIONID_DISPLAY_BASE+14)
#define EVENT_DISPLAY_SETLUT (FUNCTIONID_DISPLAY_BASE+15)
#define EVENT_DISPLAY_COLORPALETTE (FUNCTIONID_DISPLAY_BASE+17)

/* Graphics */
#define EVENT_GRAPHICS_INITIALIZE (FUNCTIONID_GRAPHICS_BASE+1)
#define EVENT_GRAPHICS_QUIT (FUNCTIONID_GRAPHICS_BASE+2)
#define EVENT_GRAPHICS_IMAGECONVERSION (FUNCTIONID_GRAPHICS_BASE+3)
#define EVENT_GRAPHICS_IMAGEBLEND (FUNCTIONID_GRAPHICS_BASE+5)
#define EVENT_GRAPHICS_IMAGEOUTPUT (FUNCTIONID_GRAPHICS_BASE+8)

#define EVENT_GRAPHICS_IMAGEEDIT (FUNCTIONID_GRAPHICS_BASE+11)

#define EVENT_GRAPHICS_SETBLENDSIZE (FUNCTIONID_GRAPHICS_BASE+14)
#define EVENT_DISPLAY_SETDSIMODE (FUNCTIONID_DISPLAY_BASE+65)

#endif /* __SCREEN_ID_H__ */
