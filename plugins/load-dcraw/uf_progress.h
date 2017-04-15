/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * uf_progress.h - progress bar header
 * Copyright 2009-2015 by Frank van Maarseveen, Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _UF_PROGRESS_H
#define _UF_PROGRESS_H

#define PROGRESS_WAVELET_DENOISE	1
#define PROGRESS_DESPECKLE		2
#define PROGRESS_INTERPOLATE		3
#define PROGRESS_RENDER			4	/* tiled work */

#define PROGRESS_LOAD			5
#define PROGRESS_SAVE			6

extern void (*ufraw_progress)(int what, int ticks);

/*
 * The first call for a PROGRESS_* activity should specify a negative number
 * of ticks. This call will prepare the corresponding progress bar segment.
 * Subsequent calls for the same activity should specify a non-negative number
 * of ticks corresponding to the amount of work just done. The total number
 * of ticks including the initialization call should be approximately zero.
 *
 * This function is thread safe. See also preview_progress().
 */
static inline void progress(int what, int ticks)
{
    if (ufraw_progress)
        ufraw_progress(what, ticks);
}

#endif /* _UF_PROGRESS_H */
