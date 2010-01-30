/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef RS_STOCK_H
#define RS_STOCK_H

#define RS_STOCK_CROP                     "tool-crop"
#define RS_STOCK_ROTATE                   "tool-rotate"
#define RS_STOCK_COLOR_PICKER             "tool-color-picker"
#define RS_STOCK_ROTATE_CLOCKWISE         "tool-rotate-clockwise"
#define RS_STOCK_ROTATE_COUNTER_CLOCKWISE "tool-rotate-counter-clockwise"
#define RS_STOCK_FLIP                     "tool-flip"
#define RS_STOCK_MIRROR                   "tool-mirror"

void rs_stock_init(void);

typedef enum  {
   RS_CURSOR_CROP = 0,
   RS_CURSOR_ROTATE,
   RS_CURSOR_COLOR_PICKER
} RSCursorType;

GdkCursor* rs_cursor_new(GdkDisplay *display, RSCursorType cursor_type);

#endif /* RS_STOCK_H */
