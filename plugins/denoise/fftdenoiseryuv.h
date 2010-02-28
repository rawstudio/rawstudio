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

#ifndef fftdenoiseryuv_h__
#define fftdenoiseryuv_h__

#include "fftdenoiser.h"

class FFTDenoiserYUV :
  public FFTDenoiser
{
public:
  FFTDenoiserYUV();
  virtual ~FFTDenoiserYUV(void);
  virtual void denoiseImage(RS_IMAGE16* image);
  virtual void setParameters( FFTDenoiseInfo *info);
  float betaChroma;
  float sigmaLuma;
  float sigmaChroma;
  float sharpenChroma;           
  float sharpenCutoffChroma;      
  float sharpenMinSigmaChroma;  
  float sharpenMaxSigmaChroma;
  float redCorrection;
  float blueCorrection;
};
#endif // fftdenoiseryuv_h__
