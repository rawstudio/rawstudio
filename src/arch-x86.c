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

#if defined (__i386__) || defined (__x86_64__)

#include <rawstudio.h>

/******************************************************************************
 * The core feature of this module
 *****************************************************************************/

/* ia32/x86_64 optimized DSP binder */
void
rs_bind_optimized_functions()
{
	guint cpuflags;

	/* Detect CPU features */
	cpuflags = rs_detect_cpu_features();

	/* Bind functions according to available features */

	/* Image size doubler */
	if (cpuflags & RS_CPU_FLAG_MMX)
	{
//		rs_image16_copy_double = rs_image16_copy_double_mmx;
	}

	/* Black and shift applier */
	if (cpuflags & RS_CPU_FLAG_MMX)
	{
//		rs_image16_open_dcraw_apply_black_and_shift = rs_image16_open_dcraw_apply_black_and_shift_mmx;
	}

	/* Photo renderers */
	if (cpuflags & RS_CPU_FLAG_SSE)
	{
		/* SSE is favored over 3dnow in case both are available */
		transform_nocms8 = transform_nocms8_sse;
		transform_cms8 = transform_cms8_sse;
	}
	else if (cpuflags & RS_CPU_FLAG_3DNOW)
	{
		/* Only 3dnow */
		transform_nocms8 = transform_nocms8_3dnow;
		transform_cms8 = transform_cms8_3dnow;
	}
}

#endif /* __i386__ || __x86_64__ */
