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

#ifndef imageplane_h__
#define imageplane_h__
#include "jobqueue.h"
#include <rawstudio.h>
#include <vector>
#include "complexfilter.h"


using namespace std;
class FFTWindow;

class FloatImagePlane
{
public:
  FloatImagePlane(int _w, int _h, int id = -1);
  FloatImagePlane(const FloatImagePlane& p);
  virtual ~FloatImagePlane(void);
  void allocateImage(); 
  void mirrorEdges(int mirror_x, int mirror_y);
  gfloat* getLine(int y);
  gfloat* getAt(int x, int y);
  FloatImagePlane* getSlice(int x,int y,int new_w, int new_h);
  void blitOnto(FloatImagePlane *dst);
  void multiply(float mul);
  void addJobs(JobQueue *jobs, int bw, int bh, int ox, int oy, FloatImagePlane *outPlane);
  void applySlice(PlanarImageSlice *p);
  const int w;
  const int h;
  gfloat* data;
  int plane_id;
  ComplexFilter* filter;
  FFTWindow* window;
  int pitch;    // Not in bytes, but in floats
private:
  gfloat* allocated;
};

void FBitBlt(guchar* dstp, int dst_pitch, const guchar* srcp, int src_pitch, int row_size, int height);

#endif // imageplane_h__
