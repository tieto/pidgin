#ifndef GNT_MENU_ITEM_CHECK_H
#define GNT_MENU_ITEM_CHECK_H

#include "gnt.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntmenuitem.h"

#define GNT_TYPE_MENU_ITEM_CHECK				(gnt_menuitem_check_get_gtype())
#define GNT_MENU_ITEM_CHECK(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENU_ITEM_CHECK, GntMenuItemCheck))
#define GNT_MENU_ITEM_CHECK_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENU_ITEM_CHECK, GntMenuItemCheckClass))
#define GNT_IS_MENU_ITEM_CHECK(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENU_ITEM_CHECK))
#define GNT_IS_MENU_ITEM_CHECK_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENU_ITEM_CHECK))
#define GNT_MENU_ITEM_CHECK_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENU_ITEM_CHECK, GntMenuItemCheckClass))

#define GNT_MENU_ITEM_CHECK_FLAGS(obj)				(GNT_MENU_ITEM_CHECK(obj)->priv.flags)
#define GNT_MENU_ITEM_CHECK_SET_FLAGS(obj, flags)		(GNT_MENU_ITEM_CHECK_FLAGS(obj) |= flags)
#define GNT_MENU_ITEM_CHECK_UNSET_FLAGS(obj, flags)	(GNT_MENU_ITEM_CHECK_FLAGS(obj) &= ~(flags))

typedef struct _GntMenuItemCheck			GntMenuItemCheck;
typedef struct _GntMenuItemCheckPriv		GntMenuItemCheckPriv;
typedef struct _GntMenuItemCheckClass		GntMenuItemCheckClass;

struct _GntMenuItemCheck
{
	GntMenuItem parent;
	gboolean checked;
};

struct _GntMenuItemCheckClass
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

#endif /* GNT_MENU_ITEM_CHECK_H */
