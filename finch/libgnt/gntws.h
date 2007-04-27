#ifndef GNTWS_H
#define GNTWS_H

#include "gntwidget.h"

#include <panel.h>

#define GNT_TYPE_WS				(gnt_ws_get_gtype())
#define GNT_WS(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WS, GntWS))
#define GNT_IS_WS(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WS))
#define GNT_IS_WS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WS))
#define GNT_WS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WS, GntWSClass))

typedef struct _GntWS GntWS;

struct _GntWS
{
	GntBindable inherit;
	gchar *name;
	GList *list;
	GList *ordered;
	gpointer ui_data;
	
	void *res1;
	void *res2;
	void *res3;
	void *res4;
};

typedef struct _GntWSClass GntWSClass;

struct _GntWSClass
{
	GntBindableClass parent;

	void (*draw_taskbar)(GntWS *ws, gboolean );

	void (*res1)(void);
	void (*res2)(void);
	void (*res3)(void);
	void (*res4)(void);
};

G_BEGIN_DECLS

GType gnt_ws_get_gtype(void);

void gnt_ws_set_name(GntWS *, const gchar *);
void gnt_ws_add_widget(GntWS *, GntWidget *);
void gnt_ws_remove_widget(GntWS *, GntWidget *);
void gnt_ws_widget_hide(GntWidget *, GHashTable *nodes);
void gnt_ws_widget_show(GntWidget *, GHashTable *nodes);
void gnt_ws_draw_taskbar(GntWS *, gboolean reposition);
void gnt_ws_hide(GntWS *, GHashTable *);
void gnt_ws_show(GntWS *, GHashTable *);

#endif
