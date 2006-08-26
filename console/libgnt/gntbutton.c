#include <string.h>

#include "gntbutton.h"
#include "gntutils.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_button_draw(GntWidget *widget)
{
	GntButton *button = GNT_BUTTON(widget);
	GntColorType type;

	if (gnt_widget_has_focus(widget))
		type = GNT_COLOR_HIGHLIGHT;
	else
		type = GNT_COLOR_NORMAL;
	
	wbkgdset(widget->window, '\0' | COLOR_PAIR(type));
	mvwprintw(widget->window, 1, 2, button->priv->text);

	DEBUG;
}

static void
gnt_button_size_request(GntWidget *widget)
{
	GntButton *button = GNT_BUTTON(widget);
	gnt_util_get_text_bound(button->priv->text,
			&widget->priv.width, &widget->priv.height);
	widget->priv.width += 4;
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_NO_BORDER))
		widget->priv.height += 2;
}

static void
gnt_button_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static gboolean
gnt_button_key_pressed(GntWidget *widget, const char *key)
{
	if (strcmp(key, GNT_KEY_ENTER) == 0)
	{
		gnt_widget_activate(widget);
		return TRUE;
	}
	return FALSE;
}

static gboolean
gnt_button_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	if (event == GNT_LEFT_MOUSE_DOWN) {
		gnt_widget_activate(widget);
		return TRUE;
	}
	return FALSE;
}

static void
gnt_button_class_init(GntWidgetClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->draw = gnt_button_draw;
	parent_class->map = gnt_button_map;
	parent_class->size_request = gnt_button_size_request;
	parent_class->key_pressed = gnt_button_key_pressed;
	parent_class->clicked = gnt_button_clicked;

	DEBUG;
}

static void
gnt_button_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GntButton *button = GNT_BUTTON(instance);
	button->priv = g_new0(GntButtonPriv, 1);

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X);

	widget->priv.minw = 4;
	widget->priv.minh = 3;
	DEBUG;
}

/******************************************************************************
 * GntButton API
 *****************************************************************************/
GType
gnt_button_get_gtype(void) {
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo info = {
			sizeof(GntButtonClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_button_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntButton),
			0,						/* n_preallocs		*/
			gnt_button_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntButton",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_button_new(const char *text)
{
	GntWidget *widget = g_object_new(GNT_TYPE_BUTTON, NULL);
	GntButton *button = GNT_BUTTON(widget);

	button->priv->text = g_strdup(text);
	gnt_widget_set_take_focus(widget, TRUE);

	return widget;
}

