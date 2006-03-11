enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN,
	FULLNAME_COLUMN,
	NUM_COLUMNS
};

#define GUI_CATCHUP() while (gtk_events_pending()) gtk_main_iteration()

void gui_status_push(const char *text);
void gui_status_pop();
void update_histogram(RS_BLOB *rs);
gboolean update_preview_callback(GtkAdjustment *caller, RS_BLOB *rs);
GtkObject *make_adj(RS_BLOB *rs, double value, double min, double max, double step, double page);
GtkWidget *gui_hist(RS_BLOB *rs, const gchar *label);
GtkWidget *gui_box(const gchar *title, GtkWidget *in);
void gui_transform_rot90_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_rot180_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_rot270_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_mirror_clicked(GtkWidget *w, RS_BLOB *rs);
void gui_transform_flip_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *gui_transform(RS_BLOB *rs);
GtkWidget *gui_rgb_mixer(RS_BLOB *rs);
GtkWidget *gui_slider(GtkObject *adj, const gchar *label);
void gui_reset_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *gui_reset(RS_BLOB *rs);
void save_file_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *save_file(RS_BLOB *rs);
GtkWidget *make_toolbox(RS_BLOB *rs);
void fill_model(GtkListStore *store, const char *path);
void icon_activated(GtkIconView *iconview, RS_BLOB *rs);
void gui_cd_clicked(GtkWidget *button, GtkListStore *store);
GtkWidget *make_iconbox(RS_BLOB *rs, GtkListStore *store);
gboolean drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
int gui_init(int argc, char **argv);
