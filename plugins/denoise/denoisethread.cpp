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

#include "floatimageplane.h"
#include "denoisethread.h"
#include "complexfilter.h"
#include "fftwindow.h"
#include "floatplanarimage.h"

void *StartDenoiseThread(void *_this) {
  DenoiseThread *d = (DenoiseThread*)_this;
  d->threadExited = false;
  d->runDenoise();
  d->threadExited = true;
  pthread_exit(NULL);
  return NULL;
}

DenoiseThread::DenoiseThread(void) {
  complex = 0;
  input_plane = 0;
  exitThread = false;
  threadExited = false;
  pthread_mutex_init(&run_thread_mutex, NULL);
  pthread_cond_init (&run_thread, NULL);        // Signal thread to run
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_create(&thread_id,&attr,StartDenoiseThread,this);
  pthread_attr_destroy(&attr);
}

DenoiseThread::~DenoiseThread(void) {
  if (!threadExited)
    exitThread = true;
  waiting = 0;
  pthread_mutex_lock(&run_thread_mutex);
  pthread_cond_signal(&run_thread);       // Start thread
  pthread_mutex_unlock(&run_thread_mutex);
  pthread_join(thread_id, NULL);
  pthread_mutex_destroy(&run_thread_mutex);
  pthread_cond_destroy(&run_thread);
  if (complex)
    delete complex;
  complex = 0;
  if (input_plane)
    delete input_plane;
  input_plane = 0;
}

void DenoiseThread::addJobs( JobQueue *_waiting, JobQueue *_finished )
{
  pthread_mutex_lock(&run_thread_mutex);
  waiting = _waiting;
  finished = _finished;
  pthread_cond_signal(&run_thread);
  pthread_mutex_unlock(&run_thread_mutex);
}

void DenoiseThread::jobsEnded()
{
  pthread_mutex_lock(&run_thread_mutex);
  waiting = 0;
  finished = 0;
  pthread_mutex_unlock(&run_thread_mutex);
}

void DenoiseThread::runDenoise() {
  pthread_mutex_lock(&run_thread_mutex);
  while (!exitThread) {
    pthread_cond_wait(&run_thread,&run_thread_mutex); // Wait for jobs
    vector<Job*> jobs;
    if (waiting)
      jobs = waiting->getJobsPercent(10);
    while (!exitThread && !jobs.empty()) {
      Job* j = jobs[0];
      jobs.erase(jobs.begin());

      switch (j->type) {
        case JOB_FFT:
          procesFFT((FFTJob*)j);
          break;
          case JOB_CONVERT_FROMFLOAT_YUV:
            {
              ImgConvertJob *job = (ImgConvertJob*)j;
              job->img->packInterleavedYUV(job);
              break;
            }
          case JOB_CONVERT_TOFLOAT_YUV: 
            {
              ImgConvertJob *job = (ImgConvertJob*)j;
              job->img->unpackInterleavedYUV(job);
              break;
            }
        default:
          break;
      }
      finished->addJob(j);
      if (jobs.empty())
        jobs = waiting->getJobsPercent(10);
      
    }
  }
  pthread_mutex_unlock(&run_thread_mutex);
}

void DenoiseThread::procesFFT( FFTJob* j )
{
  FloatImagePlane* input = j->p->in;
  g_assert(j->p->filter);

  if (j->p->filter->skipBlock()) {
    j->outPlane->applySlice(j->p);
    return;
  }

  if (!complex)
    complex = new ComplexBlock(input->w, input->h);

  if (!input_plane) {
    input_plane = new FloatImagePlane(input->w, input->h);
    input_plane->allocateImage();
  }

  j->p->window->applyAnalysisWindow(input, input_plane);

  fftwf_execute_dft_r2c(forward, input_plane->data, complex->complex);

  j->p->filter->process(complex);

  fftwf_execute_dft_c2r(reverse, complex->complex, input_plane->data);

  j->p->setOut(input_plane);

  // Currently not used, as no overlapped data is used.
  //j->p->window->applySynthesisWindow(j->p->out);

  j->outPlane->applySlice(j->p);

}
 