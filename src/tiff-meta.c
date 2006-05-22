#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include "dcraw_api.h"
#include "matrix.h"
#include "rawstudio.h"
#include "tiff-meta.h"

typedef struct _rawfile {
	guint size;
	void *map;
	gushort byteorder;
	guint first_ifd_offset;
} RAWFILE;

static int cpuorder;

RAWFILE *raw_open_file(const gchar *filename);
gboolean raw_get_uint(RAWFILE *rawfile, gint pos, guint *target);
gboolean raw_get_ushort(RAWFILE *rawfile, gint pos, gushort *target);
gboolean raw_get_float(RAWFILE *rawfile, gint pos, gfloat *target);
void raw_close_file(RAWFILE *rawfile);

gboolean
raw_get_uint(RAWFILE *rawfile, gint pos, guint *target)
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
raw_get_ushort(RAWFILE *rawfile, gint pos, gushort *target)
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
raw_get_float(RAWFILE *rawfile, gint pos, gfloat *target)
{
	if((pos+4)>rawfile->size)
		return(FALSE);

	if (rawfile->byteorder == cpuorder)
		*target = *(gfloat *)(rawfile->map+pos);
	else
		*target = (gfloat) (ENDIANSWAP4(*(gint *)(rawfile->map+pos)));
	return(TRUE);
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
	g_free(rawfile);
	return;
}

gboolean
raw_ifd_walker(RAWFILE *rawfile, guint offset, RS_METADATA *meta)
{
	gushort number_of_entries;
	gushort fieldtag=0;
/*	gushort fieldtype;
	guint valuecount; */
	guint tmp=0;
	gfloat float_temp1=0.0, float_temp2=0.0;

	if(!raw_get_ushort(rawfile, offset, &number_of_entries)) return(FALSE);
	offset += 2;

	while(number_of_entries--)
	{
		raw_get_ushort(rawfile, offset, &fieldtag);
/*		raw_get_ushort(rawfile, offset+2, &fieldtype);
		raw_get_uint(rawfile, offset+4, &valuecount); */
		offset += 8;
		switch(fieldtag)
		{
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
				raw_get_uint(rawfile, offset, &meta->jpeg_start);
				break;
			case 0x0202: /* jpeg length */
				raw_get_uint(rawfile, offset, &meta->jpeg_length);
				break;
			case 0x829D: /* FNumber */
				raw_get_uint(rawfile, offset, &tmp);
				raw_get_float(rawfile, tmp, &float_temp1);
				raw_get_float(rawfile, tmp+4, &float_temp2);
				meta->aperture = pow(2.0,float_temp1/(float_temp2*2.0));
				break;
			case 0x8827: /* ISOSpeedRatings */
				raw_get_ushort(rawfile, offset, &meta->iso);
				break;
			case 0x8769: /* ExifIFDPointer */
				raw_get_uint(rawfile, offset, &tmp);
				raw_ifd_walker(rawfile, tmp, meta);
				break;
			case 0x9201: /* ShutterSpeedValue */
				raw_get_uint(rawfile, offset, &tmp);
				raw_get_float(rawfile, tmp, &float_temp1);
				raw_get_float(rawfile, tmp+4, &float_temp2);
				meta->shutterspeed = 1.0/pow(2.0,-float_temp1/float_temp2);
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

	meta->aperture = 0.0;
	meta->iso = 0;
	meta->shutterspeed = 0.0;
	meta->jpeg_start = 0;
	meta->jpeg_length = 0;
	meta->preview_start = 0;
	meta->preview_length = 0;

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
			return(pixbuf);
		}
	}

	meta.jpeg_start = 0;
	meta.jpeg_length = 0;

	rawfile = raw_open_file(src);
	offset = rawfile->first_ifd_offset;
	do {
		if (!raw_get_ushort(rawfile, offset, &ifd_num)) break;
		if (!raw_get_uint(rawfile, offset+2+ifd_num*12, &next)) break;
		raw_ifd_walker(rawfile, offset, &meta);
		if (offset == next) break; /* avoid infinite loops */
		offset = next;
	} while (next>0);

	if ((meta.jpeg_start>0) && (meta.jpeg_length>0))
	{
		GdkPixbufLoader *pl;
		gdouble ratio;

		pl = gdk_pixbuf_loader_new();
		gdk_pixbuf_loader_write(pl, rawfile->map+meta.jpeg_start, meta.jpeg_length, NULL);
		pixbuf = gdk_pixbuf_loader_get_pixbuf(pl);
		gdk_pixbuf_loader_close(pl, NULL);
		if (pixbuf==NULL) return(NULL);
		if ((gdk_pixbuf_get_width(pixbuf) == 160) && (gdk_pixbuf_get_height(pixbuf)==120))
		{
			pixbuf2 = gdk_pixbuf_new_subpixbuf(pixbuf, 0, 7, 160, 106);
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
