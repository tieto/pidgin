#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "gntbox.h"
#include "gntentry.h"
#include "gnttree.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;
static guint signals[SIGS] = { 0 };

static int
get_onscreen_width(const char *start, const char *end)
{
	wchar_t wch;
	int size;
	int width = 0;

	while (start < end) {
		if ((size = mbtowc(&wch, start, end - start)) > 0) {
			start += size;
			width += wcwidth(wch);
		} else {
			++width;
			++start;
		}
	}
	return width;
}

static void
destroy_suggest(GntEntry *entry)
{
	if (entry->ddown)
	{
		gnt_widget_destroy(entry->ddown->parent);
		entry->ddown = NULL;
	}
}

static char *
get_beginning_of_word(GntEntry *entry)
{
	char *s = entry->cursor;
	while (s > entry->start)
	{
		char *t = g_utf8_find_prev_char(entry->start, s);
		if ((*t < 'A' || *t > 'Z') && (*t < 'a' || *t > 'z'))
			break;
		s = t;
	}
	return s;
}

static gboolean
show_suggest_dropdown(GntEntry *entry)
{
	char *suggest = NULL;
	int len;
	int offset = 0, x, y;
	int count = 0;
	GList *iter;

	if (entry->word)
	{
		char *s = get_beginning_of_word(entry);
		suggest = g_strndup(s, entry->cursor - s);
		if (entry->scroll < s)
			offset = get_onscreen_width(entry->scroll, s);
	}
	else
		suggest = g_strdup(entry->start);
	len = strlen(suggest);  /* Don't need to use the utf8-function here */
	
	if (entry->ddown == NULL)
	{
		GntWidget *box = gnt_vbox_new(FALSE);
		entry->ddown = gnt_tree_new();
		gnt_tree_set_compare_func(GNT_TREE(entry->ddown), (GCompareFunc)g_utf8_collate);
		gnt_box_add_widget(GNT_BOX(box), entry->ddown);

		GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_TRANSIENT);

		gnt_widget_get_position(GNT_WIDGET(entry), &x, &y);
		x += offset;
		y++;
		if (y + 10 >= getmaxy(stdscr))
			y -= 11;
		gnt_widget_set_position(box, x, y);
		
		gnt_widget_draw(box);
	}
	else
		gnt_tree_remove_all(GNT_TREE(entry->ddown));

	for (count = 0, iter = entry->suggests; iter; iter = iter->next)
	{
		const char *text = iter->data;
		if (g_ascii_strncasecmp(suggest, text, len) == 0 && strlen(text) >= len)
		{
			gnt_tree_add_row_after(GNT_TREE(entry->ddown), (gpointer)text,
					gnt_tree_create_row(GNT_TREE(entry->ddown), text),
					NULL, NULL);
			count++;
		}
	}
	g_free(suggest);

	if (count == 0)
	{
		destroy_suggest(entry);
		return FALSE;
	}

	return TRUE;
}

static void
gnt_entry_draw(GntWidget *widget)
{
	GntEntry *entry = GNT_ENTRY(widget);
	int stop;
	gboolean focus;

	if ((focus = gnt_widget_has_focus(widget)))
		wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_TEXT_NORMAL));
	else
		wbkgdset(widget->window, '\0' | COLOR_PAIR(GNT_COLOR_HIGHLIGHT_D));

	if (entry->masked)
	{
		mvwhline(widget->window, 0, 0, gnt_ascii_only() ? '*' : ACS_BULLET,
				g_utf8_pointer_to_offset(entry->scroll, entry->end));
	}
	else
		mvwprintw(widget->window, 0, 0, "%s", entry->scroll);

	stop = get_onscreen_width(entry->scroll, entry->end);
	if (stop < widget->priv.width)
		whline(widget->window, ENTRY_CHAR, widget->priv.width - stop);

	if (focus)
		mvwchgat(widget->window, 0, get_onscreen_width(entry->scroll, entry->cursor),
				1, A_REVERSE, COLOR_PAIR(GNT_COLOR_TEXT_NORMAL), NULL);

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

static void
move_back(GntEntry *entry)
{
	if (entry->cursor <= entry->start)
		return;
	entry->cursor = g_utf8_find_prev_char(entry->start, entry->cursor);
	if (entry->cursor < entry->scroll)
		entry->scroll = entry->cursor;
	entry_redraw(GNT_WIDGET(entry));
}

static void
move_forward(GntEntry *entry)
{
	if (entry->cursor >= entry->end)
		return;
	entry->cursor = g_utf8_find_next_char(entry->cursor, NULL);
	while (get_onscreen_width(entry->scroll, entry->cursor) >= GNT_WIDGET(entry)->priv.width)
		entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
	entry_redraw(GNT_WIDGET(entry));
}

static void
backspace(GntEntry *entry)
{
	int len;

	if (entry->cursor <= entry->start)
		return;
	
	len = entry->cursor - g_utf8_find_prev_char(entry->start, entry->cursor);
	entry->cursor -= len;
	memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor);
	entry->end -= len;

	if (entry->scroll > entry->start)
		entry->scroll = g_utf8_find_prev_char(entry->start, entry->scroll);

	entry_redraw(GNT_WIDGET(entry));
	if (entry->ddown)
		show_suggest_dropdown(entry);
}

static void
delkey(GntEntry *entry)
{
	int len;

	if (entry->cursor >= entry->end)
		return;
	
	len = g_utf8_find_next_char(entry->cursor, NULL) - entry->cursor;
	memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor - len + 1);
	entry->end -= len;
	entry_redraw(GNT_WIDGET(entry));

	if (entry->ddown)
		show_suggest_dropdown(entry);
}

static void
move_start(GntEntry *entry)
{
	entry->scroll = entry->cursor = entry->start;
	entry_redraw(GNT_WIDGET(entry));
}

static void
move_end(GntEntry *entry)
{
	entry->cursor = entry->end;
	/* This should be better than this */
	while (get_onscreen_width(entry->scroll, entry->cursor) >= GNT_WIDGET(entry)->priv.width)
		entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
	entry_redraw(GNT_WIDGET(entry));
}

static gboolean
gnt_entry_key_pressed(GntWidget *widget, const char *text)
{
	GntEntry *entry = GNT_ENTRY(widget);

	if (text[0] == 27)
	{
		if (strcmp(text + 1, GNT_KEY_DEL) == 0 && entry->cursor < entry->end)
		{
			delkey(entry);
			return TRUE;
		}
		else if (strcmp(text + 1, GNT_KEY_LEFT) == 0)
		{
			move_back(entry);
			return TRUE;
		}
		else if (strcmp(text + 1, GNT_KEY_RIGHT) == 0)
		{
			move_forward(entry);
			return TRUE;
		}
		else if (strcmp(text + 1, GNT_KEY_HOME) == 0)
		{
			move_start(entry);
			return TRUE;
		}
		else if (strcmp(text + 1, GNT_KEY_END) == 0)
		{
			move_end(entry);
			return TRUE;
		}
		else if (strcmp(text + 1, GNT_KEY_CTRL_DOWN) == 0 && entry->histlength)
		{
			if (entry->history->prev)
			{
				entry->history = entry->history->prev;
				gnt_entry_set_text(entry, entry->history->data);
				destroy_suggest(entry);

				return TRUE;
			}
		}
		else if (strcmp(text + 1, GNT_KEY_UP) == 0 ||
				strcmp(text + 1, GNT_KEY_DOWN) == 0)
		{
			if (entry->ddown)
			{
				gnt_widget_key_pressed(entry->ddown, text);
				return TRUE;
			}
		}
		else if (strcmp(text + 1, GNT_KEY_CTRL_UP) == 0 && entry->histlength)
		{
			if (entry->history->next)
			{
				if (entry->history->prev == NULL)
				{
					/* Save the current contents */
					char *text = g_strdup(gnt_entry_get_text(entry));
					g_free(entry->history->data);
					entry->history->data = text;
				}

				entry->history = entry->history->next;
				gnt_entry_set_text(entry, entry->history->data);
				destroy_suggest(entry);

				return TRUE;
			}
		}
		else if (text[1] == 0)
		{
			destroy_suggest(entry);
		}

		return FALSE;
	}
	else
	{
		if (text[0] == '\t')
		{
			if (entry->ddown)
				destroy_suggest(entry);
			else if (entry->suggests)
				return show_suggest_dropdown(entry);

			return FALSE;
		}
		else if (text[0] == '\r' && entry->ddown)
		{
			char *text = g_strdup(gnt_tree_get_selection_data(GNT_TREE(entry->ddown)));
			destroy_suggest(entry);
			if (entry->word)
			{
				char *s = get_beginning_of_word(entry);
				char *iter = text;
				while (*iter && toupper(*s) == toupper(*iter))
				{
					*s++ = *iter++;
				}
				gnt_entry_key_pressed(widget, iter);
			}
			else
			{
				gnt_entry_set_text(entry, text);
			}
			g_free(text);
			return TRUE;
		}

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
				{
					if (*str == '\r' || *str == '\n')
						*entry->cursor = ' ';
					else
						*entry->cursor = *str;
					entry->cursor++;
					str++;
				}

				while (get_onscreen_width(entry->scroll, entry->cursor) >= widget->priv.width)
					entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);

				if (entry->ddown)
					show_suggest_dropdown(entry);
			}
			entry_redraw(widget);
			return TRUE;
		}
		else
		{
			if (strcmp(text, GNT_KEY_BACKSPACE) == 0 && entry->cursor > entry->start)
			{
				backspace(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_A) == 0)
			{
				move_start(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_B) == 0)
			{
				move_back(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_D) == 0)
			{
				delkey(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_E) == 0)
			{
				move_end(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_F) == 0)
			{
				move_forward(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_H) == 0)
			{
				backspace(entry);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_K) == 0)
			{
				entry->end = entry->cursor;
				memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
				entry_redraw(widget);
				return TRUE;
			}
			else if (strcmp(text, GNT_KEY_CTRL_U) == 0)
			{
				memmove(entry->start, entry->cursor, entry->end - entry->cursor);
				entry->end -= (entry->cursor - entry->start);
				entry->cursor = entry->scroll = entry->start;
				memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
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

	if (entry->history)
	{
		entry->history = g_list_first(entry->history);
		g_list_foreach(entry->history, (GFunc)g_free, NULL);
		g_list_free(entry->history);
	}

	if (entry->suggests)
	{
		g_list_foreach(entry->suggests, (GFunc)g_free, NULL);
		g_list_free(entry->suggests);
	}

	if (entry->ddown)
	{
		gnt_widget_destroy(entry->ddown->parent);
	}
}

static void
gnt_entry_lost_focus(GntWidget *widget)
{
	GntEntry *entry = GNT_ENTRY(widget);
	destroy_suggest(entry);
	entry_redraw(widget);
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
	parent_class->lost_focus = gnt_entry_lost_focus;

	DEBUG;
}

static void
gnt_entry_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GntEntry *entry = GNT_ENTRY(instance);

	entry->flag = GNT_ENTRY_FLAG_ALL;
	entry->max = 0;
	
	entry->histlength = 0;
	entry->history = NULL;

	entry->word = TRUE;
	entry->always = FALSE;
	entry->suggests = NULL;

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
	destroy_suggest(entry);
}

void gnt_entry_set_masked(GntEntry *entry, gboolean set)
{
	entry->masked = set;
}

void gnt_entry_add_to_history(GntEntry *entry, const char *text)
{
	g_return_if_fail(entry->history != NULL);   /* Need to set_history_length first */

	if (g_list_length(entry->history) >= entry->histlength)
		return;

	entry->history = g_list_first(entry->history);
	g_free(entry->history->data);
	entry->history->data = g_strdup(text);
	entry->history = g_list_prepend(entry->history, NULL);
}

void gnt_entry_set_history_length(GntEntry *entry, int num)
{
	if (num == 0)
	{
		entry->histlength = num;
		if (entry->history)
		{
			entry->history = g_list_first(entry->history);
			g_list_foreach(entry->history, (GFunc)g_free, NULL);
			g_list_free(entry->history);
			entry->history = NULL;
		}
		return;
	}

	if (entry->histlength == 0)
	{
		entry->histlength = num;
		entry->history = g_list_append(NULL, NULL);
		return;
	}

	if (num > 0 && num < entry->histlength)
	{
		GList *first, *iter;
		int index = 0;
		for (first = entry->history, index = 0; first->prev; first = first->prev, index++);
		while ((iter = g_list_nth(first, num)) != NULL)
		{
			g_free(iter->data);
			first = g_list_delete_link(first, iter);
		}
		entry->histlength = num;
		if (index >= num)
			entry->history = g_list_last(first);
		return;
	}

	entry->histlength = num;
}

void gnt_entry_set_word_suggest(GntEntry *entry, gboolean word)
{
	entry->word = word;
}

void gnt_entry_set_always_suggest(GntEntry *entry, gboolean always)
{
	entry->always = always;
}

void gnt_entry_add_suggest(GntEntry *entry, const char *text)
{
	GList *find;

	if (!text || !*text)
		return;
	
	find = g_list_find_custom(entry->suggests, text, (GCompareFunc)g_utf8_collate);
	if (find)
		return;
	entry->suggests = g_list_append(entry->suggests, g_strdup(text));
}

void gnt_entry_remove_suggest(GntEntry *entry, const char *text)
{
	GList *find = g_list_find_custom(entry->suggests, text, (GCompareFunc)g_utf8_collate);
	if (find)
	{
		g_free(find->data);
		entry->suggests = g_list_delete_link(entry->suggests, find);
	}
}

