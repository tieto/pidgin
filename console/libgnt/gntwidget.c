/* Stuff brutally ripped from Gflib */

#include "gntwidget.h"
#include "gntutils.h"
#include "gnt.h"

enum
{
	SIG_DESTROY,
	SIG_DRAW,
	SIG_GIVE_FOCUS,
	SIG_LOST_FOCUS,
	SIG_KEY_PRESSED,
	SIG_MAP,
	SIG_ACTIVATE,
	SIG_EXPOSE,
	SIG_SIZE_REQUEST,
	SIG_POSITION,
	SIGS
};

static GObjectClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_widget_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	widget->priv.name = NULL;
	DEBUG;
}

static void
gnt_widget_map(GntWidget *widget)
{
	/* Get some default size for the widget */
	DEBUG;
	g_signal_emit(widget, signals[SIG_MAP], 0);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_MAPPED);
}

static void
gnt_widget_dispose(GObject *obj)
{
	GntWidget *self = GNT_WIDGET(obj);

	if(!(GNT_WIDGET_FLAGS(self) & GNT_WIDGET_DESTROYING)) {
		GNT_WIDGET_SET_FLAGS(self, GNT_WIDGET_DESTROYING);

		g_signal_emit(self, signals[SIG_DESTROY], 0);

		GNT_WIDGET_UNSET_FLAGS(self, GNT_WIDGET_DESTROYING);
	}

	parent_class->dispose(obj);
	DEBUG;
}

static void
gnt_widget_focus_change(GntWidget *widget)
{
	if (GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_MAPPED)
		gnt_widget_draw(widget);
}

static void
gnt_widget_class_init(GntWidgetClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->dispose = gnt_widget_dispose;

	klass->destroy = gnt_widget_destroy;
	klass->show = gnt_widget_show;
	klass->draw = gnt_widget_draw;
	klass->expose = gnt_widget_expose;
	klass->map = gnt_widget_map;
	klass->lost_focus = gnt_widget_focus_change;
	klass->gained_focus = gnt_widget_focus_change;
	
	klass->key_pressed = NULL;
	klass->activate = NULL;
	
	signals[SIG_DESTROY] = 
		g_signal_new("destroy",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, destroy),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_GIVE_FOCUS] = 
		g_signal_new("gained-focus",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, gained_focus),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_LOST_FOCUS] = 
		g_signal_new("lost-focus",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, lost_focus),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_ACTIVATE] = 
		g_signal_new("activate",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, activate),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_MAP] = 
		g_signal_new("map",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, map),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_DRAW] = 
		g_signal_new("draw",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, draw),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_EXPOSE] = 
		g_signal_new("expose",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, expose),
					 NULL, NULL,
					 gnt_closure_marshal_VOID__INT_INT_INT_INT,
					 G_TYPE_NONE, 4, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
	signals[SIG_POSITION] = 
		g_signal_new("position-set",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, set_position),
					 NULL, NULL,
					 gnt_closure_marshal_VOID__INT_INT,
					 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	signals[SIG_SIZE_REQUEST] = 
		g_signal_new("size_request",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, size_request),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[SIG_KEY_PRESSED] = 
		g_signal_new("key_pressed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, key_pressed),
					 NULL, NULL,
					 gnt_closure_marshal_BOOLEAN__STRING,
					 G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
	DEBUG;
}

/******************************************************************************
 * GntWidget API
 *****************************************************************************/
GType
gnt_widget_get_gtype(void)
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GntWidgetClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_widget_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntWidget),
			0,						/* n_preallocs		*/
			gnt_widget_init,					/* instance_init	*/
		};

		type = g_type_register_static(G_TYPE_OBJECT,
									  "GntWidget",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

static void
gnt_widget_take_focus(GntWidget *widget)
{
	gnt_screen_take_focus(widget);
}

void gnt_widget_set_take_focus(GntWidget *widget, gboolean can)
{
	if (can)
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
	else
		GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
}

/**
 * gnt_widget_destroy:
 * @obj: The #GntWidget instance.
 *
 * Emits the "destroy" signal notifying all reference holders that they
 * should release @obj.
 */
void
gnt_widget_destroy(GntWidget *obj)
{
	int id;
	g_return_if_fail(GNT_IS_WIDGET(obj));

	if ((id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(obj), "gnt:queue_update"))))
	{
		g_source_remove(id);
		g_object_set_data(G_OBJECT(obj), "gnt:queue_update", NULL);
	}

	gnt_widget_hide(obj);
	delwin(obj->window);
	if(!(GNT_WIDGET_FLAGS(obj) & GNT_WIDGET_DESTROYING))
		g_object_run_dispose(G_OBJECT(obj));
	DEBUG;
}

void
gnt_widget_show(GntWidget *widget)
{
	/* Draw the widget and take focus */
	gnt_widget_draw(widget);
	if (GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_CAN_TAKE_FOCUS)
	{
		gnt_widget_take_focus(widget);
	}
}

void
gnt_widget_draw(GntWidget *widget)
{
	/* Draw the widget */
	DEBUG;
	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_DRAWING))
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_DRAWING);
	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_MAPPED))
	{
		gnt_widget_map(widget);
		gnt_screen_occupy(widget);
	}

	if (widget->window == NULL)
	{
		/* XXX: It may be necessary to make sure the size hasn't changed */
		widget->window = newwin(widget->priv.height, widget->priv.width,
						widget->priv.y, widget->priv.x);
		wbkgd(widget->window, COLOR_PAIR(GNT_COLOR_NORMAL));
	}

	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_NO_BORDER))
		box(widget->window, 0, 0);
	else
		werase(widget->window);
	
#if 0
	/* XXX: No shadow for now :( */
	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_NO_SHADOW))
	{
		widget->back = newwin(widget->priv.height, widget->priv.width,
						widget->priv.y + 1, widget->priv.x + 1);
		wbkgd(widget->back, COLOR_PAIR(GNT_COLOR_SHADOW));
		werase(widget->back);

		mvwchgat(widget->back, 0, 0, widget->priv.height,
					A_REVERSE | A_BLINK, 0, 0);
		touchline(widget->back, 0, widget->priv.height);
		wrefresh(widget->back);
	}

	wrefresh(widget->window);
#endif
	g_signal_emit(widget, signals[SIG_DRAW], 0);
	gnt_widget_queue_update(widget);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_DRAWING);
}

gboolean
gnt_widget_key_pressed(GntWidget *widget, const char *keys)
{
	gboolean ret;
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_CAN_TAKE_FOCUS))
		return FALSE;
	g_signal_emit(widget, signals[SIG_KEY_PRESSED], 0, keys, &ret);
	return ret;
}

void
gnt_widget_expose(GntWidget *widget, int x, int y, int width, int height)
{
	g_signal_emit(widget, signals[SIG_EXPOSE], 0, x, y, width, height);
}

void
gnt_widget_hide(GntWidget *widget)
{
	wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(widget->window);
	gnt_screen_release(widget);
}

void
gnt_widget_set_position(GntWidget *wid, int x, int y)
{
	/* XXX: Need to install properties for these and g_object_notify */
	wid->priv.x = x;
	wid->priv.y = y;
	g_signal_emit(wid, signals[SIG_POSITION], 0, x, y);
}

void
gnt_widget_get_position(GntWidget *wid, int *x, int *y)
{
	if (x)
		*x = wid->priv.x;
	if (y)
		*y = wid->priv.y;
}

void
gnt_widget_size_request(GntWidget *widget)
{
	g_signal_emit(widget, signals[SIG_SIZE_REQUEST], 0);
}

void
gnt_widget_get_size(GntWidget *wid, int *width, int *height)
{
	if (width)
		*width = wid->priv.width;
	if (height)
		*height = wid->priv.height;
}

void
gnt_widget_set_size(GntWidget *widget, int width, int height)
{
	widget->priv.width = width;
	widget->priv.height = height;
}

gboolean
gnt_widget_set_focus(GntWidget *widget, gboolean set)
{
	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_CAN_TAKE_FOCUS))
		return FALSE;

	if (set && !GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_HAS_FOCUS))
	{
		g_signal_emit(widget->parent, signals[SIG_LOST_FOCUS], 0);
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_HAS_FOCUS);
		g_signal_emit(widget, signals[SIG_GIVE_FOCUS], 0);
	}
	else if (!set)
	{
		GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_HAS_FOCUS);
		g_signal_emit(widget, signals[SIG_LOST_FOCUS], 0);
	}
	else
		return FALSE;

	return TRUE;
}

void gnt_widget_set_name(GntWidget *widget, const char *name)
{
	g_free(widget->priv.name);
	widget->priv.name = g_strdup(name);
}

void gnt_widget_activate(GntWidget *widget)
{
	g_signal_emit(widget, signals[SIG_ACTIVATE], 0);
}

static gboolean
update_queue_callback(gpointer data)
{
	GntWidget *widget = GNT_WIDGET(data);

	if (!g_object_get_data(G_OBJECT(widget), "gnt:queue_update"))
		return FALSE;
	gnt_screen_update(widget);
	g_object_set_data(G_OBJECT(widget), "gnt:queue_update", GINT_TO_POINTER(FALSE));
	return FALSE;
}

void gnt_widget_queue_update(GntWidget *widget)
{
	while (widget->parent)
		widget = widget->parent;
	
	if (!g_object_get_data(G_OBJECT(widget), "gnt:queue_update"))
	{
		int id = g_timeout_add(0, update_queue_callback, widget);
		g_object_set_data(G_OBJECT(widget), "gnt:queue_update", GINT_TO_POINTER(id));
	}
}

