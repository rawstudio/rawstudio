/*
 * Copyright (C) 2006, 2007, 2008 Anders Brander <anders@brander.dk> and 
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

/* Accepts old DBUS (before 1.0) installations */
#define DBUS_API_SUBJECT_TO_CHANGE

#include <glib.h>
#include <glib/gstdio.h>
#include <dbus/dbus.h>
#include "application.h"
#include "rs-photo.h"

static gboolean rs_has_gimp(gint major, gint minor, gint micro);

#define EXPORT_TO_GIMP_TIMEOUT_SECONDS 30

gboolean
rs_external_editor_gimp(RS_PHOTO *photo, guint snapshot, void *cms) {

	g_assert(RS_IS_PHOTO(photo));

	// We need at least GIMP 2.4.0 to export photo
	if (!rs_has_gimp(2,4,0)) {
		return FALSE;
	}

	DBusConnection *bus;
	DBusMessage *message, *reply;
	GString *filename;

	bus = dbus_bus_get (DBUS_BUS_SESSION, NULL);

	filename = g_string_new("");
        g_string_printf(filename, "%s/.rawstudio_%.0f.tif",g_get_tmp_dir(), g_random_double()*10000);

	rs_photo_save(photo, filename->str, FILETYPE_TIFF8, -1, -1, FALSE, 1.0, snapshot, cms);

	message = dbus_message_new_method_call("org.gimp.GIMP.UI",
                                                "/org/gimp/GIMP/UI",
                                                "org.gimp.GIMP.UI",
                                                "OpenAsNew");
	dbus_message_append_args (message,
                                        DBUS_TYPE_STRING, &filename->str,
					DBUS_TYPE_INVALID);
	reply = dbus_connection_send_with_reply_and_block (bus, message, -1, NULL);

	if (!reply) {
		system("gimp &");
		gint i = 0;

		// FIXME: We need to sleep a bit with GIMP 2.6 as it doesn't wait until it has opened the photo before it replies...
		if (rs_has_gimp(2,6,0)) {
			sleep(5);
		}

		while (!reply && i < EXPORT_TO_GIMP_TIMEOUT_SECONDS ) {
			sleep(1);
			reply = dbus_connection_send_with_reply_and_block (bus, message, -1, NULL);
			i++;
		}
	}

	dbus_message_unref (message);

	// FIXME: We still need to sleep a bit because of GIMP 2.6...
	if (rs_has_gimp(2,6,0)) {
		sleep(2);
	}

	g_unlink(filename->str);
	g_string_free(filename, TRUE);

        if (reply)
                return TRUE;
        else
                return FALSE;
}

static gboolean
rs_has_gimp(gint major, gint minor, gint micro) {
	FILE *fp;
	char line[128];
	int _major, _minor, _micro;
	gboolean retval = FALSE;

	fp = popen("gimp -v","r");
	fgets( line, sizeof line, fp);
	pclose(fp);

#if GLIB_CHECK_VERSION(2,14,0)
	GRegex *regex;
	gchar **tokens;
	
	regex = g_regex_new(".*([0-9])\x2E([0-9]+)\x2E([0-9]+).*", 0, 0, NULL);
	tokens = g_regex_split(regex, line, 0);
	g_regex_unref(regex);

	if (tokens[1])
		_major = atoi(tokens[1]);
	else
	{
		g_strfreev(tokens);
		return FALSE;
	}

	if (_major > major) {
		retval = TRUE;
	} else if (_major == major) {

		if (tokens[2])
			_minor = atoi(tokens[2]);
		else
		{
			g_strfreev(tokens);
			return FALSE;
		}

		if (_minor > minor) {
			retval = TRUE;
		} else if (_minor == minor) {
	
			if (tokens[3])
				_micro = atoi(tokens[3]);
			else
			{
				g_strfreev(tokens);
				return FALSE;
			}

			if (_micro >= micro) {
				retval = TRUE;
			}
		}
	}
	g_strfreev(tokens);
#else
	sscanf(line,"GNU Image Manipulation Program version %d.%d.%d", &_major, &_minor, &_micro);

	if (_major > major) {
		retval = TRUE;
	} else if (_major == major) {
		if (_minor > minor) {
			retval = TRUE;
		} else if (_minor == minor) {
			if (_micro >= micro) {
				retval = TRUE;
			}
		}
	}
#endif

	return retval;
}
