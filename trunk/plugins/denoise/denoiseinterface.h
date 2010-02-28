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

#ifndef denoiseinterface_h__
#define denoiseinterface_h__
#include <rawstudio.h>

#ifdef _unix_
G_BEGIN_DECLS
#endif

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

typedef enum {
  PROCESS_RGB, PROCESS_YUV, PROCESS_PATTERN_RGB, PROCESS_PATTERN_YUV
} InitDenoiseMode;

typedef struct {
  InitDenoiseMode processMode;  // Set this before initializing, DO NOT modify after that.
  RS_IMAGE16* image;            // This will be input and output
  float sigmaLuma;              // In RGB mode this is used for all planes, YUV mode only luma.
  float sigmaChroma;            // Used only in YUV mode.
  float betaLuma;               // In RGB mode this is used for all planes, YUV mode only luma.
  float betaChroma;             // Used only in YUV mode.

  /* Sharpening - Luma is used for all planes in RGB */
  float sharpenLuma;            // sharpening strength (default=0 - not sharpen)
  float sharpenCutoffLuma;      // sharpening cutoff frequency, relative to max (default=0.3)
  float sharpenMinSigmaLuma;    // Minimum limit (approximate noise margin) for sharpening stage (default=4.0)
  float sharpenMaxSigmaLuma;    // Maximum limit (approximate oversharping margin) for sharpening stage (default=20.0)
  float sharpenChroma;          // sharpening strength (default=0 - not sharpen)
  float sharpenCutoffChroma;    // sharpening cutoff frequency, relative to max (default=0.3)
  float sharpenMinSigmaChroma;  // Minimum limit (approximate noise margin) for sharpening stage (default=4.0)
  float sharpenMaxSigmaChroma;  // Maximum limit (approximate oversharping margin) for sharpening stage (default=20.0)

  float redCorrection;          // Red coefficient, multiplid to R in YUV conversion. (default: 1.0)
  float blueCorrection;         // Blue coefficient, multiplid to R in YUV conversion. (default: 1.0)
  void* _this;                  // Do not modify this value.
} FFTDenoiseInfo;

void initDenoiser(FFTDenoiseInfo* info);
void denoiseImage(FFTDenoiseInfo* info);
void destroyDenoiser(FFTDenoiseInfo* info);
void abortDenoiser(FFTDenoiseInfo* info);

#ifdef _unix_
G_END_DECLS
#endif

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif
#endif // denoiseinterface_h__
