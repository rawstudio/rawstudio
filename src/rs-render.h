void rs_render_select(gboolean cms);
void rs_render_previewtable(const double contrast);
void (*rs_render)(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile);
void (*rs_render_histogram_table)(RS_PHOTO *photo, RS_IMAGE16 *input, guint *table);
