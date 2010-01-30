#ifndef RS_PROFILE_SELECTOR_H
#define RS_PROFILE_SELECTOR_H

#include <rs-dcp-file.h>
#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RS_TYPE_PROFILE_SELECTOR rs_profile_selector_get_type()
#define RS_PROFILE_SELECTOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RS_TYPE_PROFILE_SELECTOR, RSProfileSelector))
#define RS_PROFILE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), RS_TYPE_PROFILE_SELECTOR, RSProfileSelectorClass))
#define RS_IS_PROFILE_SELECTOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RS_TYPE_PROFILE_SELECTOR))
#define RS_IS_PROFILE_SELECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RS_TYPE_PROFILE_SELECTOR))
#define RS_PROFILE_SELECTOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), RS_TYPE_PROFILE_SELECTOR, RSProfileSelectorClass))

typedef struct {
	GtkComboBox parent;

	gpointer selected;
} RSProfileSelector;

typedef struct {
	GtkComboBoxClass parent_class;
} RSProfileSelectorClass;

GType rs_profile_selector_get_type(void);

RSProfileSelector *
rs_profile_selector_new(void);

void
rs_profile_selector_set_profiles(RSProfileSelector *selector, GList *profiles);

void
rs_profile_selector_select_profile(RSProfileSelector *selector, gpointer profile);

void
rs_profile_selector_set_profiles_steal(RSProfileSelector *selector, GList *profiles);

void
rs_profile_selector_set_model_filter(RSProfileSelector *selector, GtkTreeModelFilter *filter);

G_END_DECLS

#endif /* RS_PROFILE_SELECTOR_H */
