/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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

#ifndef DRAWINGAREA_H
#define DRAWINGAREA_H

extern GtkWidget *gui_drawingarea_make(RS_BLOB *rs);

extern GdkPixmap *blitter;
extern GdkCursor *cur_normal;
extern GdkCursor *cur_n;
extern GdkCursor *cur_e;
extern GdkCursor *cur_s;
extern GdkCursor *cur_w;
extern GdkCursor *cur_nw;
extern GdkCursor *cur_ne;
extern GdkCursor *cur_se;
extern GdkCursor *cur_sw;
extern GdkCursor *cur_pencil;

#endif /* DRAWINGAREA_H */
