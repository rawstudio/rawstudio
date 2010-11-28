/*
 * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>,
 * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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
#include <unistd.h>
#include <stdlib.h>
#include "application.h"
#include "rs-photo.h"
#ifndef WIN32
#include <dbus/dbus.h>


static gboolean rs_has_gimp(gint major, gint minor, gint micro);

#define EXPORT_TO_GIMP_TIMEOUT_SECONDS 30

DBusHandlerResult
dbus_gimp_opened (DBusConnection * connection, DBusMessage * message, void *user_data) {

	/* Check if image has been opened by GIMP */
	if (dbus_message_is_signal(message, "org.gimp.GIMP.UI", "Opened"))
	{
		gchar *argument = NULL;
		gchar *filename = (gchar *) user_data;

		dbus_message_get_args(message, NULL,
				      DBUS_TYPE_STRING, &argument,
				      DBUS_TYPE_INVALID);

		/* Cleaning up */
		dbus_connection_remove_filter(connection, dbus_gimp_opened, user_data);
		unlink(filename); /*FIXME: filename should almost match argument - will cause error if user opens a photo in GIMP while exporting */

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif

gboolean
rs_external_editor_gimp(RS_PHOTO *photo, RSFilter *prior_to_resample, guint snapshot)
{
#ifdef WIN32
	return FALSE;
#else
	RSOutput *output = NULL;
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

	/* Setup our filter chain for saving */
        RSFilter *fdcp = rs_filter_new("RSDcp", prior_to_resample);
        RSFilter *fdenoise= rs_filter_new("RSDenoise", fdcp);
        RSFilter *ftransform_display = rs_filter_new("RSColorspaceTransform", fdenoise);
        RSFilter *fend = ftransform_display;

	/* Set DCP profile - we do NOT set ICC profiles, since this will already be set further up the chain */
	RSDcpFile *dcp_profile  = rs_photo_get_dcp_profile(photo);

	if (dcp_profile != NULL)
		g_object_set(fdcp, "profile", dcp_profile, "use-profile", TRUE, NULL);
	else
		g_object_set(fdcp, "use-profile", FALSE, NULL);

	rs_filter_set_recursive(fdenoise, "settings", photo->settings[snapshot], NULL);


	output = rs_output_new("RSTifffile");
	g_object_set(output, "filename", filename->str, NULL);
	rs_output_execute(output, fend);
	g_object_unref(output);
	g_object_unref(ftransform_display);
	g_object_unref(fdenoise);
	g_object_unref(fdcp);

	message = dbus_message_new_method_call("org.gimp.GIMP.UI",
                                                "/org/gimp/GIMP/UI",
                                                "org.gimp.GIMP.UI",
                                                "OpenAsNew");
	dbus_message_append_args (message,
                                        DBUS_TYPE_STRING, &filename->str,
					DBUS_TYPE_INVALID);

	/* Send DBus message to GIMP */
	reply = dbus_connection_send_with_reply_and_block (bus, message, -1, NULL);

	/* If we didn't get a reply from GIMP - we try to start it and resend the message */
	if (!reply) {
		gint retval = system("gimp &");
		if (retval != 0) {
			g_warning("system(\"gimp &\") returned: %d\n", retval);
			g_unlink(filename->str);
			g_string_free(filename, TRUE);
			dbus_message_unref (message);
			return FALSE;
		}
	}

	/* Allow GIMP to start - we send the message every one second */
	while (!reply) {
		gint i = 0;
		if (i > EXPORT_TO_GIMP_TIMEOUT_SECONDS) {
			g_warning("Never got a reply from GIMP - deleting temporary file");
			g_unlink(filename->str);
			g_string_free(filename, TRUE);
			dbus_message_unref (message);
			return FALSE;
		}
		sleep(1);
		i++;
		reply = dbus_connection_send_with_reply_and_block (bus, message, -1, NULL);
	}

	dbus_message_unref (message);

	/* Depends on GIMP DBus signal: 'Opened' */
	if (rs_has_gimp(2,6,2)) {
		/* Connect to GIMP and listen for "Opened" signal */
		dbus_bus_add_match (bus, "type='signal',interface='org.gimp.GIMP.UI'", NULL);
		dbus_connection_add_filter(bus, dbus_gimp_opened, filename->str , NULL);
		g_string_free(filename, FALSE);
	} else {
		/* Old sad way - GIMP doesn't let us know that it has opened the photo */
		g_warning("You have an old version of GIMP and we suggest that you upgrade to at least 2.6.2");
		g_warning("Rawstudio will stop responding for 10 seconds while it waits for GIMP to open the file");
		sleep(10);
		g_unlink(filename->str);
		g_string_free(filename, TRUE);
	}

	return TRUE;
#endif
}
#ifndef WIN32
static gboolean
rs_has_gimp(gint major, gint minor, gint micro) {
	FILE *fp;
	char line[128];
	int _major, _minor, _micro;
	gboolean retval = FALSE;

	fp = popen("gimp -v","r");
	if (fgets( line, sizeof line, fp) == NULL)
	{
		g_warning("fgets returned: %d\n", retval);
		return FALSE;
	}
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
#endif
