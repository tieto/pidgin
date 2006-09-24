#ifndef GNT_MENUITEM_H
#define GNT_MENUITEM_H

#include <glib.h>
#include <glib-object.h>

#define GNT_TYPE_MENUITEM				(gnt_menuitem_get_gtype())
#define GNT_MENUITEM(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENUITEM, GntMenuItem))
#define GNT_MENUITEM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENUITEM, GntMenuItemClass))
#define GNT_IS_MENUITEM(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENUITEM))
#define GNT_IS_MENUITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENUITEM))
#define GNT_MENUITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENUITEM, GntMenuItemClass))

#define GNT_MENUITEM_FLAGS(obj)				(GNT_MENUITEM(obj)->priv.flags)
#define GNT_MENUITEM_SET_FLAGS(obj, flags)		(GNT_MENUITEM_FLAGS(obj) |= flags)
#define GNT_MENUITEM_UNSET_FLAGS(obj, flags)	(GNT_MENUITEM_FLAGS(obj) &= ~(flags))

typedef struct _GnMenuItem			GntMenuItem;
typedef struct _GnMenuItemPriv		GntMenuItemPriv;
typedef struct _GnMenuItemClass		GntMenuItemClass;

struct _GnMenuItemPriv
{
	/* These will be used to determine the position of the submenu */
	int x;
	int y;
};

typedef void (*GntMenuItemCallback)(GntMenuItem *item, gpointer data);

struct _GnMenuItem
{
	GObject parent;
	GntMenuItemPriv priv;

	char *text;

	/* A GntMenuItem can have a callback associated with it.
	 * The callback will be activated whenever the suer selects it and presses enter (or clicks).
	 * However, if the GntMenuItem has some child, then the callback and callbackdata will be ignored. */
	gpointer callbackdata;
	GntMenuItemCallback callback;

	GntMenu *submenu;
};

struct _GnMenuItemClass
{
	GObjectClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_menuitem_get_gtype(void);

GObject *gnt_menuitem_new(const char *text);

void gnt_menuitem_set_callback(GntMenuItem *item, GntMenuItemCallback callback, gpointer data);

void gnt_menuitem_set_submenu(GntMenuItem *item, GntMenu *menu);

G_END_DECLS

#endif /* GNT_MENUITEM_H */
