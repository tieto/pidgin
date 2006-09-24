#ifndef GNT_MENU_H
#define GNT_MENU_H

#include "gnttree.h"
#include "gntcolors.h"
#include "gntkeys.h"

#define GNT_TYPE_MENU				(gnt_menu_get_gtype())
#define GNT_MENU(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_MENU, GntMenu))
#define GNT_MENU_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_MENU, GntMenuClass))
#define GNT_IS_MENU(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_MENU))
#define GNT_IS_MENU_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_MENU))
#define GNT_MENU_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_MENU, GntMenuClass))

#define GNT_MENU_FLAGS(obj)				(GNT_MENU(obj)->priv.flags)
#define GNT_MENU_SET_FLAGS(obj, flags)		(GNT_MENU_FLAGS(obj) |= flags)
#define GNT_MENU_UNSET_FLAGS(obj, flags)	(GNT_MENU_FLAGS(obj) &= ~(flags))

typedef struct _GnMenu			GntMenu;
typedef struct _GnMenuPriv		GntMenuPriv;
typedef struct _GnMenuClass		GntMenuClass;

#include "gntmenuitem.h"

/**
 * A toplevel-menu is displayed at the top of the screen, and it spans accross
 * the entire width of the screen.
 * A popup-menu could be displayed, for example, as a context menu for widgets.
 */
typedef enum
{
	GNT_MENU_TOPLEVEL = 1,  /* Menu for a toplevel window */
	GNT_MENU_POPUP,         /* A popup menu */
} GntMenuType;

struct _GnMenu
{
	GntTree parent;
	GntMenuType type;
	
	GList *list;
	int selected;

	/* This will keep track of its immediate submenu which is visible so that
	 * keystrokes can be passed to it. */
	GntMenu *submenu;
	GntMenu *parentmenu;
};

struct _GnMenuClass
{
	GntTreeClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_menu_get_gtype(void);

GntWidget *gnt_menu_new(GntMenuType type);

void gnt_menu_add_item(GntMenu *menu, GntMenuItem *item);

G_END_DECLS

#endif /* GNT_MENU_H */
