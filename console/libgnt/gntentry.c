#include <ctype.h>
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

	if (gnt_widget_has_focus(widget))
		wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_TEXT_NORMAL));
	else
		wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));

	if (entry->masked)
	{
		mvwhline(widget->window, 0, 0, gnt_ascii_only() ? '*' : ACS_BULLET,
				g_utf8_pointer_to_offset(entry->scroll, entry->end));
	}
	else
		mvwprintw(widget->window, 0, 0, entry->scroll);

	stop = g_utf8_pointer_to_offset(entry->scroll, entry->end);
	if (stop < widget->priv.width)
		mvwhline(widget->window, 0, stop, ENTRY_CHAR, widget->priv.width - stop);

	mvwchgat(widget->window, 0, g_utf8_pointer_to_offset(entry->scroll, entry->cursor), 1,
			A_REVERSE, COLOR_PAIR(GNT_COLOR_TEXT_NORMAL), NULL);

	DEBUG;
}

static void
gnt_entry_size_request(GntWidget *widget)
{
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
			int len = g_utf8_find_next_char(entry->cursor, NULL) - entry->cursor;
			memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor - len + 1);
			entry->end -= len;
			entry_redraw(widget);
		}
		else if (strcmp(text + 1, GNT_KEY_LEFT) == 0 && entry->cursor > entry->start)
		{
			entry->cursor = g_utf8_find_prev_char(entry->start, entry->cursor);
			if (entry->cursor < entry->scroll)
				entry->scroll = entry->cursor;
			entry_redraw(widget);
		}
		else if (strcmp(text + 1, GNT_KEY_RIGHT) == 0 && entry->cursor < entry->end)
		{
			entry->cursor = g_utf8_find_next_char(entry->cursor, NULL);
			if (g_utf8_pointer_to_offset(entry->scroll, entry->cursor) >= widget->priv.width)
				entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
			entry_redraw(widget);
		}
		/* XXX: handle other keys, like home/end, and ctrl+ goodness */
		else
			return FALSE;

		return TRUE;
	}
	else
	{
		if (!iscntrl(text[0]))
		{
			const char *str, *next;

			for (str = text; *str;)
			{
				int len;
				next = g_utf8_find_next_char(str, NULL);
				len = next - str;

				/* Valid input? */
				/* XXX: Is it necessary to use _unichar_ variants here? */
				if (ispunct(*str) && (entry->flag & GNT_ENTRY_FLAG_NO_PUNCT))
					continue;
				if (isspace(*str) && (entry->flag & GNT_ENTRY_FLAG_NO_SPACE))
					continue;
				if (isalpha(*str) && !(entry->flag & GNT_ENTRY_FLAG_ALPHA))
					continue;
				if (isdigit(*str) && !(entry->flag & GNT_ENTRY_FLAG_INT))
					continue;

				/* Reached the max? */
				if (entry->max && g_utf8_pointer_to_offset(entry->start, entry->end) >= entry->max)
					continue;

				if (entry->end - entry->start >= entry->buffer)
				{
					char *tmp = g_strdup_printf(entry->start);
					gnt_entry_set_text(entry, tmp);
					g_free(tmp);
				}

				memmove(entry->cursor + len, entry->cursor, entry->end - entry->cursor + 1);
				entry->end += len;

				while (str < next)
					*(entry->cursor++) = *str++;

				while (g_utf8_pointer_to_offset(entry->scroll, entry->cursor) >= widget->priv.width)
					entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
			}
			entry_redraw(widget);
			return TRUE;
		}
		else
		{
			/* Backspace is here */
			if (strcmp(text, GNT_KEY_BACKSPACE) == 0 && entry->cursor > entry->start)
			{
				int len = entry->cursor - g_utf8_find_prev_char(entry->start, entry->cursor);
				entry->cursor -= len;
				memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor);
				entry->end -= len;

				if (entry->scroll > entry->start)
					entry->scroll = g_utf8_find_prev_char(entry->start, entry->scroll);

				entry_redraw(widget);
				return TRUE;
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
	GntWidget *widget = GNT_WIDGET(instance);
	GntEntry *entry = GNT_ENTRY(instance);

	entry->flag = GNT_ENTRY_FLAG_ALL;
	entry->max = 0;

	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(entry),
			GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW | GNT_WIDGET_CAN_TAKE_FOCUS);
	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(entry), GNT_WIDGET_GROW_X);

	widget->priv.minw = 3;
	widget->priv.minh = 1;
	
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
		len = strlen(text);
	}
	else
	{
		len = 0;
	}

	entry->buffer = len + 128;

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

void gnt_entry_set_masked(GntEntry *entry, gboolean set)
{
	entry->masked = set;
}

