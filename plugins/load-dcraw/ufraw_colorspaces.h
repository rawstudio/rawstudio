/*
 * UFRaw - Unidentified Flying Raw converter for digital camera images
 *
 * ufraw_colorspaces.h - Built-in color profile declarations.
 * Copyright 2004-2015 by Udi Fuchs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/** create the ICC virtual profile for srgb space. */
cmsHPROFILE uf_colorspaces_create_srgb_profile(void);
