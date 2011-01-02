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

#ifndef planarimageslice_h__
#define planarimageslice_h__
#include <rawstudio.h>

namespace RawStudio {
namespace FFTFilter {


class FloatImagePlane;
class ComplexFilter;
class FFTWindow;

class PlanarImageSlice
{
public:
  PlanarImageSlice(void);
  virtual ~PlanarImageSlice(void);
  void setOut(FloatImagePlane *p);
  FloatImagePlane *in;
  FloatImagePlane *out;
  void allocateOut();
  gint offset_x;
  gint offset_y;
  gint overlap_x;
  gint overlap_y;
  gboolean blockSkipped;
  gboolean ownAlloc;
  ComplexFilter *filter;
  FFTWindow *window;
};

}} // namespace RawStudio::FFTFilter

#endif // planarimageslice_h__
