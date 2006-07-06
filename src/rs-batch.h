typedef struct {
	const gchar *path_file;
	gchar *output_file;
	gint setting_id;
} RS_QUEUE_ELEMENT;

typedef struct {
	GArray *array;
} RS_QUEUE;


RS_QUEUE* batch_new_queue();
gboolean batch_add_element_to_queue(RS_QUEUE *queue, RS_QUEUE_ELEMENT *element);
gboolean batch_add_to_queue(RS_QUEUE *queue, const gchar *file_path, gint setting_id, gchar *output_file);
gboolean batch_remove_from_queue(RS_QUEUE *queue, const gchar *path_file, gint setting_id);
RS_QUEUE_ELEMENT* batch_get_next_in_queue(RS_QUEUE *queue);
void batch_remove_next_in_queue(RS_QUEUE *queue);
gint batch_find_in_queue(RS_QUEUE *queue, const gchar *file_path, gint setting_id);
RS_QUEUE_ELEMENT* batch_get_element(RS_QUEUE *queue, gint index);
