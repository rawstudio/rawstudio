/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * gtk-helper.h - some helper functions to gtk-interface
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

GtkWidget *gui_tooltip_no_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
void *gui_tooltip_window(GtkWidget *widget, gchar *tip_tip, gchar *tip_private);
