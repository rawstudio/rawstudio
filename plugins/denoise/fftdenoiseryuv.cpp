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

#include "fftdenoiseryuv.h"
#include <math.h>

namespace RawStudio {
namespace FFTFilter {

FFTDenoiserYUV::FFTDenoiserYUV(void)
{
}

FFTDenoiserYUV::~FFTDenoiserYUV(void)
{
}

void FFTDenoiserYUV::denoiseImage( RS_IMAGE16* image )
{
  FloatPlanarImage img;
  img.bw = FFT_BLOCK_SIZE;
  img.bh = FFT_BLOCK_SIZE;
  img.ox = FFT_BLOCK_OVERLAP;
  img.oy = FFT_BLOCK_OVERLAP;

  img.redCorrection = redCorrection;
  img.blueCorrection = blueCorrection;

  if ((image->w < FFT_BLOCK_SIZE) || (image->h < FFT_BLOCK_SIZE))
     return;   // Image too small to denoise

  if (image->channels != 3 || image->filters!=0)
     return;   // No conversion possible with this image

  waitForJobs(img.getUnpackInterleavedYUVJobs(image));

  if (abort) return;

  img.mirrorEdges();
  if (abort) return;

  FFTWindow window(img.bw,img.bh);
  window.createHalfCosineWindow(img.ox, img.oy);

  ComplexFilter *filter = new ComplexWienerFilterDeGrid(img.bw, img.bh, beta, sigmaLuma, 1.0, plan_forward, &window);
  filter->setSharpen(sharpen, sharpenMinSigma, sharpenMaxSigma, sharpenCutoff);
  img.setFilter(0,filter,&window);

  filter = new ComplexWienerFilterDeGrid(img.bw, img.bh, betaChroma, sigmaChroma, 1.0, plan_forward, &window);
  filter->setSharpen(sharpenChroma, sharpenMinSigmaChroma, sharpenMaxSigmaChroma, sharpenCutoffChroma);
  img.setFilter(1,filter,&window);

  filter = new ComplexWienerFilterDeGrid(img.bw, img.bh, betaChroma, sigmaChroma, 1.0, plan_forward, &window);
  filter->setSharpen(sharpenChroma, sharpenMinSigmaChroma, sharpenMaxSigmaChroma, sharpenCutoffChroma);
  img.setFilter(2,filter,&window);

  FloatPlanarImage outImg(img);

  processJobs(img, outImg);
  if (abort) return;

  // Convert back
  waitForJobs(outImg.getPackInterleavedYUVJobs(image));
}


void FFTDenoiserYUV::setParameters( FFTDenoiseInfo *info )
{
  FFTDenoiser::setParameters(info);
  sigmaLuma = info->sigmaLuma*SIGMA_FACTOR;
  sigmaChroma = info->sigmaChroma*SIGMA_FACTOR;
  betaChroma = info->betaChroma;
  sharpen = info->sharpenLuma;
  sharpenCutoff = info->sharpenCutoffLuma;
  sharpenMinSigma = info->sharpenMinSigmaLuma*SIGMA_FACTOR;
  sharpenMaxSigma = info->sharpenMaxSigmaLuma*SIGMA_FACTOR;
  sharpenChroma = info->sharpenChroma;
  sharpenCutoffChroma = info->sharpenCutoffChroma*SIGMA_FACTOR;
  sharpenMinSigmaChroma = info->sharpenMinSigmaChroma*SIGMA_FACTOR;
  sharpenMaxSigmaChroma = info->sharpenMaxSigmaChroma*SIGMA_FACTOR;
  redCorrection = info->redCorrection;
  blueCorrection = info->blueCorrection;
}

}}// namespace RawStudio::FFTFilter
