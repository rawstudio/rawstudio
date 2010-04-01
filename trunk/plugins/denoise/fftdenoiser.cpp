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

#include "fftdenoiser.h"
#include "complexblock.h"
#include "fftdenoiseryuv.h"

#ifdef WIN32
int rs_get_number_of_processor_cores(){return 4;}
#endif

namespace RawStudio {
namespace FFTFilter {


FFTDenoiser::FFTDenoiser(void)
{
  nThreads = rs_get_number_of_processor_cores();
  threads = new DenoiseThread[nThreads];
  initializeFFT();
  FloatPlanarImage::initConvTable();
}

FFTDenoiser::~FFTDenoiser(void)
{
  delete[] threads;
  fftwf_destroy_plan(plan_forward);
  fftwf_destroy_plan(plan_reverse); 
}

void FFTDenoiser::denoiseImage( RS_IMAGE16* image )
{
  FloatPlanarImage img;
  img.bw = FFT_BLOCK_SIZE;
  img.bh = FFT_BLOCK_SIZE;
  img.ox = FFT_BLOCK_OVERLAP;
  img.oy = FFT_BLOCK_OVERLAP;

  if ((image->w < FFT_BLOCK_SIZE) || (image->h < FFT_BLOCK_SIZE))
     return;   // Image too small to denoise

  if (image->channels > 1 && image->filters==0) {
     img.unpackInterleaved(image);
  } else {
    return;
  }
  if (abort) return;

  img.mirrorEdges();
  if (abort) return;

  FFTWindow window(img.bw,img.bh);
  window.createHalfCosineWindow(img.ox, img.oy);
  
  ComplexFilter *filter = new ComplexWienerFilterDeGrid(img.bw, img.bh, beta, sigma, 1.0, plan_forward, &window);
  filter->setSharpen(sharpen, sharpenMinSigma, sharpenMaxSigma, sharpenCutoff);
  img.setFilter(0,filter,&window);

  filter = new ComplexWienerFilterDeGrid(img.bw, img.bh, beta, sigma, 1.0, plan_forward, &window);
  filter->setSharpen(sharpen, sharpenMinSigma, sharpenMaxSigma, sharpenCutoff);
  img.setFilter(1,filter,&window);

  filter = new ComplexWienerFilterDeGrid(img.bw, img.bh, beta, sigma, 1.0, plan_forward, &window);
  filter->setSharpen(sharpen, sharpenMinSigma, sharpenMaxSigma, sharpenCutoff);
  img.setFilter(2,filter,&window);

  FloatPlanarImage outImg(img);

  processJobs(img, outImg);
  if (abort) return;

  // Convert back
  if (image->channels > 1 && image->filters==0) {
     outImg.packInterleaved(image);
  }
}

void FFTDenoiser::processJobs(FloatPlanarImage &img, FloatPlanarImage &outImg)
{
  // Prepare for reassembling the image
  outImg.allocate_planes();
  // Split input image
  JobQueue* waiting_jobs = img.getJobs(outImg);
  JobQueue* finished_jobs = new JobQueue();

  // Count waiting jobs
  gint njobs = waiting_jobs->jobsLeft();

  for (guint i = 0; i < nThreads; i++) {
    threads[i].addJobs(waiting_jobs,finished_jobs);
  }


  gint jobs_added  = 0;
  while (jobs_added < njobs) {
    Job *_j = finished_jobs->waitForJob();

    if (_j->type == JOB_FFT) {
      delete _j;
      jobs_added++;
      if (abort) {
        jobs_added += waiting_jobs->removeRemaining();
        jobs_added += finished_jobs->removeRemaining();
      }
    }
  }

  for (guint i = 0; i < nThreads; i++)
    threads[i].jobsEnded();

  delete finished_jobs;
  delete waiting_jobs;
}

void FFTDenoiser::waitForJobs(JobQueue *waiting_jobs)
{
  JobQueue* finished_jobs = new JobQueue();

  gint njobs = waiting_jobs->jobsLeft();

  for (guint i = 0; i < nThreads; i++) {
    threads[i].addJobs(waiting_jobs,finished_jobs);
  }

  gint jobs_added  = 0;
  while (jobs_added < njobs) {
    Job *j = finished_jobs->waitForJob();
    delete j;
    jobs_added++;
  }

  for (guint i = 0; i < nThreads; i++)
    threads[i].jobsEnded();
  
  delete waiting_jobs;
  delete finished_jobs;
}

gboolean FFTDenoiser::initializeFFT()
{
  // Create dummy block
  FloatImagePlane plane(FFT_BLOCK_SIZE,FFT_BLOCK_SIZE);
  plane.allocateImage();
  ComplexBlock complex(FFT_BLOCK_SIZE,FFT_BLOCK_SIZE);
  int dim[2];
  dim[0] = FFT_BLOCK_SIZE;
  dim[1] = FFT_BLOCK_SIZE;
  plan_forward = fftwf_plan_dft_r2c(2, dim, plane.data, complex.complex,FFTW_MEASURE|FFTW_DESTROY_INPUT);
  plan_reverse = fftwf_plan_dft_c2r(2, dim, complex.complex, plane.data,FFTW_MEASURE|FFTW_DESTROY_INPUT);
  for (guint i = 0; i < nThreads; i++) {
    threads[i].forward = plan_forward;
    threads[i].reverse = plan_reverse;
  }
  return (plan_forward && plan_reverse);
}


void FFTDenoiser::setParameters( FFTDenoiseInfo *info )
{
  sigma = info->sigmaLuma *SIGMA_FACTOR;
  beta = max(1.0f, info->betaLuma);
  sharpen = info->sharpenLuma;
  sharpenCutoff = info->sharpenCutoffLuma;
  sharpenMinSigma = info->sharpenMinSigmaLuma*SIGMA_FACTOR;
  sharpenMaxSigma = info->sharpenMaxSigmaLuma*SIGMA_FACTOR;
}

}}// namespace RawStudio::FFTFilter

extern "C" {

  /** INTERFACE **/

  void initDenoiser(FFTDenoiseInfo* info) {
    RawStudio::FFTFilter::FFTDenoiser *t;
    switch (info->processMode) {
    case PROCESS_RGB:
      t = new RawStudio::FFTFilter::FFTDenoiser();
      break;
    case PROCESS_YUV:
      t = new RawStudio::FFTFilter::FFTDenoiserYUV();
      break;
    default:
      g_assert(false);
    }
    info->_this = t;
    // Initialize parameters to default
    info->betaLuma = 1.0f;
	info->betaChroma = 1.0f;
	info->sigmaLuma = 1.0f;
    info->sigmaChroma = 1.0f;
    info->sharpenLuma = 0.0f;
    info->sharpenChroma = 0.0f;
    info->sharpenCutoffLuma = 0.10f;
    info->sharpenCutoffChroma = 0.3f;
    info->sharpenMinSigmaLuma = 4.0f;
    info->sharpenMinSigmaChroma = 4.0f;
    info->sharpenMaxSigmaLuma = 20.0f;
    info->sharpenMaxSigmaChroma = 20.0f;
    info->redCorrection = 1.0f;
    info->blueCorrection = 1.0f;
  }

  void denoiseImage(FFTDenoiseInfo* info) {
    RawStudio::FFTFilter::FFTDenoiser *t = (RawStudio::FFTFilter::FFTDenoiser*)info->_this;  
    t->abort = false;
    t->setParameters(info);
    t->denoiseImage(info->image);
  }

  void destroyDenoiser(FFTDenoiseInfo* info) {
    RawStudio::FFTFilter::FFTDenoiser *t = (RawStudio::FFTFilter::FFTDenoiser*)info->_this;  
    delete t;
  }

  void abortDenoiser(FFTDenoiseInfo* info) {
    RawStudio::FFTFilter::FFTDenoiser *t = (RawStudio::FFTFilter::FFTDenoiser*)info->_this;
    t->abort = true;
  }

} // extern "C"

