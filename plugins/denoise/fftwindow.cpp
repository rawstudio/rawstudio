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
 *
 * Based on FFT3DFilter plugin for Avisynth 2.5 - 3D Frequency Domain filter
 * Copyright(C)2004-2005 A.G.Balakhnin aka Fizick, email: bag@hotmail.ru, web: http://bag.hotmail.ru
 */

#include "fftwindow.h"
#include <math.h>

#define PI_F 3.14159265358979323846f

namespace RawStudio {
namespace FFTFilter {


FFTWindow::FFTWindow( int _w, int _h ) : 
analysis(FloatImagePlane(_w, _h)), 
synthesis(FloatImagePlane(_w,_h))
{
  analysisIsFlat = true;
  synthesisIsFlat = true;
  analysis.allocateImage();
  synthesis.allocateImage();
  SSEAvailable = !!(rs_detect_cpu_features() & RS_CPU_FLAG_SSE);
}


FFTWindow::~FFTWindow(void)
{}


void FFTWindow::createHalfCosineWindow( int ox, int oy )
{
  float *wanx=new float[ox];//analysis windox
  float *wsynx=new float[ox];//syntesis windox

  //Calc 1d half-cosine window
  for(int i=0;i<ox;i++) {
    wanx[i] = cosf(PI_F*(i-ox+0.5f)/(ox*2));
    wsynx[i] = wanx[i];
  }

  createWindow(analysis, ox, wanx);
  createWindow(synthesis, ox, wsynx);
  analysisIsFlat = false;
  synthesisIsFlat = false;
  delete[] wanx;
  delete[] wsynx;
}

void FFTWindow::createRaisedCosineWindow( int ox, int oy )
{
  float *wanx=new float[ox];//analysis windox
  float *wsynx=new float[ox];//syntesis windox

  for(int i=0;i<ox;i++) {
    wanx[i] = sqrt(cosf(PI_F*(i-ox+0.5f)/(ox*2)));
    wsynx[i]=wanx[i]*wanx[i]*wanx[i];
  }

  createWindow(analysis, ox, wanx);
  createWindow(synthesis, ox, wsynx);
  analysisIsFlat = false;
  synthesisIsFlat = false;

  delete[] wanx;
  delete[] wsynx;

}

void FFTWindow::createSqrtHalfCosineWindow( int ox, int oy )
{
  float *wanx=new float[ox];//analysis windox
  float *wsynx=new float[ox];//syntesis windox

  for(int i=0;i<ox;i++)
  {
    wanx[i] = 1.0;
    wsynx[i]=cosf(PI_F*(i-ox+0.5f)/(ox*2)); 
    wsynx[i]*=wsynx[i];
  }

  createWindow(analysis, ox, wanx);
  createWindow(synthesis, ox, wsynx);
  analysisIsFlat = true;
  synthesisIsFlat = false;

  delete[] wanx;
  delete[] wsynx;
}

float FFTWindow::createWindow( FloatImagePlane &window, int overlap, float* weight)
{
  //Setup the 2D window;
  int bw = window.w;
  int bh = window.h;
  float sum = 0.0f;
  for (int y = 0; y < bh; y++) {

    float yfactor = 1.0;

    if (y<overlap)
      yfactor *= weight[y];
    else if (y>bh-overlap)
      yfactor *= weight[bh - y];

    float *m = window.getLine(y);

    for (int x = 0; x < bw; x++) {
      float factor = yfactor;
      if (x<overlap)
        factor *= weight[x];
      else if (x > bw-overlap)
        factor *= weight[bw - x];

      m[x] = factor;
      sum += factor;
    }
  } 
  return sum;
}

void FFTWindow::applyAnalysisWindow( FloatImagePlane *image, FloatImagePlane *dst )
{
  g_assert(image->w == analysis.w);
  g_assert(image->h == analysis.h);
  g_assert(dst->w == analysis.w);
  g_assert(dst->h == analysis.h);

  if (analysisIsFlat) {
    image->blitOnto(dst);
    return;
  }
#if defined (__i386__) || defined (__x86_64__)
  if (SSEAvailable && ((analysis.w & 15) == 0)) {
    applyAnalysisWindowSSE( image, dst);
    return;
  }
#endif // defined (__i386__) || defined (__x86_64__)

  for (int y = 0; y < analysis.h; y++) {
    float *srcp1 = analysis.getLine(y);
    float *srcp2 = image->getLine(y);
    float *dstp = dst->getLine(y);
    for (int x = 0; x < analysis.w; x++) {
      dstp[x] = srcp1[x] * srcp2[x];
    }
  }
}

#if defined (__i386__) || defined (__x86_64__)

void FFTWindow::applyAnalysisWindowSSE( FloatImagePlane *image, FloatImagePlane *dst )
{
  for (int y = 0; y < analysis.h; y++) {
    int sizew = analysis.w>>4;    // Size in loops
    float* src1 = image->getLine(y);
    if ((uintptr_t)src1 & 15) {
      asm volatile 
      ( 
      "loop_analysis_sse_ua:\n"
      "prefetchnta (%4)\n"        // Prefetch next line (Used once only, so don't pollute cache)
      "movups (%1), %%xmm0\n"       // src1 pt1
      "movups 16(%1), %%xmm1\n"     // src1 pt2 
      "movups 32(%1), %%xmm2\n"     // src1 pt3
      "movups 48(%1), %%xmm3\n"     // src1 pt4 
      "mulps (%0), %%xmm0\n"       // src1 * window pt1
      "mulps 16(%0), %%xmm1\n"     // src1 * window pt2 
      "mulps 32(%0), %%xmm2\n"     // src1 * window pt3
      "mulps 48(%0), %%xmm3\n"     // src1 * window pt4 
      "movaps %%xmm0, (%2)\n"       // store pt1
      "movaps %%xmm1, 16(%2)\n"     // store pt2
      "movaps %%xmm2, 32(%2)\n"       // store pt1
      "movaps %%xmm3, 48(%2)\n"     // store pt2
      "add $64, %0\n"
      "add $64, %1\n"
      "add $64, %2\n"
      "add $64, %4\n"
      "dec %3\n"
      "jnz loop_analysis_sse_ua\n"

      : /* no output registers */
      : "r" (analysis.getLine(y)), "r" (src1),  "r" (dst->getLine(y)), "r" (sizew), "r" (&src1[image->pitch])
      : /*          %0                  %1                  %2               %3             %4  */
      );
    } else {
    asm volatile 
      ( 
      "loop_analysis_sse_a:\n"
      "prefetchnta (%4)\n"        // Prefetch next line (Used once only, so don't pollute cache)
      "movaps (%1), %%xmm0\n"       // src1 pt1
      "movaps 16(%1), %%xmm1\n"     // src1 pt2 
      "movaps 32(%1), %%xmm2\n"     // src1 pt3
      "movaps 48(%1), %%xmm3\n"     // src1 pt4 
      "mulps (%0), %%xmm0\n"       // src1 * window pt1
      "mulps 16(%0), %%xmm1\n"     // src1 * window pt2 
      "mulps 32(%0), %%xmm2\n"     // src1 * window pt3
      "mulps 48(%0), %%xmm3\n"     // src1 * window pt4 
      "movaps %%xmm0, (%2)\n"       // store pt1
      "movaps %%xmm1, 16(%2)\n"     // store pt2
      "movaps %%xmm2, 32(%2)\n"       // store pt1
      "movaps %%xmm3, 48(%2)\n"     // store pt2
      "add $64, %0\n"
      "add $64, %1\n"
      "add $64, %2\n"
      "add $64, %4\n"
      "dec %3\n"
      "jnz loop_analysis_sse_a\n"

      : /* no output registers */
      : "r" (analysis.getLine(y)), "r" (src1),  "r" (dst->getLine(y)), "r" (sizew), "r" (&src1[image->pitch])
      : /*          %0                  %1                  %2               %3             %4  */
      );
    }
  }
}

#endif // defined (__i386__) || defined (__x86_64__)


// FIXME: SSE2 me, if used some time in the future
void FFTWindow::applySynthesisWindow( FloatImagePlane *image )
{
  g_assert(image->w == synthesis.w);
  g_assert(image->h == synthesis.h);
  if (synthesisIsFlat)
    return;

  for (int y = 0; y < synthesis.h; y++) {
    float *srcp1 = image->getLine(y);
    float *srcp2 = synthesis.getLine(y);
    for (int x = 0; x < synthesis.w; x++) {
      srcp1[x] *= srcp2[x];
    }
  }
}

}}// namespace RawStudio::FFTFilter
