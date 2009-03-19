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

void FloatPlanarImage::unpackInterleavedYUV( const ImgConvertJob* j )
{
  RS_IMAGE16* image = j->rs;
#if defined (__i386__) || defined (__x86_64__) 
  guint cpu = rs_detect_cpu_features();
#if defined (__x86_64__) 
//  if ((image->pixelsize == 4 ) && (cpu & RS_CPU_FLAG_SSE4_1) && ((ox&3)==0))    // TODO: Test before enabling
//    return unpackInterleavedYUV_SSE4(j);
#endif
  if ((image->pixelsize == 4 ) && (cpu & RS_CPU_FLAG_SSE3) && ((ox&3)==0)) 
    return unpackInterleavedYUV_SSE3(j);
#endif

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

#if defined (__i386__) || defined (__x86_64__) 

void FloatPlanarImage::unpackInterleavedYUV_SSE3( const ImgConvertJob* j )
{
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data; 
  temp[0] = 0.299f; temp[1] = 0.587f; temp[2] = 0.114f; temp[3] = 0.0f;
  temp[4] = -0.169f; temp[5] = -0.331f; temp[6] = 0.499; temp[7] = 0.0f;
  temp[8] = 0.499f; temp[9] = -0.418f; temp[10] = -0.0813f; temp[11] = 0.0f;
	for (int i = 0; i < 3; i++) {
		temp[i*4] *= 2.4150f;
		temp[i*4+2] *= 1.4140f;
	}
  asm volatile 
  (
    "movaps (%0), %%xmm5\n"     // Y values
    "movaps 16(%0), %%xmm6\n"   // Cb values
    "movaps 32(%0), %%xmm7\n"   // Cr values
    : // no output registers 
    : "r" (temp)
    : //  %0 
  );
  for (int y = j->start_y; y < j->end_y; y++ ) {
    const gushort* pix = GET_PIXEL(image,0,y);
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    int n = (image->w+1)>>1;
    asm volatile 
    (

      "loopback_uiYUV_SSE3:\n"
      "movdqa (%0), %%xmm1\n"     // Load 2 pixels p0p1
      "punpcklwd %%xmm1, %%xmm0\n"  // xmm0: unpack p0,  xx xx| b0 xx| g0 xx | r0 xx 
      "punpckhwd %%xmm1, %%xmm1\n"  // xmm1: unpack p1,  xx xx| b1 b1| g1 g1 | r1 r1  
      "psrld $16, %%xmm0\n"         // Shift down p0, 00 xx| 00 b0 | 00 g0 | 00 r0 
      "psrld $16, %%xmm1\n"         // Shift down p1
      "cvtdq2ps %%xmm0, %%xmm0\n"   // Convert to float  xx | b0 | g0 | r0
      "cvtdq2ps %%xmm1, %%xmm1\n"   // Convert to float  xx | b1 | g1 | r1
      "movaps %%xmm0, %%xmm2\n"
      "movaps %%xmm1, %%xmm3\n"
      "mulps %%xmm5, %%xmm2\n"      // Y coefficients output to p0
      "mulps %%xmm5, %%xmm3\n"      // Y for p1
      "movaps %%xmm1, %%xmm4\n"     // p1
      "haddps %%xmm3, %%xmm2\n"     // Y coefficients xmm 3 free [two lower Y p0][two upper Y p0]
      "movaps %%xmm0, %%xmm3\n"     // p0
      "mulps %%xmm7, %%xmm0\n"      // Cr coefficients output to p0
      "mulps %%xmm7, %%xmm1\n"      // Cr coeffs to p1
      "mulps %%xmm6, %%xmm3\n"      // Cb coefficients output to p0
      "mulps %%xmm6, %%xmm4\n"      // Cb coeffs to p1
      "haddps %%xmm1, %%xmm0\n"     // Cr coefficients 
      "haddps %%xmm4, %%xmm3\n"     // Cb coefficients xmm4 free
      "haddps %%xmm0, %%xmm0\n"     // Two Cr ready for output in lower
      "haddps %%xmm3, %%xmm2\n"     // lower: Y ready for output, high Cb ready for output
      "movq %%xmm2, (%1)\n"         // Store Y
      "movq %%xmm0, (%3)\n"         // Store Cr
      "movhps %%xmm2, (%2)\n"       // Store Cb
      "add $8, %1\n"
      "add $8, %2\n"
      "add $8, %3\n"
      "add $16, %0\n"
      "dec %4\n"
      "jnz loopback_uiYUV_SSE3\n"
      "emms\n"
      : // no output registers 
      : "r" (pix), "r" (Y), "r" (Cb),  "r" (Cr),  "r"(n)
      : //  %0         %1       %2         %3       %4
     );
  }
}
#endif // defined (__i386__) || defined (__x86_64__) 

#if defined (__x86_64__) 

void FloatPlanarImage::unpackInterleavedYUV_SSE4( const ImgConvertJob* j )
{
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data; 
  temp[0] = 0.299f; temp[1] = 0.587f; temp[2] = 0.114f; temp[3] = 0.0f;
  temp[4] = -0.169f; temp[5] = -0.331f; temp[6] = 0.499; temp[7] = 0.0f;
  temp[8] = 0.499f; temp[9] = -0.418f; temp[10] = -0.0813f; temp[11] = 0.0f;
	for (int i = 0; i < 3; i++) {
		temp[i*4] *= 2.4150f;
		temp[i*4+2] *= 1.4140f;
	}
  asm volatile 
  (
    "movaps (%0), %%xmm13\n"     // Y values
    "movaps 16(%0), %%xmm14\n"   // Cb values
    "movaps 32(%0), %%xmm15\n"   // Cr values
    : // no output registers 
    : "r" (temp)
    : //  %0 
  );
  for (int y = j->start_y; y < j->end_y; y++ ) {
    const gushort* pix = GET_PIXEL(image,0,y);
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    int n = (image->w+3)>>2;
    asm volatile 
    (
      "loopback_uiYUV_SSE4:\n"     // We attempt to spread out dpps instructions due to high latency
      "movdqa (%0), %%xmm2\n"     // Load 2 pixels p0p1
      "pxor %%xmm1, %%xmm1\n"     // 0
      "pmovzxwd %%xmm1, %%xmm0\n"      // and unpack p0 into xmm0
      "punpckhwd %%xmm2, %%xmm1\n"  // xmm1: unpack p1
      "movdqa 16(%0), %%xmm4\n"     // Load 2 pixels p2p3
      "pxor %%xmm3, %%xmm3\n"       // 0
      "pmovzxwd %%xmm4, %%xmm2\n"      // unpack p2 into xmm2
      "punpckhwd %%xmm4, %%xmm3\n"  // unpack p3 into xmm3
      "cvtdq2ps %%xmm0, %%xmm0\n"   // Convert to float  xx | b0 | g0 | r0
      "cvtdq2ps %%xmm1, %%xmm1\n"   // Convert to float  xx | b1 | g1 | r1
      "movaps %%xmm0, %%xmm4\n"
      "cvtdq2ps %%xmm2, %%xmm2\n"   // Convert to float  xx | b0 | g0 | r0
      "movaps %%xmm1, %%xmm5\n"
      "dpps $241, %%xmm13, %%xmm4\n"    // p0 Y - f1 = 241
      "cvtdq2ps %%xmm3, %%xmm3\n"   // Convert to float  xx | b1 | g1 | r1
      "dpps $242, %%xmm13, %%xmm5\n"    // p1 Y - f2 = 242
      "movaps %%xmm2, %%xmm6\n"
      "dpps $244, %%xmm13, %%xmm6\n"    // p2 Y - f4 = 244
      "movaps %%xmm3, %%xmm7\n"
      "dpps $248, %%xmm13, %%xmm7\n"    // p3 Y - f8 = 248
      "movaps %%xmm4, %%xmm8\n"          // Y into xmm8
      "movaps %%xmm0, %%xmm4\n"
      "orps %%xmm5, %%xmm8\n"
      "dpps $241, %%xmm14, %%xmm4\n"    // p0 Cb - f1 = 241
      "movaps %%xmm1, %%xmm5\n"
      "orps %%xmm6, %%xmm8\n"
      "dpps $242, %%xmm14, %%xmm5\n"    // p1 Cb - f2 = 242
      "movaps %%xmm2, %%xmm6\n"
      "orps %%xmm7, %%xmm8\n"
      "movaps %%xmm3, %%xmm7\n"
      "dpps $244, %%xmm14, %%xmm6\n"    // p2 Cb - f4 = 244
      "orps %%xmm5, %%xmm4\n"            // Cb into xmm4
      "dpps $248, %%xmm14, %%xmm7\n"    // p3 Cb - f8 = 248
      "orps %%xmm6, %%xmm4\n"          
      "dpps $241, %%xmm15, %%xmm0\n"    // p0 Cr - f1 = 241
      "orps %%xmm7, %%xmm4\n"          
      "dpps $242, %%xmm15, %%xmm1\n"    // p1 Cr - f2 = 242
      "dpps $244, %%xmm15, %%xmm2\n"    // p2 Cr - f4 = 244
      "orps %%xmm1, %%xmm0\n"            // Cr into xmm0
      "dpps $248, %%xmm15, %%xmm3\n"    // p3 Cr - f8 = 248
      "orps %%xmm2, %%xmm0\n"          
      "movdqa %%xmm8, (%1)\n"         // Store Y
      "orps %%xmm3, %%xmm0\n"          
      "movdqa %%xmm4, (%2)\n"         // Store Cb
      "movdqa %%xmm0, (%3)\n"         // Store Cr
      "add $16, %1\n"
      "add $16, %2\n"
      "add $16, %3\n"
      "add $32, %0\n"
      "dec %4\n"
      "jnz loopback_uiYUV_SSE4\n"
      "emms\n"
      : // no output registers 
      : "r" (pix), "r" (Y), "r" (Cb),  "r" (Cr),  "r"(n)
      : //  %0         %1       %2         %3       %4
     );
  }
}
#endif// defined (__x86_64__) 


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
      int r = (int)((Y[x] + 1.402 * Cr[x]) * (1.0f/2.415f));
      int g = (int)(Y[x] - 0.344 * Cb[x] - 0.714 * Cr[x]);
      int b = (int)((Y[x] + 1.772 * Cb[x]) * (1.0f/1.414f));
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
  int plane = slice->in->plane_id;
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


