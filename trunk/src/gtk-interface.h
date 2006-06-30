/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * gtk-interface.h - the gtk interface
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */


enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN,
	FULLNAME_COLUMN,
	PRIORITY_COLUMN,
	NUM_COLUMNS
};

enum {
	PRIO_U = 0,
	PRIO_D = 51,
	PRIO_1 = 1,
	PRIO_2 = 2,
	PRIO_3 = 3,
	PRIO_ALL = 255
};

enum {
	OP_NONE = 0,
	OP_MOVE
};

#define GUI_CATCHUP() while (gtk_events_pending()) gtk_main_iteration()

void update_histogram(RS_BLOB *rs);
gboolean update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs);
void gui_dialog_simple(gchar *title, gchar *message);
int gui_init(int argc, char **argv);
