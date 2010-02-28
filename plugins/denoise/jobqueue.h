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

#ifndef jobqueue_h__
#define jobqueue_h__

#include <vector>
#include "planarimageslice.h"
#include "pthread.h"

class FloatPlanarImage;
using namespace std;
typedef enum {
  JOB_FFT,
  JOB_CONVERT_TOFLOAT_YUV,
  JOB_CONVERT_FROMFLOAT_YUV
} JobType;

class Job 
{
public:
  Job(JobType _type) : type(_type) {};
  virtual ~Job(void) {};
  JobType type;
};

class FFTJob : public Job
{
public:
  FFTJob(PlanarImageSlice *slice);
  virtual ~FFTJob(void);
  PlanarImageSlice *p;
  FloatImagePlane *outPlane;
};

class ImgConvertJob : public Job
{
public:
  ImgConvertJob(FloatPlanarImage *_img, JobType _type) : Job(_type), img(_img) {};
  virtual ~ImgConvertJob(void) {};
  RS_IMAGE16 *rs;
  FloatPlanarImage *img;
  int start_y;
  int end_y;
};

class JobQueue
{
public:
  JobQueue(void);
  virtual ~JobQueue(void);
  Job* getJob();
  void addJob(Job*);
  int removeRemaining();  // Removes remaining jobs, and returns the number of deleted jobs.
  int jobsLeft();
  Job* waitForJob();
  vector<Job*> getJobs(int n);
  vector<Job*> getJobsPercent(int percent);
private:
  vector<Job*> jobs;      // Requires a mutex, so private.
  pthread_mutex_t job_mutex;
  pthread_cond_t job_added_notify;
};
#endif // jobqueue_h__
