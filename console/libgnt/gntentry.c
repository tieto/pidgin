#include <ctype.h>
#include <string.h>

#include "gntbox.h"
#include "gntentry.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"

enum
{
	SIGS = 1,
};

static GntWidgetClass *parent_class = NULL;

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
		if (isspace(*t))
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
			offset = gnt_util_onscreen_width(entry->scroll, s);
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
		/* XXX: Connect to the "activate" signal for the dropdown tree */

		GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_TRANSIENT);

		gnt_widget_get_position(GNT_WIDGET(entry), &x, &y);
		x += offset;
		y++;
		if (y + 10 >= getmaxy(stdscr))
			y -= 11;
		gnt_widget_set_position(box, x, y);
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

	gnt_widget_draw(entry->ddown->parent);
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

	stop = gnt_util_onscreen_width(entry->scroll, entry->end);
	if (stop < widget->priv.width)
		whline(widget->window, ENTRY_CHAR, widget->priv.width - stop);

	if (focus)
		mvwchgat(widget->window, 0, gnt_util_onscreen_width(entry->scroll, entry->cursor),
				1, A_REVERSE, GNT_COLOR_TEXT_NORMAL, NULL);

	GNTDEBUG;
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
	GNTDEBUG;
}

static void
entry_redraw(GntWidget *widget)
{
	gnt_entry_draw(widget);
	gnt_widget_queue_update(widget);
}

static gboolean
move_back(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->cursor <= entry->start)
		return FALSE;
	entry->cursor = g_utf8_find_prev_char(entry->start, entry->cursor);
	if (entry->cursor < entry->scroll)
		entry->scroll = entry->cursor;
	entry_redraw(GNT_WIDGET(entry));
	return TRUE;
}

static gboolean
move_forward(GntBindable *bind, GList *list)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->cursor >= entry->end)
		return FALSE;
	entry->cursor = g_utf8_find_next_char(entry->cursor, NULL);
	while (gnt_util_onscreen_width(entry->scroll, entry->cursor) >= GNT_WIDGET(entry)->priv.width)
		entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
	entry_redraw(GNT_WIDGET(entry));
	return TRUE;
}

static gboolean
backspace(GntBindable *bind, GList *null)
{
	int len;
	GntEntry *entry = GNT_ENTRY(bind);

	if (entry->cursor <= entry->start)
		return TRUE;
	
	len = entry->cursor - g_utf8_find_prev_char(entry->start, entry->cursor);
	entry->cursor -= len;
	memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor);
	entry->end -= len;

	if (entry->scroll > entry->start)
		entry->scroll = g_utf8_find_prev_char(entry->start, entry->scroll);

	entry_redraw(GNT_WIDGET(entry));
	if (entry->ddown)
		show_suggest_dropdown(entry);
	return TRUE;
}

static gboolean
delkey(GntBindable *bind, GList *null)
{
	int len;
	GntEntry *entry = GNT_ENTRY(bind);

	if (entry->cursor >= entry->end)
		return FALSE;
	
	len = g_utf8_find_next_char(entry->cursor, NULL) - entry->cursor;
	memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor - len + 1);
	entry->end -= len;
	entry_redraw(GNT_WIDGET(entry));

	if (entry->ddown)
		show_suggest_dropdown(entry);
	return TRUE;
}

static gboolean
move_start(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	entry->scroll = entry->cursor = entry->start;
	entry_redraw(GNT_WIDGET(entry));
	return TRUE;
}

static gboolean
move_end(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	entry->cursor = entry->end;
	/* This should be better than this */
	while (gnt_util_onscreen_width(entry->scroll, entry->cursor) >= GNT_WIDGET(entry)->priv.width)
		entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
	entry_redraw(GNT_WIDGET(entry));
	return TRUE;
}

static gboolean
history_prev(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->histlength && entry->history->prev)
	{
		entry->history = entry->history->prev;
		gnt_entry_set_text(entry, entry->history->data);
		destroy_suggest(entry);

		return TRUE;
	}
	return FALSE;
}

static gboolean
history_next(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->histlength && entry->history->next)
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
	return FALSE;
}

static gboolean
suggest_show(GntBindable *bind, GList *null)
{
	return show_suggest_dropdown(GNT_ENTRY(bind));
}

static gboolean
suggest_next(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->ddown) {
		gnt_bindable_perform_action_named(GNT_BINDABLE(entry->ddown), "move-down", NULL);
		return TRUE;
	}
	return FALSE;
}

static gboolean
suggest_prev(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->ddown) {
		gnt_bindable_perform_action_named(GNT_BINDABLE(entry->ddown), "move-up", NULL);
		return TRUE;
	}
	return FALSE;
}

static gboolean
del_to_home(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	memmove(entry->start, entry->cursor, entry->end - entry->cursor);
	entry->end -= (entry->cursor - entry->start);
	entry->cursor = entry->scroll = entry->start;
	memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
	entry_redraw(GNT_WIDGET(bind));
	return TRUE;
}

static gboolean
del_to_end(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	entry->end = entry->cursor;
	memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
	entry_redraw(GNT_WIDGET(bind));
	return TRUE;
}

#define SAME(a,b)    ((g_unichar_isalpha(a) && g_unichar_isalpha(b)) || \
				(g_unichar_isdigit(a) && g_unichar_isdigit(b)) || \
				(g_unichar_isspace(a) && g_unichar_isspace(b)) || \
				(g_unichar_iswide(a) && g_unichar_iswide(b)))

static const char *
begin_word(const char *text, const char *begin)
{
	gunichar ch = 0;
	while (text > begin && (!*text || g_unichar_isspace(g_utf8_get_char(text))))
		text = g_utf8_find_prev_char(begin, text);
	ch = g_utf8_get_char(text);
	while ((text = g_utf8_find_prev_char(begin, text)) >= begin) {
		gunichar cur = g_utf8_get_char(text);
		if (!SAME(ch, cur))
			break;
	}

	return (text ? g_utf8_find_next_char(text, NULL) : begin);
}

static const char *
next_begin_word(const char *text, const char *end)
{
	gunichar ch = 0;
	ch = g_utf8_get_char(text);
	while ((text = g_utf8_find_next_char(text, end)) != NULL && text <= end) {
		gunichar cur = g_utf8_get_char(text);
		if (!SAME(ch, cur))
			break;
	}

	while (text && text < end && g_unichar_isspace(g_utf8_get_char(text)))
		text = g_utf8_find_next_char(text, end);
	return (text ? text : end);
}

#undef SAME
static gboolean
move_back_word(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	const char *iter = g_utf8_find_prev_char(entry->start, entry->cursor);

	if (iter < entry->start)
		return TRUE;
	iter = begin_word(iter, entry->start);
	entry->cursor = (char*)iter;
	if (entry->cursor < entry->scroll)
		entry->scroll = entry->cursor;
	entry_redraw(GNT_WIDGET(bind));
	return TRUE;
}

static gboolean
del_prev_word(GntBindable *bind, GList *null)
{
	GntWidget *widget = GNT_WIDGET(bind);
	GntEntry *entry = GNT_ENTRY(bind);
	char *iter = g_utf8_find_prev_char(entry->start, entry->cursor);
	int count;

	if (iter < entry->start)
		return TRUE;
	iter = (char*)begin_word(iter, entry->start);
	count = entry->cursor - iter;
	memmove(iter, entry->cursor, entry->end - entry->cursor);
	entry->end -= count;
	entry->cursor = iter;
	if (entry->cursor <= entry->scroll) {
		entry->scroll = entry->cursor - widget->priv.width + 2;
		if (entry->scroll < entry->start)
			entry->scroll = entry->start;
	}
	memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
	entry_redraw(widget);

	return TRUE;
}

static gboolean
move_forward_word(GntBindable *bind, GList *list)
{
	GntEntry *entry = GNT_ENTRY(bind);
	GntWidget *widget = GNT_WIDGET(bind);
	entry->cursor = (char *)next_begin_word(entry->cursor, entry->end);
	while (gnt_util_onscreen_width(entry->scroll, entry->cursor) >= widget->priv.width) {
		entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);
	}
	entry_redraw(widget);
	return TRUE;
}

static gboolean
delete_forward_word(GntBindable *bind, GList *list)
{
	GntEntry *entry = GNT_ENTRY(bind);
	GntWidget *widget = GNT_WIDGET(bind);
	char *iter = (char *)next_begin_word(entry->cursor, entry->end);
	int len = entry->end - iter + 1;
	memmove(entry->cursor, iter, len);
	len = iter - entry->cursor;
	entry->end -= len;
	memset(entry->end, '\0', len);
	entry_redraw(widget);
	return TRUE;
}

static gboolean
gnt_entry_key_pressed(GntWidget *widget, const char *text)
{
	GntEntry *entry = GNT_ENTRY(widget);

	if (text[0] == 27)
	{
		if (text[1] == 0)
		{
			destroy_suggest(entry);
			return TRUE;
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

			for (str = text; *str; str = next)
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

				if (entry->end + len - entry->start >= entry->buffer)
				{
					/* This will cause the buffer to grow */
					char *tmp = g_strdup_printf("%s%*s", entry->start, len, "");
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

				while (gnt_util_onscreen_width(entry->scroll, entry->cursor) >= widget->priv.width)
					entry->scroll = g_utf8_find_next_char(entry->scroll, NULL);

				if (entry->ddown)
					show_suggest_dropdown(entry);
			}
			entry_redraw(widget);
			return TRUE;
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
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	char s[2] = {erasechar(), 0};

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->destroy = gnt_entry_destroy;
	parent_class->draw = gnt_entry_draw;
	parent_class->map = gnt_entry_map;
	parent_class->size_request = gnt_entry_size_request;
	parent_class->key_pressed = gnt_entry_key_pressed;
	parent_class->lost_focus = gnt_entry_lost_focus;

	gnt_bindable_class_register_action(bindable, "cursor-home", move_start,
				GNT_KEY_CTRL_A, NULL);
	gnt_bindable_register_binding(bindable, "cursor-home", GNT_KEY_HOME, NULL);
	gnt_bindable_class_register_action(bindable, "cursor-end", move_end,
				GNT_KEY_CTRL_E, NULL);
	gnt_bindable_register_binding(bindable, "cursor-end", GNT_KEY_END, NULL);
	gnt_bindable_class_register_action(bindable, "delete-prev", backspace,
				GNT_KEY_BACKSPACE, NULL);
	gnt_bindable_register_binding(bindable, "delete-prev", s, NULL);
	gnt_bindable_class_register_action(bindable, "delete-next", delkey,
				GNT_KEY_DEL, NULL);
	gnt_bindable_register_binding(bindable, "delete-next", GNT_KEY_CTRL_D, NULL);
	gnt_bindable_class_register_action(bindable, "delete-start", del_to_home,
				GNT_KEY_CTRL_U, NULL);
	gnt_bindable_class_register_action(bindable, "delete-end", del_to_end,
				GNT_KEY_CTRL_K, NULL);
	gnt_bindable_class_register_action(bindable, "delete-prev-word", del_prev_word,
				GNT_KEY_CTRL_W, NULL);
	gnt_bindable_class_register_action(bindable, "cursor-prev-word", move_back_word,
				"\033" "b", NULL);
	gnt_bindable_class_register_action(bindable, "cursor-prev", move_back,
				GNT_KEY_LEFT, NULL);
	gnt_bindable_register_binding(bindable, "cursor-prev", GNT_KEY_CTRL_B, NULL);
	gnt_bindable_class_register_action(bindable, "cursor-next", move_forward,
				GNT_KEY_RIGHT, NULL);
	gnt_bindable_register_binding(bindable, "cursor-next", GNT_KEY_CTRL_F, NULL);
	gnt_bindable_class_register_action(bindable, "cursor-next-word", move_forward_word,
				"\033" "f", NULL);
	gnt_bindable_class_register_action(bindable, "delete-next-word", delete_forward_word,
				"\033" "d", NULL);
	gnt_bindable_class_register_action(bindable, "suggest-show", suggest_show,
				"\t", NULL);
	gnt_bindable_class_register_action(bindable, "suggest-next", suggest_next,
				GNT_KEY_DOWN, NULL);
	gnt_bindable_class_register_action(bindable, "suggest-prev", suggest_prev,
				GNT_KEY_UP, NULL);
	gnt_bindable_class_register_action(bindable, "history-prev", history_prev,
				GNT_KEY_CTRL_DOWN, NULL);
	gnt_bindable_class_register_action(bindable, "history-next", history_next,
				GNT_KEY_CTRL_UP, NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
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
	
	GNTDEBUG;
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

