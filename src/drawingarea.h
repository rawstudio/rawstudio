gboolean drawingarea_expose (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
gboolean drawingarea_configure (GtkWidget *widget, GdkEventExpose *event, RS_BLOB *rs);
gboolean gui_drawingarea_move_callback(GtkWidget *widget, GdkEventMotion *event, RS_BLOB *rs);
gboolean gui_drawingarea_button(GtkWidget *widget, GdkEventButton *event, RS_BLOB *rs);
GtkWidget *gui_drawingarea_make(RS_BLOB *rs);
