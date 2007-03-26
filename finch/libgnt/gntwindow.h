#ifndef GNT_WINDOW_H
#define GNT_WINDOW_H

#include "gnt.h"
#include "gntbox.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntmenu.h"

#define GNT_TYPE_WINDOW				(gnt_window_get_gtype())
#define GNT_WINDOW(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WINDOW, GntWindow))
#define GNT_WINDOW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_WINDOW, GntWindowClass))
#define GNT_IS_WINDOW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WINDOW))
#define GNT_IS_WINDOW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WINDOW))
#define GNT_WINDOW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WINDOW, GntWindowClass))

#define GNT_WINDOW_FLAGS(obj)				(GNT_WINDOW(obj)->priv.flags)
#define GNT_WINDOW_SET_FLAGS(obj, flags)		(GNT_WINDOW_FLAGS(obj) |= flags)
#define GNT_WINDOW_UNSET_FLAGS(obj, flags)	(GNT_WINDOW_FLAGS(obj) &= ~(flags))

typedef struct _GntWindow			GntWindow;
typedef struct _GntWindowPriv		GntWindowPriv;
typedef struct _GntWindowClass		GntWindowClass;

struct _GntWindow
{
	GntBox parent;
	GntMenu *menu;
};

struct _GntWindowClass
{
	GntBoxClass parent;

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_window_get_gtype(void);

#define gnt_vwindow_new(homo) gnt_window_box_new(homo, TRUE)
#define gnt_hwindow_new(homo) gnt_window_box_new(homo, FALSE)

GntWidget *gnt_window_new(void);

GntWidget *gnt_window_box_new(gboolean homo, gboolean vert);

void gnt_window_set_menu(GntWindow *window, GntMenu *menu);

G_END_DECLS

#endif /* GNT_WINDOW_H */
