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

/* luminance weight, notice that these is used for linear data */

#define RLUM (0.3086)
#define GLUM (0.6094)
#define BLUM (0.0820)

#ifdef __i686__
#define HAVE_CMOV
#endif

#ifdef HAVE_CMOV
#define _MAX(in, max) \
asm volatile (\
	"cmpl	%1, %0\n\t"\
	"cmovl	%1, %0\n\t"\
	:"+r" (max)\
	:"r" (in)\
)
#else
#define _MAX(in, max) if (in>max) max=in
#endif

#ifdef HAVE_CMOV
#define _CLAMP(in, max) \
asm volatile (\
	"cmpl	%0, %1\n\t"\
	"cmovl	%1, %0\n\t"\
	:"+r" (in)\
	:"r" (max)\
)
#else
#define _CLAMP(in, max) if (in>max) in=max
#endif

#ifdef HAVE_CMOV
#define _CLAMP65535(value) \
asm volatile (\
	"xorl %%ecx, %%ecx\n\t"\
	"cmpl %%ecx, %0\n\t"\
	"cmovl %%ecx, %0\n\t"\
	"movl	$65535, %%ecx\n\t"\
	"cmpl %0, %%ecx\n\t"\
	"cmovl %%ecx, %0\n\t"\
	:"+r" (value)\
	:\
	:"%ecx"\
)
#else
#define _CLAMP65535(a) a = MAX(MIN(65535,a),0)
#endif

#ifdef HAVE_CMOV
#define _CLAMP65535_TRIPLET(a, b, c) \
asm volatile (\
	"xorl %%ecx, %%ecx\n\t"\
	"cmpl %%ecx, %0\n\t"\
	"cmovl %%ecx, %0\n\t"\
	"cmpl %%ecx, %1\n\t"\
	"cmovl %%ecx, %1\n\t"\
	"cmpl %%ecx, %2\n\t"\
	"cmovl %%ecx, %2\n\t"\
	"movl $65535, %%ecx\n\t"\
	"cmp %%ecx, %0\n\t"\
	"cmovg %%ecx, %0\n\t"\
	"cmp %%ecx, %1\n\t"\
	"cmovg %%ecx, %1\n\t"\
	"cmp %%ecx, %2\n\t"\
	"cmovg %%ecx, %2\n\t"\
	:"+r" (a), "+r" (b), "+r" (c)\
	:\
	:"%ecx"\
)
#else
#define _CLAMP65535_TRIPLET(a, b, c) \
a = MAX(MIN(65535,a),0);b = MAX(MIN(65535,b),0);c = MAX(MIN(65535,c),0)
#endif

#ifdef HAVE_CMOV
#define _CLAMP255(value) \
asm volatile (\
	"xorl %%ecx, %%ecx\n\t"\
	"cmpl %%ecx, %0\n\t"\
	"cmovl %%ecx, %0\n\t"\
	"movl	$255, %%ecx\n\t"\
	"cmpl %0, %%ecx\n\t"\
	"cmovl %%ecx, %0\n\t"\
	:"+r" (value)\
	:\
	:"%ecx"\
)
#else
#define _CLAMP255(a) a = MAX(MIN(255,a),0)
#endif

#define COLOR_BLACK(c) do { c.red=0; c.green=0; c.blue=0; } while (0)

enum {
	R=0,
	G,
	B,
	G2
};
