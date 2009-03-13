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

#ifndef complexfilter_h__
#define complexfilter_h__
#include "complexblock.h"
#include "floatimageplane.h"

class FFTWindow;

class ComplexFilter
{
public:
  ComplexFilter(int block_width, int block_height);
  ComplexFilter(const ComplexFilter& p);
  virtual ~ComplexFilter(void);
  void process(ComplexBlock* block);
  virtual void setSharpen( float sharpen, float sigmaSharpenMin, float sigmaSharpenMax, float scutoff );
  const int bw;
  const int bh;
  const float norm; // Normalization factor
  float sharpen;
  float sigmaSquaredSharpenMin;
  float sigmaSquaredSharpenMax;
  FloatImagePlane *sharpenWindow;
protected:
  virtual void processNoSharpen(ComplexBlock* block) = 0;
  virtual void processSharpen(ComplexBlock* block) = 0;
};

class ComplexWienerFilter : public ComplexFilter
{
public:
  ComplexWienerFilter(int block_width, int block_height, float beta, float sigma);
  virtual ~ComplexWienerFilter(void);
  const float beta;
  float sigmaSquaredNoiseNormed;
protected:
  virtual void processNoSharpen(ComplexBlock* block);
  virtual void processSharpen(ComplexBlock* block);
};

class ComplexWienerFilterDeGrid : public ComplexFilter
{
public:
  ComplexWienerFilterDeGrid(int block_width, int block_height, float beta, float sigma, float degrid, fftwf_plan plan, FFTWindow *window);
  virtual ~ComplexWienerFilterDeGrid(void);
  const float beta;
  float sigmaSquaredNoiseNormed;
  float degrid;
  const ComplexBlock* grid;
  FFTWindow *window;
protected:
  virtual void processNoSharpen(ComplexBlock* block);
  virtual void processSharpen(ComplexBlock* block);
};

class ComplexPatternFilter : public ComplexFilter
{
public:
  ComplexPatternFilter(int block_width, int block_height, float beta, FloatImagePlane* pattern, float pattern_strength );
  virtual ~ComplexPatternFilter(void);
  const float beta;
  FloatImagePlane* pattern;
  const float pfactor;
protected:
  virtual void processNoSharpen(ComplexBlock* block);
  virtual void processSharpen(ComplexBlock* block);
};



#endif // complexfilter_h__
