/*
 * Copyright (C) 2006, 2007 Anders Brander <anders@brander.dk> and 
 * Anders Kvist <akv@lnxbx.dk>
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
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "rawfile.h"

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
	if((rawfile->base+pos+length)>rawfile->size)
		return(NULL);

	pl = gdk_pixbuf_loader_new();
	gdk_pixbuf_loader_write(pl, rawfile->map+rawfile->base+pos, length, NULL);
	pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
	gdk_pixbuf_loader_close(pl, NULL);
	return(pixbuf);
}

RAWFILE *
raw_open_file(const gchar *filename)
{
	struct stat st;
	gint fd;
	RAWFILE *rawfile;

	if(stat(filename, &st))
		return(NULL);
	if ((fd = open(filename, O_RDONLY)) == -1)
		return(NULL);
	rawfile = g_malloc(sizeof(RAWFILE));
	rawfile->fd = fd;
	rawfile->size = st.st_size;
	rawfile->base = 0;
	rawfile->map = mmap(NULL, rawfile->size, PROT_READ, MAP_SHARED, fd, 0);
	if(rawfile->map == MAP_FAILED)
	{
		g_free(rawfile);
		return(NULL);
	}
	rawfile->byteorder = 0x4D4D;
	return(rawfile);
}

gboolean
raw_init_file_tiff(RAWFILE *rawfile, guint pos)
{
	guchar tmp;
	if((pos+12)>rawfile->size)
		return(FALSE);
	rawfile->byteorder = *((gushort *) rawfile->map+pos);
	raw_get_uint(rawfile, pos+4, &rawfile->first_ifd_offset);
	if (rawfile->first_ifd_offset > rawfile->size)
		return(FALSE);
	raw_get_uchar(rawfile, pos+2, &tmp);
	rawfile->base = pos;
	return(TRUE);
}

void
raw_close_file(RAWFILE *rawfile)
{
	munmap(rawfile->map, rawfile->size);
	close(rawfile->fd);
	g_free(rawfile);
	return;
}

void
raw_reset_base(RAWFILE *rawfile)
{
	rawfile->base = 0;
	return;
}
