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

#include "complexblock.h"
#include <math.h>
#include "floatimageplane.h"


ComplexBlock::ComplexBlock(int _w, int _h): w(_w), h(_h)
{
  pitch = w * sizeof(fftwf_complex);
  complex = (fftwf_complex*)fftwf_malloc(h*pitch); 
  g_assert(complex);
  temp = new FloatImagePlane(256,1);
  temp->allocateImage();
}

ComplexBlock::~ComplexBlock(void)
{
  fftwf_free(complex);
  complex = 0;
  delete temp;
}

