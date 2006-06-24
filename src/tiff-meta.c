#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include "matrix.h"
#include "rawstudio.h"
#include "tiff-meta.h"

typedef struct _rawfile {
	gint fd;
	guint size;
	void *map;
	gushort byteorder;
	guint first_ifd_offset;
} RAWFILE;

static int cpuorder;

RAWFILE *raw_open_file(const gchar *filename);
gboolean raw_get_uint(RAWFILE *rawfile, guint pos, guint *target);
gboolean raw_get_ushort(RAWFILE *rawfile, guint pos, gushort *target);
gboolean raw_get_float(RAWFILE *rawfile, guint pos, gfloat *target);
gboolean raw_strcmp(RAWFILE *rawfile, guint pos, const gchar *needle, gint len);
void raw_close_file(RAWFILE *rawfile);

gboolean
raw_get_uint(RAWFILE *rawfile, guint pos, guint *target)
{
	if((pos+4)>rawfile->size)
		return(FALSE);
	if (rawfile->byteorder == cpuorder)
		*target = *(guint *)(rawfile->map+pos);
	else
		*target = ENDIANSWAP4(*(guint *)(rawfile->map+pos));
	return(TRUE);
}

gboolean
raw_get_ushort(RAWFILE *rawfile, guint pos, gushort *target)
{
	if((pos+2)>rawfile->size)
		return(FALSE);
	if (rawfile->byteorder == cpuorder)
		*target = *(gushort *)(rawfile->map+pos);
	else
		*target = ENDIANSWAP2(*(gushort *)(rawfile->map+pos));
	return(TRUE);
}

gboolean
raw_get_float(RAWFILE *rawfile, guint pos, gfloat *target)
{
	if((pos+4)>rawfile->size)
		return(FALSE);

	if (rawfile->byteorder == cpuorder)
		*target = *(gfloat *)(rawfile->map+pos);
	else
		*target = (gfloat) (ENDIANSWAP4(*(gint *)(rawfile->map+pos)));
	return(TRUE);
}

gint
raw_strcmp(RAWFILE *rawfile, guint pos, const gchar *needle, gint len)
{
	if((pos+len) > rawfile->size)
		return(FALSE);
	return(g_ascii_strncasecmp(needle, rawfile->map+pos, len));
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
	rawfile->map = mmap(NULL, rawfile->size, PROT_READ, MAP_SHARED, fd, 0);
	if(rawfile->map == MAP_FAILED)
	{
		g_free(rawfile);
		return(NULL);
	}
	rawfile->byteorder = *((gushort *) rawfile->map);
	raw_get_uint(rawfile, 4, &rawfile->first_ifd_offset);
	return(rawfile);
}

void
raw_close_file(RAWFILE *rawfile)
{
	munmap(rawfile->map, rawfile->size);
	close(rawfile->fd);
	g_free(rawfile);
	return;
}

gboolean
raw_ifd_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	gushort number_of_entries;
	gushort fieldtag=0;
/*	gushort fieldtype; */
	gushort ushort_temp1=0;
	guint valuecount;
	guint uint_temp1=0;
	gfloat float_temp1=0.0, float_temp2=0.0;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries)) return(FALSE);
	offset += 2;

	while(number_of_entries--)
	{
		raw_get_ushort(rawfile, offset, &fieldtag);
/*		raw_get_ushort(rawfile, offset+2, &fieldtype); */
		raw_get_uint(rawfile, offset+4, &valuecount);
		offset += 8;
		switch(fieldtag)
		{
			case 0x010f: /* Make */
				raw_get_uint(rawfile, offset, &uint_temp1);
				if (0 == raw_strcmp(rawfile, uint_temp1, "Canon", 5))
					meta->make = MAKE_CANON;
				else if (0 == raw_strcmp(rawfile, uint_temp1, "NIKON", 5))
					meta->make = MAKE_NIKON;
				break;
			case 0x0111: /* PreviewImageStart */
				raw_get_uint(rawfile, offset, &meta->preview_start);
				break;
			case 0x0117: /* PreviewImageLength */
				raw_get_uint(rawfile, offset, &meta->preview_length);
				break;
			case 0x0112: /* Orientation */
				raw_get_ushort(rawfile, offset, &meta->orientation);
				break;
			case 0x0201: /* jpeg start */
				raw_get_uint(rawfile, offset, &meta->thumbnail_start);
				break;
			case 0x0202: /* jpeg length */
				raw_get_uint(rawfile, offset, &meta->thumbnail_length);
				break;
			case 0x829D: /* FNumber */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_float(rawfile, uint_temp1, &float_temp1);
				raw_get_float(rawfile, uint_temp1+4, &float_temp2);
				meta->aperture = float_temp1/float_temp2;
				break;
			case 0x8827: /* ISOSpeedRatings */
				raw_get_ushort(rawfile, offset, &meta->iso);
				break;
			case 0x014a: /* SubIFD */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_uint(rawfile, uint_temp1, &uint_temp1);
				raw_ifd_walker(rawfile, uint_temp1, meta);
				break;
			case 0x927c: /* MakerNote */
				if (meta->make == MAKE_CANON)
				{
					raw_get_uint(rawfile, offset, &uint_temp1);
					raw_ifd_walker(rawfile, uint_temp1, meta);
				}
				break;
			case 0x8769: /* ExifIFDPointer */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_ifd_walker(rawfile, uint_temp1, meta);
				break;
			case 0x9201: /* ShutterSpeedValue */
				raw_get_uint(rawfile, offset, &uint_temp1);
				raw_get_float(rawfile, uint_temp1, &float_temp1);
				raw_get_float(rawfile, uint_temp1+4, &float_temp2);
				meta->shutterspeed = 1.0/pow(2.0,-float_temp1/float_temp2);
				break;
			case 0x4001: /* white balance for Canon 20D & 350D */
				raw_get_uint(rawfile, offset, &uint_temp1);
				switch (valuecount)
				{
					case 582:
						uint_temp1 += 50;
						break;
					case 653:
						uint_temp1 += 68;
						break;
					case 796:
						uint_temp1 += 126;
						break;
				}
				/* RGGB-format! */
				raw_get_ushort(rawfile, uint_temp1, &ushort_temp1);
				meta->cam_mul[0] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, uint_temp1+2, &ushort_temp1);
				meta->cam_mul[1] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, uint_temp1+4, &ushort_temp1);
				meta->cam_mul[3] = (gdouble) ushort_temp1;
				raw_get_ushort(rawfile, uint_temp1+6, &ushort_temp1);
				meta->cam_mul[2] = (gdouble) ushort_temp1;
				{
					gdouble div;
					div = 2/(meta->cam_mul[1]+meta->cam_mul[3]);
					meta->cam_mul[0] *= div;
					meta->cam_mul[1] = 1.0;
					meta->cam_mul[2] *= div;
					meta->cam_mul[3] = 1.0;
				}
				break;
		}
		offset += 4;
	}
	return(TRUE);
}

void
rs_tiff_load_meta(const gchar *filename, RS_METADATA *meta)
{
	RAWFILE *rawfile;
	guint next, offset;
	gushort ifd_num;

	if (ntohs(0x1234) == 0x1234)
		cpuorder = 0x4D4D;
	else
		cpuorder = 0x4949;

	meta->make = MAKE_UNKNOWN;
	meta->aperture = 0.0;
	meta->iso = 0;
	meta->shutterspeed = 0.0;
	meta->thumbnail_start = 0;
	meta->thumbnail_length = 0;
	meta->preview_start = 0;
	meta->preview_length = 0;
	meta->cam_mul[0] = 1.0;
	meta->cam_mul[1] = 1.0;
	meta->cam_mul[2] = 1.0;
	meta->cam_mul[3] = 1.0;

	rawfile = raw_open_file(filename);

	offset = rawfile->first_ifd_offset;
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break;
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break;
		raw_ifd_walker(rawfile, offset, meta);
		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

	raw_close_file(rawfile);
}

GdkPixbuf *
rs_tiff_load_thumb(const gchar *src)
{
	RAWFILE *rawfile;
	guint next, offset;
	gushort ifd_num;
	GdkPixbuf *pixbuf=NULL, *pixbuf2=NULL;
	RS_METADATA meta;
	gchar *thumbname;

	if (ntohs(0x1234) == 0x1234)
		cpuorder = 0x4D4D;
	else
		cpuorder = 0x4949;

	thumbname = rs_thumb_get_name(src);
	if (thumbname)
	{
		if (g_file_test(thumbname, G_FILE_TEST_EXISTS))
		{
			pixbuf = gdk_pixbuf_new_from_file(thumbname, NULL);
			g_free(thumbname);
			if (pixbuf) return(pixbuf);
		}
	}

	meta.thumbnail_start = 0;
	meta.thumbnail_length = 0;

	rawfile = raw_open_file(src);
	offset = rawfile->first_ifd_offset;
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break;
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break;
		raw_ifd_walker(rawfile, offset, &meta);
		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

	if ((meta.thumbnail_start>0) && (meta.thumbnail_length>0))
	{
		GdkPixbufLoader *pl;
		gdouble ratio;

		pl = gdk_pixbuf_loader_new();
		gdk_pixbuf_loader_write(pl, rawfile->map+meta.thumbnail_start, meta.thumbnail_length, NULL);
		pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
		gdk_pixbuf_loader_close(pl, NULL);
		if (pixbuf==NULL) return(NULL);
		if ((gdk_pixbuf_get_width(pixbuf) == 160) && (gdk_pixbuf_get_height(pixbuf)==120))
		{
			pixbuf2 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 160, 106);
			gdk_pixbuf_copy_area(pixbuf, 0, 7, 160, 106, pixbuf2, 0, 0);
			g_object_unref(pixbuf);
			pixbuf = pixbuf2;
		}
		ratio = ((gdouble) gdk_pixbuf_get_width(pixbuf))/((gdouble) gdk_pixbuf_get_height(pixbuf));
		if (ratio>1.0)
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, 128, (gint) (128.0/ratio), GDK_INTERP_BILINEAR);
		else
			pixbuf2 = gdk_pixbuf_scale_simple(pixbuf, (gint) (128.0*ratio), 128, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = pixbuf2;
		switch (meta.orientation)
		{
			/* this is very COUNTER-intuitive - gdk_pixbuf_rotate_simple() is wierd */
			case 6:
				pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
				break;
			case 8:
				pixbuf2 = gdk_pixbuf_rotate_simple(pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
				g_object_unref(pixbuf);
				pixbuf = pixbuf2;
				break;
		}
		if (thumbname)
			gdk_pixbuf_save(pixbuf, thumbname, "png", NULL, NULL);
	}

	raw_close_file(rawfile);

	return(pixbuf);
}
