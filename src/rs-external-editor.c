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

#include <glib.h>
#include <glib/gstdio.h>
#include <dbus/dbus.h>
#include "rawstudio.h"
#include "rs-photo.h"

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

		while (!reply && i < EXPORT_TO_GIMP_TIMEOUT_SECONDS ) {
			sleep(1);
			reply = dbus_connection_send_with_reply_and_block (bus, message, -1, NULL);
			i++;
		}
	}

	dbus_message_unref (message);

	g_unlink(filename->str);
	g_string_free(filename, TRUE);

        if (reply)
                return TRUE;
        else
                return FALSE;
}
