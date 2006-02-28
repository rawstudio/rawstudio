enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN
};

gboolean update_preview_callback(GtkAdjustment *caller, RS_IMAGE *rs);
GtkObject *make_adj(RS_IMAGE *rs, double value, double min, double max, double step, double page);
GtkWidget *gui_box(const gchar *title, GtkWidget *in);
GtkWidget *gui_rgb_mixer(RS_IMAGE *rs);
GtkWidget *gui_slider(GtkObject *adj, const gchar *label);
void gui_reset_clicked(GtkWidget *w, RS_IMAGE *rs);
GtkWidget *gui_reset(RS_IMAGE *rs);
void save_file_clicked(GtkWidget *w, RS_IMAGE *rs);
GtkWidget *save_file(RS_IMAGE *rs);
GtkWidget *make_toolbox(RS_IMAGE *rs);
void open_file_ok(GtkWidget *w, RS_IMAGE *rs);
gboolean open_file(GtkWidget *caller, RS_IMAGE *rs);
void fill_model(GtkListStore *store, const char *path);
void icon_activated(GtkIconView *iconview, RS_IMAGE *rs);
GtkWidget *make_iconbox(RS_IMAGE *rs, GtkListStore *store);
int gui_init(int argc, char **argv);
