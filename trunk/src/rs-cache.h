/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * rs-cache.h - cache interface
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

gchar *rs_cache_get_name(const gchar *src);
void rs_cache_save(RS_PHOTO *photo);
void rs_cache_load(RS_PHOTO *photo);
void rs_cache_load_quick(const gchar *filename, gint *priority);
