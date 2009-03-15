/*
* Copyright (C) 2009 Klaus Post
*
* Contains code from:
*
* Based on FFT3DFilter plugin for Avisynth 2.5 - 3D Frequency Domain filter
* Copyright(C)2004-2005 A.G.Balakhnin aka Fizick, email: bag@hotmail.ru, web: http://bag.hotmail.ru

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


#include "fftwindow.h"
#include <math.h>

#define PI_F 3.14159265358979323846f


FFTWindow::FFTWindow( int _w, int _h ) : 
analysis(FloatImagePlane(_w, _h)), 
synthesis(FloatImagePlane(_w,_h))
{
  analysis.allocateImage();
  synthesis.allocateImage();
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

  delete[] wanx;
  delete[] wsynx;
}

void FFTWindow::createWindow( FloatImagePlane &window, int overlap, float* weight)
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
  if (sum > (bw*bh-1.0f)) {  /* Account for some rounding */
    isFlat = true;
  }
}
// FIXME: SSE2 me
void FFTWindow::applyAnalysisWindow( FloatImagePlane *image, FloatImagePlane *dst )
{
  g_assert(image->w == analysis.w);
  g_assert(image->h == analysis.h);
  g_assert(dst->w == analysis.w);
  g_assert(dst->h == analysis.h);
  if (isFlat) {
    image->blitOnto(dst);
    return;
  }

#if defined (__i386__) || defined (__x86_64__)
/*  if (analysis.pitch == dst->pitch && ((dst->pitch&15) == 0)) {
    applyAnalysisWindowSSE( image, dst);
    return;
  }*/
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

// FIXME: Fix, if needed. Crashes.
void FFTWindow::applyAnalysisWindowSSE( FloatImagePlane *image, FloatImagePlane *dst )
{
  int sizew = analysis.pitch * 4;    // Size in bytes
  float* srcp1 = image->getLine(0);
  g_assert(analysis.pitch == dst->pitch);
  g_assert(analysis.w == dst->pitch);
  int remainpitch = image->pitch*4 - sizew;
  asm volatile 
    ( 
//      "mov (%3), %3\n"
      "loop_analysis_sse_ua:\n"
//      "prefetcht0 (%1,%3)\n"        // Prefetch next line
      "movups (%1), %%xmm0\n"       // src1 pt1
      "movups 16(%1), %%xmm1\n"     // src1 pt2 
      "movaps (%0), %%xmm2\n"       // window pt1
      "movaps 16(%0), %%xmm3\n"     // window pt2 
      "mulps %%xmm0, %%xmm2\n"      // p1 multiplied
      "mulps %%xmm1, %%xmm3\n"      // p1 multiplied
      "movaps %%xmm2, (%2)\n"       // store pt1
      "movaps %%xmm3, 16(%2)\n"     // stote pt2
      "add $32, %0\n"
      "add $32, %1\n"
      "sub $32 ,%4\n"
      "cmp $0, %4\n"
      "jg loop_analysis_sse_ua\n"
      "add %3, %1\n"                  // Add pitch
      "dec %5\n"
      "jnz loop_analysis_sse_ua"
      : /* no output registers */
      : "r" (analysis.getLine(0)), "r" (srcp1),  "r" (dst->getLine(0)), "r" (&remainpitch), "r"(sizew), "r" (image->h)
      : /*          %0                    %1                 %2              %3                        %4               %5    */
    );
}
#endif // defined (__i386__) || defined (__x86_64__)

// FIXME: SSE2 me
void FFTWindow::applySynthesisWindow( FloatImagePlane *image )
{
  g_assert(image->w == synthesis.w);
  g_assert(image->h == synthesis.h);
  if (isFlat)
    return;

  for (int y = 0; y < synthesis.h; y++) {
    float *srcp1 = image->getLine(y);
    float *srcp2 = synthesis.getLine(y);
    for (int x = 0; x < synthesis.w; x++) {
      srcp1[x] *= srcp2[x];
    }
  }
}