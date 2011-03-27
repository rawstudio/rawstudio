/*
 * * Copyright (C) 2006-2011 Anders Brander <anders@brander.dk>, 
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

#define _XOPEN_SOURCE 500 /* strptime() and realpath() */
#include <rawstudio.h>
#include <config.h>
#include <glib.h>
#include <glib/gstdio.h>
#ifdef WIN32
#include <pthread.h> /* MinGW WIN32 gmtime_r() */
#endif
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "conf_interface.h"

#define DOTDIR ".rawstudio"

/**
 * A version of atof() that isn't locale specific
 * @note This doesn't do any error checking!
 * @param str A NULL terminated string representing a number
 * @return The number represented by str or 0.0 if str is NULL
 */
gdouble
rs_atof(const gchar *str)
{
	gdouble result = 0.0f;
	gdouble div = 1.0f;
	gboolean point_passed = FALSE;

	gchar *ptr = (gchar *) str;

	while(str && *ptr)
	{
		if (g_ascii_isdigit(*ptr))
		{
			result = result * 10.0f + g_ascii_digit_value(*ptr);
			if (point_passed)
				div *= 10.0f;
		}
		else if (*ptr == '-')
			div *= -1.0f;
		else if (g_ascii_ispunct(*ptr))
			point_passed = TRUE;
		ptr++;
	}

	return result / div;
}

/**
 * A convenience function to convert an EXIF timestamp to a unix timestamp.
 * @note This will only work until 2038 unless glib fixes its GTime
 * @param str A NULL terminated string containing a timestamp in the format "YYYY:MM:DD HH:MM:SS" (EXIF 2.2 section 4.6.4)
 * @return A unix timestamp or -1 on error
 */
GTime
rs_exiftime_to_unixtime(const gchar *str)
{
	struct tm *tm = g_new0(struct tm, 1);
	GTime timestamp = -1;
#ifndef WIN32 /* There is no strptime() in time.h in MinGW */
	if (strptime(str, "%Y:%m:%d %H:%M:%S", tm))
		timestamp = (GTime) mktime(tm);
#endif

	g_free(tm);

	return timestamp;
}

/**
 * A convenience function to convert an unix timestamp to an EXIF timestamp.
 * @note This will only work until 2038 unless glib fixes its GTime
 * @param timestamp A unix timestamp
 * @return A string formatted as specified in EXIF 2.2 section 4.6.4
 */
gchar *
rs_unixtime_to_exiftime(GTime timestamp)
{
	struct tm *tm = g_new0(struct tm, 1);
	time_t tt = (time_t) timestamp;
	gchar *result = g_new0(gchar, 20);

	gmtime_r(&tt, tm);

	if (strftime(result, 20, "%Y:%m:%d %H:%M:%S", tm) != 19)
	{
		g_free(result);
		result = NULL;
	}

	g_free(tm);

	return result;
}

/**
 * Constrains a box to fill a bounding box without changing aspect
 * @param target_width The width of the bounding box
 * @param target_height The height of the bounding box
 * @param width The input and output width
 * @param height The input and output height
 */
void
rs_constrain_to_bounding_box(gint target_width, gint target_height, gint *width, gint *height)
{
	gdouble target_aspect = ((gdouble)target_width) / ((gdouble)target_height);
	gdouble input_aspect = ((gdouble)*width) / ((gdouble)*height);
	gdouble scale;

	if (target_aspect < input_aspect)
		scale = ((gdouble) *width) / ((gdouble) target_width);
	else
		scale = ((gdouble) *height) / ((gdouble) target_height);

	*width = MIN((gint) ((gdouble)*width) / scale, target_width);
	*height = MIN((gint) ((gdouble)*height) / scale, target_height);
}

/**
 * Try to count the number of processor cores in a system.
 * @note This currently only works for systems with /proc/cpuinfo
 * @return The numver of cores or 1 if the system is unsupported
 */
gint
rs_get_number_of_processor_cores(void)
{
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	/* We assume processors will not be added/removed during our lifetime */
	static gint num = 0;

	if (num)
		return num;

	g_static_mutex_lock (&lock);
	if (num == 0)
	{
		/* Use a temporary for thread safety */
		gint temp_num = 0;
#if defined(_SC_NPROCESSORS_ONLN)
		/* Use the POSIX way of getting the number of processors */
		temp_num = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined (__linux__) && (defined (__i386__) || defined (__x86_64__))
		/* Parse the /proc/cpuinfo exposed by Linux i386/amd64 kernels */
		GIOChannel *io;
		gchar *line;

		io = g_io_channel_new_file("/proc/cpuinfo", "r", NULL);
		if (io)
		{
			/* Count the "processor"-lines, there should be one for each processor/core */
			while (G_IO_STATUS_NORMAL == g_io_channel_read_line(io, &line, NULL, NULL, NULL))
				if (line)
				{
					if (g_str_has_prefix(line, "processor"))
						temp_num++;
					g_free(line);
				}
			g_io_channel_shutdown(io, FALSE, NULL);
			g_io_channel_unref(io);
		}
#elif defined(_WIN32)
		/* Use pthread on windows */
		temp_num = pthread_num_processors_np();
#endif
		/* Be sure we have at least 1 processor and as sanity check, clamp to no more than 127 */
		temp_num = (temp_num <= 0) ? 1 : MIN(temp_num, 127);
		RS_DEBUG(PERFORMANCE, "Detected %d CPU cores.", temp_num);
		num = temp_num;
	}
	g_static_mutex_unlock (&lock);

	return num;
}

#if defined (__i386__) || defined (__x86_64__)

#define xgetbv(index,eax,edx)                                   \
   __asm__ (".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c" (index))

/**
 * Detect cpu features
 * @return A bitmask of @RSCpuFlags
 */
guint
rs_detect_cpu_features(void)
{
#define cpuid(cmd, eax, ecx, edx) \
  do { \
     eax = edx = 0;	\
     asm ( \
       "push %%"REG_b"\n\t"\
       "cpuid\n\t" \
       "pop %%"REG_b"\n\t" \
       : "=a" (eax), "=c" (ecx),  "=d" (edx) \
       : "0" (cmd) \
     ); \
} while(0)
	guint eax;
	guint edx;
	guint ecx;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;
	static guint stored_cpuflags = -1;

	if (stored_cpuflags != -1)
		return stored_cpuflags;

	g_static_mutex_lock(&lock);
	if (stored_cpuflags == -1)
	{
		guint cpuflags = 0;
		/* Test cpuid presence comparing eflags */
		asm (
			"push %%"REG_b"\n\t"
			"pushf\n\t"
			"pop %%"REG_a"\n\t"
			"mov %%"REG_a", %%"REG_b"\n\t"
			"xor $0x00200000, %%"REG_a"\n\t"
			"push %%"REG_a"\n\t"
			"popf\n\t"
			"pushf\n\t"
			"pop %%"REG_a"\n\t"
			"cmp %%"REG_a", %%"REG_b"\n\t"
			"je notfound\n\t"
			"mov $1, %0\n\t"
			"notfound:\n\t"
			"pop %%"REG_b"\n\t"
			: "=r" (eax)
			:
			: REG_a

			);

		if (eax)
		{
			guint std_dsc;
			guint ext_dsc;

			/* Get the standard level */
			cpuid(0x00000000, std_dsc, ecx, edx);

			if (std_dsc)
			{
				/* Request for standard features */
				cpuid(0x00000001, std_dsc, ecx, edx);

				if (edx & 0x00800000)
					cpuflags |= RS_CPU_FLAG_MMX;
				if (edx & 0x02000000)
					cpuflags |= RS_CPU_FLAG_SSE;
				if (edx & 0x04000000)
					cpuflags |= RS_CPU_FLAG_SSE2;
				if (edx & 0x00008000)
					cpuflags |= RS_CPU_FLAG_CMOV;

				if (ecx & 0x00000001)
					cpuflags |= RS_CPU_FLAG_SSE3;
				if (ecx & 0x00000200)
					cpuflags |= RS_CPU_FLAG_SSSE3;
				if (ecx & 0x00080000)
					cpuflags |= RS_CPU_FLAG_SSE4_1;
				if (ecx & 0x00100000)
					cpuflags |= RS_CPU_FLAG_SSE4_2;
				if ((ecx & 0x18000000) == 0x18000000)
				{
					xgetbv(0, eax, edx);
						if ((eax & 0x6) == 0x6)
							cpuflags |= RS_CPU_FLAG_AVX;
				}
			}

			/* Is there extensions */
			cpuid(0x80000000, ext_dsc, ecx, edx);

			if (ext_dsc)
			{
				/* Request for extensions */
				cpuid(0x80000001, eax, ecx, edx);

				if (edx & 0x80000000)
					cpuflags |= RS_CPU_FLAG_3DNOW;
				if (edx & 0x40000000)
					cpuflags |= RS_CPU_FLAG_3DNOW_EXT;
				if (edx & 0x00400000)
					cpuflags |= RS_CPU_FLAG_AMD_ISSE;
			}

			/* ISSE is also implied in SSE */
			if (cpuflags & RS_CPU_FLAG_SSE)
				cpuflags |= RS_CPU_FLAG_AMD_ISSE;
		}
		stored_cpuflags = cpuflags;
	}
	g_static_mutex_unlock(&lock);

#define report(a, x) RS_DEBUG(PERFORMANCE, "CPU Feature: "a" = %d", !!(stored_cpuflags&x));
	report("MMX",RS_CPU_FLAG_MMX);
	report("SSE",RS_CPU_FLAG_SSE);
	report("CMOV",RS_CPU_FLAG_CMOV);
	report("3DNOW",RS_CPU_FLAG_3DNOW);
	report("3DNOW_EXT",RS_CPU_FLAG_3DNOW_EXT);
	report("Integer SSE",RS_CPU_FLAG_AMD_ISSE);
	report("SSE2",RS_CPU_FLAG_SSE2);
	report("SSE3",RS_CPU_FLAG_SSE3);
	report("SSSE3",RS_CPU_FLAG_SSSE3);
	report("SSE4.1",RS_CPU_FLAG_SSE4_1);
	report("SSE4.2",RS_CPU_FLAG_SSE4_2);
	report("AVX",RS_CPU_FLAG_AVX);
#undef report

	return(stored_cpuflags);
#undef cpuid
}

#else
guint
rs_detect_cpu_features()
{
	return 0;
}
#endif /* __i386__ || __x86_64__ */

/**
 * Return a path to the current config directory for Rawstudio - this is the
 * .rawstudio direcotry in home
 * @return A path to an existing directory
 */
const gchar *
rs_confdir_get(void)
{
	static gchar *dir = NULL;
	static GStaticMutex lock = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&lock);
	if (!dir)
	{
		const gchar *home = g_get_home_dir();
		dir = g_build_filename(home, ".rawstudio", NULL);
	}

	g_mkdir_with_parents(dir, 00755);
	g_static_mutex_unlock(&lock);

	return dir;
}

/**
 * Return a cache directory for filename
 * @param filename A complete path to a photo
 * @return A directory to hold the cache. This is guarenteed to exist
 */
gchar *
rs_dotdir_get(const gchar *filename)
{
	gchar *ret = NULL;
	gchar *directory;
	GString *dotdir;
	gboolean dotdir_is_local = FALSE;

	rs_conf_get_boolean(CONF_CACHEDIR_IS_LOCAL, &dotdir_is_local);

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		directory = g_strdup(filename);
	else
		directory = g_path_get_dirname(filename);
	
	if (dotdir_is_local)
	{
		dotdir = g_string_new(g_get_home_dir());
		dotdir = g_string_append(dotdir, G_DIR_SEPARATOR_S);
		dotdir = g_string_append(dotdir, DOTDIR);
		dotdir = g_string_append(dotdir, G_DIR_SEPARATOR_S);
		dotdir = g_string_append(dotdir, directory);
	}
	else
	{
		dotdir = g_string_new(directory);
		dotdir = g_string_append(dotdir, G_DIR_SEPARATOR_S);
		dotdir = g_string_append(dotdir, DOTDIR);
	}

	if (!g_file_test(dotdir->str, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
	{
		if (g_mkdir_with_parents(dotdir->str, 0700) != 0)
			ret = NULL;
		else
			ret = dotdir->str;
	}
	else if (g_file_test(dotdir->str, G_FILE_TEST_IS_DIR))
		ret = dotdir->str;
	else 
		ret = NULL;

	/* If we for some reason cannot write to the current directory, */
	/* we save it to a new folder named as the md5 of the file content, */
	/* not particularly fast, but ensures that the images can be moved */
	if (ret == NULL)
	{
		g_string_free(dotdir, TRUE);
		g_free(directory);
		if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		{
			gchar* md5 = rs_file_checksum(filename);
			ret = g_strdup_printf("%s/read-only-cache/%s", rs_confdir_get(), md5);
			g_free(md5);
			if (!g_file_test(ret, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
			{
				if (g_mkdir_with_parents(ret, 0700) != 0)
					ret = NULL;
			}
		}
		return ret;
	}
	g_free(directory);
	g_string_free(dotdir, FALSE);
	return (ret);
}

/**
 * Normalize a RS_RECT, ie makes sure that x1 < x2 and y1<y2
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 */
void
rs_rect_normalize(RS_RECT *in, RS_RECT *out)
{
	gint n;
	gint x1,y1;
	gint x2,y2;

	x1 = in->x2;
	x2 = in->x1;
	y1 = in->y1;
	y2 = in->y2;

	if (x1>x2)
	{
		n = x1;
		x1 = x2;
		x2 = n;
	}
	if (y1>y2)
	{
		n = y1;
		y1 = y2;
		y2 = n;
	}

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
}

/**
 * Flip a RS_RECT
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 * @param w The width of the data OUTSIDE the RS_RECT
 * @param h The height of the data OUTSIDE the RS_RECT
 */
void
rs_rect_flip(RS_RECT *in, RS_RECT *out, gint w, gint h)
{
	gint x1,y1;
	gint x2,y2;

	x1 = in->x1;
	x2 = in->x2;
	y1 = h - in->y2 - 1;
	y2 = h - in->y1 - 1;

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
	rs_rect_normalize(out, out);
}

/**
 * Mirrors a RS_RECT
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 * @param w The width of the data OUTSIDE the RS_RECT
 * @param h The height of the data OUTSIDE the RS_RECT
 */
void
rs_rect_mirror(RS_RECT *in, RS_RECT *out, gint w, gint h)
{
	gint x1,y1;
	gint x2,y2;

	x1 = w - in->x2 - 1;
	x2 = w - in->x1 - 1;
	y1 = in->y1;
	y2 = in->y2;

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
	rs_rect_normalize(out, out);
}

/**
 * Rotate a RS_RECT in 90 degrees steps
 * @param in A RS_RECT to read values from
 * @param out A RS_RECT to write the values to (can be the same as in)
 * @param w The width of the data OUTSIDE the RS_RECT
 * @param h The height of the data OUTSIDE the RS_RECT
 * @param quarterturns How many times to turn the rect clockwise
 */
void
rs_rect_rotate(RS_RECT *in, RS_RECT *out, gint w, gint h, gint quarterturns)
{
	gint x1,y1;
	gint x2,y2;

	x1 = in->x2;
	x2 = in->x1;
	y1 = in->y1;
	y2 = in->y2;

	switch(quarterturns)
	{
		case 1:
			x1 = h - in->y1-1;
			x2 = h - in->y2-1;
			y1 = in->x1;
			y2 = in->x2;
			break;
		case 2:
			x1 = w - in->x1 - 1;
			x2 = w - in->x2 - 1;
			y1 = h - in->y1 - 1;
			y2 = h - in->y2 - 1;
			break;
		case 3:
			x1 = in->y1;
			x2 = in->y2;
			y1 = w - in->x1 - 1;
			y2 = w - in->x2 - 1;
			break;
	}

	out->x1 = x1;
	out->x2 = x2;
	out->y1 = y1;
	out->y2 = y2;
	rs_rect_normalize(out, out);
}

/**
 * Reset a property on a GObject to it's default
 * @param object A GObject
 * @param property_name A name of a property installed in object's class
 */
void
rs_object_class_property_reset(GObject *object, const gchar *property_name)
{
	GObjectClass *klass = G_OBJECT_GET_CLASS(object);
	GParamSpec *spec;
	GValue value = {0};

	spec = g_object_class_find_property(klass, property_name);
	g_assert(spec != NULL);

	g_value_init(&value, spec->value_type);

	g_param_value_set_default(spec, &value);
	g_object_set_property(object, spec->name, &value);

	g_value_unset(&value);
}

/**
 * Check (and complain if needed) the Rawstudio install
 */
void
check_install(void)
{
#define TEST_FILE_ACCESS(path) do { if (g_access(path, R_OK)!=0) g_debug("Cannot access %s\n", path);} while (0)
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "icons" G_DIR_SEPARATOR_S PACKAGE ".png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "overlay_priority1.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "overlay_priority2.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "overlay_priority3.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "overlay_deleted.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "overlay_exported.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "transform_flip.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "transform_mirror.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "transform_90.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "transform_180.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "transform_270.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "cursor-color-picker.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "cursor-crop.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "cursor-rotate.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "tool-color-picker.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "tool-crop.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "tool-rotate.png");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "ui.xml");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "rawstudio.gtkrc");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "profiles" G_DIR_SEPARATOR_S "generic_camera_profile.icc");
	TEST_FILE_ACCESS(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "profiles" G_DIR_SEPARATOR_S "sRGB.icc");
#undef TEST_FILE_ACCESS
}

/* Rewritten from Exiftools - lib/Image/ExifTool/Canon.pm*/
gfloat
CanonEv(gint val)
{
	gfloat sign;
	gfloat frac;

	/* temporarily make the number positive */
	if (val < 0)
	{
		val = -val;
		sign = -1.0;
	}
	else
	{
		sign = 1.0;
	}

	gint ifrac = val & 0x1f;

	/* remove fraction */
	val -= ifrac;

	/* Convert 1/3 and 2/3 codes */
	if (ifrac == 0x0c)
		frac = 32.0 / 3.0; /* 0x20 / 3 */
	else if (ifrac == 0x14)
		frac = 64.0 / 3.0; /* 0x40 / 3 */
	else
		frac = (gfloat) ifrac;

	return sign * (((gfloat)val) + frac) / 32.0;
}

/**
 * Split a char * with a given delimiter
 * @param str The gchar * to be splitted
 * @param delimiters The gchar * to be used as delimiter (can be more than 1 char)
 * @return A GList consisting of the different parts of the input string, must be freed using g_free() and g_list_free().
 */
GList *
rs_split_string(const gchar *str, const gchar *delimiters) {
	gchar **temp = g_strsplit_set(str, delimiters, 0);

	int i = 0;
	GList *glist = NULL;
	while (temp[i])
	{
		gchar* text = (gchar *) temp[i];
		if (text[0] != 0)
			glist = g_list_append(glist, text);
		else
			g_free(text);
		i++;
	}
	g_free(temp);
	return glist;
}

gchar *
rs_file_checksum(const gchar *filename)
{
	gchar *checksum = NULL;
	struct stat st;
	gint fd = open(filename, O_RDONLY);

	if (fd > 0)
	{
		fstat(fd, &st);

		gint offset = 0;
		gint length = st.st_size;

		/* If the file is bigger than 2 KiB, we sample 1 KiB in the middle of the file */
		if (st.st_size > 2048)
		{
			offset = st.st_size/2;
			length = 1024;
		}

		guchar buffer[length];

		lseek(fd, offset, SEEK_SET);
		gint bytes_read = read(fd, buffer, length);

		close(fd);

		if (bytes_read == length)
			checksum = g_compute_checksum_for_data(G_CHECKSUM_MD5, buffer, length);
	}

	return checksum;
}

const gchar *
rs_human_aperture(gdouble aperture)
{
	gchar *ret = NULL;

	if (aperture < 8)
		ret = g_strdup_printf("f/%.1f", aperture);
	else
		ret = g_strdup_printf("f/%.0f", aperture);

	return ret;
}

const gchar *
rs_human_focal(gdouble min, gdouble max)
{
	gchar *ret = NULL;

	if (min == max)
		ret = g_strdup_printf("%.0fmm", max);
	else
		ret = g_strdup_printf("%.0f-%.0fmm", min, max);
	return ret;
}

gchar *
rs_normalize_path(const gchar *path)
{
#ifdef PATH_MAX
	gint path_max = PATH_MAX;
#else
	gint path_max = pathconf(path, _PC_PATH_MAX);
	if (path_max <= 0)
		path_max = 4096;
#endif
	gchar *buffer = g_new0(gchar, path_max);

	gchar *ret = NULL;
#ifdef WIN32
	int length = GetFullPathName(path, path_max, buffer, NULL);
	if(length == 0){
	  g_error("Error normalizing path: %s\n", path);
	}
	ret = buffer;
#else
	ret = realpath(path, buffer);
#endif

	if (ret == NULL)
		g_free(buffer);

	return ret;
}

/**
 * Copy a file from one location to another
 * @param source An absolute path to a source file
 * @param deastination An absolute path to a destination file (not folder), will be overwritten if exists
 * @return TRUE on success, FALSE on failure
 */
gboolean
rs_file_copy(const gchar *source, const gchar *destination)
{
	gboolean ret = FALSE;
	const gint buffer_size = 1024*1024;
	gint source_fd, destination_fd;
	gint bytes_read, bytes_written;
	struct stat st;
	mode_t default_mode = 00666; /* We set this relaxed to respect the users umask */

	g_return_val_if_fail(source != NULL, FALSE);
	g_return_val_if_fail(source[0] != '\0', FALSE);
	g_return_val_if_fail(g_path_is_absolute(source), FALSE);

	g_return_val_if_fail(destination != NULL, FALSE);
	g_return_val_if_fail(destination[0] != '\0', FALSE);
	g_return_val_if_fail(g_path_is_absolute(destination), FALSE);

	source_fd = open(source, O_RDONLY);
	if (source_fd > 0)
	{
		/* Try to copy permissions too */
		if (fstat(source_fd, &st) == 0)
			default_mode = st.st_mode;
		destination_fd = creat(destination, default_mode);

		if (destination_fd > 0)
		{
			gpointer buffer = g_malloc(buffer_size);
			do {
				bytes_read = read(source_fd, buffer, buffer_size);
				bytes_written = write(destination_fd, buffer, bytes_read);
				if (bytes_written != bytes_read)
					g_warning("%s was truncated", destination);
			} while(bytes_read > 0);
			g_free(buffer);

			ret = TRUE;

			close(destination_fd);
		}
		close(source_fd);
	}

	return ret;
}

/**
 * Removes tailing spaces from a gchar *
 * @param str A gchar * to have tailing spaces removed
 * @param inplace Set to TRUE if string should be edited inplace
 * @return A gchar * with tailing spaces removed
 */
gchar *
rs_remove_tailing_spaces(gchar *str, gboolean inplace)
{
	gint i;
	gchar *ret = str;

	g_return_val_if_fail(str != NULL, NULL);

	if (!inplace)
		ret = g_strdup(str);

	for(i = strlen(ret)-1; i > 0; i--)
	{
		if (ret[i] == 0x20)
			ret[i] = 0x00;
		else
			i = 0;
	}

	return ret;
}
