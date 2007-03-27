#ifndef GNT_CHECK_BOX_H
#define GNT_CHECK_BOX_H

#include "gntbutton.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_CHECK_BOX				(gnt_check_box_get_gtype())
#define GNT_CHECK_BOX(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_CHECK_BOX, GntCheckBox))
#define GNT_CHECK_BOX_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_CHECK_BOX, GntCheckBoxClass))
#define GNT_IS_CHECK_BOX(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_CHECK_BOX))
#define GNT_IS_CHECK_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_CHECK_BOX))
#define GNT_CHECK_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_CHECK_BOX, GntCheckBoxClass))

#define GNT_CHECK_BOX_FLAGS(obj)				(GNT_CHECK_BOX(obj)->priv.flags)
#define GNT_CHECK_BOX_SET_FLAGS(obj, flags)		(GNT_CHECK_BOX_FLAGS(obj) |= flags)
#define GNT_CHECK_BOX_UNSET_FLAGS(obj, flags)	(GNT_CHECK_BOX_FLAGS(obj) &= ~(flags))

typedef struct _GntCheckBox			GntCheckBox;
typedef struct _GntCheckBoxPriv		GntCheckBoxPriv;
typedef struct _GntCheckBoxClass		GntCheckBoxClass;

struct _GntCheckBox
{
	GntButton parent;
	gboolean checked;
};

struct _GntCheckBoxClass
{
	GntButtonClass parent;

	void (*toggled)(void);

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_check_box_get_gtype(void);

GntWidget *gnt_check_box_new(const char *text);

void gnt_check_box_set_checked(GntCheckBox *box, gboolean set);

gboolean gnt_check_box_get_checked(GntCheckBox *box);

G_END_DECLS

#endif /* GNT_CHECK_BOX_H */
