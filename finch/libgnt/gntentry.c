/**
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <ctype.h>
#include <string.h>

#include "gntbox.h"
#include "gntentry.h"
#include "gntmarshal.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"

enum
{
	SIG_TEXT_CHANGED,
	SIG_COMPLETION,
	SIGS,
};

typedef enum
{
	ENTRY_JAIL = -1,    /* Suspend the kill ring. */
	ENTRY_DEL_BWD_WORD = 1,
	ENTRY_DEL_BWD_CHAR,
	ENTRY_DEL_FWD_WORD,
	ENTRY_DEL_FWD_CHAR,
	ENTRY_DEL_EOL,
	ENTRY_DEL_BOL,
} GntEntryAction;

struct _GntEntryKillRing
{
	GString *buffer;
	GntEntryAction last;
};

static guint signals[SIGS] = { 0 };

static GntWidgetClass *parent_class = NULL;

static gboolean gnt_entry_key_pressed(GntWidget *widget, const char *text);
static void gnt_entry_set_text_internal(GntEntry *entry, const char *text);

static gboolean
update_kill_ring(GntEntry *entry, GntEntryAction action, const char *text, int len)
{
	if (action < 0) {
		entry->killring->last = action;
		return FALSE;
	}

	if (len == 0)
		len = strlen(text);
	else if (len < 0) {
		text += len;
		len = -len;
	}

	if (action != entry->killring->last) {
		struct {
			GntEntryAction one;
			GntEntryAction two;
		} merges[] = {
			{ENTRY_DEL_BWD_WORD, ENTRY_DEL_FWD_WORD},
			{ENTRY_DEL_BWD_CHAR, ENTRY_DEL_FWD_CHAR},
			{ENTRY_DEL_BOL, ENTRY_DEL_EOL},
			{ENTRY_JAIL, ENTRY_JAIL},
		};
		int i;

		for (i = 0; merges[i].one != ENTRY_JAIL; i++) {
			if (merges[i].one == entry->killring->last &&
					merges[i].two == action) {
				g_string_append_len(entry->killring->buffer, text, len);
				break;
			} else if (merges[i].one == action &&
					merges[i].two == entry->killring->last) {
				g_string_prepend_len(entry->killring->buffer, text, len);
				break;
			}
		}
		if (merges[i].one == ENTRY_JAIL) {
			g_string_assign(entry->killring->buffer, text);
			g_string_truncate(entry->killring->buffer, len);
		}
		entry->killring->last = action;
	} else {
		if (action == ENTRY_DEL_BWD_CHAR || action == ENTRY_DEL_BWD_WORD)
			g_string_prepend_len(entry->killring->buffer, text, len);
		else
			g_string_append_len(entry->killring->buffer, text, len);
	}
	return TRUE;
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
		if (isspace(*t))
			break;
		s = t;
	}
	return s;
}

static gboolean
complete_suggest(GntEntry *entry, const char *text)
{
	int offstart = 0, offend = 0;

	if (entry->word) {
		char *s = get_beginning_of_word(entry);
		const char *iter = text;
		offstart = g_utf8_pointer_to_offset(entry->start, s);
		while (*iter && toupper(*s) == toupper(*iter)) {
			*s++ = *iter++;
		}
		if (*iter) {
			gnt_entry_key_pressed(GNT_WIDGET(entry), iter);
		}
		offend = g_utf8_pointer_to_offset(entry->start, entry->cursor);
	} else {
		offstart = 0;
		gnt_entry_set_text_internal(entry, text);
		offend = g_utf8_strlen(text, -1);
	}

	g_signal_emit(G_OBJECT(entry), signals[SIG_COMPLETION], 0,
			entry->start + offstart, entry->start + offend);
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
	return TRUE;
}

static int
max_common_prefix(const char *s, const char *t)
{
	const char *f = s;
	while (*f && *t && *f == *t++)
		f++;
	return f - s;
}

static gboolean
show_suggest_dropdown(GntEntry *entry)
{
	char *suggest = NULL;
	int len;
	int offset = 0, x, y;
	int count = 0;
	GList *iter;
	const char *text = NULL;
	const char *sgst = NULL;
	int max = -1;

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
		text = iter->data;
		if (g_ascii_strncasecmp(suggest, text, len) == 0 && strlen(text) >= len)
		{
			gnt_tree_add_row_after(GNT_TREE(entry->ddown), (gpointer)text,
					gnt_tree_create_row(GNT_TREE(entry->ddown), text),
					NULL, NULL);
			count++;
			if (max == -1)
				max = strlen(text) - len;
			else if (max)
				max = MIN(max, max_common_prefix(sgst + len, text + len));
			sgst = text;
		}
	}
	g_free(suggest);

	if (count == 0) {
		destroy_suggest(entry);
		return FALSE;
	} else if (count == 1) {
		char *store = g_strndup(entry->start, entry->end - entry->start);
		gboolean ret;

		destroy_suggest(entry);
		complete_suggest(entry, sgst);

		ret = (strncmp(store, entry->start, entry->end - entry->start) != 0);
		g_free(store);
		return ret;
	} else {
		if (max > 0) {
			GntWidget *ddown = entry->ddown;
			char *match = g_strndup(sgst + len, max);
			entry->ddown = NULL;
			gnt_entry_key_pressed(GNT_WIDGET(entry), match);
			g_free(match);
			if (entry->ddown)
				gnt_widget_destroy(ddown);
			else
				entry->ddown = ddown;
		}
		gnt_widget_draw(entry->ddown->parent);
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
		wbkgdset(widget->window, '\0' | gnt_color_pair(GNT_COLOR_TEXT_NORMAL));
	else
		wbkgdset(widget->window, '\0' | gnt_color_pair(GNT_COLOR_HIGHLIGHT_D));

	if (entry->masked)
	{
		mvwhline(widget->window, 0, 0, gnt_ascii_only() ? '*' : ACS_BULLET,
				g_utf8_pointer_to_offset(entry->scroll, entry->end));
	}
	else
		mvwprintw(widget->window, 0, 0, "%s", entry->scroll);

	stop = gnt_util_onscreen_width(entry->scroll, entry->end);
	if (stop < widget->priv.width)
		mvwhline(widget->window, 0, stop, ENTRY_CHAR, widget->priv.width - stop);

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

static void
entry_text_changed(GntEntry *entry)
{
	g_signal_emit(entry, signals[SIG_TEXT_CHANGED], 0);
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
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
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
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
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
	update_kill_ring(entry, ENTRY_DEL_BWD_CHAR, entry->cursor, -len);
	entry->cursor -= len;

	memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor);
	entry->end -= len;

	if (entry->scroll > entry->start)
		entry->scroll = g_utf8_find_prev_char(entry->start, entry->scroll);

	entry_redraw(GNT_WIDGET(entry));
	if (entry->ddown)
		show_suggest_dropdown(entry);
	entry_text_changed(entry);
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
	update_kill_ring(entry, ENTRY_DEL_FWD_CHAR, entry->cursor, len);
	memmove(entry->cursor, entry->cursor + len, entry->end - entry->cursor - len + 1);
	entry->end -= len;
	entry_redraw(GNT_WIDGET(entry));

	if (entry->ddown)
		show_suggest_dropdown(entry);
	entry_text_changed(entry);
	return TRUE;
}

static gboolean
move_start(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	entry->scroll = entry->cursor = entry->start;
	entry_redraw(GNT_WIDGET(entry));
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
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
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
	return TRUE;
}

static gboolean
history_next(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->histlength && entry->history->prev)
	{
		entry->history = entry->history->prev;
		gnt_entry_set_text_internal(entry, entry->history->data);
		destroy_suggest(entry);
		entry_text_changed(entry);

		update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
		return TRUE;
	}
	return FALSE;
}

static gboolean
history_prev(GntBindable *bind, GList *null)
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
		gnt_entry_set_text_internal(entry, entry->history->data);
		destroy_suggest(entry);
		entry_text_changed(entry);

		update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
		return TRUE;
	}
	return FALSE;
}

static gboolean
clipboard_paste(GntBindable *bind, GList *n)
{
	GntEntry *entry = GNT_ENTRY(bind);
	gchar *i, *text, *a, *all;
	text = i = gnt_get_clipboard_string();
	while (*i != '\0') {
		i = g_utf8_next_char(i);
		if (*i == '\r' || *i == '\n')
			*i = ' ';
	}
	a = g_strndup(entry->start, entry->cursor - entry->start);
	all = g_strconcat(a, text, entry->cursor, NULL);
	gnt_entry_set_text_internal(entry, all);
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
	g_free(a);
	g_free(text);
	g_free(all);
	return TRUE;
}

static gboolean
suggest_show(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->ddown) {
		gnt_bindable_perform_action_named(GNT_BINDABLE(entry->ddown), "move-down");
		return TRUE;
	}
	return show_suggest_dropdown(entry);
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
	if (entry->cursor <= entry->start)
		return TRUE;
	update_kill_ring(entry, ENTRY_DEL_BOL, entry->start, entry->cursor - entry->start);
	memmove(entry->start, entry->cursor, entry->end - entry->cursor);
	entry->end -= (entry->cursor - entry->start);
	entry->cursor = entry->scroll = entry->start;
	memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
	entry_redraw(GNT_WIDGET(bind));
	entry_text_changed(entry);
	return TRUE;
}

static gboolean
del_to_end(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	if (entry->end <= entry->cursor)
		return TRUE;
	update_kill_ring(entry, ENTRY_DEL_EOL, entry->cursor, entry->end - entry->cursor);
	entry->end = entry->cursor;
	memset(entry->end, '\0', entry->buffer - (entry->end - entry->start));
	entry_redraw(GNT_WIDGET(bind));
	entry_text_changed(entry);
	return TRUE;
}

#define SAME(a,b)    ((g_unichar_isalnum(a) && g_unichar_isalnum(b)) || \
				(g_unichar_isspace(a) && g_unichar_isspace(b)) || \
				(g_unichar_iswide(a) && g_unichar_iswide(b)) || \
				(g_unichar_ispunct(a) && g_unichar_ispunct(b)))

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

	while (text && text < end && g_unichar_isspace(g_utf8_get_char(text)))
		text = g_utf8_find_next_char(text, end);

	if (text) {
		ch = g_utf8_get_char(text);
		while ((text = g_utf8_find_next_char(text, end)) != NULL && text <= end) {
			gunichar cur = g_utf8_get_char(text);
			if (!SAME(ch, cur))
				break;
		}
	}
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
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
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
	update_kill_ring(entry, ENTRY_DEL_BWD_WORD, iter, count);
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
	entry_text_changed(entry);

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
	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
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
	if (len <= 0)
		return TRUE;
	update_kill_ring(entry, ENTRY_DEL_FWD_WORD, entry->cursor, iter - entry->cursor);
	memmove(entry->cursor, iter, len);
	len = iter - entry->cursor;
	entry->end -= len;
	memset(entry->end, '\0', len);
	entry_redraw(widget);
	entry_text_changed(entry);
	return TRUE;
}

static gboolean
transpose_chars(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	char *current, *prev;
	char hold[8];  /* that's right */

	if (entry->cursor <= entry->start)
		return FALSE;

	if (!*entry->cursor)
		entry->cursor = g_utf8_find_prev_char(entry->start, entry->cursor);

	current = entry->cursor;
	prev = g_utf8_find_prev_char(entry->start, entry->cursor);
	move_forward(bind, null);

	/* Let's do this dance! */
	memcpy(hold, prev, current - prev);
	memmove(prev, current, entry->cursor - current);
	memcpy(prev + (entry->cursor - current), hold, current - prev);

	update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
	entry_redraw(GNT_WIDGET(entry));
	entry_text_changed(entry);
	return TRUE;
}

static gboolean
entry_yank(GntBindable *bind, GList *null)
{
	GntEntry *entry = GNT_ENTRY(bind);
	gnt_entry_key_pressed(GNT_WIDGET(entry), entry->killring->buffer->str);
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

	if ((text[0] == '\r' || text[0] == ' ' || text[0] == '\n') && entry->ddown)
	{
		char *text = g_strdup(gnt_tree_get_selection_data(GNT_TREE(entry->ddown)));
		destroy_suggest(entry);
		complete_suggest(entry, text);
		g_free(text);
		update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
		entry_text_changed(entry);
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
				char *tmp = g_strdup(entry->start);
				gnt_entry_set_text_internal(entry, tmp);
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
		update_kill_ring(entry, ENTRY_JAIL, NULL, 0);
		entry_redraw(widget);
		entry_text_changed(entry);
		return TRUE;
	}

	if (text[0] == '\r' || text[0] == '\n') {
		gnt_widget_activate(widget);
		return TRUE;
	}

	return FALSE;
}

static void
jail_killring(GntEntryKillRing *kr)
{
	g_string_free(kr->buffer, TRUE);
	g_free(kr);
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

	jail_killring(entry->killring);
}

static void
gnt_entry_lost_focus(GntWidget *widget)
{
	GntEntry *entry = GNT_ENTRY(widget);
	destroy_suggest(entry);
	entry_redraw(widget);
}

static gboolean
gnt_entry_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	if (event == GNT_MIDDLE_MOUSE_DOWN) {
		clipboard_paste(GNT_BINDABLE(widget), NULL);
		return TRUE;
	}
	return FALSE;

}

static void
gnt_entry_class_init(GntEntryClass *klass)
{
	GntBindableClass *bindable = GNT_BINDABLE_CLASS(klass);
	char s[2] = {erasechar(), 0};

	parent_class = GNT_WIDGET_CLASS(klass);
	parent_class->clicked = gnt_entry_clicked;
	parent_class->destroy = gnt_entry_destroy;
	parent_class->draw = gnt_entry_draw;
	parent_class->map = gnt_entry_map;
	parent_class->size_request = gnt_entry_size_request;
	parent_class->key_pressed = gnt_entry_key_pressed;
	parent_class->lost_focus = gnt_entry_lost_focus;

	signals[SIG_TEXT_CHANGED] =
		g_signal_new("text_changed",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET(GntEntryClass, text_changed),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

	signals[SIG_COMPLETION] =
		g_signal_new("completion",
					 G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST,
					 0, NULL, NULL,
					 gnt_closure_marshal_VOID__POINTER_POINTER,
					 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	gnt_bindable_class_register_action(bindable, "cursor-home", move_start,
				GNT_KEY_CTRL_A, NULL);
	gnt_bindable_register_binding(bindable, "cursor-home", GNT_KEY_HOME, NULL);
	gnt_bindable_class_register_action(bindable, "cursor-end", move_end,
				GNT_KEY_CTRL_E, NULL);
	gnt_bindable_register_binding(bindable, "cursor-end", GNT_KEY_END, NULL);
	gnt_bindable_class_register_action(bindable, "delete-prev", backspace,
				GNT_KEY_BACKSPACE, NULL);
	gnt_bindable_register_binding(bindable, "delete-prev", s, NULL);
	gnt_bindable_register_binding(bindable, "delete-prev", GNT_KEY_CTRL_H, NULL);
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
	gnt_bindable_class_register_action(bindable, "transpose-chars", transpose_chars,
				GNT_KEY_CTRL_T, NULL);
	gnt_bindable_class_register_action(bindable, "yank", entry_yank,
				GNT_KEY_CTRL_Y, NULL);
	gnt_bindable_class_register_action(bindable, "suggest-show", suggest_show,
				"\t", NULL);
	gnt_bindable_class_register_action(bindable, "suggest-next", suggest_next,
				GNT_KEY_DOWN, NULL);
	gnt_bindable_class_register_action(bindable, "suggest-prev", suggest_prev,
				GNT_KEY_UP, NULL);
	gnt_bindable_class_register_action(bindable, "history-next", history_next,
				GNT_KEY_CTRL_DOWN, NULL);
	gnt_bindable_class_register_action(bindable, "history-prev", history_prev,
				GNT_KEY_CTRL_UP, NULL);
	gnt_bindable_register_binding(bindable, "history-prev", GNT_KEY_CTRL_P, NULL);
	gnt_bindable_register_binding(bindable, "history-next", GNT_KEY_CTRL_N, NULL);
	gnt_bindable_class_register_action(bindable, "clipboard-paste", clipboard_paste,
				GNT_KEY_CTRL_V, NULL);

	gnt_style_read_actions(G_OBJECT_CLASS_TYPE(klass), GNT_BINDABLE_CLASS(klass));
	GNTDEBUG;
}

static GntEntryKillRing *
new_killring(void)
{
	GntEntryKillRing *kr = g_new0(GntEntryKillRing, 1);
	kr->buffer = g_string_new(NULL);
	return kr;
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
	entry->killring = new_killring();

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
			NULL					/* value_table		*/
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

	gnt_entry_set_text_internal(entry, text);

	return widget;
}

static void
gnt_entry_set_text_internal(GntEntry *entry, const char *text)
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

void gnt_entry_set_text(GntEntry *entry, const char *text)
{
	gboolean changed = TRUE;
	if (text == NULL && entry->start == NULL)
		changed = FALSE;
	if (text && entry->start && g_utf8_collate(text, entry->start) == 0)
		changed = FALSE;
	gnt_entry_set_text_internal(entry, text);
	if (changed)
		entry_text_changed(entry);
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
	gnt_entry_set_text_internal(entry, NULL);
	entry->scroll = entry->cursor = entry->end = entry->start;
	entry_redraw(GNT_WIDGET(entry));
	destroy_suggest(entry);
	entry_text_changed(entry);
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

