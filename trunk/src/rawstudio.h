#define PITCH(width) ((((width)+31)/32)*32)

enum {
	FILE_UNKN,
	FILE_RAW
};

typedef struct {
	gboolean in_use;
	guint w;
	guint h;
	gint pitch;
	guint channels;
	gushort *pixels[4];
	dcraw_data *raw;
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *scale;
	guint vis_scale;
	guint vis_w;
	guint vis_h;
	guint vis_pitch;
	gushort *vis_pixels[4];
	guint vis_histogram[4][256];
	GdkPixbuf *vis_pixbuf;
	GtkWidget *vis_image;
	GtkWidget *files; /* ugly hack */
} RS_IMAGE;

void update_gammatable(const double g);
void update_previewtable(RS_IMAGE *rs);
void rs_debug(RS_IMAGE *rs);
void update_scaled(RS_IMAGE *rs);
void update_preview(RS_IMAGE *rs);
void rs_reset(RS_IMAGE *rs);
void rs_free_raw(RS_IMAGE *rs);
void rs_free(RS_IMAGE *rs);
void rs_alloc(RS_IMAGE *rs, const guint width, const guint height, const guint channels);
RS_IMAGE *rs_new();
void rs_load_raw_from_memory(RS_IMAGE *rs);
void rs_load_raw_from_file(RS_IMAGE *rs, const gchar *filename);
