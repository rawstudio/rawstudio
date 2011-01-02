/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
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

#ifndef complexblock_h__
#define complexblock_h__
#include "fftw3.h"
#include <rawstudio.h>

namespace RawStudio {
namespace FFTFilter {

class FloatImagePlane;

class ComplexBlock
{
public:
  ComplexBlock(int w, int h);
  ~ComplexBlock(void);
  fftwf_complex* complex;
  FloatImagePlane *temp;
  const int w;
  const int h;
private:
  int pitch;
};

}} // namespace RawStudio::FFTFilter

#endif // complexblock_h__
