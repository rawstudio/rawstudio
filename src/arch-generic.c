/*
 * Copyright (C) 2006 Anders Brander <anders@brander.dk> and 
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

#include "rawstudio.h"
#include "rs-render.h"

/* Default dsp function binder, defined for all archs so that a common C
 * implementation of every optimized function is shared among archs */
void
rs_bind_default_functions()
{
	/* Bind all default C implementation fucntions */

	/* Black point and shift applier */
	rs_photo_open_dcraw_apply_black_and_shift = rs_photo_open_dcraw_apply_black_and_shift_c;

	/* Renderers */
	rs_render_cms   = rs_render_cms_c;
	rs_render_nocms = rs_render_nocms_c;
	rs_render16_cms   = rs_render16_cms_c;
	rs_render16_nocms = rs_render16_nocms_c;
	rs_render_histogram_table = rs_render_histogram_table_c;
}

#if !defined (__i386__) && !defined(__x86_64__)
/* Optimized dsp function binder, defined for all archs that don't have 
 * custom code - a stub for the generic C arch */
void
rs_bind_optimized_functions()
{
}
#endif
