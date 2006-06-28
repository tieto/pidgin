#include <string.h>
#include "gntentry.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static void
gnt_entry_draw(GntWidget *widget)
{
	GntEntry *entry = GNT_ENTRY(widget);
	int stop;

	wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_TEXT_NORMAL));
	mvwprintw(widget->window, 0, 0, entry->scroll);

	stop = entry->end - entry->scroll;
	if (stop < widget->priv.width)
		mvwhline(widget->window, 0, stop, ENTRY_CHAR, widget->priv.width - stop);

	DEBUG;
}

static void
gnt_entry_size_request(GntWidget *widget)
{
	GntEntry *entry = GNT_ENTRY(widget);

	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
	{
		widget->priv.height = 1;
		widget->priv.width = 20;
	}
}

static void
gnt_entry_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	DEBUG;
}

static void
entry_redraw(GntWidget *widget)
{
	gnt_entry_draw(widget);
	gnt_widget_queue_update(widget);
}

static gboolean
gnt_entry_key_pressed(GntWidget *widget, const char *text)
{
	GntEntry *entry = GNT_ENTRY(widget);

	if (text[0] == 27)
	{
		if (strcmp(text + 1, GNT_KEY_DEL) == 0 && entry->cursor < entry->end)
		{
			memmove(entry->cursor, entry->cursor + 1, entry->end - entry->cursor + 1);
			entry->end--;
			entry_redraw(widget);
		}
		else if (strcmp(text + 1, GNT_KEY_LEFT) == 0 && entry->cursor > entry->start)
		{
			entry->cursor--;
			if (entry->cursor < entry->scroll)
				entry->scroll--;
			entry_redraw(widget);
		}
		else if (strcmp(text + 1, GNT_KEY_RIGHT) == 0 && entry->cursor < entry->end)
		{
			entry->cursor++;
			if (entry->cursor - entry->scroll > widget->priv.width)
				entry->scroll++;
			entry_redraw(widget);
		}
		/* XXX: handle other keys, like home/end, and ctrl+ goodness */
	}
	else
	{
		if (!iscntrl(text[0]))
		{
			int i;

			for (i = 0; text[i]; i++)
			{
				/* Valid input? */
				if (ispunct(text[i]) && (entry->flag & GNT_ENTRY_FLAG_NO_PUNCT))
					continue;
				if (isspace(text[i]) && (entry->flag & GNT_ENTRY_FLAG_NO_SPACE))
					continue;
				if (isalpha(text[i]) && !(entry->flag & GNT_ENTRY_FLAG_ALPHA))
					continue;
				if (isdigit(text[i]) && !(entry->flag & GNT_ENTRY_FLAG_INT))
					continue;

				/* Reached the max? */
				if (entry->max && entry->end - entry->start >= entry->max)
					continue;

				if (entry->end - entry->start >= entry->buffer)
				{
					char *tmp = g_strdup_printf(entry->start);
					gnt_entry_set_text(entry, tmp);
					g_free(tmp);
				}

				*(entry->cursor) = text[i];
				entry->cursor++;

				entry->end++;
				if (entry->cursor - entry->scroll > widget->priv.width)
					entry->scroll++;
			}
			entry_redraw(widget);
			return TRUE;
		}
		else
		{
			/* Backspace is here */
			if (strcmp(text, GNT_KEY_BACKSPACE) == 0 && entry->cursor > entry->start)
			{
				entry->cursor--;
				memmove(entry->cursor, entry->cursor + 1, entry->end - entry->cursor);
				entry->end--;

				if (entry->scroll > entry->start)
					entry->scroll--;

				entry_redraw(widget);
			}
		}
	}

	return FALSE;
}

static void
gnt_entry_destroy(GntWidget *widget)
{
	GntEntry *entry = GNT_ENTRY(widget);
	g_free(entry->start);
}

static void
gnt_entry_class_init(GntEntryClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_entry_destroy;
	parent_class->draw = gnt_entry_draw;
	parent_class->map = gnt_entry_map;
	parent_class->size_request = gnt_entry_size_request;
	parent_class->key_pressed = gnt_entry_key_pressed;

	DEBUG;
}

static void
gnt_entry_init(GTypeInstance *instance, gpointer class)
{
	GntEntry *entry = GNT_ENTRY(instance);

	entry->flag = GNT_ENTRY_FLAG_ALL;
	entry->max = 0;

	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(entry),
			GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW | GNT_WIDGET_CAN_TAKE_FOCUS);
	
	DEBUG;
}

/******************************************************************************
 * GntEntry API
 *****************************************************************************/
GType
gnt_entry_get_gtype(void)
{
	static GType type = 0;

	if(type == 0)
	{
		static const GTypeInfo info = {
			sizeof(GntEntryClass),
			NULL,					/* base_init		*/
			NULL,					/* base_finalize	*/
			(GClassInitFunc)gnt_entry_class_init,
			NULL,					/* class_finalize	*/
			NULL,					/* class_data		*/
			sizeof(GntEntry),
			0,						/* n_preallocs		*/
			gnt_entry_init,			/* instance_init	*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntEntry",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_entry_new(const char *text)
{
	GntWidget *widget = g_object_new(GNT_TYPE_ENTRY, NULL);
	GntEntry *entry = GNT_ENTRY(widget);

	gnt_entry_set_text(entry, text);

	return widget;
}

void gnt_entry_set_text(GntEntry *entry, const char *text)
{
	int len;
	int scroll, cursor;

	g_free(entry->start);

	if (text && text[0])
	{
		len = g_utf8_strlen(text, -1);
		entry->buffer = len * 2;
	}
	else
	{
		entry->buffer = 128;
		len = 0;
	}

	scroll = entry->scroll - entry->start;
	cursor = entry->end - entry->cursor;

	entry->start = g_new0(char, entry->buffer);
	if (text)
		snprintf(entry->start, len + 1, "%s", text);
	entry->end = entry->start + len;

	entry->scroll = entry->start + scroll;
	entry->cursor = entry->end - cursor;

	if (GNT_WIDGET_IS_FLAG_SET(GNT_WIDGET(entry), GNT_WIDGET_MAPPED))
		entry_redraw(GNT_WIDGET(entry));
}

void gnt_entry_set_max(GntEntry *entry, int max)
{
	entry->max = max;
}

void gnt_entry_set_flag(GntEntry *entry, GntEntryFlag flag)
{
	entry->flag = flag;
	/* XXX: Check the existing string to make sure the flags are respected? */
}

const char *gnt_entry_get_text(GntEntry *entry)
{
	return entry->start;
}

void gnt_entry_clear(GntEntry *entry)
{
	gnt_entry_set_text(entry, NULL);
	entry->scroll = entry->cursor = entry->end = entry->start;
	entry_redraw(GNT_WIDGET(entry));
}


