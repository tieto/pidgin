/* Stuff brutally ripped from Gflib */

#include "gntwidget.h"
#include "gntstyle.h"
#include "gntmarshal.h"
#include "gnt.h"

enum
{
	SIG_DESTROY,
	SIG_DRAW,
	SIG_HIDE,
	SIG_GIVE_FOCUS,
	SIG_LOST_FOCUS,
	SIG_KEY_PRESSED,
	SIG_MAP,
	SIG_ACTIVATE,
	SIG_EXPOSE,
	SIG_SIZE_REQUEST,
	SIG_CONFIRM_SIZE,
	SIG_SIZE_CHANGED,
	SIG_POSITION,
	SIG_CLICKED,
	SIG_CONTEXT_MENU,
	SIGS
};

static GObjectClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void init_widget(GntWidget *widget);

static void
gnt_widget_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	widget->priv.name = NULL;
	GNTDEBUG;
}

static void
gnt_widget_map(GntWidget *widget)
{
	/* Get some default size for the widget */
	GNTDEBUG;
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
	GNTDEBUG;
}

static void
gnt_widget_focus_change(GntWidget *widget)
{
	if (GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_MAPPED)
		gnt_widget_draw(widget);
}

static gboolean
gnt_widget_dummy_confirm_size(GntWidget *widget, int width, int height)
{
	if (width < widget->priv.minw || height < widget->priv.minh)
		return FALSE;
	if (widget->priv.width != width && !GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_GROW_X))
		return FALSE;
	if (widget->priv.height != height && !GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_GROW_Y))
		return FALSE;
	return TRUE;
}

static gboolean
context_menu(GntBindable *bind, GList *null)
{
	gboolean ret = FALSE;
	g_signal_emit(bind, signals[SIG_CONTEXT_MENU], 0, &ret);
	return ret;
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
	klass->confirm_size = gnt_widget_dummy_confirm_size;
	
	klass->key_pressed = NULL;
	klass->activate = NULL;
	klass->clicked = NULL;
	
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
	signals[SIG_HIDE] = 
		g_signal_new("hide",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, hide),
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
	signals[SIG_SIZE_CHANGED] = 
		g_signal_new("size_changed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, size_changed),
					 NULL, NULL,
					 gnt_closure_marshal_VOID__INT_INT,
					 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);
	signals[SIG_CONFIRM_SIZE] = 
		g_signal_new("confirm_size",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, confirm_size),
					 NULL, NULL,
					 gnt_closure_marshal_BOOLEAN__INT_INT,
					 G_TYPE_BOOLEAN, 2, G_TYPE_INT, G_TYPE_INT);
	signals[SIG_KEY_PRESSED] = 
		g_signal_new("key_pressed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, key_pressed),
					 gnt_boolean_handled_accumulator, NULL,
					 gnt_closure_marshal_BOOLEAN__STRING,
					 G_TYPE_BOOLEAN, 1, G_TYPE_STRING);

	signals[SIG_CLICKED] = 
		g_signal_new("clicked",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntWidgetClass, clicked),
					 gnt_boolean_handled_accumulator, NULL,
					 gnt_closure_marshal_BOOLEAN__INT_INT_INT,
					 G_TYPE_BOOLEAN, 3, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

	signals[SIG_CONTEXT_MENU] = 
		g_signal_new("context-menu",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0,
					 gnt_boolean_handled_accumulator, NULL,
					 gnt_closure_marshal_BOOLEAN__VOID,
					 G_TYPE_BOOLEAN, 0);

	/* This is relevant for all widgets */
	gnt_bindable_class_register_action(GNT_BINDABLE_CLASS(klass), "context-menu", context_menu,
				GNT_KEY_POPUP, NULL);
	gnt_bindable_register_binding(GNT_BINDABLE_CLASS(klass), "context-menu", GNT_KEY_F11, NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
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
			NULL,
			NULL,					/* class_data		*/
			sizeof(GntWidget),
			0,						/* n_preallocs		*/
			gnt_widget_init,					/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_BINDABLE,
									  "GntWidget",
									  &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
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
	g_return_if_fail(GNT_IS_WIDGET(obj));

	gnt_widget_hide(obj);
	delwin(obj->window);
	if(!(GNT_WIDGET_FLAGS(obj) & GNT_WIDGET_DESTROYING))
		g_object_run_dispose(G_OBJECT(obj));
	GNTDEBUG;
}

void
gnt_widget_show(GntWidget *widget)
{
	gnt_widget_draw(widget);
	gnt_screen_occupy(widget);
}

void
gnt_widget_draw(GntWidget *widget)
{
	/* Draw the widget */
	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_DRAWING))
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_DRAWING);
	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_MAPPED)) {
		gnt_widget_map(widget);
	}

	if (widget->window == NULL)
	{
		int x, y, maxx, maxy, w, h;
		int oldw, oldh;
		gboolean shadow = TRUE;

		if (!gnt_widget_has_shadow(widget))
			shadow = FALSE;

		x = widget->priv.x;
		y = widget->priv.y;
		w = oldw = widget->priv.width + shadow;
		h = oldh = widget->priv.height + shadow;

		getmaxyx(stdscr, maxy, maxx);
		maxy -= 1;		/* room for the taskbar */

		x = MAX(0, x);
		y = MAX(0, y);
		if (x + w >= maxx)
			x = MAX(0, maxx - w);
		if (y + h >= maxy)
			y = MAX(0, maxy - h);

		w = MIN(w, maxx);
		h = MIN(h, maxy);

		widget->priv.x = x;
		widget->priv.y = y;
		if (w != oldw || h != oldh) {
			widget->priv.width = w - shadow;
			widget->priv.height = h - shadow;
			g_signal_emit(widget, signals[SIG_SIZE_CHANGED], 0, oldw, oldh);
		}

		widget->window = newwin(widget->priv.height + shadow, widget->priv.width + shadow,
						widget->priv.y, widget->priv.x);
		init_widget(widget);
	}

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

	if (gnt_bindable_perform_action_key(GNT_BINDABLE(widget), keys))
		return TRUE;

	keys = gnt_bindable_remap_keys(GNT_BINDABLE(widget), keys);
	g_signal_emit(widget, signals[SIG_KEY_PRESSED], 0, keys, &ret);
	return ret;
}

gboolean
gnt_widget_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	gboolean ret;
	g_signal_emit(widget, signals[SIG_CLICKED], 0, event, x, y, &ret);
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
	g_signal_emit(widget, signals[SIG_HIDE], 0);
	wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
#if 0
	/* XXX: I have no clue why, but this seemed to be necessary. */
	if (gnt_widget_has_shadow(widget))
		mvwvline(widget->window, 1, widget->priv.width, ' ', widget->priv.height);
#endif
	gnt_screen_release(widget);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_INVISIBLE);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_MAPPED);
}

void
gnt_widget_set_position(GntWidget *wid, int x, int y)
{
	g_signal_emit(wid, signals[SIG_POSITION], 0, x, y);
	/* XXX: Need to install properties for these and g_object_notify */
	wid->priv.x = x;
	wid->priv.y = y;
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
	gboolean shadow = TRUE;
	if (!gnt_widget_has_shadow(wid))
		shadow = FALSE;

	if (width)
		*width = wid->priv.width + shadow;
	if (height)
		*height = wid->priv.height + shadow;
	
}

static void
init_widget(GntWidget *widget)
{
	gboolean shadow = TRUE;

	if (!gnt_widget_has_shadow(widget))
		shadow = FALSE;

	wbkgd(widget->window, COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(widget->window);

	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_NO_BORDER))
	{
		/* - This is ugly. */
		/* - What's your point? */
		mvwvline(widget->window, 0, 0, ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), widget->priv.height);
		mvwvline(widget->window, 0, widget->priv.width - 1,
				ACS_VLINE | COLOR_PAIR(GNT_COLOR_NORMAL), widget->priv.height);
		mvwhline(widget->window, widget->priv.height - 1, 0,
				ACS_HLINE | COLOR_PAIR(GNT_COLOR_NORMAL), widget->priv.width);
		mvwhline(widget->window, 0, 0, ACS_HLINE | COLOR_PAIR(GNT_COLOR_NORMAL), widget->priv.width);
		mvwaddch(widget->window, 0, 0, ACS_ULCORNER | COLOR_PAIR(GNT_COLOR_NORMAL));
		mvwaddch(widget->window, 0, widget->priv.width - 1,
				ACS_URCORNER | COLOR_PAIR(GNT_COLOR_NORMAL));
		mvwaddch(widget->window, widget->priv.height - 1, 0,
				ACS_LLCORNER | COLOR_PAIR(GNT_COLOR_NORMAL));
		mvwaddch(widget->window, widget->priv.height - 1, widget->priv.width - 1,
				ACS_LRCORNER | COLOR_PAIR(GNT_COLOR_NORMAL));
	}

	if (shadow)
	{
		wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_SHADOW));
		mvwvline(widget->window, 1, widget->priv.width, ' ', widget->priv.height);
		mvwhline(widget->window, widget->priv.height, 1, ' ', widget->priv.width);
	}
}

gboolean
gnt_widget_set_size(GntWidget *widget, int width, int height)
{
	gboolean ret = TRUE;

	if (gnt_widget_has_shadow(widget))
	{
		width--;
		height--;
	}
	if (width <= 0)
		width = widget->priv.width;
	if (height <= 0)
		height = widget->priv.height;

	if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
	{
		ret = gnt_widget_confirm_size(widget, width, height);
	}

	if (ret)
	{
		gboolean shadow = TRUE;
		int oldw, oldh;

		if (!gnt_widget_has_shadow(widget))
			shadow = FALSE;

		oldw = widget->priv.width;
		oldh = widget->priv.height;

		widget->priv.width = width;
		widget->priv.height = height;

		g_signal_emit(widget, signals[SIG_SIZE_CHANGED], 0, oldw, oldh);

		if (widget->window)
		{
			wresize(widget->window, height + shadow, width + shadow);
			init_widget(widget);
		}
		if (GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
			init_widget(widget);
		else
			GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_MAPPED);
	}

	return ret;
}

gboolean
gnt_widget_set_focus(GntWidget *widget, gboolean set)
{
	if (!(GNT_WIDGET_FLAGS(widget) & GNT_WIDGET_CAN_TAKE_FOCUS))
		return FALSE;

	if (set && !GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_HAS_FOCUS))
	{
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

const char *gnt_widget_get_name(GntWidget *widget)
{
	return widget->priv.name;
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
	g_object_set_data(G_OBJECT(widget), "gnt:queue_update", NULL);
	return FALSE;
}

void gnt_widget_queue_update(GntWidget *widget)
{
	if (widget->window == NULL)
		return;
	while (widget->parent)
		widget = widget->parent;
	
	if (!g_object_get_data(G_OBJECT(widget), "gnt:queue_update"))
	{
		int id = g_timeout_add(0, update_queue_callback, widget);
		g_object_set_data_full(G_OBJECT(widget), "gnt:queue_update", GINT_TO_POINTER(id),
				(GDestroyNotify)g_source_remove);
	}
}

gboolean gnt_widget_confirm_size(GntWidget *widget, int width, int height)
{
	gboolean ret = FALSE;
	g_signal_emit(widget, signals[SIG_CONFIRM_SIZE], 0, width, height, &ret);
	return ret;
}

void gnt_widget_set_visible(GntWidget *widget, gboolean set)
{
	if (set)
		GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_INVISIBLE);
	else
		GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_INVISIBLE);
}

gboolean gnt_widget_has_shadow(GntWidget *widget)
{
	return (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_SHADOW) &&
			gnt_style_get_bool(GNT_STYLE_SHADOW, FALSE));
}

