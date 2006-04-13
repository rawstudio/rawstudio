enum { 
	PIXBUF_COLUMN,
	TEXT_COLUMN,
	DATA_COLUMN,
	FULLNAME_COLUMN,
	NUM_COLUMNS
};

enum {
	OP_NONE = 0,
	OP_MOVE
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
GtkWidget *gui_tool_rgb_mixer(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_warmth(RS_BLOB *rs, gint n);
GtkWidget *gui_slider(GtkObject *adj, const gchar *label);
void gui_reset_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *gui_reset(RS_BLOB *rs);
void save_file_clicked(GtkWidget *w, RS_BLOB *rs);
GtkWidget *save_file(RS_BLOB *rs);
GtkWidget *gui_make_scale(RS_BLOB *rs, GCallback cb, double value, double min, double max, double step, double page);
GtkWidget *gui_make_scale_from_adj(RS_BLOB *rs, GCallback cb, GtkObject *adj);
GtkWidget *gui_tool_exposure(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_saturation(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_hue(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_contrast(RS_BLOB *rs, gint n);
GtkWidget *gui_tool_gamma(RS_BLOB *rs, gint n);
GtkWidget *gui_make_tools(RS_BLOB *rs, gint n);
void gui_notebook_callback(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, RS_BLOB *rs);
GtkWidget *make_toolbox(RS_BLOB *rs);
gint fill_model_compare_func (GtkTreeModel *model, GtkTreeIter *tia,
	GtkTreeIter *tib, gpointer userdata);
void fill_model(GtkListStore *store, const char *path);
void icon_activated_helper(GtkIconView *iconview, GtkTreePath *path, gpointer user_data);
void icon_activated(GtkIconView *iconview, RS_BLOB *rs);
GtkWidget *make_iconbox(RS_BLOB *rs, GtkListStore *store);
gboolean drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
gboolean drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
void gui_menu_open_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_reload_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_preview_bg_color_changed(GtkColorButton *widget, RS_BLOB *rs);
gboolean gui_fullscreen_callback(GtkWidget *widget, GdkEventWindowState *event, GtkWidget *iconbox);
void gui_menu_widget_visible_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_fullscreen_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_iconbar_previous_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_iconbar_next_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_menu_preference_callback(gpointer callback_data, guint callback_action, GtkWidget *widget);
void gui_about();
void gui_dialog_simple(gchar *title, gchar *message);
GtkWidget *gui_make_menubar(RS_BLOB *rs, GtkWidget *window, GtkListStore *store, GtkWidget *iconbox, GtkWidget *toolbox);
gboolean gui_drawingarea_move_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
gboolean gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);
int gui_init(int argc, char **argv);
