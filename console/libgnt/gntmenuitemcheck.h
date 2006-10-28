#ifndef GNT_MENUITEM_CHECK_H
#define GNT_MENUITEM_CHECK_H

#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntmenuitem.h"

#define GNT_TYPE_MENUITEM_CHECK				(gnt_menuitem_check_get_gtype())
#define GNT_MENUITEM_CHECK(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENUITEM_CHECK, GntMenuItemCheck))
#define GNT_MENUITEM_CHECK_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENUITEM_CHECK, GntMenuItemCheckClass))
#define GNT_IS_MENUITEM_CHECK(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENUITEM_CHECK))
#define GNT_IS_MENUITEM_CHECK_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENUITEM_CHECK))
#define GNT_MENUITEM_CHECK_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENUITEM_CHECK, GntMenuItemCheckClass))

#define GNT_MENUITEM_CHECK_FLAGS(obj)				(GNT_MENUITEM_CHECK(obj)->priv.flags)
#define GNT_MENUITEM_CHECK_SET_FLAGS(obj, flags)		(GNT_MENUITEM_CHECK_FLAGS(obj) |= flags)
#define GNT_MENUITEM_CHECK_UNSET_FLAGS(obj, flags)	(GNT_MENUITEM_CHECK_FLAGS(obj) &= ~(flags))

typedef struct _GnMenuItemCheck			GntMenuItemCheck;
typedef struct _GnMenuItemCheckPriv		GntMenuItemCheckPriv;
typedef struct _GnMenuItemCheckClass		GntMenuItemCheckClass;

struct _GnMenuItemCheck
{
	GntMenuItem parent;
	gboolean checked;
};

struct _GnMenuItemCheckClass
{
	GntMenuItemClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_menuitem_check_get_gtype(void);

GntMenuItem *gnt_menuitem_check_new(const char *text);

gboolean gnt_menuitem_check_get_checked(GntMenuItemCheck *item);

void gnt_menuitem_check_set_checked(GntMenuItemCheck *item, gboolean set);

G_END_DECLS

#endif /* GNT_MENUITEM_CHECK_H */
