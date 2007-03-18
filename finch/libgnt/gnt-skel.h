#ifndef GNT_SKEL_H
#define GNT_SKEL_H

#include "gntwidget.h"
#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_SKEL				(gnt_skel_get_gtype())
#define GNT_SKEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_SKEL, GntSkel))
#define GNT_SKEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_SKEL, GntSkelClass))
#define GNT_IS_SKEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_SKEL))
#define GNT_IS_SKEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_SKEL))
#define GNT_SKEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_SKEL, GntSkelClass))

#define GNT_SKEL_FLAGS(obj)				(GNT_SKEL(obj)->priv.flags)
#define GNT_SKEL_SET_FLAGS(obj, flags)		(GNT_SKEL_FLAGS(obj) |= flags)
#define GNT_SKEL_UNSET_FLAGS(obj, flags)	(GNT_SKEL_FLAGS(obj) &= ~(flags))

typedef struct _GnSkel			GntSkel;
typedef struct _GnSkelPriv		GntSkelPriv;
typedef struct _GnSkelClass		GntSkelClass;

struct _GnSkel
{
	GntWidget parent;
};

struct _GnSkelClass
{
	GntWidgetClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_skel_get_gtype(void);

GntWidget *gnt_skel_new();

G_END_DECLS

#endif /* GNT_SKEL_H */
