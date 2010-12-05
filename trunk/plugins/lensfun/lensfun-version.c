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

/* This is various techniques to guess Lensfun version. We need this because
   Lensfun didn't include a LF_VERSION in any release before 0.2.5.1 - but the API/ABI changed
   in 0.2.5.0. */

#include "lensfun-version.h"

#ifdef LF_VERSION

/* First we try to use LF_VERSION from Lensfun, this will work from Lensfun version > 0.2.5.1 */

guint
rs_guess_lensfun_version()
{
	return LF_VERSION;
}

#elif defined(__gnu_linux__) \
	&& (__GLIBC__ > 2 \
	|| (__GLIBC__ == 2 && __GLIBC_MINOR__ > 2) \
	|| (__GLIBC__ == 2 && __GLIBC_MINOR__ == 2))

/* As a fallback we try to look at the name of the linked library path (only
   for Linux hosts) */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <lensfun.h>
#include <stdio.h>

#define __USE_GNU
#include <link.h>

static gint _guess_lensfun_iterator(struct dl_phdr_info *info, gsize size, gpointer user_data);

guint
rs_guess_lensfun_version()
{
	gint max_unwind_levels;
	gint major=0, minor=0, micro=0, bugfix=0;
	guint version = 0;
	gchar *library_path = NULL, *filename = NULL;

	if (version > 0)
		return version;

	dl_iterate_phdr(_guess_lensfun_iterator, &library_path);

	/* Try to unwind symlinks */
	max_unwind_levels = 10;
	while (library_path && (max_unwind_levels > 0))
	{
		gchar new_path[400];

		/* Break if it's not a symlink, we must be done */
		if (!g_file_test(library_path, G_FILE_TEST_IS_SYMLINK))
			break;

		/* We just try to read the link, this should fail if the file is not a
		   symbolic link */
		gsize len;

		if ((len = readlink(library_path, new_path, 399)) > 0)
		{
			/* null-terminate new path */
			new_path[len] = '\0';
			g_free(library_path);
			library_path = g_strdup(new_path);
		}

		max_unwind_levels--;
	}

	/* Remove path */
	filename = g_path_get_basename(library_path);

	/* Try to read the version number from the library name. If not all
	   tokens are found, sscanf() will still fill in the ones available */
	if (filename)
		sscanf(filename, "liblensfun.so.%d.%d.%d.%d", &major, &minor, &micro, &bugfix);

	/* Build integer mathcing LF_VERSION */
	version = major << 24 | minor << 16 | micro << 8 | bugfix;

	g_free(filename);
	g_free(library_path);

	if (version == 0)
		g_warning("Lensfun library version is unknown.");

	return version;
}

static gint
_guess_lensfun_iterator(struct dl_phdr_info *info, gsize size, gpointer user_data)
{
	gchar **library_path = (gchar **) user_data;

	if (g_strstr_len(info->dlpi_name, -1, "liblensfun.so"))
	{
		*library_path = g_strdup(info->dlpi_name);

		return 1;
	}
	else
		return 0;
}

#else

/* - or we give up and return 0, this is bad */

guint
rs_guess_lensfun_version()
{
	g_warning("Lensfun library version is unknown.");
	return 0;
}

#endif
