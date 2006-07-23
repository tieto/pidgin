#include "gntcheckbox.h"

enum
{
	SIG_TOGGLED = 1,
	SIGS,
};

static GntButtonClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_check_box_draw(GntWidget *widget)
{
	GntCheckBox *cb = GNT_CHECK_BOX(widget);
	GntColorType type;
	char *text;

	if (gnt_widget_has_focus(widget))
		type = GNT_COLOR_HIGHLIGHT;
	else
		type = GNT_COLOR_NORMAL;
	
	wbkgdset(widget->window, '\0' | COLOR_PAIR(type));

	text = g_strdup_printf("[%c]", cb->checked ? 'X' : ' ');
	mvwprintw(widget->window, 0, 0, text);
	g_free(text);

	wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	mvwprintw(widget->window, 0, 4, GNT_BUTTON(cb)->priv->text);
	
	DEBUG;
}

static void
gnt_check_box_size_request(GntWidget *widget)
{
}

static void
gnt_check_box_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static gboolean
gnt_check_box_key_pressed(GntWidget *widget, const char *text)
{
	if (text[0] == ' ' && text[1] == '\0')
	{
		GNT_CHECK_BOX(widget)->checked = !GNT_CHECK_BOX(widget)->checked;
		g_signal_emit(widget, signals[SIG_TOGGLED], 0);
		gnt_widget_draw(widget);
		return TRUE;
	}

	return FALSE;
}

static void
gnt_check_box_destroy(GntWidget *widget)
{
}

static void
gnt_check_box_class_init(GntCheckBoxClass *klass)
{
	GntWidgetClass *wclass = GNT_WIDGET_CLASS(klass);

	parent_class = GNT_BUTTON_CLASS(klass);
	/*parent_class->destroy = gnt_check_box_destroy;*/
	wclass->draw = gnt_check_box_draw;
	/*parent_class->map = gnt_check_box_map;*/
	/*parent_class->size_request = gnt_check_box_size_request;*/
	wclass->key_pressed = gnt_check_box_key_pressed;

	signals[SIG_TOGGLED] = 
		g_signal_new("toggled",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntCheckBoxClass, toggled),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	DEBUG;
}

static void
gnt_check_box_init(GTypeInstance *instance, gpointer class)
{
	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(instance), GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);
	DEBUG;
}

/******************************************************************************
 * GntCheckBox API
 *****************************************************************************/
GType
gnt_check_box_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntCheckBoxClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_check_box_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntCheckBox),
			0,						/* n_preallocs		*/
			gnt_check_box_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_BUTTON,
									  "GntCheckBox",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_check_box_new(const char *text)
{
	GntWidget *widget = g_object_new(GNT_TYPE_CHECK_BOX, NULL);

	GNT_BUTTON(widget)->priv->text = g_strdup(text);
	gnt_widget_set_take_focus(widget, TRUE);

	return widget;
}

void gnt_check_box_set_checked(GntCheckBox *box, gboolean set)
{
	if (set != box->checked)
	{
		box->checked = set;
		g_signal_emit(box, signals[SIG_TOGGLED], 0);
	}
}

gboolean gnt_check_box_get_checked(GntCheckBox *box)
{
	return box->checked;
}



