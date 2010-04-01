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

#include "jobqueue.h"
#include "floatplanarimage.h"

namespace RawStudio {
namespace FFTFilter {

FFTJob::FFTJob( PlanarImageSlice *s ) : Job(JOB_FFT), p(s) {
}

FFTJob::~FFTJob( void ) {
  if (p)
    delete(p);
}

JobQueue::JobQueue(void)
{
  pthread_mutex_init(&job_mutex, NULL);
  pthread_cond_init(&job_added_notify, NULL);
}

JobQueue::~JobQueue(void)
{
  pthread_mutex_lock(&job_mutex);
  pthread_mutex_unlock(&job_mutex);
  pthread_mutex_destroy(&job_mutex);
  pthread_cond_destroy(&job_added_notify);
}

Job* JobQueue::getJob()
{
  Job *j;
  pthread_mutex_lock(&job_mutex);
  if (jobs.empty())
    j = 0; 
  else {
    j = jobs[0];
    jobs.erase(jobs.begin());
  }
  pthread_mutex_unlock(&job_mutex);
  return j;
}

vector<Job*> JobQueue::getJobs(int n)
{
  vector<Job*> j;
  pthread_mutex_lock(&job_mutex);
  n = MIN(n,(int)jobs.size());
  for (int i = 0; i < n; i++) {
    j.push_back(jobs[0]);
    jobs.erase(jobs.begin());
  }
  pthread_mutex_unlock(&job_mutex);
  return j;
}

vector<Job*> JobQueue::getJobsPercent( int percent )
{
  vector<Job*> j;
  pthread_mutex_lock(&job_mutex);
  if (jobs.empty()) {
    pthread_mutex_unlock(&job_mutex);
    return j;
  }
  // Ensure that we get at least 1 job, otherwise respect percentage
  int n = MAX(1, percent * jobs.size() / 100);
  for (int i = 0; i < n; i++) {
    j.push_back(jobs[0]);
    jobs.erase(jobs.begin());
  }
  pthread_mutex_unlock(&job_mutex);
  return j;
}
void JobQueue::addJob( Job* job)
{
  pthread_mutex_lock(&job_mutex);
  jobs.push_back(job);
  pthread_cond_signal(&job_added_notify);
  pthread_mutex_unlock(&job_mutex);
}

int JobQueue::jobsLeft(void) {
  int size;
  pthread_mutex_lock(&job_mutex);
  size = jobs.size();
  pthread_mutex_unlock(&job_mutex);
  return size;
}

Job* JobQueue::waitForJob()
{
  Job *j;
  pthread_mutex_lock(&job_mutex);
  if (jobs.empty())
    pthread_cond_wait(&job_added_notify, &job_mutex);
  
  j = jobs[0];
  jobs.erase(jobs.begin());

  pthread_mutex_unlock(&job_mutex);
  return j;
}

int JobQueue::removeRemaining()
{
  pthread_mutex_lock(&job_mutex);
  int n = jobs.size();
  for (int i = 0; i < n; i++) {
    delete jobs[i];
  }
  jobs.clear();
  pthread_mutex_unlock(&job_mutex);
  return n;
}

}}// namespace RawStudio::FFTFilter

