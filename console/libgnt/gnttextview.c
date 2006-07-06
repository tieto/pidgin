#include "gnttextview.h"

enum
{
	SIGS = 1,
};

typedef struct
{
	GntTextViewFlags flags;
	char *text;
} GntTextSegment;

typedef struct
{
	GList *segments;         /* A list of GntTextSegments */
	int length;              /* The current length of the line so far */
} GntTextLine;

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_text_view_draw(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);
	int i = 0;
	GList *lines;

	werase(widget->window);

	for (i = 0, lines = view->list; i < widget->priv.height && lines; i++, lines = lines->next)
	{
		GList *iter;
		GntTextLine *line = lines->data;

		wmove(widget->window, widget->priv.height - 1 - i, 0);

		for (iter = line->segments; iter; iter = iter->next)
		{
			GntTextSegment *seg = iter->data;
			wattrset(widget->window, seg->flags);
			wprintw(widget->window, "%s", seg->text);
			if (!iter->next)
				whline(widget->window, ' ' | seg->flags, widget->priv.width - line->length - 1);
		}
	}
	
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
free_text_segment(gpointer data, gpointer null)
{
	GntTextSegment *seg = data;
	g_free(seg->text);
	g_free(seg);
}

static void
free_text_line(gpointer data, gpointer null)
{
	GntTextLine *line = data;
	g_list_foreach(line->segments, free_text_segment, NULL);
	g_list_free(line->segments);
	g_free(line);
}

static void
gnt_text_view_destroy(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);
	g_list_foreach(view->list, free_text_line, NULL);
	g_list_free(view->list);
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
	GntTextLine *line = g_new0(GntTextLine, 1);

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW);

	view->list = g_list_append(view->list, line);

	return widget;
}

void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextViewFlags flags)
{
	GntWidget *widget = GNT_WIDGET(view);
	int fl = 0;
	char **split;
	int i;
	GList *list = view->list;

	if (text == NULL || *text == '\0')
		return;

	if (flags & GNT_TEXT_FLAG_BOLD)
		fl |= A_BOLD;
	if (flags & GNT_TEXT_FLAG_UNDERLINE)
		fl |= A_UNDERLINE;
	if (flags & GNT_TEXT_FLAG_BLINK)
		fl |= A_BLINK;

	if (flags & GNT_TEXT_FLAG_DIM)
		fl |= (A_DIM | COLOR_PAIR(GNT_COLOR_DISABLED));
	else if (flags & GNT_TEXT_FLAG_HIGHLIGHT)
		fl |= (A_DIM | COLOR_PAIR(GNT_COLOR_HIGHLIGHT));
	else
		fl |= COLOR_PAIR(GNT_COLOR_NORMAL);

	view->list = g_list_first(view->list);

	split = g_strsplit(text, "\n", 0);
	for (i = 0; split[i]; i++)
	{
		GntTextLine *line = view->list->data;
		int len = g_utf8_strlen(split[i], -1);
		char *iter = split[i];
		int prev = 0;

		while (iter && *iter)
		{
			GntTextSegment *seg = g_new0(GntTextSegment, 1);
			seg->flags = fl;
			seg->text = g_new0(char, len + 1);		/* XXX: MUST be improved */
			g_utf8_strncpy(seg->text, iter, widget->priv.width - line->length - 1);
			line->segments = g_list_append(line->segments, seg);

			prev = g_utf8_strlen(seg->text, -1);
			line->length += prev;
			iter = g_utf8_offset_to_pointer(iter, prev);
			if (line->length >= widget->priv.width - 1 && *iter)
			{
				line = g_new0(GntTextLine, 1);
				view->list = g_list_prepend(g_list_first(view->list), line);
			}
			/*len -= prev;*/
		}
	}

	g_strfreev(split);
	view->list = list;

	gnt_widget_draw(widget);
}

void gnt_text_view_scroll(GntTextView *view, int scroll)
{
	if (scroll == 0)
	{
		view->list = g_list_first(view->list);
	}
	else if (scroll > 0)
	{
		GList *list = g_list_nth_prev(view->list, scroll);
		if (list == NULL)
			list = g_list_first(view->list);
		view->list = list;
	}
	else if (scroll < 0)
	{
		GList *list = g_list_nth(view->list, -scroll);
		if (list == NULL)
			list = g_list_last(view->list);
		view->list = list;
	}
		
	gnt_widget_draw(GNT_WIDGET(view));
}

void gnt_text_view_next_line(GntTextView *view)
{
	GntTextLine *line = g_new0(GntTextLine, 1);
	GList *list = view->list;
	
	view->list = g_list_prepend(g_list_first(view->list), line);
	view->list = list;
	gnt_widget_draw(GNT_WIDGET(view));
}

