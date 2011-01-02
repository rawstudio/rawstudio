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

#include "planarimageslice.h"
#include "floatimageplane.h"

namespace RawStudio {
namespace FFTFilter {

PlanarImageSlice::PlanarImageSlice(void)
{
  filter = 0;
  in = 0;
  out = 0;
  blockSkipped = true;
  ownAlloc = false;
}

PlanarImageSlice::~PlanarImageSlice(void) {
  if (ownAlloc && out)
    delete out;
  out = 0;
  if (in)
    delete in;
  in = 0;
}

void PlanarImageSlice::setOut(FloatImagePlane *p) {
  ownAlloc = false;
  out = p;
  blockSkipped = false;
}

void PlanarImageSlice::allocateOut() {
  if (ownAlloc || out)
    return;
  out = new FloatImagePlane(in->w, in->h, in->plane_id);
  out->allocateImage();
  blockSkipped = false;
  ownAlloc = true;
}

}}// namespace RawStudio::FFTFilter
