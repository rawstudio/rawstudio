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

#ifndef RS_MACROS_H
#define RS_MACROS_H

#include <stdint.h>

#define ORIENTATION_RESET(orientation) orientation = 0
#define ORIENTATION_90(orientation) orientation = (orientation&4) | ((orientation+1)&3)
#define ORIENTATION_180(orientation) orientation = (orientation^2)
#define ORIENTATION_270(orientation) orientation = (orientation&4) | ((orientation+3)&3)
#define ORIENTATION_FLIP(orientation) orientation = (orientation^4)
#define ORIENTATION_MIRROR(orientation) orientation = ((orientation&4)^4) | ((orientation+2)&3)

/* The problem with the align GNU extension, is that it doesn't work
 * reliably with local variables, depending on versions and targets.
 * So better use a tricky define to ensure alignment even in these
 * cases. */
#define RS_DECLARE_ALIGNED(type, name, sizex, sizey, alignment) \
	type name##_s[(sizex)*(sizey)+(alignment)-1];	\
	type * name = (type *)(((uintptr_t)name##_s+(alignment - 1))&~((uintptr_t)(alignment)-1))

#include <gdk/gdkx.h>
#define GUI_CATCHUP() do { \
  GdkDisplay *__gui_catchup_display = gdk_display_get_default (); \
  XFlush (GDK_DISPLAY_XDISPLAY (__gui_catchup_display)); } while (0)
#define GTK_CATCHUP() while (gtk_events_pending()) gtk_main_iteration()

#if __GNUC__ >= 3
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define align(x) __attribute__ ((aligned (x)))
#define __deprecated __attribute__ ((deprecated))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define align(x)
#define __deprecated
#endif

/* Default gamma */
#define GAMMA (2.2)

#define _CLAMP65535(a) do { (a) = CLAMP((a), 0, 65535); } while(0)

#define _CLAMP65535_TRIPLET(a, b, c) do {_CLAMP65535(a); _CLAMP65535(b); _CLAMP65535(c); } while (0)

#define _CLAMP255(a) do { (a) = CLAMP((a), 0, 255); } while (0)

#define COLOR_BLACK(c) do { (c).red=0; (c).green=0; (c).blue=0; } while (0)

/* Compatibility with GTK+ <2.14.0 */
#if !GTK_CHECK_VERSION(2,14,0)
#define gtk_adjustment_get_lower(adjustment) adjustment->lower
#define gtk_adjustment_get_upper(adjustment) adjustment->upper
#define gtk_adjustment_get_step_increment(adjustment) adjustment->step_increment
#define gtk_adjustment_get_page_increment(adjustment) adjustment->page_increment
#define gtk_adjustment_get_page_size(adjustment) adjustment->page_size
#endif /* !GTK_CHECK_VERSION(2.14.0) */

#endif /* RS_MACROS_H */
