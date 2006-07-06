typedef struct {
	const gchar *path_file;
	gchar *output_file;
	gint setting_id;
} RS_QUEUE_ELEMENT;

GArray* batch_new_queue();
gboolean batch_add_element_to_queue(GArray *queue, RS_QUEUE_ELEMENT *element);
gboolean batch_add_to_queue(GArray *queue, const gchar *file_path, gint setting_id, gchar *output_file);
gboolean batch_remove_from_queue(GArray *queue, const gchar *path_file, gint setting_id);
RS_QUEUE_ELEMENT* batch_get_next_in_queue(GArray *queue);
void batch_remove_next_in_queue(GArray *queue);
gint batch_find_in_queue(GArray *queue, const gchar *file_path, gint setting_id);
RS_QUEUE_ELEMENT* batch_get_element(GArray *queue, gint index);
