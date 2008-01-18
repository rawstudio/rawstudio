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

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "rawstudio.h"
#include "filename.h"
#include "conf_interface.h"
#include "gettext.h"
#include "gtk-helper.h"

static void filename_entry_changed_writeback(GtkEntry *entry, gpointer user_data);
static void filename_entry_changed_writeconf(GtkEntry *entry, gpointer user_data);
static void filename_add_clicked(GtkButton *button, gpointer user_data);

gchar *
filename_parse(const gchar *in, const gchar *filename, const gint snapshot)
{
	/*
	 * %f = filename
	 * %c = incremental counter
	 * %s = setting (A,B or C)
	 * %d = date (will have to wait until read from exif)
	 * %t = time (will have to wait until read from exif)
	 */

	gchar temp[1024];
	gchar tempc[32];
	gchar *output = NULL;
	gint n=0, m=0;
	gint c=0;
	gboolean counter = FALSE;
	gboolean file_exists = FALSE;
	gint i = 1;
	gchar *basename;

	if (filename == NULL) return NULL;
	if (in == NULL) return NULL;

	basename = g_path_get_basename(filename);
	output = g_strrstr(basename, ".");
	if (output != NULL) {
		*output = '\0';
	}
	output = NULL;
	do {
		
		while (in[n])
		{
  			switch(in[n])
			{
				case '%':
					switch (in[n+1])
					{
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							c = (gint) in[n+1];
							switch (in[n+2])
							{
								case 'c':
									counter = TRUE;
									if (c == 49)
										sprintf(tempc, "%01d",i);
									else if (c == 50)
										sprintf(tempc, "%02d",i);
									else if (c == 51)
										sprintf(tempc, "%03d",i);
									else if (c == 52)
										sprintf(tempc, "%04d",i);
									else if (c == 53)
										sprintf(tempc, "%05d",i);
									else if (c == 54)
										sprintf(tempc, "%06d",i);
									else if (c == 55)
										sprintf(tempc, "%07d",i);
									else if (c == 56)
										sprintf(tempc, "%08d",i);
									else if (c == 57)
										sprintf(tempc, "%09d",i);
									n += 3;
									strcpy(&temp[m], tempc);
									m += strlen(tempc);
									break;
								default:
									temp[m++] = in[n];
									temp[m++] = in[n+1];
									temp[m++] = in[n+2];
									n += 3;
									break;
								}
							break;
						case 'f':
							strcpy(&temp[m], basename);
							m += strlen(basename);
							n += 2;
							break;
						case 'c':
							counter = TRUE;
							g_sprintf(tempc, "%d", i);
							strcpy(&temp[m], tempc);
							m += strlen(tempc);
						    n += 2;
							break;
						case 's':
							switch (snapshot)
							{
								case 0:
									strcpy(&temp[m], "A");
									break;
								case 1:
									strcpy(&temp[m], "B");
									break;
								case 2:
									strcpy(&temp[m], "C");
									break;
							}
							m += 1;
							n += 2;
							break;
						default:
							temp[m++] = in[n];
							temp[m++] = in[n+1];
							n += 2;
						break;
					}
					break;
				default:
					temp[m++] = in[n++];
					break;
				}
			}

			temp[m] = (gint) NULL;

			if (output)
				g_free(output);
			
			output = g_strdup(temp);

			file_exists = g_file_test(output, G_FILE_TEST_EXISTS);

			if (counter == FALSE)
				file_exists = FALSE;

			// resets for next run	
			i++;
			n = 0;
			m = 0;

	} while (file_exists == TRUE);
	
	g_free(basename);
	
	return output;
}

static void
filename_entry_changed_writeback(GtkEntry *entry, gpointer user_data)
{
	gchar **filename = (gchar **) user_data;

	if (filename)
	{
		if (*filename) g_free(*filename);
		*filename = g_strdup(gtk_entry_get_text(entry));
	}
	return;
}

static void
filename_entry_changed_writeconf(GtkEntry *entry, gpointer user_data)
{
	gchar *conf_key = (gchar *) user_data;

	if (conf_key)
		rs_conf_set_string(conf_key, gtk_entry_get_text(entry));
	return;
}

static void add_f(GtkMenuItem *menuitem, GtkEntry *entry) { gtk_entry_append_text(entry, "%f"); };
static void add_c(GtkMenuItem *menuitem, GtkEntry *entry) { gtk_entry_append_text(entry, "%2c"); };
static void add_s(GtkMenuItem *menuitem, GtkEntry *entry) { gtk_entry_append_text(entry, "%s"); };

static void
filename_add_clicked(GtkButton *button, gpointer user_data)
{
	gui_menu_popup(GTK_WIDGET(button), user_data,
		_("%f - Original filename"), add_f,
		_("%2c - Incremental counter"), add_c,
		_("%s - Setting id (A, B or C)"), add_s,
		-1
	);
}

GtkWidget *
rs_filename_chooser_button_new(gchar **filename, const gchar *conf_key)
{
	GtkWidget *addbutton;
	GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
	GtkWidget *entry;

	addbutton = gtk_button_new_with_label("+");

	entry = gtk_entry_new();
	if (filename)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), *filename);
		g_signal_connect (G_OBJECT(entry), "changed",
			G_CALLBACK(filename_entry_changed_writeback), (gpointer) filename);
	}
	if (conf_key)
		g_signal_connect (G_OBJECT(entry), "changed",
			G_CALLBACK(filename_entry_changed_writeconf), (gpointer) conf_key);
	g_signal_connect (G_OBJECT(addbutton), "clicked",
		G_CALLBACK(filename_add_clicked), (gpointer) entry);

	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), addbutton, FALSE, FALSE, 0);

	return(hbox);
}
