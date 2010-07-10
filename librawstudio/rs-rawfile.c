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

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef G_OS_WIN32
 #include <windows.h>
#else
 #include <arpa/inet.h>
 #include <sys/mman.h>
#endif
#include <string.h>
#include "rs-rawfile.h"

struct _RAWFILE {
#ifdef G_OS_WIN32
	HANDLE filehandle;
	HANDLE maphandle;
#else
	gint fd;
#endif
	gboolean is_map;
	guint size;
	void *map;
	gushort byteorder;
	guint first_ifd_offset;
	guint base;
};

#if BYTE_ORDER == LITTLE_ENDIAN
const static gushort cpuorder = 0x4949;
#elif BYTE_ORDER == BIG_ENDIAN
const static gushort cpuorder = 0x4D4D;
#endif

void
raw_init(void)
{
	/* stub */
	return;
}

gboolean
raw_get_uint(RAWFILE *rawfile, guint pos, guint *target)
{
	if((rawfile->base+pos+4)>rawfile->size)
		return(FALSE);
	if (rawfile->byteorder == cpuorder)
		*target = *(guint *)(rawfile->map+pos+rawfile->base);
	else
		*target = ENDIANSWAP4(*(guint *)(rawfile->map+pos+rawfile->base));
	return(TRUE);
}

gboolean
raw_get_ushort(RAWFILE *rawfile, guint pos, gushort *target)
{
	if((rawfile->base+pos+2)>rawfile->size)
		return(FALSE);
	if (rawfile->byteorder == cpuorder)
		*target = *(gushort *)(rawfile->map+rawfile->base+pos);
	else
		*target = ENDIANSWAP2(*(gushort *)(rawfile->map+rawfile->base+pos));
	return(TRUE);
}

gushort
raw_get_ushort_from_string(RAWFILE *rawfile, gchar *source)
{
	gushort target;

	if (rawfile->byteorder == cpuorder)
		target = *(gushort *)(source);
	else
		target = ENDIANSWAP2(*(gushort *)(source));
	return(target);
}

gboolean
raw_get_short(RAWFILE *rawfile, guint pos, gshort *target)
{
	if((rawfile->base+pos+2)>rawfile->size)
		return(FALSE);
	if (rawfile->byteorder == cpuorder)
		*target = *(gshort *)(rawfile->map+rawfile->base+pos);
	else
		*target = ENDIANSWAP2(*(gshort *)(rawfile->map+rawfile->base+pos));
	return(TRUE);
}

gshort
raw_get_short_from_string(RAWFILE *rawfile, gchar *source)
{
	gushort target;

	if (rawfile->byteorder == cpuorder)
		target = *(gshort *)(source);
	else
		target = ENDIANSWAP2(*(gshort *)(source));
	return(target);
}

gboolean
raw_get_float(RAWFILE *rawfile, guint pos, gfloat *target)
{
	if((rawfile->base+pos+4)>rawfile->size)
		return(FALSE);

	if (rawfile->byteorder == cpuorder)
		*target = *(gfloat *)(rawfile->map+rawfile->base+pos);
	else
		*target = (gfloat) (ENDIANSWAP4(*(gint *)(rawfile->map+rawfile->base+pos)));
	return(TRUE);
}

gboolean
raw_get_uchar(RAWFILE *rawfile, guint pos, guchar *target)
{
	if((rawfile->base+pos+1)>rawfile->size)
		return(FALSE);

	*target = *(guchar *)(rawfile->map+rawfile->base+pos);
	return(TRUE);
}

gboolean
raw_get_rational(RAWFILE *rawfile, guint pos, gfloat *target)
{
	if((rawfile->base+pos+8)>rawfile->size)
		return(FALSE);

	guint counter, divisor;
	raw_get_uint(rawfile, pos, &counter);
	raw_get_uint(rawfile, pos+4, &divisor);

	if (divisor == 0)
		return(FALSE);

	*target = (gfloat) counter / (gfloat) divisor;
	return(TRUE);
}

gboolean
raw_strcmp(RAWFILE *rawfile, guint pos, const gchar *needle, gint len)
{
	if((rawfile->base+pos+len) > rawfile->size)
		return(FALSE);
	if(0 == g_ascii_strncasecmp(needle, rawfile->map+rawfile->base+pos, len))
		return(TRUE);
	else
		return(FALSE);
}

gboolean
raw_strcpy(RAWFILE *rawfile, guint pos, void *target, gint len)
{
	if((rawfile->base+pos+len) > rawfile->size)
		return(FALSE);
	g_memmove(target, rawfile->map+rawfile->base+pos, len);
	return(TRUE);
}

gchar *
raw_strdup(RAWFILE *rawfile, guint pos, gint len)
{
	if((rawfile->base+pos+len) > rawfile->size)
		return(FALSE);
	return(g_strndup(rawfile->map+rawfile->base+pos, len));
}

GdkPixbuf *
raw_get_pixbuf(RAWFILE *rawfile, guint pos, guint length)
{
	GdkPixbufLoader *pl;
	GdkPixbuf *pixbuf = NULL;
	gboolean cont = TRUE; /* Are we good to continue? */
	if((rawfile->base+pos+length)>rawfile->size)
		return(NULL);

	pl = gdk_pixbuf_loader_new();
	while((length > 100000) && cont)
	{
		cont = gdk_pixbuf_loader_write(pl, rawfile->map+rawfile->base+pos, 80000, NULL);
		length -= 80000;
		pos += 80000;
	}
	if (cont)
		gdk_pixbuf_loader_write(pl, rawfile->map+rawfile->base+pos, length, NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
	gdk_pixbuf_loader_close(pl, NULL);
	return(pixbuf);
}

RAWFILE *
raw_create_from_memory(void *memory, guint size, guint first_ifd_offset, gushort byteorder)
{
	RAWFILE *rawfile;
	rawfile = g_malloc(sizeof(RAWFILE));

	rawfile->is_map = FALSE;
	rawfile->size = size;
	rawfile->map = memory;
	rawfile->base = 0;
	rawfile->byteorder = byteorder;
	rawfile->first_ifd_offset = first_ifd_offset;
	return rawfile;
}

RAWFILE *
raw_open_file(const gchar *filename)
{
	struct stat st;
#ifndef G_OS_WIN32
	gint fd;
#endif
	RAWFILE *rawfile;

	if(stat(filename, &st))
		return(NULL);
	rawfile = g_malloc(sizeof(RAWFILE));
	rawfile->size = st.st_size;
#ifdef G_OS_WIN32

	rawfile->filehandle = CreateFile(filename, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (rawfile->filehandle == INVALID_HANDLE_VALUE)
	{
		g_free(rawfile);
		return(NULL);
	}

	if ((rawfile->maphandle = CreateFileMapping(rawfile->filehandle, NULL, PAGE_READONLY, 0, 0, NULL))==NULL)
	{
		g_free(rawfile);
		return(NULL);
	}

	rawfile->map = MapViewOfFile(rawfile->maphandle, FILE_MAP_READ, 0, 0, rawfile->size);
	if (rawfile->map == NULL)
	{
		g_free(rawfile);
		return(NULL);
	}

#else
	if ((fd = open(filename, O_RDONLY)) == -1)
	{
		g_free(rawfile);
		return(NULL);
	}
	rawfile->map = mmap(NULL, rawfile->size, PROT_READ, MAP_SHARED, fd, 0);
	if(rawfile->map == MAP_FAILED)
	{
		g_free(rawfile);
		return(NULL);
	}
	rawfile->is_map = TRUE;
	rawfile->fd = fd;
#endif
	rawfile->base = 0;
	rawfile->byteorder = 0x4D4D;
	return(rawfile);
}

guchar
raw_init_file_tiff(RAWFILE *rawfile, guint pos)
{
	guchar version = 0;

	if((pos+12)>rawfile->size)
		return version;
	rawfile->byteorder = *((gushort *) (rawfile->map+pos));
	raw_get_uint(rawfile, pos+4, &rawfile->first_ifd_offset);
	if (rawfile->first_ifd_offset > rawfile->size)
		return version;

	raw_get_uchar(rawfile, pos+2, &version);

	rawfile->base = pos;
	return version;
}


void
raw_close_file(RAWFILE *rawfile)
{
	if (rawfile->is_map)
	{
#ifdef G_OS_WIN32
		UnmapViewOfFile(rawfile->map);
		CloseHandle(rawfile->maphandle);
		CloseHandle(rawfile->filehandle);
#else
		munmap(rawfile->map, rawfile->size);
		close(rawfile->fd);
#endif
	}
	g_free(rawfile);
	return;
}

void
raw_reset_base(RAWFILE *rawfile)
{
	rawfile->base = 0;
	return;
}

gint
raw_get_base(RAWFILE *rawfile)
{
	return rawfile->base;
}

gushort
raw_get_byteorder(RAWFILE *rawfile)
{
	return rawfile->byteorder;
}

void
raw_set_byteorder(RAWFILE *rawfile, gushort byteorder)
{
	rawfile->byteorder = byteorder;
}

guint
get_first_ifd_offset(RAWFILE *rawfile)
{
	return rawfile->first_ifd_offset;
}

void *
raw_get_map(RAWFILE *rawfile)
{
	return rawfile->map;
}

guint
raw_get_filesize(RAWFILE *rawfile)
{
	return rawfile->size;
}
