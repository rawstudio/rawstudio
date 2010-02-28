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

#ifndef fftdenoiser_h__
#define fftdenoiser_h__
#include <rawstudio.h>
#include "floatplanarimage.h"
#include "denoisethread.h"
#include "denoiseinterface.h"

#define FFT_BLOCK_SIZE 128       // Preferable able to be factorized into primes, must be divideable by 4.
#define FFT_BLOCK_OVERLAP 24    // Must be dividable by 4 (OVERLAP * 2 must be < SIZE)
#define SIGMA_FACTOR 0.25f;    // Amount to multiply sigma by to give reasonable amount
class FFTDenoiser
{
public:
  FFTDenoiser(void);
  virtual ~FFTDenoiser(void);
  gboolean initializeFFT();
  virtual void setParameters( FFTDenoiseInfo *info);
  virtual void denoiseImage(RS_IMAGE16* image);
  gboolean abort;
protected:
  virtual void processJobs(FloatPlanarImage &img, FloatPlanarImage &outImg);
  void waitForJobs(JobQueue *waiting_jobs);
  guint nThreads;
  DenoiseThread *threads;
  fftwf_plan plan_forward;
  fftwf_plan plan_reverse;
  float sigma;
  float beta;
  float sharpen;           
  float sharpenCutoff;      
  float sharpenMinSigma;  
  float sharpenMaxSigma;
};
#endif // fftdenoiser_h__
