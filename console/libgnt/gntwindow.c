#include "gntwindow.h"

#include <string.h>

enum
{
	SIGS = 1,
};

static GntBoxClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

gboolean (*org_keypress)(GntWidget *widget, const char *text);
void (*org_destroy)(GntWidget *widget);

static gboolean
gnt_window_key_pressed(GntWidget *widget, const char *text)
{
	if (strcmp(text, GNT_KEY_CTRL_O) == 0 && GNT_WINDOW(widget)->menu) {
		gnt_screen_menu_show(GNT_WINDOW(widget)->menu);
		return TRUE;
	} else
		return org_keypress(widget, text);
		
}

static void
gnt_window_destroy(GntWidget *widget)
{
	GntWindow *window = GNT_WINDOW(widget);
	if (window->menu)
		gnt_widget_destroy(GNT_WIDGET(window->menu));
	org_destroy(widget);
}

static void
gnt_window_class_init(GntWindowClass *klass)
{
	GntWidgetClass *wid_class = GNT_WIDGET_CLASS(klass);
	parent_class = GNT_BOX_CLASS(klass);

	org_keypress = wid_class->key_pressed;
	wid_class->key_pressed = gnt_window_key_pressed;

	org_destroy = wid_class->destroy;
	wid_class->destroy = gnt_window_destroy;

	GNTDEBUG;
}

static void
gnt_window_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_UNSET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_CAN_TAKE_FOCUS);
	GNTDEBUG;
}

/******************************************************************************
 * GntWindow API
 *****************************************************************************/
GType
gnt_window_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntWindowClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_window_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntWindow),
			0,						/* n_preallocs		*/
			gnt_window_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_BOX,
									  "GntWindow",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_window_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_WINDOW, NULL);

	return widget;
}

GntWidget *gnt_window_box_new(gboolean homo, gboolean vert)
{
	GntWidget *wid = gnt_window_new();
	GntBox *box = GNT_BOX(wid);

	box->homogeneous = homo;
	box->vertical = vert;
	box->alignment = vert ? GNT_ALIGN_LEFT : GNT_ALIGN_MID;

	return wid;
}

void gnt_window_set_menu(GntWindow *window, GntMenu *menu)
{
	window->menu = menu;
}

