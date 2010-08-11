#ifndef GNT_COMBO_BOX_H
#define GNT_COMBO_BOX_H

#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntwidget.h"

#define GNT_TYPE_COMBO_BOX				(gnt_combo_box_get_gtype())
#define GNT_COMBO_BOX(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_COMBO_BOX, GntComboBox))
#define GNT_COMBO_BOX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_COMBO_BOX, GntComboBoxClass))
#define GNT_IS_COMBO_BOX(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_COMBO_BOX))
#define GNT_IS_COMBO_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_COMBO_BOX))
#define GNT_COMBO_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_COMBO_BOX, GntComboBoxClass))

#define GNT_COMBO_BOX_FLAGS(obj)				(GNT_COMBO_BOX(obj)->priv.flags)
#define GNT_COMBO_BOX_SET_FLAGS(obj, flags)		(GNT_COMBO_BOX_FLAGS(obj) |= flags)
#define GNT_COMBO_BOX_UNSET_FLAGS(obj, flags)	(GNT_COMBO_BOX_FLAGS(obj) &= ~(flags))

typedef struct _GntComboBox			GntComboBox;
typedef struct _GntComboBoxPriv		GntComboBoxPriv;
typedef struct _GntComboBoxClass		GntComboBoxClass;

struct _GntComboBox
{
	GntWidget parent;

	GntWidget *dropdown;   /* This is a GntTree */

	void *selected;        /* Currently selected key */
};

struct _GntComboBoxClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_combo_box_get_gtype(void);

GntWidget *gnt_combo_box_new(void);

void gnt_combo_box_add_data(GntComboBox *box, gpointer key, const char *text);

void gnt_combo_box_remove(GntComboBox *box, gpointer key);

void gnt_combo_box_remove_all(GntComboBox *box);

gpointer gnt_combo_box_get_selected_data(GntComboBox *box);

void gnt_combo_box_set_selected(GntComboBox *box, gpointer key);

G_END_DECLS

#endif /* GNT_COMBO_BOX_H */
