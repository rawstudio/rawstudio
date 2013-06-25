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

#include <rawstudio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <config.h>
#include <gettext.h>
#include "rs-enfuse.h"
#include <stdlib.h>
#include <fcntl.h>
#include "filename.h"
#include <rs-store.h>
#include "gtk-helper.h"
#include "rs-photo.h"
#include "rs-cache.h"
#include "gtk-progress.h"
#include "rs-metadata.h"
#include "conf_interface.h"

#define ENFUSE_OPTIONS "-d 8"
#define ENFUSE_OPTIONS_QUICK "-d 8"

gboolean has_align_image_stack ();
gboolean has_enfuse_mp ();

gint calculate_lightness(RSFilter *filter)
{
      RSFilterRequest *request = rs_filter_request_new();
      rs_filter_request_set_quick(RS_FILTER_REQUEST(request), TRUE);
      rs_filter_param_set_object(RS_FILTER_PARAM(request), "colorspace", rs_color_space_new_singleton("RSSrgb"));
      rs_filter_request_set_quick(RS_FILTER_REQUEST(request), TRUE);

      rs_filter_set_recursive(filter,
			      "bounding-box", TRUE,
			      "width", 256,
			      "height", 256,
			      NULL);

      RSFilterResponse *response = rs_filter_get_image8(filter, request);
      g_object_unref(request);

      if(!rs_filter_response_has_image8(response))
	return 127;

      GdkPixbuf *pixbuf = rs_filter_response_get_image8(response);
      g_object_unref(response);

      guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
      gint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
      gint height = gdk_pixbuf_get_height(pixbuf);
      gint width = gdk_pixbuf_get_width(pixbuf);
      gint channels = gdk_pixbuf_get_n_channels(pixbuf);

      gint x,y,c;
      gulong sum = 0;
      gint num = 0;

      for (y = 0; y < height; y++)
        {
	  for (x = 0; x < width; x++)
	    {
	      for (c = 0; c < channels; c++)
		{
		  sum += pixels[x*c+y*rowstride];
		}
	    }
	}

      g_object_unref(pixbuf);

      num = width*height*channels;
      return (gint) (sum/num);
}

gint export_image(gchar *filename, GHashTable *cache, RSOutput *output, RSFilter *filter, gint snapshot, double exposure, gchar *outputname, gint boundingbox, RSFilter *resample) {
  RS_PHOTO *photo = NULL;

  if (cache) 
    {
      photo = (RS_PHOTO *) g_hash_table_lookup(cache, filename);
      if (!photo)
	{
	  photo = rs_photo_load_from_file(filename);
	  g_hash_table_insert(cache, filename, photo);
	  printf("Adding %s to cache\n", filename);
	}
    }
  else
    photo = rs_photo_load_from_file(filename);

  if (photo)
    {
      rs_metadata_load_from_file(photo->metadata, filename);
      rs_cache_load(photo);

      GList *filters = g_list_append(NULL, filter);
      rs_photo_set_exposure(photo, 0, exposure);
      rs_photo_apply_to_filters(photo, filters, snapshot);
      
      if (boundingbox > 0) 
	{
	  rs_filter_set_enabled(resample, TRUE);
	  rs_filter_set_recursive(filter,
				  "image", photo->input_response,
				  "filename", photo->filename,
				  "bounding-box", TRUE,
				  "width", boundingbox,
				  "height", boundingbox,
				  NULL);
	}
      else
	{
	  rs_filter_set_enabled(resample, FALSE);
	  rs_filter_set_recursive(filter,
				  "image", photo->input_response,
				  "filename", photo->filename,
				  NULL);
	}

      if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "filename"))
	g_object_set(output, "filename", outputname, NULL);

      rs_output_set_from_conf(output, "batch");
      rs_output_execute(output, filter);

      gint value = calculate_lightness(filter);
      printf("%s: %d\n", filename, value);
      g_list_free(filters);
      return value;
    }
  else
    return -1;
}

GList * export_images(RS_BLOB *rs, GList *files, gboolean extend, gint dark, gfloat darkstep, gint bright, gfloat brightstep, gint boundingbox, gboolean quick)
{
  gint num_selected = g_list_length(files);
  gint i = 0;
  gchar *name;

  GString *output_str = g_string_new(g_get_tmp_dir());
  output_str = g_string_append(output_str, "/");
  output_str = g_string_append(output_str, ".rawstudio-enfuse-");
  GString *output_unique = NULL;

  /* a simple chain - we wanna use the "original" image with only white balance corrected and nothing else to get the best result */
  RSFilter *ftransform_input = rs_filter_new("RSColorspaceTransform", rs->filter_demosaic_cache);
  RSFilter *fdcp= rs_filter_new("RSDcp", ftransform_input);
  RSFilter *fresample= rs_filter_new("RSResample", fdcp);
  RSFilter *ftransform_display = rs_filter_new("RSColorspaceTransform", fresample);
  RSFilter *fend = ftransform_display;
  
  RSOutput *output = rs_output_new("RSPngfile");

  GList *exported_names = NULL;

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "save16bit"))
    g_object_set(output, "save16bit", !quick, NULL); /* We get odd results if we use 16 bit output - probably due to liniearity */
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "copy-metadata"))
    g_object_set(output, "copy-metadata", TRUE, NULL); /* Doesn't make sense to enable - Enfuse doesn't copy it */
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(output), "quick"))
    g_object_set(output, "quick", quick, NULL); /* Allow for quick exports when generating thumbnails */

  gint lightness = 0;
  gint darkval = 255;
  gint brightval = 0;
  gchar *darkest = NULL;
  gchar *brightest = NULL;

  num_selected = g_list_length(files);
  if (g_list_length(files))
    {
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  output_unique = g_string_append(output_unique, ".png");
	  lightness = export_image(name, rs->enfuse_cache, output, fend, 0, 0.0, output_unique->str, boundingbox, fresample); /* FIXME: snapshot hardcoded */
  	  exported_names = g_list_append(exported_names, g_strdup(output_unique->str));
	  g_string_free(output_unique, TRUE);

	  if (lightness > brightval)
	    {
	      brightval = lightness;
	      brightest = g_strdup(name);
	    }

	  if (lightness < darkval)
	    {
	      darkval = lightness;
	      darkest = g_strdup(name);
	    }
	}
    }

  if (extend)
    {
      gint n;
      for (n = 1; n <= dark; n++)
	{
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  g_string_append_printf(output_unique, "_%.1f", (darkstep*n*-1));
	  output_unique = g_string_append(output_unique, ".png");
	  exported_names = g_list_append(exported_names, g_strdup(output_unique->str));
	  export_image(darkest, rs->enfuse_cache, output, fend, 0, (darkstep*n*-1), output_unique->str, boundingbox, fresample); /* FIXME: snapshot hardcoded */
	  g_string_free(output_unique, TRUE);
	  i++;
	}
      g_free(darkest);
      for (n = 1; n <= bright; n++)
	{
	  output_unique = g_string_new(output_str->str);
	  g_string_append_printf(output_unique, "%d", i);
	  g_string_append_printf(output_unique, "_%.1f", (brightstep*n));
	  output_unique = g_string_append(output_unique, ".png");
	  exported_names = g_list_append(exported_names, g_strdup(output_unique->str));
	  export_image(brightest, rs->enfuse_cache, output, fend, 0, (brightstep*n), output_unique->str, boundingbox, fresample); /* FIXME: snapshot hardcoded */
	  g_string_free(output_unique, TRUE);
	  i++;
	}
      g_free(brightest);
    }

  /* FIXME: shouldn't 'files' be freed here? It breaks RSStore... */

  return exported_names;
}

GList * align_images (GList *files, gchar *options) {
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name;
  GList *aligned_names = NULL;

  if (g_list_length(files))
    {
      GString *command = g_string_new("align_image_stack -a /tmp/.rawstudio-enfuse-aligned- ");
      if (options)
         command = g_string_append(command, options);
      command = g_string_append(command, " ");
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  command = g_string_append(command, name);
	  command = g_string_append(command, " ");
	  aligned_names = g_list_append(aligned_names, g_strdup_printf ("/tmp/.rawstudio-enfuse-aligned-%04d.tif", i));
	}
      printf("command: %s\n", command->str);
      if (system(command->str));
      g_string_free(command, TRUE);
    }
  return aligned_names;
}

void enfuse_images(GList *files, gchar *out, gchar *options, gboolean has_enfuse_mp) {
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name;

  if (g_list_length(files))
    {
      GString *command = NULL;
      if (has_enfuse_mp)
	command = g_string_new("enfuse-mp ");
      else
	command = g_string_new("enfuse");
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  command = g_string_append(command, name);
	  command = g_string_append(command, " ");
	}
      command = g_string_append(command, options);
      command = g_string_append(command, " -o ");
      command = g_string_append(command, out);
      printf("command: %s\n", command->str);
      if(system(command->str));
      g_string_free(command, TRUE);
    }
}

void
delete_files_in_list(GList *list)
{
  gint i;
  gchar *name = NULL;
  for(i=0; i<g_list_length(list); i++)
    {
      name = (gchar*) g_list_nth_data(list, i);
      g_unlink(name);
    }
}

gchar * rs_enfuse(RS_BLOB *rs, GList *files, gboolean quick, gint boundingbox)
{
  gint num_selected = g_list_length(files);
  gint i;
  gchar *name = NULL;
  gchar *file = NULL;
  GString *outname = g_string_new("");
  GString *fullpath = NULL;
  gchar *align_options = NULL;
  GString *enfuse_options = g_string_new("");
  gdouble extend_negative = 0.0;
  gdouble extend_positive = 0.0;
  gdouble extend_step = 0.0;

  GTimer *timer = g_timer_new();
  g_timer_start(timer);

  if (!rs_conf_get_double(
			  (num_selected == 1) ? CONF_ENFUSE_EXTEND_NEGATIVE_SINGLE : CONF_ENFUSE_EXTEND_NEGATIVE_MULTI,
			  &extend_negative))
    extend_negative = (num_selected == 1) ? DEFAULT_CONF_ENFUSE_EXTEND_NEGATIVE_SINGLE : DEFAULT_CONF_ENFUSE_EXTEND_NEGATIVE_MULTI;

  if (!rs_conf_get_double(
			  (num_selected == 1) ? CONF_ENFUSE_EXTEND_POSITIVE_SINGLE : CONF_ENFUSE_EXTEND_POSITIVE_MULTI,
			  &extend_positive))
    extend_positive = (num_selected == 1) ? DEFAULT_CONF_ENFUSE_EXTEND_POSITIVE_SINGLE : DEFAULT_CONF_ENFUSE_EXTEND_POSITIVE_MULTI;

  if (!rs_conf_get_double(
			  (num_selected == 1) ? CONF_ENFUSE_EXTEND_STEP_SINGLE : CONF_ENFUSE_EXTEND_STEP_MULTI,
			  &extend_step))
    extend_step = (num_selected == 1) ? DEFAULT_CONF_ENFUSE_EXTEND_STEP_SINGLE : DEFAULT_CONF_ENFUSE_EXTEND_STEP_MULTI;

  gboolean align = DEFAULT_CONF_ENFUSE_ALIGN_IMAGES;
  rs_conf_get_boolean_with_default(CONF_ENFUSE_ALIGN_IMAGES, &align, DEFAULT_CONF_ENFUSE_ALIGN_IMAGES);

  gboolean extend = DEFAULT_CONF_ENFUSE_EXTEND;
  rs_conf_get_boolean_with_default(CONF_ENFUSE_EXTEND, &extend, DEFAULT_CONF_ENFUSE_EXTEND);

  gint method = 0;
  if (!rs_conf_get_integer(CONF_ENFUSE_METHOD, &method))
    method = DEFAULT_CONF_ENFUSE_METHOD;

  if (boundingbox == -1) 
    {
      if (!rs_conf_get_integer(CONF_ENFUSE_SIZE, &boundingbox))
	boundingbox = DEFAULT_CONF_ENFUSE_SIZE;
    }

  gchar *method_options = NULL;
  if (method == ENFUSE_METHOD_EXPOSURE_BLENDING_ID) {
    method_options = g_strdup(ENFUSE_OPTIONS_EXPOSURE_BLENDING);
  } else if (method == ENFUSE_METHOD_FOCUS_STACKING_ID) {
    method_options = g_strdup(ENFUSE_OPTIONS_FOCUS_STACKING);
    extend = FALSE;
  }

  if (quick)
    enfuse_options = g_string_append(enfuse_options, ENFUSE_OPTIONS_QUICK);
  else
    enfuse_options = g_string_append(enfuse_options, ENFUSE_OPTIONS);
  enfuse_options = g_string_append(enfuse_options, " ");
  enfuse_options = g_string_append(enfuse_options, method_options);

  gchar *first = NULL;
  gchar *parsed_filename = NULL;
  gchar *temp_filename = g_strdup("/tmp/.rawstudio-temp.png");

  RS_PROGRESS *progress = NULL;
  if (quick == FALSE)
    {
      progress = gui_progress_new("Enfusing...", 5);
      GUI_CATCHUP();
    }

  /* if only one picture is selected we have to do extending, it doesn't make sense to run enfuse on a single photo otherwise... */
  if (num_selected == 1)
      extend = TRUE;

  if (g_list_length(files))
    {
      for(i=0; i<num_selected; i++)
	{
	  name = (gchar*) g_list_nth_data(files, i);
	  if (first == NULL)
	    first = g_strdup(name);
	  file = g_malloc(sizeof(char)*strlen(name));
	  sscanf(g_path_get_basename(name), "%[^.]", file);
	  outname = g_string_append(outname, file);
	  g_free(file);
	  if (i+1 != num_selected)
	    outname = g_string_append(outname, "+");
	}
      fullpath = g_string_new(g_path_get_dirname(name));
      fullpath = g_string_append(fullpath, "/");
      fullpath = g_string_append(fullpath, outname->str);
      fullpath = g_string_append(fullpath, "_%2c");
      fullpath = g_string_append(fullpath, ".rse");
      parsed_filename = filename_parse(fullpath->str, g_strdup(first), 0, FALSE);
      g_string_free(outname, TRUE);
      g_string_free(fullpath, TRUE);
    }

  if (quick == FALSE)
    {
      g_usleep(500000); /* FIXME */
      gui_progress_advance_one(progress); /* 1 - initiate */
    }

  GList *exported_names = export_images(rs, files, extend, extend_negative, extend_step, extend_positive, extend_step, boundingbox, quick);

  if (quick == FALSE)
    gui_progress_advance_one(progress); /* 2 - after exported images */

  GList *aligned_names = NULL;
  if (has_align_image_stack() && num_selected > 1 && quick == FALSE && align == TRUE)
    {
      aligned_names = align_images(exported_names, align_options);
      g_free(align_options);
    }
  else
      aligned_names = exported_names;

  if (quick == FALSE)
    gui_progress_advance_one(progress); /* 3 - after aligned images */

  enfuse_images(aligned_names, temp_filename, enfuse_options->str, has_enfuse_mp());
  g_string_free(enfuse_options, TRUE);

  if (quick == FALSE)
    gui_progress_advance_one(progress); /* 4 - after enfusing */

  /* delete all temporary files */
  if (exported_names != aligned_names) {
      delete_files_in_list(aligned_names);
      g_list_free(aligned_names);
  }
  delete_files_in_list(exported_names);
  g_list_free(exported_names);

  /* FIXME: should use the photo in the middle as it's averaged between it... */
  rs_exif_copy(first, temp_filename, "sRGB", RS_EXIF_FILE_TYPE_PNG);
  if (first)
    g_free(first);

  GString *mv = g_string_new("mv ");
  mv = g_string_append(mv, temp_filename);
  mv = g_string_append(mv, " ");
  mv = g_string_append(mv, parsed_filename);
  printf("command: %s\n", mv->str);
  if(system(mv->str)); 
  g_string_free(mv, TRUE);

  if (quick == FALSE)
    {
      gui_progress_advance_one(progress); /* 5 - misc file operations */
      gui_progress_free(progress);
    }

  g_timer_stop(timer);
  printf("Total execution time: %.2f\n", g_timer_elapsed(timer, NULL));

  return parsed_filename;
}

gboolean has_enfuse_mp()
{
  if (popen("enfuse-mp", "r"))
    return TRUE;
  else
    return FALSE;
}

gboolean rs_has_enfuse (gint major, gint minor)
{
  FILE *fp;
  char line1[128];
  char line2[128];
  int _major = 0, _minor = 0;
  gboolean retval = FALSE;

  fp = popen("enfuse -V","r"); /* enfuse 4.0-753b534c819d */
  if (fgets(line1, sizeof line1, fp) == NULL)
    {
      g_warning("fgets returned: %d\n", retval);
      return FALSE;
    }
  pclose(fp);

  fp = popen("enfuse -h","r"); /* ==== enfuse, version 3.2 ==== */
  if (fgets(line2, sizeof line2, fp) == NULL)
    {
      g_warning("fgets returned: %d\n", retval);
      return FALSE;
    }
  pclose(fp);

  GRegex *regex;
  gchar **tokens;

  regex = g_regex_new("(enfuse|.* enfuse, version) ([0-9])\x2E([0-9]+).*", 0, 0, NULL);
  tokens = g_regex_split(regex, line1, 0);
  if (tokens)
    {
      g_regex_unref(regex);
    }
  else 
    {
      tokens = g_regex_split(regex, line2, 0);
      g_regex_unref(regex);
      if (!tokens)
	return FALSE;
    }

  _major = atoi(tokens[2]);
  _minor = atoi(tokens[3]);

  if (_major > major) {
    retval = TRUE;
  } else if (_major == major) {
    if (_minor >= minor) {
      retval = TRUE;
    }
  }

  g_free(tokens);

  return retval;
}

gboolean has_align_image_stack ()
{
  FILE *fp;
  char line[128];
  gboolean retval = FALSE;

  fp = popen("align_image_stack 2>&1","r");
  if (fgets(line, sizeof line, fp) == NULL)
    {
      g_warning("fgets returned: %d\n", retval);
      return FALSE;
    }
  pclose(fp);
  return TRUE;
}
