#ifndef RS_TIFF_IFD_H
#define RS_TIFF_IFD_H

#include <glib-object.h>
#include "rs-tiff.h"

G_BEGIN_DECLS

#define RS_TYPE_TIFF_IFD rs_tiff_ifd_get_type()
#define RS_TIFF_IFD(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TIFF_IFD, RSTiffIfd))
#define RS_TIFF_IFD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TIFF_IFD, RSTiffIfdClass))
#define RS_IS_TIFF_IFD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TIFF_IFD))
#define RS_IS_TIFF_IFD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_TIFF_IFD))
#define RS_TIFF_IFD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_TIFF_IFD, RSTiffIfdClass))

typedef struct {
	GObject parent;

	gboolean dispose_has_run;

	RSTiff *tiff;
	guint offset;

	gushort num_entries;
	GList *entries;
	guint next_ifd;
} RSTiffIfd;

typedef struct {
	GObjectClass parent_class;

	void (*read)(RSTiffIfd *ifd);
} RSTiffIfdClass;

GType rs_tiff_ifd_get_type(void);

RSTiffIfd *rs_tiff_ifd_new(RSTiff *tiff, guint offset);

guint rs_tiff_ifd_get_next(RSTiffIfd *ifd);

RSTiffIfdEntry *rs_tiff_ifd_get_entry_by_tag(RSTiffIfd *ifd, gushort tag);

G_END_DECLS

#endif /* RS_TIFF_IFD_H */
