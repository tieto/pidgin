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

#include "gntinternal.h"
#undef GNT_LOG_DOMAIN
#define GNT_LOG_DOMAIN "TextView"

#include "gntstyle.h"
#include "gnttextview.h"
#include "gntutils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum
{
	SIGS = 1,
};

typedef struct
{
	GntTextFormatFlags tvflag;
	chtype flags;
	int start;
	int end;     /* This is the next byte of the last character of this segment */
} GntTextSegment;

typedef struct
{
	GList *segments;         /* A list of GntTextSegments */
	int length;              /* The current length of the line so far (ie. onscreen width) */
	gboolean soft;           /* TRUE if it's an overflow from prev. line */
} GntTextLine;

typedef struct
{
	char *name;
	int start;
	int end;
} GntTextTag;

static GntWidgetClass *parent_class = NULL;

static gchar *select_start;
static gchar *select_end;
static gboolean double_click;

static void reset_text_view(GntTextView *view);

static gboolean
text_view_contains(GntTextView *view, const char *str)
{
	return (str >= view->string->str && str < view->string->str + view->string->len);
}

static void
gnt_text_view_draw(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);
	int n;
	int i = 0;
	GList *lines;
	int rows, scrcol;
	int comp = 0;          /* Used for top-aligned text */
	gboolean has_scroll = !(view->flags & GNT_TEXT_VIEW_NO_SCROLL);

	wbkgd(widget->window, gnt_color_pair(GNT_COLOR_NORMAL));
	werase(widget->window);

	n = g_list_length(view->list);
	if ((view->flags & GNT_TEXT_VIEW_TOP_ALIGN) &&
			n < widget->priv.height) {
		GList *now = view->list;
		comp = widget->priv.height - n;
		view->list = g_list_nth_prev(view->list, comp);
		if (!view->list) {
			view->list = g_list_first(now);
			comp = widget->priv.height - g_list_length(view->list);
		} else {
			comp = 0;
		}
	}

	for (i = 0, lines = view->list; i < widget->priv.height && lines; i++, lines = lines->next)
	{
		GList *iter;
		GntTextLine *line = lines->data;

		wmove(widget->window, widget->priv.height - 1 - i - comp, 0);

		for (iter = line->segments; iter; iter = iter->next)
		{
			GntTextSegment *seg = iter->data;
			char *end = view->string->str + seg->end;
			char back = *end;
			chtype fl = seg->flags;
			*end = '\0';
			if (select_start && select_start < view->string->str + seg->start && select_end > view->string->str + seg->end) {
				fl |= A_REVERSE;
				wattrset(widget->window, fl);
				wprintw(widget->window, "%s", C_(view->string->str + seg->start));
			} else if (select_start && select_end &&
				((select_start >= view->string->str + seg->start && select_start <= view->string->str + seg->end) ||
				(select_end <= view->string->str + seg->end && select_start <= view->string->str + seg->start))) {
				char *cur = view->string->str + seg->start;
				while (*cur != '\0') {
					gchar *last = g_utf8_next_char(cur);
					gchar *str;
					if (cur >= select_start && cur <= select_end)
						fl |= A_REVERSE;
					else
						fl = seg->flags;
					str = g_strndup(cur, last - cur);
					wattrset(widget->window, fl);
					waddstr(widget->window, C_(str));
					g_free(str);
					cur = g_utf8_next_char(cur);
				}
			} else {
				wattrset(widget->window, fl);
				wprintw(widget->window, "%s", C_(view->string->str + seg->start));
			}
			*end = back;
		}
		wattroff(widget->window, A_UNDERLINE | A_BLINK | A_REVERSE);
		whline(widget->window, ' ', widget->priv.width - line->length - has_scroll);
	}

	scrcol = widget->priv.width - 1;
	rows = widget->priv.height - 2;
	if (has_scroll && rows > 0)
	{
		int total = g_list_length(g_list_first(view->list));
		int showing, position, up, down;

		showing = rows * rows / total + 1;
		showing = MIN(rows, showing);

		total -= rows;
		up = g_list_length(lines);
		down = total - up;

		position = (rows - showing) * up / MAX(1, up + down);
		position = MAX((lines != NULL), position);

		if (showing + position > rows)
			position = rows - showing;
		
		if (showing + position == rows && view->list && view->list->prev)
			position = MAX(1, rows - 1 - showing);
		else if (showing + position < rows && view->list && !view->list->prev)
			position = rows - showing;

		mvwvline(widget->window, position + 1, scrcol,
				ACS_CKBOARD | gnt_color_pair(GNT_COLOR_HIGHLIGHT_D), showing);
	}

	if (has_scroll) {
		mvwaddch(widget->window, 0, scrcol,
				(lines ? ACS_UARROW : ' ') | gnt_color_pair(GNT_COLOR_HIGHLIGHT_D));
		mvwaddch(widget->window, widget->priv.height - 1, scrcol,
				((view->list && view->list->prev) ? ACS_DARROW : ' ') |
					gnt_color_pair(GNT_COLOR_HIGHLIGHT_D));
	}

	wmove(widget->window, 0, 0);
}

static void
gnt_text_view_size_request(GntWidget *widget)
{
	if (!GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED))
	{
		gnt_widget_set_size(widget, 64, 20);
	}
}

static void
gnt_text_view_map(GntWidget *widget)
{
	if (widget->priv.width == 0 || widget->priv.height == 0)
		gnt_widget_size_request(widget);
	GNTDEBUG;
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
free_tag(gpointer data, gpointer null)
{
	GntTextTag *tag = data;
	g_free(tag->name);
	g_free(tag);
}

static void
gnt_text_view_destroy(GntWidget *widget)
{
	GntTextView *view = GNT_TEXT_VIEW(widget);
	view->list = g_list_first(view->list);
	g_list_foreach(view->list, free_text_line, NULL);
	g_list_free(view->list);
	g_list_foreach(view->tags, free_tag, NULL);
	g_list_free(view->tags);
	g_string_free(view->string, TRUE);
}

static char *
gnt_text_view_get_p(GntTextView *view, int x, int y)
{
	int n;
	int i = 0;
	GntWidget *wid = GNT_WIDGET(view);
	GntTextLine *line;
	GList *lines;
	GList *segs;
	GntTextSegment *seg;
	gchar *pos;

	n = g_list_length(view->list);
	y = wid->priv.height - y;
	if (n < y) {
		x = 0;
		y = n - 1;
	}

	lines = g_list_nth(view->list, y - 1);
	if (!lines)
		return NULL;
	do {
		line = lines->data;
		lines = lines->next;
	} while (line && !line->segments && lines);

	if (!line || !line->segments) /* no valid line */
		return NULL;
	segs = line->segments;
	seg = (GntTextSegment *)segs->data;
	pos = view->string->str + seg->start;
	x = MIN(x, line->length);
	while (++i <= x) {
		gunichar *u;
		pos = g_utf8_next_char(pos);
		u = g_utf8_to_ucs4(pos, -1, NULL, NULL, NULL);
		if (u && g_unichar_iswide(*u))
			i++;
		g_free(u);
	}
	return pos;
}

static GString *
select_word_text(GntTextView *view, gchar *c)
{
	gchar *start = c;
	gchar *end = c;
	gchar *t, *endsize;
	while ((t = g_utf8_prev_char(start))) {
		if (!g_ascii_isspace(*t)) {
			if (start == view->string->str)
				break;
			start = t;
		} else
			break;
	}
	while ((t = g_utf8_next_char(end))) {
		if (!g_ascii_isspace(*t))
			end = t;
		else
			break;
	}
	select_start = start;
	select_end = end;
	endsize = g_utf8_next_char(select_end); /* End at the correct byte */
	return g_string_new_len(start, endsize - start);
}

static gboolean too_slow(gpointer n)
{
	double_click = FALSE;
	return FALSE;
}

static gboolean
gnt_text_view_clicked(GntWidget *widget, GntMouseEvent event, int x, int y)
{
	if (event == GNT_MOUSE_SCROLL_UP) {
		gnt_text_view_scroll(GNT_TEXT_VIEW(widget), -1);
	} else if (event == GNT_MOUSE_SCROLL_DOWN) {
		gnt_text_view_scroll(GNT_TEXT_VIEW(widget), 1);
	} else if (event == GNT_LEFT_MOUSE_DOWN) {
		select_start = gnt_text_view_get_p(GNT_TEXT_VIEW(widget), x - widget->priv.x, y - widget->priv.y);
		g_timeout_add(500, too_slow, NULL);
	} else if (event == GNT_MOUSE_UP) {
		GntTextView *view = GNT_TEXT_VIEW(widget);
		if (text_view_contains(view, select_start)) {
			GString *clip;
			select_end = gnt_text_view_get_p(view, x - widget->priv.x, y - widget->priv.y);
			if (select_end < select_start) {
				gchar *t = select_start;
				select_start = select_end;
				select_end = t;
			}
			if (select_start == select_end) {
				if (double_click) {
					clip = select_word_text(view, select_start);
					double_click = FALSE;
				} else {
					double_click = TRUE;
					select_start = 0;
					select_end = 0;
					gnt_widget_draw(widget);
					return TRUE;
				}
			} else {
				gchar *endsize = g_utf8_next_char(select_end); /* End at the correct byte */
				clip = g_string_new_len(select_start, endsize - select_start);
			}
			gnt_widget_draw(widget);
			gnt_set_clipboard_string(clip->str);
			g_string_free(clip, TRUE);
		}
	} else
		return FALSE;
	return TRUE;
}

static void
gnt_text_view_reflow(GntTextView *view)
{
	/* This is pretty ugly, and inefficient. Someone do something about it. */
	GntTextLine *line;
	GList *back, *iter, *list;
	GString *string;
	int pos = 0;    /* no. of 'real' lines */

	list = view->list;
	while (list->prev) {
		line = list->data;
		if (!line->soft)
			pos++;
		list = list->prev;
	}

	back = g_list_last(view->list);
	view->list = NULL;

	string = view->string;
	view->string = NULL;
	reset_text_view(view);

	view->string = g_string_set_size(view->string, string->len);
	view->string->len = 0;
	GNT_WIDGET_SET_FLAGS(GNT_WIDGET(view), GNT_WIDGET_DRAWING);

	for (; back; back = back->prev) {
		line = back->data;
		if (back->next && !line->soft) {
			gnt_text_view_append_text_with_flags(view, "\n", GNT_TEXT_FLAG_NORMAL);
		}

		for (iter = line->segments; iter; iter = iter->next) {
			GntTextSegment *seg = iter->data;
			char *start = string->str + seg->start;
			char *end = string->str + seg->end;
			char back = *end;
			*end = '\0';
			gnt_text_view_append_text_with_flags(view, start, seg->tvflag);
			*end = back;
		}
		free_text_line(line, NULL);
	}
	g_list_free(list);

	list = view->list = g_list_first(view->list);
	/* Go back to the line that was in view before resizing started */
	while (pos--) {
		while (((GntTextLine*)list->data)->soft)
			list = list->next;
		list = list->next;
	}
	view->list = list;
	GNT_WIDGET_UNSET_FLAGS(GNT_WIDGET(view), GNT_WIDGET_DRAWING);
	if (GNT_WIDGET(view)->window)
		gnt_widget_draw(GNT_WIDGET(view));
	g_string_free(string, TRUE);
}

static void
gnt_text_view_size_changed(GntWidget *widget, int w, int h)
{
	if (w != widget->priv.width && GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_MAPPED)) {
		gnt_text_view_reflow(GNT_TEXT_VIEW(widget));
	}
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
	parent_class->clicked = gnt_text_view_clicked;
	parent_class->size_changed = gnt_text_view_size_changed;

	GNTDEBUG;
}

static void
gnt_text_view_init(GTypeInstance *instance, gpointer class)
{
	GntWidget *widget = GNT_WIDGET(instance);
	GntTextView *view = GNT_TEXT_VIEW(widget);
	GntTextLine *line = g_new0(GntTextLine, 1);

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_NO_BORDER | GNT_WIDGET_NO_SHADOW | 
            GNT_WIDGET_GROW_Y | GNT_WIDGET_GROW_X);
	widget->priv.minw = 5;
	widget->priv.minh = 2;
	view->string = g_string_new(NULL);
	view->list = g_list_append(view->list, line);

	GNTDEBUG;
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
			NULL					/* value_table		*/
		};

		type = g_type_register_static(GNT_TYPE_WIDGET,
									  "GntTextView",
									  &info, 0);
	}

	return type;
}

GntWidget *gnt_text_view_new()
{
	GntWidget *widget = g_object_new(GNT_TYPE_TEXT_VIEW, NULL);

	return widget;
}

void gnt_text_view_append_text_with_flags(GntTextView *view, const char *text, GntTextFormatFlags flags)
{
	gnt_text_view_append_text_with_tag(view, text, flags, NULL);
}

void gnt_text_view_append_text_with_tag(GntTextView *view, const char *text,
			GntTextFormatFlags flags, const char *tagname)
{
	GntWidget *widget = GNT_WIDGET(view);
	int fl = 0;
	const char *start, *end;
	GList *list = view->list;
	GntTextLine *line;
	int len;
	gboolean has_scroll = !(view->flags & GNT_TEXT_VIEW_NO_SCROLL);
	gboolean wrap_word = !(view->flags & GNT_TEXT_VIEW_WRAP_CHAR);

	if (text == NULL || *text == '\0')
		return;

	fl = gnt_text_format_flag_to_chtype(flags);

	len = view->string->len;
	view->string = g_string_append(view->string, text);

	if (tagname) {
		GntTextTag *tag = g_new0(GntTextTag, 1);
		tag->name = g_strdup(tagname);
		tag->start = len;
		tag->end = view->string->len;
		view->tags = g_list_append(view->tags, tag);
	}

	view->list = g_list_first(view->list);

	start = end = view->string->str + len;

	while (*start) {
		GntTextLine *oldl;
		GntTextSegment *seg = NULL;

		if (*end == '\n' || *end == '\r') {
			if (!strncmp(end, "\r\n", 2))
				end++;
			end++;
			start = end;
			gnt_text_view_next_line(view);
			view->list = g_list_first(view->list);
			continue;
		}

		line = view->list->data;
		if (line->length == widget->priv.width - has_scroll) {
			/* The last added line was exactly the same width as the widget */
			line = g_new0(GntTextLine, 1);
			line->soft = TRUE;
			view->list = g_list_prepend(view->list, line);
		}

		if ((end = strchr(start, '\r')) != NULL ||
			(end = strchr(start, '\n')) != NULL) {
			len = gnt_util_onscreen_width(start, end - has_scroll);
			if (widget->priv.width > 0 &&
					len >= widget->priv.width - line->length - has_scroll) {
				end = NULL;
			}
		}

		if (end == NULL)
			end = gnt_util_onscreen_width_to_pointer(start,
					widget->priv.width - line->length - has_scroll, &len);

		/* Try to append to the previous segment if possible */
		if (line->segments) {
			seg = g_list_last(line->segments)->data;
			if (seg->flags != fl)
				seg = NULL;
		}

		if (seg == NULL) {
			seg = g_new0(GntTextSegment, 1);
			seg->start = start - view->string->str;
			seg->tvflag = flags;
			seg->flags = fl;
			line->segments = g_list_append(line->segments, seg);
		}

		oldl = line;
		if (wrap_word && *end && *end != '\n' && *end != '\r') {
			const char *tmp = end;
			while (end && *end != '\n' && *end != '\r' && !g_ascii_isspace(*end)) {
				end = g_utf8_find_prev_char(seg->start + view->string->str, end);
			}
			if (!end || !g_ascii_isspace(*end))
				end = tmp;
			else
				end++; /* Remove the space */

			line = g_new0(GntTextLine, 1);
			line->soft = TRUE;
			view->list = g_list_prepend(view->list, line);
		}
		seg->end = end - view->string->str;
		oldl->length += len;
		start = end;
	}

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

chtype gnt_text_format_flag_to_chtype(GntTextFormatFlags flags)
{
	chtype fl = 0;

	if (flags & GNT_TEXT_FLAG_BOLD)
		fl |= A_BOLD;
	if (flags & GNT_TEXT_FLAG_UNDERLINE)
		fl |= A_UNDERLINE;
	if (flags & GNT_TEXT_FLAG_BLINK)
		fl |= A_BLINK;

	if (flags & GNT_TEXT_FLAG_DIM)
		fl |= (A_DIM | gnt_color_pair(GNT_COLOR_DISABLED));
	else if (flags & GNT_TEXT_FLAG_HIGHLIGHT)
		fl |= (A_DIM | gnt_color_pair(GNT_COLOR_HIGHLIGHT));
	else if ((flags & A_COLOR) == 0)
		fl |= gnt_color_pair(GNT_COLOR_NORMAL);
	else
		fl |= (flags & A_COLOR);

	return fl;
}

static void reset_text_view(GntTextView *view)
{
	GntTextLine *line;

	g_list_foreach(view->list, free_text_line, NULL);
	g_list_free(view->list);
	view->list = NULL;

	line = g_new0(GntTextLine, 1);
	view->list = g_list_append(view->list, line);
	if (view->string)
		g_string_free(view->string, TRUE);
	view->string = g_string_new(NULL);
}

void gnt_text_view_clear(GntTextView *view)
{
	reset_text_view(view);

	g_list_foreach(view->tags, free_tag, NULL);
	view->tags = NULL;

	if (GNT_WIDGET(view)->window)
		gnt_widget_draw(GNT_WIDGET(view));
}

int gnt_text_view_get_lines_below(GntTextView *view)
{
	int below = 0;
	GList *list = view->list;
	while ((list = list->prev))
		++below;
	return below;
}

int gnt_text_view_get_lines_above(GntTextView *view)
{
	int above = 0;
	GList *list;
	list = g_list_nth(view->list, GNT_WIDGET(view)->priv.height);
	if (!list)
		return 0;
	while ((list = list->next))
		++above;
	return above;
}

/**
 * XXX: There are quite possibly more than a few bugs here.
 */
int gnt_text_view_tag_change(GntTextView *view, const char *name, const char *text, gboolean all)
{
	GList *alllines = g_list_first(view->list);
	GList *list, *next, *iter, *inext;
	const int text_length = text ? strlen(text) : 0;
	int count = 0;
	for (list = view->tags; list; list = next) {
		GntTextTag *tag = list->data;
		next = list->next;
		if (strcmp(tag->name, name) == 0) {
			int change;
			char *before, *after;

			count++;

			before = g_strndup(view->string->str, tag->start);
			after = g_strdup(view->string->str + tag->end);
			change = (tag->end - tag->start) - text_length;

			g_string_printf(view->string, "%s%s%s", before, text ? text : "", after);
			g_free(before);
			g_free(after);

			/* Update the offsets of the next tags */
			for (iter = next; iter; iter = iter->next) {
				GntTextTag *t = iter->data;
				t->start -= change;
				t->end -= change;
			}

			/* Update the offsets of the segments */
			for (iter = alllines; iter; iter = inext) {
				GList *segs, *snext;
				GntTextLine *line = iter->data;
				inext = iter->next;
				for (segs = line->segments; segs; segs = snext) {
					GntTextSegment *seg = segs->data;
					snext = segs->next;
					if (seg->start >= tag->end) {
						/* The segment is somewhere after the tag */
						seg->start -= change;
						seg->end -= change;
					} else if (seg->end <= tag->start) {
						/* This segment is somewhere in front of the tag */
					} else if (seg->start >= tag->start) {
						/* This segment starts in the middle of the tag */
						if (text == NULL) {
							free_text_segment(seg, NULL);
							line->segments = g_list_delete_link(line->segments, segs);
							if (line->segments == NULL) {
								free_text_line(line, NULL);
								line = NULL;
								if (view->list == iter) {
									if (inext)
										view->list = inext;
									else
										view->list = iter->prev;
								}
								alllines = g_list_delete_link(alllines, iter);
							}
						} else {
							/* XXX: (null) */
							seg->start = tag->start;
							seg->end = tag->end - change;
						}
						if (line)
							line->length -= change;
						/* XXX: Make things work if the tagged text spans over several lines. */
					} else {
						/* XXX: handle the rest of the conditions */
						gnt_warning("WTF! This needs to be handled properly!!%s", "");
					}
				}
			}
			if (text == NULL) {
				/* Remove the tag */
				view->tags = g_list_delete_link(view->tags, list);
				free_tag(tag, NULL);
			} else {
				tag->end -= change;
			}
			if (!all)
				break;
		}
	}
	gnt_widget_draw(GNT_WIDGET(view));
	return count;
}

static gboolean
scroll_tv(GntWidget *wid, const char *key, GntTextView *tv)
{
	if (strcmp(key, GNT_KEY_PGUP) == 0) {
		gnt_text_view_scroll(tv, -(GNT_WIDGET(tv)->priv.height - 2));
	} else if (strcmp(key, GNT_KEY_PGDOWN) == 0) {
		gnt_text_view_scroll(tv, GNT_WIDGET(tv)->priv.height - 2);
	} else if (strcmp(key, GNT_KEY_DOWN) == 0) {
		gnt_text_view_scroll(tv, 1);
	} else if (strcmp(key, GNT_KEY_UP) == 0) {
		gnt_text_view_scroll(tv, -1);
	} else {
		return FALSE;
	}
	return TRUE;
}

void gnt_text_view_attach_scroll_widget(GntTextView *view, GntWidget *widget)
{
	g_signal_connect(G_OBJECT(widget), "key_pressed", G_CALLBACK(scroll_tv), view);
}

void gnt_text_view_set_flag(GntTextView *view, GntTextViewFlag flag)
{
	view->flags |= flag;
}

/* Pager and editor setups */
struct
{
	GntTextView *tv;
	char *file;
} pageditor;


static void
cleanup_pageditor(void)
{
	unlink(pageditor.file);
	g_free(pageditor.file);

	pageditor.file = NULL;
	pageditor.tv = NULL;
}

static void
editor_end_cb(int status, gpointer data)
{
	if (status == 0) {
		char *text = NULL;
		if (g_file_get_contents(pageditor.file, &text, NULL, NULL)) {
			reset_text_view(pageditor.tv);
			gnt_text_view_append_text_with_flags(pageditor.tv, text, GNT_TEXT_FLAG_NORMAL);
			gnt_text_view_scroll(GNT_TEXT_VIEW(pageditor.tv), 0);
			g_free(text);
		}
	}
	cleanup_pageditor();
}

static void
pager_end_cb(int status, gpointer data)
{
	cleanup_pageditor();
}

static gboolean
check_for_ext_cb(GntWidget *widget, const char *key, GntTextView *view)
{
	static const char *pager = NULL;
	static const char *editor = NULL;
	char *argv[] = {NULL, NULL, NULL};
	static char path[1024];
	static int len = -1;
	FILE *file;
	gboolean ret;
	gboolean pg;

	if (pager == NULL) {
		pager = gnt_key_translate(gnt_style_get_from_name("pager", "key"));
		if (pager == NULL)
			pager = "\033" "v";
		editor = gnt_key_translate(gnt_style_get_from_name("editor", "key"));
		if (editor == NULL)
			editor = "\033" "e";
		len = g_snprintf(path, sizeof(path), "%s" G_DIR_SEPARATOR_S "gnt", g_get_tmp_dir());
	} else {
		g_snprintf(path + len, sizeof(path) - len, "XXXXXX");
	}

	if (strcmp(key, pager) == 0) {
		if (g_object_get_data(G_OBJECT(widget), "pager-for") != view)
			return FALSE;
		pg = TRUE;
	} else if (strcmp(key, editor) == 0) {
		if (g_object_get_data(G_OBJECT(widget), "editor-for") != view)
			return FALSE;
		pg = FALSE;
	} else {
		return FALSE;
	}

	file = fdopen(g_mkstemp(path), "wb");
	if (!file)
		return FALSE;
	fprintf(file, "%s", view->string->str);
	fclose(file);

	pageditor.tv = view;
	pageditor.file = g_strdup(path);

	argv[0] = gnt_style_get_from_name(pg ? "pager" : "editor", "path");
	argv[0] = argv[0] ? argv[0] : getenv(pg ? "PAGER" : "EDITOR");
	argv[0] = argv[0] ? argv[0] : (pg ? "less" : "vim");
	argv[1] = path;
	ret = gnt_giveup_console(NULL, argv, NULL, NULL, NULL, NULL, pg ? pager_end_cb : editor_end_cb, NULL);
	return ret;
}

void gnt_text_view_attach_pager_widget(GntTextView *view, GntWidget *pager)
{
	g_signal_connect(pager, "key_pressed", G_CALLBACK(check_for_ext_cb), view);
	g_object_set_data(G_OBJECT(pager), "pager-for", view);
}

void gnt_text_view_attach_editor_widget(GntTextView *view, GntWidget *wid)
{
	g_signal_connect(wid, "key_pressed", G_CALLBACK(check_for_ext_cb), view);
	g_object_set_data(G_OBJECT(wid), "editor-for", view);
}

