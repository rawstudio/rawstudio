#define PITCH(width) ((((width)+31)/32)*32)

#define SWAP( a, b ) a ^= b ^= a ^= b

#define DOTDIR ".rawstudio"
#define HISTOGRAM_DATASET_WIDTH (250)

#define ORIENTATION_RESET(orientation) orientation = 0
#define ORIENTATION_90(orientation) orientation = (orientation&4) | ((orientation+1)&3)
#define ORIENTATION_180(orientation) orientation = (orientation^2)
#define ORIENTATION_270(orientation) orientation = (orientation&4) | ((orientation+3)&3)
#define ORIENTATION_FLIP(orientation) orientation = (orientation^4)
#define ORIENTATION_MIRROR(orientation) orientation = ((orientation&4)^4) | ((orientation+2)&3)

#define GETVAL(adjustment) \
	gtk_adjustment_get_value((GtkAdjustment *) adjustment)
#define SETVAL(adjustment, value) \
	gtk_adjustment_set_value((GtkAdjustment *) adjustment, value)

enum {
	MASK_EXPOSURE = 1,
	MASK_GAMMA = 2,
	MASK_SATURATION = 4,
	MASK_HUE = 8,
	MASK_RGBMIXER = 16,
	MASK_CONTRAST = 32,
	MASK_WARMTH = 64,
	MASK_TINT = 128,
	MASK_ALL = 255
};

#define MASK_WB (MASK_WARMTH|MASK_TINT)

enum {
	MASK_OVER = 128,
	MASK_UNDER = 64,
};

enum {
_MMX = 1,
_SSE = 2,
_CMOV = 4,
_3DNOW = 8
};

#if __GNUC__ >= 3
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define align(x) __attribute__ ((aligned (x)))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define align(x)
#endif

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in CHARS */
	guint orientation;
	guchar *pixels;
} RS_IMAGE8;

typedef struct {
	guint w;
	guint h;
	gint pitch;
	gint rowstride;
	guint channels;
	guint pixelsize; /* the size of a pixel in SHORTS */
	guint orientation;
	gushort *pixels;
} RS_IMAGE16;

typedef struct {
	gint x1;
	gint y1;
	gint x2;
	gint y2;
} RS_RECT;

typedef struct {
	GtkObject *exposure;
	GtkObject *gamma;
	GtkObject *saturation;
	GtkObject *hue;
	GtkObject *rgb_mixer[3];
	GtkObject *contrast;
	GtkObject *warmth;
	GtkObject *tint;
} RS_SETTINGS;

typedef struct {
	gdouble exposure;
	gdouble gamma;
	gdouble saturation;
	gdouble hue;
	gdouble rgb_mixer[3];
	gdouble contrast;
	gdouble warmth;
	gdouble tint;
} RS_SETTINGS_DOUBLE;

typedef struct _metadata {
	gushort orientation;
	gfloat aperture;
	gushort iso;
	gfloat shutterspeed;
	guint thumbnail_start;
	guint thumbnail_length;
	guint preview_start;
	guint preview_length;
} RS_METADATA;

typedef struct {
	gboolean in_use;
	const gchar *filename;
	RS_IMAGE16 *input;
	RS_IMAGE16 *scaled;
	RS_IMAGE8 *preview;
	RS_IMAGE8 *mask;
	RS_SETTINGS *settings[3];
	RS_SETTINGS_DOUBLE *settings_buffer;
	gint current_setting;
	gint priority;
	GtkObject *scale;
	gfloat pre_mul[4];
	guint orientation;
	guint preview_scale;
	RS_RECT *preview_exposed;
	RS_IMAGE16 *histogram_dataset;
	guint histogram_table[3][256];
	GtkImage *histogram_image;
	GtkWidget *preview_drawingarea;
	gboolean preview_idle_render;
	gboolean preview_done;
	GdkPixmap *preview_backing;
	RS_METADATA *metadata;
	gint preview_idle_render_lastrow;
	RS_MATRIX4Int mati;
	RS_MATRIX4 mat;
	gboolean show_exposure_overlay;
} RS_BLOB;

typedef struct {
	const gchar *ext;
	void (*load)(RS_BLOB *, const gchar *);
	GdkPixbuf *(*thumb)(const gchar *);
	void (*load_meta)(const gchar *, RS_METADATA *);
} RS_FILETYPE;

void print_debug_line(const char *format, const gint value, const gboolean a);
void update_preview(RS_BLOB *rs);
void update_preview_region(RS_BLOB *rs, RS_RECT *region);
inline void rs_render(RS_BLOB *rs, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride);
void rs_reset(RS_BLOB *rs);
void rs_settings_reset(RS_SETTINGS *rss, guint mask);
RS_BLOB *rs_new();
void rs_free(RS_BLOB *rs);
RS_FILETYPE *rs_filetype_get(const gchar *filename, gboolean load);
gchar *rs_dotdir_get(const gchar *filename);
gchar *rs_thumb_get_name(const gchar *src);
void rs_set_wb_auto(RS_BLOB *rs);
void rs_set_wb_from_pixels(RS_BLOB *rs, gint x, gint y);
void rs_set_wb_from_color(RS_BLOB *rs, gdouble r, gdouble g, gdouble b);
void rs_set_wb_from_mul(RS_BLOB *rs, gdouble *mul);
void rs_set_wb(RS_BLOB *rs, gfloat warmth, gfloat tint);
gboolean rs_shutdown(GtkWidget *dummy1, GdkEvent *dummy2, RS_BLOB *rs);
