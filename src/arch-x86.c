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

#if defined (__i386__) || defined (__x86_64__)

#include "rawstudio.h"
#include "rs-color-transform.h"
#include "rs-image.h"

#include "x86_cpu.h"


/******************************************************************************
 * Structures etc...
 *****************************************************************************/

enum
{
	_MMX   = 1<<0,
	_SSE   = 1<<1,
	_CMOV  = 1<<2,
	_3DNOW = 1<<3
};

/******************************************************************************
 * Function declarations
 *****************************************************************************/

/* Detect cpu features */
static guint
rs_detect_cpu_features();

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
	if (cpuflags & _MMX)
	{
		rs_image16_copy_double = rs_image16_copy_double_mmx;
	}

	/* Black and shift applier */
	if (cpuflags & _MMX)
	{
		rs_photo_open_dcraw_apply_black_and_shift = rs_photo_open_dcraw_apply_black_and_shift_mmx;
	}

	/* Photo renderers */
	if (cpuflags & _SSE)
	{
		/* SSE is favored over 3dnow in case both are available */
		transform_nocms8 = transform_nocms8_sse;
		transform_cms8 = transform_cms8_sse;
	}
	else if (cpuflags & _3DNOW)
	{
		/* Only 3dnow */
		transform_nocms8 = transform_nocms8_3dnow;
		transform_cms8 = transform_cms8_3dnow;
	}
}

/******************************************************************************
 * Function definitions
 *****************************************************************************/

#define cpuid(cmd, eax, ebx, ecx, edx) \
  do { \
     eax = ebx = ecx = edx = 0;	\
     asm ( \
       "cpuid" \
       : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) \
       : "0" (cmd) \
     ); \
} while(0)

static guint
rs_detect_cpu_features()
{
	guint eax;
	guint ebx;
	guint ecx;
	guint edx;
	guint cpuflags = 0;

	/* Test cpuid presence comparing eflags */
	asm (
		"pushf\n\t"
		"pop %%"REG_a"\n\t"
		"mov %%"REG_a", %%"REG_b"\n\t"
		"xor $0x00200000, %%"REG_a"\n\t"
		"push %%"REG_a"\n\t"
		"popf\n\t"
		"pushf\n\t"
		"pop %%"REG_a"\n\t"
		"cmp %%"REG_a", %%"REG_b"\n\t"
		"je notfound\n\t"
		"mov $1, %0\n\t"
		"notfound:\n\t"
		: "=r" (eax)
		:
		: REG_a, REG_b

		);

	if (eax)
	{
		guint std_dsc;
		guint ext_dsc;

		/* Get the standard level */
		cpuid(0x00000000, std_dsc, ebx, ecx, edx);

		if (std_dsc)
		{
			/* Request for standard features */
			cpuid(0x00000001, std_dsc, ebx, ecx, edx);

			if (edx & 0x00800000)
				cpuflags |= _MMX;
			if (edx & 0x02000000)
				cpuflags |= _SSE;
			if (edx & 0x00008000)
				cpuflags |= _CMOV;
		}

		/* Is there extensions */
		cpuid(0x80000000, ext_dsc, ebx, ecx, edx);

		if (ext_dsc)
		{
			/* Request for extensions */
			cpuid(0x80000001, eax, ebx, ecx, edx);

			if (edx & 0x80000000)
				cpuflags |= _3DNOW;
			if (edx & 0x00400000)
				cpuflags |= _MMX;
		}
	}
	return(cpuflags);
}


#endif /* __i386__ || __x86_64__ */
