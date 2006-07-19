#include "gntlabel.h"

#include <string.h>

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_label_destroy(GntWidget *widget)
{
	GntLabel *label = GNT_LABEL(widget);
	g_free(label->text);
}

static void
gnt_label_draw(GntWidget *widget)
{
	GntLabel *label = GNT_LABEL(widget);
	chtype flag = gnt_text_format_flag_to_chtype(label->flags);

	wbkgdset(widget->window, '\0' | flag);
	mvwprintw(widget->window, 0, 0, label->text);

	DEBUG;
}

static void
gnt_label_size_request(GntWidget *widget)
{
	GntLabel *label = GNT_LABEL(widget);
	char *s = label->text, *last = s;
	int count = 1;
	int max = 0;

	/* XXX: ew ... everyone look away */
	while (*s)
	{
		if (*s == '\n' || *s == '\r')
		{
			count++;
			if (max < s - last + 1)
				max = s - last + 1;
			last = s + 1;
		}
		s++;
	}
	if (max < s - last + 1)
		max = s - last + 1;
	widget->priv.height = count;

	widget->priv.width = max;
}

static void
gnt_label_class_init(GntLabelClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_label_destroy;
	parent_class->draw = gnt_label_draw;
	parent_class->map = NULL;
	parent_class->size_request = gnt_label_size_request;

	DEBUG;
}

static void
gnt_label_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_GROW_X);
	widget->priv.minw = 3;
	widget->priv.minh = 1;
	DEBUG;
}

/******************************************************************************
 * GntLabel API
 *****************************************************************************/
GType
gnt_label_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntLabelClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_label_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntLabel),
			0,						/* n_preallocs		*/
			gnt_label_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntLabel",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_label_new(const char *text)
{
	return gnt_label_new_with_format(text, 0);
}

GntWidget *gnt_label_new_with_format(const char *text, GntTextFormatFlags flags)
{
	GntWidget *widget = g_object_new(GNT_TYPE_LABEL, NULL);
	GntLabel *label = GNT_LABEL(widget);

	label->text = g_strdup(text);
	label->flags = flags;
	gnt_widget_set_take_focus(widget, FALSE);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);

	return widget;
}

void gnt_label_set_text(GntLabel *label, const char *text)
{
	g_free(label->text);
	label->text = g_strdup(text);

	if (GNT_WIDGET(label)->window)
	{
		gnt_widget_hide(GNT_WIDGET(label));
		gnt_label_size_request(GNT_WIDGET(label));
		gnt_widget_draw(GNT_WIDGET(label));
	}
}

