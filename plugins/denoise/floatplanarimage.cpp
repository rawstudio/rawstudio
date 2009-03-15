/*
* Copyright (C) 2009 Klaus Post
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
#include "floatplanarimage.h"
#include "complexfilter.h"

FloatPlanarImage::FloatPlanarImage(void) {
  p = 0;
}

FloatPlanarImage::FloatPlanarImage( const FloatPlanarImage &img )
{
  nPlanes = img.nPlanes;
  p = new FloatImagePlane*[nPlanes];
  for (int i = 0; i < nPlanes; i++)
    p[i] = new FloatImagePlane(img.p[i]->w, img.p[i]->h, i);

  bw = img.bw;
  bh = img.bh;
  ox = img.ox;
  oy = img.oy;
}

FloatPlanarImage::~FloatPlanarImage(void) {
  if (p != 0) {
    for (int i = 0; i < nPlanes; i++) {
      if (p[i])
        delete p[i];
      p[i] = 0;
    }
    delete[] p;
    p = 0;
  }
}

void FloatPlanarImage::allocate_planes() {
  for (int i = 0; i < nPlanes; i++)
    p[i]->allocateImage();
}

void FloatPlanarImage::mirrorEdges()
{
  for (int i = 0; i < nPlanes; i++)
    p[i]->mirrorEdges(ox, oy);
}

void FloatPlanarImage::setFilter( int plane, ComplexFilter *f, FFTWindow *window )
{
  if (plane >= nPlanes)
    return;
  p[plane]->filter = f;
  p[plane]->window = window;
}

// TODO: Begs to be SSE2 and/or SMP.
void FloatPlanarImage::unpackInterleaved( const RS_IMAGE16* image )
{
  // Already demosaiced
  if (image->channels != 3)
    return;

  nPlanes = 3;
  g_assert(p == 0);
  p = new FloatImagePlane*[nPlanes];

  for (int i = 0; i < nPlanes; i++)
    p[i] = new FloatImagePlane(image->w+ox*2, image->h+oy*2, i);

  allocate_planes();

  for (int y = 0; y < image->h; y++ ) {
    const gushort* pix = GET_PIXEL(image,0,y);
    gfloat *rp = p[0]->getAt(ox, y+oy);
    gfloat *gp = p[1]->getAt(ox, y+oy);
    gfloat *bp = p[2]->getAt(ox, y+oy);
    for (int x=0; x<image->w; x++) {
      *rp++ = (float)(*pix);
      *gp++ = (float)(*(pix+1));
      *bp++ = (float)(*(pix+2));
      pix += image->pixelsize;
    }
  }
}

// TODO: Begs to be SSE2 and/or SMP. Scalar int<->float is incredibly slow.
void FloatPlanarImage::packInterleaved( RS_IMAGE16* image )
{
  for (int i = 0; i < nPlanes; i++) {
    g_assert(p[i]->w == image->w+ox*2);
    g_assert(p[i]->h == image->h+oy*2);
  }

  for (int y = 0; y < image->h; y++ ) {
    for (int c = 0; c<nPlanes; c++) {
      gfloat * in = p[c]->getAt(ox, y+oy);
      gushort* out = GET_PIXEL(image,0,y) + c;
      for (int x=0; x<image->w; x++) {
        int p = (int)*(in++);
        *out = clampbits(p,16);
        out += image->pixelsize;
      }
    }
  }
}

JobQueue* FloatPlanarImage::getUnpackInterleavedYUVJobs(RS_IMAGE16* image) {
  // Already demosaiced
  JobQueue* queue = new JobQueue();

  if (image->channels != 3)
    return queue;

  g_assert(p == 0);
  nPlanes = 3;
  p = new FloatImagePlane*[nPlanes];

  for (int i = 0; i < nPlanes; i++)
    p[i] = new FloatImagePlane(image->w+ox*2, image->h+oy*2, i);

  allocate_planes();
  int threads = rs_get_number_of_processor_cores()*4;
  int hEvery = MAX(1,(image->h+threads)/threads);
  for (int i = 0; i < threads; i++) {
    ImgConvertJob *j = new ImgConvertJob(this,JOB_CONVERT_TOFLOAT_YUV);
    j->start_y = i*hEvery;
    j->end_y = MIN((i+1)*hEvery,image->h);
    j->rs = image;
    queue->addJob(j);
  }
  return queue;
}

static float shortToInt[65535];

// TODO: Begs to be SSE2.
void FloatPlanarImage::unpackInterleavedYUV( const ImgConvertJob* j )
{
  RS_IMAGE16* image = j->rs;

  for (int y = j->start_y; y < j->end_y; y++ ) {
    const gushort* pix = GET_PIXEL(image,0,y);
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    for (int x=0; x<image->w; x++) {
      float r = shortToInt[(*pix)];
      float g = shortToInt[(*(pix+1))];
      float b = shortToInt[(*(pix+2))];
      *Y++ = r * 0.299 + g * 0.587 + b * 0.114 ;
      *Cb++ = r * -0.169 + g * -0.331 + b * 0.499;
      *Cr++ = r * 0.499 + g * -0.418 - b * 0.0813;
      pix += image->pixelsize;
    }
  }
}

JobQueue* FloatPlanarImage::getPackInterleavedYUVJobs(RS_IMAGE16* image) {
  JobQueue* queue = new JobQueue();

  if (image->channels != 3)
    return queue;

  for (int i = 0; i < nPlanes; i++) {
    g_assert(p[i]->w == image->w+ox*2);
    g_assert(p[i]->h == image->h+oy*2);
  }

  int threads = rs_get_number_of_processor_cores()*4;
  int hEvery = MAX(1,(image->h+threads)/threads);
  for (int i = 0; i < threads; i++) {
    ImgConvertJob *j = new ImgConvertJob(this,JOB_CONVERT_FROMFLOAT_YUV);
    j->start_y = i*hEvery;
    j->end_y = MIN((i+1)*hEvery,image->h);
    j->rs = image;
    queue->addJob(j);
  }
  return queue;
}

// TODO: Begs to be SSE2. Scalar int<->float is incredibly slow.
void FloatPlanarImage::packInterleavedYUV( const ImgConvertJob* j)
{
  RS_IMAGE16* image = j->rs;

  for (int y = j->start_y; y < j->end_y; y++ ) {
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    gushort* out = GET_PIXEL(image,0,y);
    for (int x=0; x<image->w; x++) {
      int r = (int)(Y[x] + 1.402 * Cr[x]);
      int g = (int)(Y[x] - 0.344 * Cb[x] - 0.714 * Cr[x]);
      int b = (int)(Y[x] + 1.772 * Cb[x]);
      out[0] = clampbits(r,16);
      out[1] = clampbits(g,16);
      out[2] = clampbits(b,16);
      out += image->pixelsize;
    }
  }
}



JobQueue* FloatPlanarImage::getJobs() {
  JobQueue *jobs = new JobQueue();

  for (int i = 0; i < nPlanes; i++)
    p[i]->addJobs(jobs, bw, bh, ox, oy);
  
  return jobs;
}

void FloatPlanarImage::applySlice( PlanarImageSlice *slice )
{
  int plane = slice->out->plane_id;
  g_assert(plane>=0 && plane<nPlanes);
  p[plane]->applySlice(slice);
}

FloatImagePlane* FloatPlanarImage::getPlaneSliceFrom( int plane, int x, int y )
{
  g_assert(plane>=0 && plane<nPlanes);
  return p[plane]->getSlice(x,y,ox,oy);
}

void FloatPlanarImage::initConvTable() {
  for (int i = 0; i < 65535; i++) {
    shortToInt[i] = (float)i;
  }
}


