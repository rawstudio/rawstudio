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
 *
 * Based on FFT3DFilter plugin for Avisynth 2.5 - 3D Frequency Domain filter
 * Copyright(C)2004-2005 A.G.Balakhnin aka Fizick, email: bag@hotmail.ru, web: http://bag.hotmail.ru
 */

#include "complexfilter.h"
#include <math.h>
#include "fftwindow.h"

 /*
  * These classes define the processing that must be done
  * to each block.
  * Note that the process() function must be re-entrant, 
  * as all threads will use the same instance for each plane.
  *
  */
#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

namespace RawStudio {
namespace FFTFilter {


 /**** BASE CLASS *****/

ComplexFilter::ComplexFilter( int block_width, int block_height ) : 
bw(block_width), bh(block_height),  norm(1.0f/(block_width*block_height)),
sharpen(0), sigmaSquaredSharpenMin(0), sigmaSquaredSharpenMax(0), sharpenWindow(0)
{}

ComplexFilter::~ComplexFilter(void)
{
  if (sharpenWindow)
    delete sharpenWindow;
  sharpenWindow = 0;
}

void ComplexFilter::setSharpen( float _sharpen, float sigmaSharpenMin, float sigmaSharpenMax, float scutoff )
{
  if (ABS(_sharpen) <0.001f)
    return;
  sharpen = _sharpen;
  sigmaSquaredSharpenMin = sigmaSharpenMin*sigmaSharpenMin/norm;
  sigmaSquaredSharpenMax = sigmaSharpenMax*sigmaSharpenMax/norm;
  // window for sharpen
  float svr = 1.0f;   // Horizontal to vertical ratio
  if (!sharpenWindow) {
    sharpenWindow = new FloatImagePlane(bw, bh);
    sharpenWindow->allocateImage();
  }

  for (int j=0; j<bh; j++)
  {
    int dj = j;
    if (j>=bh/2)
      dj = bh-j;
    float d2v = float(dj*dj)*(svr*svr)/((bh/2)*(bh/2));
    float *wsharpen = sharpenWindow->getLine(j);
    for (int i=0; i<bw; i++)
    {
      float d2 = d2v + float(i*i)/((bw/2)*(bw/2)); // distance_2 - v1.7
      wsharpen[i] = sharpen * (1 - exp(-d2/(2*scutoff*scutoff)));
    }
  }
  /* In sharpen function, remember: Sharpen factor is already applied to wsharpen*/
}

void ComplexFilter::process( ComplexBlock* block )
{
  if (ABS(sharpen) >0.001f)
    processSharpen(block);
  else
    processNoSharpen(block);
}

gboolean ComplexFilter::skipBlock() {
  if (ABS(sharpen) >0.001f)
    return false;
  return true;
}

  /** DeGridComplexFilter  **/
DeGridComplexFilter::DeGridComplexFilter(int block_width, int block_height, float _degrid, FFTWindow *_window, fftwf_plan plan_forward) :
ComplexFilter(block_width, block_height), 
degrid(_degrid),
window(_window)
{
  grid = new ComplexBlock(bw, bh);
  FloatImagePlane realGrid(bw, bh);
  realGrid.allocateImage();
  float* f = realGrid.data;
  int count = bh*realGrid.pitch;

  for (int i = 0 ; i < count; i++){
    f[i] = 65535.0f;
  }
  window->applyAnalysisWindow(&realGrid,&realGrid);
  fftwf_execute_dft_r2c(plan_forward, f, grid->complex);
}

DeGridComplexFilter::~DeGridComplexFilter( void )
{
  delete grid;  
}

void DeGridComplexFilter::processSharpenOnly(ComplexBlock* block) {

#if defined (__i386__) || defined (__x86_64__)
    guint cpu = rs_detect_cpu_features();
    if (cpu & RS_CPU_FLAG_SSE3) 
      return processSharpenOnlySSE3(block);
    else if (cpu & RS_CPU_FLAG_SSE)
      return processSharpenOnlySSE(block);
#endif

  int x,y;
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;

  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  for (y=0; y<bh; y++) {
    float *wsharpen = sharpenWindow->getLine(y);
    for (x=0; x<bw; x++) {
      float gridcorrection0 = gridfraction*gridsample[x][0];
      float re = outcur[x][0] - gridcorrection0;
      float gridcorrection1 = gridfraction*gridsample[x][1];
      float im = outcur[x][1] - gridcorrection1;
      float psd = (re*re + im*im) + 1e-15f;// power spectrum density
      //improved sharpen mode to prevent grid artifactes and to limit sharpening both fo low and high amplitudes
      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ; 
      re *= sfact; // apply filter on real  part  
      im *= sfact; // apply filter on imaginary part
      outcur[x][0] = re + gridcorrection0;
      outcur[x][1] = im + gridcorrection1;
    }
    gridsample += bw;
    outcur += bw;
    wsharpen += bw;    
  }
}


/**** Basic Wiener Filter *****/


ComplexWienerFilter::ComplexWienerFilter( int block_width, int block_height,float _beta, float _sigma ) :
ComplexFilter(block_width, block_height)
{
  lowlimit = (_beta-1)/_beta;
  sigmaSquaredNoiseNormed = _sigma*_sigma/norm;
}


ComplexWienerFilter::~ComplexWienerFilter( void ){}

gboolean ComplexWienerFilter::skipBlock() {
  if (ABS(sharpen) >0.001f)
    return false;
  if (sigmaSquaredNoiseNormed > 1e-15f)
    return false;
  return true;
}

void ComplexWienerFilter::processNoSharpen( ComplexBlock* block )
{
  int x,y;
  float psd;
  float WienerFactor;
  fftwf_complex* outcur = block->complex;
  g_assert(bw == block->w);
  g_assert(bh == block->h);

  for (y=0; y<bh; y++) {
    for (x=0; x<bw; x++) {
      psd = (outcur[x][0]*outcur[x][0] + outcur[x][1]*outcur[x][1]) + 1e-15f;// power spectrum density
      WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter
      outcur[x][0] *= WienerFactor; // apply filter on real  part	
      outcur[x][1] *= WienerFactor; // apply filter on imaginary part
    }
    outcur += bw;
  }

}

void ComplexWienerFilter::processSharpen( ComplexBlock* block )
{
  int x,y;
  float psd;
  float WienerFactor;
  fftwf_complex* outcur = block->complex;
  g_assert(bw == block->w);
  g_assert(bh == block->h);
  for (y=0; y<bh; y++) {
    float *wsharpen = sharpenWindow->getLine(y);
    for (x=0; x<bw; x++) {
      psd = (outcur[x][0]*outcur[x][0] + outcur[x][1]*outcur[x][1]) + 1e-15f;// power spectrum density
      WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter
      WienerFactor *= 1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) );
      outcur[x][0] *= WienerFactor; // apply filter on real  part	
      outcur[x][1] *= WienerFactor; // apply filter on imaginary part
    }
    outcur += bw;
    wsharpen += bw;
  }

}


/**** Apply Pattern Filter *****/

ComplexPatternFilter::ComplexPatternFilter( int block_width, int block_height, float _beta, 
                                           FloatImagePlane* _pattern, float pattern_strength ) :
ComplexFilter(block_width, block_height), 
pfactor(pattern_strength)
{
  lowlimit = (_beta-1)/_beta;
  pattern = _pattern;
}

ComplexPatternFilter::~ComplexPatternFilter( void )
{
  if (pattern)
    delete pattern;
}


void ComplexPatternFilter::processNoSharpen( ComplexBlock* block )
{
  g_assert(bw == block->w);
  g_assert(bh == block->h);
  int x,y;
  float psd;
  fftwf_complex* outcur = block->complex;
  float* pattern2d = pattern->data;
  float patternfactor;

  for (y=0; y<bh; y++) {
    for (x=0; x<bw; x++) {
      psd = (outcur[x][0]*outcur[x][0] + outcur[x][1]*outcur[x][1]) + 1e-15f;
      patternfactor = MAX((psd - pfactor*pattern2d[x])/psd, lowlimit);
      outcur[x][0] *= patternfactor;
      outcur[x][1] *= patternfactor;
    }
    outcur += bw;
    pattern2d += pattern->pitch;
  }
}

void ComplexPatternFilter::processSharpen( ComplexBlock* block )
{
  g_assert(!"Not implemented");  
}

gboolean ComplexPatternFilter::skipBlock() {
  if (ABS(sharpen) >0.001f)
    return false;
  if (pfactor > 1e-15f)
    return false;
  return true;
}

ComplexWienerFilterDeGrid::ComplexWienerFilterDeGrid( int block_width, int block_height, 
                                                     float _beta, float _sigma, float _degrid,
                                                     fftwf_plan plan_forward, FFTWindow *_window)
: DeGridComplexFilter(block_width, block_height, _degrid, _window, plan_forward) 
{
  lowlimit = (_beta-1)/_beta;
  sigmaSquaredNoiseNormed = _sigma*_sigma/norm;
}

ComplexWienerFilterDeGrid::~ComplexWienerFilterDeGrid( void )
{
}

gboolean ComplexWienerFilterDeGrid::skipBlock() {
  if (ABS(sharpen) >0.001f)
    return false;
  if (sigmaSquaredNoiseNormed > 1e-15f)
    return false;
  return true;
}

void ComplexWienerFilterDeGrid::processNoSharpen( ComplexBlock* block )
{
  if (sigmaSquaredNoiseNormed <= 1e-15f)
    return;

#if defined (__i386__) || defined (__x86_64__)
  guint cpu = rs_detect_cpu_features();
  if (cpu & RS_CPU_FLAG_SSE3) 
    return processNoSharpen_SSE3(block);
  else if (cpu & RS_CPU_FLAG_SSE) 
    return processNoSharpen_SSE(block);
#endif

  int x,y;
  float psd;
  float WienerFactor;
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;

  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  for (y=0; y<bh; y++) {
    for (x=0; x<bw; x++) {
      float gridcorrection0 = gridfraction*gridsample[x][0];
      float corrected0 = outcur[x][0] - gridcorrection0;
      float gridcorrection1 = gridfraction*gridsample[x][1];
      float corrected1 = outcur[x][1] - gridcorrection1;
      psd = (corrected0*corrected0 + corrected1*corrected1 ) + 1e-15f;// power spectrum density
      WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter
      corrected0 *= WienerFactor; // apply filter on real  part	
      corrected1 *= WienerFactor; // apply filter on imaginary part
      outcur[x][0] = corrected0 + gridcorrection0;
      outcur[x][1] = corrected1 + gridcorrection1;
    }
    outcur += bw;
    gridsample += bw;
  }

}


void ComplexWienerFilterDeGrid::processSharpen( ComplexBlock* block )
{
  if (sigmaSquaredNoiseNormed <= 1e-15f)
    return processSharpenOnly(block);

#if defined (__i386__) || defined (__x86_64__)
  guint cpu = rs_detect_cpu_features();
  if (cpu & RS_CPU_FLAG_SSE3) 
    return processSharpen_SSE3(block);
  else if (cpu & RS_CPU_FLAG_SSE) 
    return processSharpen_SSE(block);
#endif

  int x,y;
  float psd;
  float WienerFactor;
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;

  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  for (y=0; y<bh; y++) {
    float *wsharpen = sharpenWindow->getLine(y);
    for (x=0; x<bw; x++) {
      float gridcorrection0 = gridfraction*gridsample[x][0];
      float corrected0 = outcur[x][0] - gridcorrection0;
      float gridcorrection1 = gridfraction*gridsample[x][1];
      float corrected1 = outcur[x][1] - gridcorrection1;
      psd = (corrected0*corrected0 + corrected1*corrected1 ) + 1e-15f;// power spectrum density
      WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter
      WienerFactor *= 1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) ); 
      corrected0 *= WienerFactor; // apply filter on real  part	
      corrected1 *= WienerFactor; // apply filter on imaginary part
      outcur[x][0] = corrected0 + gridcorrection0;
      outcur[x][1] = corrected1 + gridcorrection1;

    }
    outcur += bw;
    gridsample += bw;
  }
}

ComplexFilterPatternDeGrid::ComplexFilterPatternDeGrid( int block_width, int block_height, 
                                                     float _beta, float _sigma, float _degrid,
                                                     fftwf_plan plan_forward, FFTWindow *_window,
                                                     FloatImagePlane *_pattern)
                                                     :
DeGridComplexFilter(block_width, block_height, _degrid, _window, plan_forward), 
pattern(_pattern)
{
  lowlimit = (_beta-1)/_beta;
  sigmaSquaredNoiseNormed = _sigma*_sigma/norm;
}

ComplexFilterPatternDeGrid::~ComplexFilterPatternDeGrid( void )
{
}

gboolean ComplexFilterPatternDeGrid::skipBlock() {
  return false;
}


void ComplexFilterPatternDeGrid::processNoSharpen( ComplexBlock* block )
{
  int x,y;
  float psd;
  float WienerFactor;
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;

  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  for (y=0; y<bh; y++) {
    float *pattern2d = pattern->getLine(y);
    for (x=0; x<bw; x++) {
      float gridcorrection0 = gridfraction*gridsample[x][0];
      float corrected0 = outcur[x][0] - gridcorrection0;
      float gridcorrection1 = gridfraction*gridsample[x][1];
      float corrected1 = outcur[x][1] - gridcorrection1;
      psd = (corrected0*corrected0 + corrected1*corrected1 ) + 1e-15f;// power spectrum density
      WienerFactor = MAX((psd - pattern2d[x])/psd, lowlimit); // limited Wiener filter
      corrected0 *= WienerFactor; // apply filter on real  part	
      corrected1 *= WienerFactor; // apply filter on imaginary part
      outcur[x][0] = corrected0 + gridcorrection0;
      outcur[x][1] = corrected1 + gridcorrection1;
    }
    outcur += bw;
    gridsample += bw;
  }
}

/* Naiive cut together from ApplyPattern2D_degrid_C and Sharpen_degrid_C, 
   it may be possible to factor out some grid correction */

void ComplexFilterPatternDeGrid::processSharpen( ComplexBlock* block )
{
  if (sigmaSquaredNoiseNormed <= 1e-15f)
    return processSharpenOnly(block);

  int x,y;
  float psd;
  float WienerFactor;
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;

  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  for (y=0; y<bh; y++) {
    float *pattern2d = pattern->getLine(y);
    float *wsharpen = sharpenWindow->getLine(y);
    for (x=0; x<bw; x++) {
      float gridcorrection0 = gridfraction*gridsample[x][0];
      float corrected0 = outcur[x][0] - gridcorrection0;
      float gridcorrection1 = gridfraction*gridsample[x][1];
      float corrected1 = outcur[x][1] - gridcorrection1;
      psd = (corrected0*corrected0 + corrected1*corrected1 ) + 1e-15f;// power spectrum density
      WienerFactor = MAX((psd - pattern2d[x])/psd, lowlimit); // limited Wiener filter
      
      corrected0 *= WienerFactor; // apply filter on real  part	
      corrected1 *= WienerFactor; // apply filter on imaginary part
      corrected0 += gridcorrection0; // apply filter on real  part	
      corrected1 += gridcorrection1; // apply filter on imaginary part

      gridcorrection0 = gridfraction*corrected0;
      float re = corrected0 - gridcorrection0;
      gridcorrection1 = gridfraction*corrected0;
      float im = corrected1 - gridcorrection1;
      psd = (re*re + im*im) + 1e-15f;// power spectrum density

      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ; 

      corrected0 *= sfact;        // apply filter on real  part	
      corrected1 *= sfact;        // apply filter on imaginary part

      outcur[x][0] = corrected0 + gridcorrection0;
      outcur[x][1] = corrected1 + gridcorrection1;
    }
    outcur += bw;
    gridsample += bw;
  }
}

}}// namespace RawStudio::FFTFilter
