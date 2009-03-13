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
#include "denoisethread.h"
#include "complexfilter.h"
#include "fftwindow.h"

void *StartDenoiseThread(void *_this) {
  DenoiseThread *d = (DenoiseThread*)_this;
  d->threadExited = false;
  d->runDenoise();
  d->threadExited = true;
  pthread_exit(NULL);
  return NULL;
}

DenoiseThread::DenoiseThread(void) {
  exitThread = false;
  threadExited = false;
  pthread_mutex_init(&run_thread_mutex, NULL);
  pthread_cond_init (&run_thread, NULL);        // Signal thread to run
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_create(&thread_id,&attr,StartDenoiseThread,this);
  pthread_attr_destroy(&attr);
  complex = 0;
  input_plane = 0;
}

DenoiseThread::~DenoiseThread(void) {
  if (!threadExited)
    exitThread = true;
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

void DenoiseThread::runDenoise() {
  while (!exitThread) {
    pthread_mutex_lock(&run_thread_mutex);
    pthread_cond_wait(&run_thread,&run_thread_mutex); // Wait for jobs
    pthread_mutex_unlock(&run_thread_mutex);
    vector<Job*> jobs = waiting->getJobsPercent(10);
    while (!exitThread && !jobs.empty()) {
      Job* j = jobs[0];
      jobs.erase(jobs.begin());

      FloatImagePlane* input = j->p->in;
      
      if (!complex)
        complex = new ComplexBlock(input->w, input->h);
      if (!input_plane) {
        input_plane = new FloatImagePlane(input->w, input->h);
        input_plane->allocateImage();
      }
      
      j->p->window->applyAnalysisWindow(input, input_plane);

      fftwf_execute_dft_r2c(forward, input_plane->data, complex->complex);        

      g_assert(j->p->filter);
      if (j->p->filter)
        j->p->filter->process(complex);

      j->p->allocateOut();

      fftwf_execute_dft_c2r(reverse, complex->complex, j->p->out->data);
      //j->p->window->applySynthesisWindow(j->p->out);

      finished->addJob(j);

      if (jobs.empty())
        jobs = waiting->getJobsPercent(10);
      
    }
  }
}

#undef PTR_OFF
#undef ALIGN_OFFSET
 