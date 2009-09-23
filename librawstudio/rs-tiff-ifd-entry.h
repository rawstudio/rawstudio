#ifndef RS_TIFF_IFD_ENTRY_H
#define RS_TIFF_IFD_ENTRY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_TIFF_IFD_ENTRY rs_tiff_ifd_entry_get_type()
#define RS_TIFF_IFD_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_TIFF_IFD_ENTRY, RSTiffIfdEntry))
#define RS_TIFF_IFD_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_TIFF_IFD_ENTRY, RSTiffIfdEntryClass))
#define RS_IS_TIFF_IFD_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_TIFF_IFD_ENTRY))
#define RS_IS_TIFF_IFD_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_TIFF_IFD_ENTRY))
#define RS_TIFF_IFD_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_TIFF_IFD_ENTRY, RSTiffIfdEntryClass))

typedef struct {
	GObject parent;

	gushort tag;
	gushort type;
	guint count;
	guint value_offset;
} RSTiffIfdEntry;

typedef struct {
	GObjectClass parent_class;
} RSTiffIfdEntryClass;

#include <rawstudio.h>

GType rs_tiff_ifd_entry_get_type(void);

RSTiffIfdEntry *rs_tiff_ifd_entry_new(RSTiff *tiff, guint offset);

G_END_DECLS

#endif /* RS_TIFF_IFD_ENTRY_H */
