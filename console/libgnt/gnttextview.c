#include "gnttextview.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_text_view_draw(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);

	copywin(view->scroll, widget->window, view->pos, 0, 0, 0,
					widget->priv.height - 1, widget->priv.width - 1, FALSE);
	
	DEBUG;
}

static void
gnt_text_view_size_request(GntWidget *widget)
{
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
	{
		gnt_widget_set_size(widget, 64, 24);
	}
}

static void
gnt_text_view_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static gboolean
gnt_text_view_key_pressed(GntWidget *widget, const char *text)
{
	return FALSE;
}

static void
gnt_text_view_destroy(GntWidget *widget)
{
}

static void
gnt_text_view_class_init(GntTextViewClass *klass)
{
	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_text_view_destroy;
	parent_class->draw = gnt_text_view_draw;
	parent_class->map = gnt_text_view_map;
	parent_class->size_request = gnt_text_view_size_request;
	parent_class->key_pressed = gnt_text_view_key_pressed;

	DEBUG;
}

static void
gnt_text_view_init(GTypeInstance *instance, gpointer class)
{
	DEBUG;
}

/******************************************************************************
 * GntTextView API
 *****************************************************************************/
GType
gnt_text_view_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntTextViewClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_text_view_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntTextView),
			0,						/* n_preallocs		*/
			gnt_text_view_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntTextView",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_text_view_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_TEXTVIEW, NULL);
	GntTextView *view = GNT_TEXT_VIEW(widget);

	view->scroll = newwin(255, widget->priv.width, widget->priv.y, widget->priv.x);
	scrollok(view->scroll, TRUE);
	wsetscrreg(view->scroll, 0, 254);
	wbkgd(view->scroll, COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(view->scroll);
	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER);

	return widget;
}

void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextViewFlags flags)
{
	GntWidget *widget = GNT_WIDGET(view);
	int fl = 0;
	char **split;
	int i;

	if (text == NULL || *text == '\0')
		return;

	if (flags & GNT_TEXT_FLAG_BOLD)
		fl |= A_BOLD;
	if (flags & GNT_TEXT_FLAG_UNDERLINE)
		fl |= A_UNDERLINE;
	if (flags & GNT_TEXT_FLAG_BLINK)
		fl |= A_BLINK;

	wattrset(view->scroll, fl | COLOR_PAIR(GNT_COLOR_NORMAL));

	split = g_strsplit(text, "\n", 0);
	for (i = 0; split[i + 1]; i++)
	{
		/* XXX: Do something if the strlen of split[i] is big
		 * enough to cause the text to wrap. */
		wprintw(view->scroll, "%s\n", split[i]);
		view->lines++;
	}
	wprintw(view->scroll, "%s", split[i]);
	g_strfreev(split);

	gnt_widget_draw(widget);
}

void gnt_text_view_scroll(GntTextView *view, int scroll)
{
	GntWidget *widget = GNT_WIDGET(view);
	int height;

	if (scroll == 0)
	{
		view->pos = view->lines - widget->priv.height + 1;
	}
	else
	{
		view->pos += scroll;
	}

	if (view->pos + (height = widget->priv.height) > view->lines)
		view->pos = view->lines - height + 1;

	if (view->pos < 0)
		view->pos = 0;

	gnt_widget_draw(GNT_WIDGET(view));
}

void gnt_text_view_next_line(GntTextView *view)
{
	wclrtoeol(view->scroll);
	gnt_text_view_append_text_with_flags(view, "\n", 0);
}

