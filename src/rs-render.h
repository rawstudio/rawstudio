void rs_render_select(gboolean cms);
void rs_render_previewtable(const double contrast);
void (*rs_render)(RS_PHOTO *photo, gint width, gint height, gushort *in,
	gint in_rowstride, gint in_channels, guchar *out, gint out_rowstride, void *profile);
