/*
 * RAWstudio - Rawstudio is an open source raw-image converter written in GTK+.
 * by Anders BRander <anders@brander.dk> and Anders Kvist <akv@lnxbx.dk>
 *
 * gettext.h - gettext interface
 *
 * Rawstudio is licensed under the GNU General Public License.
 * It uses DCRaw and UFraw code to do the actual raw decoding.
 */

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  define Q_(String) g_strip_context ((String), gettext (String))
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define Q_(String) g_strip_context ((String), (String))
#  define N_(String) (String)
#endif
