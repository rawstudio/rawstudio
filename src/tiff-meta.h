/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * tiff-meta.h - functions to read metadata from tiff-alike files
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

#define ENDIANSWAP4(a) (((a) & 0x000000FF) << 24 | ((a) & 0x0000FF00) << 8 | ((a) & 0x00FF0000) >> 8) | (((a) & 0xFF000000) >> 24)
#define ENDIANSWAP2(a) (((a) & 0x00FF) << 8) | (((a) & 0xFF00) >> 8)

void rs_tiff_load_meta(const gchar *filename, RS_METADATA *meta);
GdkPixbuf *rs_tiff_load_thumb(const gchar *src);
