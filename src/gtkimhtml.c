/*
 * GtkIMHtml
 *
 * Copyright (C) 2000, Eric Warmenhoven <warmenhoven@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "gtkimhtml.h"
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

#include "pixmaps/angel.xpm"
#include "pixmaps/bigsmile.xpm"
#include "pixmaps/burp.xpm"
#include "pixmaps/crossedlips.xpm"
#include "pixmaps/cry.xpm"
#include "pixmaps/embarrassed.xpm"
#include "pixmaps/kiss.xpm"
#include "pixmaps/moneymouth.xpm"
#include "pixmaps/sad.xpm"
#include "pixmaps/scream.xpm"
#include "pixmaps/smile.xpm"
#include "pixmaps/smile8.xpm"
#include "pixmaps/think.xpm"
#include "pixmaps/tongue.xpm"
#include "pixmaps/wink.xpm"
#include "pixmaps/yell.xpm"

#define DEFAULT_FONT_NAME "helvetica"
#define MAX_SIZE 7

gint font_sizes [] = { 80, 100, 120, 140, 200, 300, 400 };

#define BORDER_SIZE 3
#define MIN_HEIGHT 20
#define HR_HEIGHT 2

#define TYPE_TEXT     0
#define TYPE_SMILEY   1
#define TYPE_IMG      2
#define TYPE_SEP      3
#define TYPE_BR       4
#define TYPE_COMMENT  5

#define DRAW_IMG(x) (((x)->type == TYPE_IMG) || (imhtml->smileys && ((x)->type == TYPE_SMILEY)))

typedef struct _GtkIMHtmlBit GtkIMHtmlBit;
typedef struct _FontDetail   FontDetail;

struct _GtkIMHtmlBit {
	gint type;

	gchar *text;
	GdkPixmap *pm;
	GdkBitmap *bm;

	GdkFont *font;
	GdkColor *fore;
	GdkColor *back;
	GdkColor *bg;
	gboolean underline;
	gboolean strike;
	gchar *url;

	GList *chunks;
};

struct _FontDetail {
	gushort size;
	gchar *face;
	GdkColor *fore;
	GdkColor *back;
};

struct line_info {
	gint x;
	gint y;
	gint width;
	gint height;
	gint ascent;

	gboolean selected;
	gchar *sel_start;
	gchar *sel_end;

	gchar *text;
	GtkIMHtmlBit *bit;
};

struct url_widget {
	gint x;
	gint y;
	gint width;
	gint height;
	gchar *url;
};

static GtkLayoutClass *parent_class = NULL;

enum {
	TARGET_STRING,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT
};

enum {
	URL_CLICKED,
	LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };

static void      gtk_imhtml_draw_bit            (GtkIMHtml *, GtkIMHtmlBit *);
static GdkColor *gtk_imhtml_get_color           (const gchar *);
static gint      gtk_imhtml_motion_notify_event (GtkWidget *, GdkEventMotion *);

static void
gtk_imhtml_destroy (GtkObject *object)
{
	GtkIMHtml *imhtml;

	imhtml = GTK_IMHTML (object);

	while (imhtml->bits) {
		GtkIMHtmlBit *bit = imhtml->bits->data;
		imhtml->bits = g_list_remove (imhtml->bits, bit);
		if (bit->text)
			g_free (bit->text);
		if (bit->font)
			gdk_font_unref (bit->font);
		if (bit->fore)
			gdk_color_free (bit->fore);
		if (bit->back)
			gdk_color_free (bit->back);
		if (bit->bg)
			gdk_color_free (bit->bg);
		if (bit->url)
			g_free (bit->url);
		if (bit->pm)
			gdk_pixmap_unref (bit->pm);
		if (bit->bm)
			gdk_bitmap_unref (bit->bm);
		while (bit->chunks) {
			struct line_info *li = bit->chunks->data;
			if (li->text)
				g_free (li->text);
			bit->chunks = g_list_remove (bit->chunks, li);
			g_free (li);
		}
		g_free (bit);
	}

	while (imhtml->urls) {
		g_free (imhtml->urls->data);
		imhtml->urls = g_list_remove (imhtml->urls, imhtml->urls->data);
	}

	if (imhtml->selected_text)
		g_string_free (imhtml->selected_text, TRUE);

	gdk_font_unref (imhtml->default_font);
	gdk_color_free (imhtml->default_fg_color);

	gdk_cursor_destroy (imhtml->hand_cursor);
	gdk_cursor_destroy (imhtml->arrow_cursor);

	g_hash_table_destroy (imhtml->smiley_hash);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_imhtml_realize (GtkWidget *widget)
{
	GtkIMHtml *imhtml;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMHTML (widget));

	imhtml = GTK_IMHTML (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gdk_window_set_events (imhtml->layout.bin_window,
			       (gdk_window_get_events (imhtml->layout.bin_window)
				| GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
				| GDK_POINTER_MOTION_MASK | GDK_EXPOSURE_MASK));

	gdk_window_set_cursor (widget->window, imhtml->arrow_cursor);

	gdk_window_set_background (GTK_LAYOUT (imhtml)->bin_window, &GTK_WIDGET (imhtml)->style->white);
}

static void
draw_text (GtkIMHtml        *imhtml,
	   struct line_info *line)
{
	GtkIMHtmlBit *bit;
	GdkGC *gc;
	GdkColormap *cmap;
	GdkWindow *window = GTK_LAYOUT (imhtml)->bin_window;
	gfloat xoff, yoff;

	bit = line->bit;
	gc = gdk_gc_new (window);
	cmap = gdk_colormap_new (gdk_visual_get_best (), FALSE);
	xoff = GTK_LAYOUT (imhtml)->hadjustment->value;
	yoff = GTK_LAYOUT (imhtml)->vadjustment->value;

	if (bit->bg != NULL) {
		gdk_color_alloc (cmap, bit->bg);
		gdk_gc_set_foreground (gc, bit->bg);
	} else
		gdk_gc_copy (gc, GTK_WIDGET (imhtml)->style->white_gc);

	gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff, line->width, line->height);

	if (!line->text) {
		gdk_colormap_unref (cmap);
		gdk_gc_unref (gc);
		return;
	}

	if (bit->back != NULL) {
		gdk_color_alloc (cmap, bit->back);
		gdk_gc_set_foreground (gc, bit->back);
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff,
				    gdk_string_width (bit->font, line->text), line->height);
	}

	if (line->selected) {
		gint width, x;
		gchar *start, *end;
		GdkColor col;

		if ((line->sel_start > line->sel_end) && (line->sel_end != NULL)) {
			start = line->sel_end;
			end = line->sel_start;
		} else {
			start = line->sel_start;
			end = line->sel_end;
		}

		if (start == NULL)
			x = 0;
		else
			x = gdk_text_width (bit->font, line->text, start - line->text);

		if (end == NULL)
			width = gdk_string_width (bit->font, line->text) - x;
		else
			width = gdk_text_width (bit->font, line->text, end - line->text) - x;

		col.red = col.green = col.blue = 0xc000;
		gdk_color_alloc (cmap, &col);
		gdk_gc_set_foreground (gc, &col);

		gdk_draw_rectangle (window, gc, TRUE, x + line->x - xoff, line->y - yoff,
				    width, line->height);
	}

	if (bit->url) {
		GdkColor *tc = gtk_imhtml_get_color ("#0000a0");
		gdk_color_alloc (cmap, tc);
		gdk_gc_set_foreground (gc, tc);
		gdk_color_free (tc);
	} else if (bit->fore) {
		gdk_color_alloc (cmap, bit->fore);
		gdk_gc_set_foreground (gc, bit->fore);
	} else
		gdk_gc_copy (gc, GTK_WIDGET (imhtml)->style->black_gc);

	gdk_draw_string (window, bit->font, gc, line->x - xoff,
			 line->y - yoff + line->ascent, line->text);

	if (bit->underline || bit->url)
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff + line->ascent + 1,
				    gdk_string_width (bit->font, line->text), 1);
	if (bit->strike)
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff,
				    line->y - yoff + line->ascent - (bit->font->ascent >> 1),
				    gdk_string_width (bit->font, line->text), 1);

	gdk_colormap_unref (cmap);
	gdk_gc_unref (gc);
}

static gint
draw_img (GtkIMHtml        *imhtml,
	  struct line_info *line)
{
	GtkIMHtmlBit *bit;
	GdkGC *gc;
	GdkColormap *cmap;
	gint width, height, hoff;
	GdkWindow *window = GTK_LAYOUT (imhtml)->bin_window;
	gfloat xoff, yoff;

	bit = line->bit;
	gdk_window_get_size (bit->pm, &width, &height);
	hoff = (line->height - height) / 2;
	xoff = GTK_LAYOUT (imhtml)->hadjustment->value;
	yoff = GTK_LAYOUT (imhtml)->vadjustment->value;
	gc = gdk_gc_new (window);
	cmap = gdk_colormap_new (gdk_visual_get_best (), FALSE);

	if (bit->bg != NULL) {
		gdk_color_alloc (cmap, bit->bg);
		gdk_gc_set_foreground (gc, bit->bg);
	} else
		gdk_gc_copy (gc, GTK_WIDGET (imhtml)->style->white_gc);

	gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff, line->width, line->height);

	if (bit->back != NULL) {
		gdk_color_alloc (cmap, bit->back);
		gdk_gc_set_foreground (gc, bit->back);
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff,
				    width, line->height);
	}

	gdk_draw_pixmap (window, gc, bit->pm, 0, 0, line->x - xoff, line->y - yoff + hoff, -1, -1);

	gdk_colormap_unref (cmap);
	gdk_gc_unref (gc);

	return TRUE;
}

static void
gtk_imhtml_draw_exposed (GtkIMHtml *imhtml)
{
	GList *bits;
	GtkIMHtmlBit *bit;
	GList *chunks;
	struct line_info *line;
	GdkRectangle area;

	area.x = GTK_LAYOUT (imhtml)->hadjustment->value;
	area.y = GTK_LAYOUT (imhtml)->vadjustment->value;
	area.width = GTK_WIDGET (imhtml)->allocation.width;
	area.height = GTK_WIDGET (imhtml)->allocation.height;

	bits = imhtml->bits;

	while (bits) {
		bit = bits->data;
		chunks = bit->chunks;
		if (DRAW_IMG (bit)) {
			line = chunks->data;
			if ((line->x <= area.x + area.width) &&
			    (line->y <= area.y + area.height) &&
			    (area.x <= line->x + line->width) &&
			    (area.y <= line->y + line->height))
			draw_img (imhtml, line);
		} else {
			while (chunks) {
				line = chunks->data;
				if ((line->x <= area.x + area.width) &&
				    (line->y <= area.y + area.height) &&
				    (area.x <= line->x + line->width) &&
				    (area.y <= line->y + line->height))
					draw_text (imhtml, line);
				chunks = g_list_next (chunks);
			}
		}
		bits = g_list_next (bits);
	}
}

static void
gtk_imhtml_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
	GtkIMHtml *imhtml;

	imhtml = GTK_IMHTML (widget);
	gtk_imhtml_draw_exposed (imhtml);
}

static void
gtk_imhtml_style_set (GtkWidget *widget,
		      GtkStyle  *style)
{
	GtkIMHtml *imhtml;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMHTML (widget));
	if (!GTK_WIDGET_REALIZED (widget))
		return;

	imhtml = GTK_IMHTML (widget);

	gdk_window_set_background (GTK_LAYOUT (imhtml)->bin_window, &GTK_WIDGET (imhtml)->style->white);
}

static gint
gtk_imhtml_expose_event (GtkWidget      *widget,
			 GdkEventExpose *event)
{
	GtkIMHtml *imhtml;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_IMHTML (widget), FALSE);

	if (GTK_WIDGET_CLASS (parent_class)->expose_event)
		(* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);

	imhtml = GTK_IMHTML (widget);
	gtk_imhtml_draw_exposed (imhtml);

	return FALSE;
}

static void
gtk_imhtml_redraw_all (GtkIMHtml *imhtml)
{
	GList *b;
	GtkIMHtmlBit *bit;
	GtkAdjustment *vadj;
	gfloat oldvalue;

	vadj = GTK_LAYOUT (imhtml)->vadjustment;
	oldvalue = vadj->value / vadj->upper;

	b = imhtml->bits;
	while (b) {
		bit = b->data;
		b = g_list_next (b);
		while (bit->chunks) {
			struct line_info *li = bit->chunks->data;
			if (li->text)
				g_free (li->text);
			bit->chunks = g_list_remove (bit->chunks, li);
			g_free (li);
		}
	}

	g_list_free (imhtml->line);
	imhtml->line = NULL;

	while (imhtml->urls) {
		g_free (imhtml->urls->data);
		imhtml->urls = g_list_remove (imhtml->urls, imhtml->urls->data);
	}

	imhtml->x = BORDER_SIZE;
	imhtml->y = BORDER_SIZE + 10;
	imhtml->llheight = 0;
	imhtml->llascent = 0;

	if (GTK_LAYOUT (imhtml)->bin_window)
		gdk_window_clear (GTK_LAYOUT (imhtml)->bin_window);

	b = imhtml->bits;
	while (b) {
		gtk_imhtml_draw_bit (imhtml, b->data);
		b = g_list_next (b);
	}

	gtk_widget_set_usize (GTK_WIDGET (imhtml), -1, imhtml->y + 5);
	gtk_adjustment_set_value (vadj, vadj->upper * oldvalue);
}

static void
gtk_imhtml_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
	GtkIMHtml *imhtml;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMHTML (widget));
	g_return_if_fail (allocation != NULL);

	imhtml = GTK_IMHTML (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		( *GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	if (allocation->width == imhtml->xsize)
		return;

	imhtml->x = BORDER_SIZE;
	imhtml->y = BORDER_SIZE + 10;
	imhtml->llheight = 0;
	imhtml->llascent = 0;

	imhtml->xsize = allocation->width;

	gtk_imhtml_redraw_all (imhtml);
}

static void
gtk_imhtml_select_none (GtkIMHtml *imhtml)
{
	GList *bits;
	GList *chunks;
	GtkIMHtmlBit *bit;
	struct line_info *chunk;

	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	bits = imhtml->bits;
	while (bits) {
		bit = bits->data;
		chunks = bit->chunks;

		while (chunks) {
			chunk = chunks->data;

			if (chunk->selected) {
				chunk->selected = FALSE;
				chunk->sel_start = chunk->text;
				chunk->sel_end = NULL;
				if (DRAW_IMG (bit))
					draw_img (imhtml, chunk);
				else
					draw_text (imhtml, chunk);
			}

			chunks = g_list_next (chunks);
		}

		bits = g_list_next (bits);
	}
}

static gchar*
get_position (struct line_info *chunk,
	      gint              x,
	      gboolean          smileys)
{
	gint width = x - chunk->x;
	gchar *text;
	gchar *pos;
	guint total = 0;

	switch (chunk->bit->type) {
	case TYPE_TEXT:
	case TYPE_COMMENT:
		text = chunk->text;
		break;
	case TYPE_SMILEY:
		if (smileys)
			return NULL;
		else
			text = chunk->text;
		break;
	default:
		return NULL;
		break;
	}

	if (width <= 0)
		return text;

	for (pos = text; *pos != '\0'; pos++) {
		gint char_width = gdk_text_width (chunk->bit->font, pos, 1);
		if ((width > total) && (width <= total + char_width)) {
			if (width < total + (char_width >> 1))
				return pos;
			else
				return ++pos;
		}
		total += char_width;
	}

	return pos;
}

static GString*
append_to_sel (GString          *string,
	       struct line_info *chunk,
	       gboolean          smileys)
{
	GString *new_string;
	gchar *buf;
	gchar *start;
	gint length;

	switch (chunk->bit->type) {
	case TYPE_TEXT:
	case TYPE_COMMENT:
		start = (chunk->sel_start == NULL) ? chunk->text : chunk->sel_start;
		length = (chunk->sel_end == NULL) ? strlen (start) : chunk->sel_end - start;
		if (length <= 0)
			return string;
		buf = g_strndup (start, length);
		break;
	case TYPE_SMILEY:
		if (smileys) {
			start = (chunk->sel_start == NULL) ? chunk->bit->text : chunk->sel_start;
			length = (chunk->sel_end == NULL) ? strlen (start) : chunk->sel_end - start;
			if (length <= 0)
				return string;
			buf = g_strndup (start, length);
		} else {
			start = (chunk->sel_start == NULL) ? chunk->text : chunk->sel_start;
			length = (chunk->sel_end == NULL) ? strlen (start) : chunk->sel_end - start;
			if (length <= 0)
				return string;
			buf = g_strndup (start, length);
		}
		break;
	case TYPE_BR:
		buf = g_strdup ("\n");
		break;
	default:
		return string;
		break;
	}

	new_string = g_string_append (string, buf);
	g_free (buf);

	return new_string;
}

#define COORDS_IN_CHUNK(xx, yy) (((xx) < chunk->x + chunk->width) && \
				 ((yy) < chunk->y + chunk->height))

static void
gtk_imhtml_select_bits (GtkIMHtml *imhtml)
{
	GList *bits;
	GList *chunks;
	GtkIMHtmlBit *bit;
	struct line_info *chunk;

	guint startx = imhtml->sel_startx,
	      starty = imhtml->sel_starty,
	      endx   = imhtml->sel_endx,
	      endy   = imhtml->sel_endy;
	gchar *new_pos;
	gint selection = 0;
	gboolean smileys = imhtml->smileys;
	gboolean redraw = FALSE;
	gboolean got_start = FALSE;
	gboolean got_end = FALSE;

	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	if (!imhtml->selection)
		return;

	if (imhtml->selected_text) {
		g_string_free (imhtml->selected_text, TRUE);
		imhtml->selected_text = g_string_new ("");
	}

	bits = imhtml->bits;
	while (bits) {
		bit = bits->data;
		chunks = bit->chunks;

		while (chunks) {
			chunk = chunks->data;

			switch (selection) {
			case 0:
				if (COORDS_IN_CHUNK (startx, starty)) {
					new_pos = get_position (chunk, startx, smileys);
					if ( !chunk->selected ||
					    (chunk->sel_start != new_pos) ||
					    (chunk->sel_end != NULL))
						redraw = TRUE;
					chunk->selected = TRUE;
					chunk->sel_start = new_pos;
					chunk->sel_end = NULL;
					selection++;
					got_start = TRUE;
				}

				if (COORDS_IN_CHUNK (endx, endy)) {
					if (got_start) {
						new_pos = get_position (chunk, endx, smileys);
						if (chunk->sel_end != new_pos)
							redraw = TRUE;
						if (chunk->sel_start > new_pos) {
							chunk->sel_end = chunk->sel_start;
							chunk->sel_start = new_pos;
						} else
							chunk->sel_end = new_pos;
						selection = 2;
						got_end = TRUE;
					} else {
						new_pos = get_position (chunk, endx, smileys);
						if ( !chunk->selected ||
						    (chunk->sel_start != new_pos) ||
						    (chunk->sel_end != NULL))
							redraw = TRUE;
						chunk->selected = TRUE;
						chunk->sel_start = new_pos;
						chunk->sel_end = NULL;
						selection++;
						got_end = TRUE;
					}
				} else if (!COORDS_IN_CHUNK (startx, starty) && !got_start) {
					if (chunk->selected)
						redraw = TRUE;
					chunk->selected = FALSE;
					chunk->sel_start = chunk->text;
					chunk->sel_end = NULL;
				}

				break;
			case 1:
				if (!got_start && COORDS_IN_CHUNK (startx, starty)) {
					new_pos = get_position (chunk, startx, smileys);
					if ( !chunk->selected ||
					    (chunk->sel_end != new_pos) ||
					    (chunk->sel_start != chunk->text))
						redraw = TRUE;
					chunk->selected = TRUE;
					chunk->sel_start = chunk->text;
					chunk->sel_end = new_pos;
					selection++;
					got_start = TRUE;
				} else if (!got_end && COORDS_IN_CHUNK (endx, endy)) {
					new_pos = get_position (chunk, endx, smileys);
					if ( !chunk->selected ||
					    (chunk->sel_end != new_pos) ||
					    (chunk->sel_start != chunk->text))
						redraw = TRUE;
					chunk->selected = TRUE;
					chunk->sel_start = chunk->text;
					chunk->sel_end = new_pos;
					selection++;
					got_end = TRUE;
				} else {
					if ( !chunk->selected ||
					    (chunk->sel_end != NULL) ||
					    (chunk->sel_start != chunk->text))
						redraw = TRUE;
					chunk->selected = TRUE;
					chunk->sel_start = chunk->text;
					chunk->sel_end = NULL;
				}

				break;
			case 2:
				if (chunk->selected)
					redraw = TRUE;
				chunk->selected = FALSE;
				chunk->sel_start = chunk->text;
				chunk->sel_end = NULL;
				break;
			}

			if (chunk->selected == TRUE)
				imhtml->selected_text = append_to_sel (imhtml->selected_text,
								       chunk, smileys);

			if (redraw) {
				if (DRAW_IMG (bit))
					draw_img (imhtml, chunk);
				else
					draw_text (imhtml, chunk);
				redraw = FALSE;
			}

			chunks = g_list_next (chunks);
		}

		bits = g_list_next (bits);
	}
}

static gint
scroll_timeout (GtkIMHtml *imhtml)
{
	GdkEventMotion event;
	gint x, y;
	GdkModifierType mask;

	imhtml->scroll_timer = 0;

	gdk_window_get_pointer (imhtml->layout.bin_window, &x, &y, &mask);

	if (mask & GDK_BUTTON1_MASK) {
		event.is_hint = 0;
		event.x = x;
		event.y = y;
		event.state = mask;

		gtk_imhtml_motion_notify_event (GTK_WIDGET (imhtml), &event);
	}

	return FALSE;
}

static gint
gtk_imhtml_motion_notify_event (GtkWidget      *widget,
				GdkEventMotion *event)
{
	gint x, y;
	GdkModifierType state;
	GtkIMHtml *imhtml = GTK_IMHTML (widget);
	GtkAdjustment *vadj = GTK_LAYOUT (widget)->vadjustment;
	GtkAdjustment *hadj = GTK_LAYOUT (widget)->hadjustment;

	if (event->is_hint)
		gdk_window_get_pointer (event->window, &x, &y, &state);
	else {
		x = event->x + hadj->value;
		y = event->y + vadj->value;
		state = event->state;
	}

	if (state & GDK_BUTTON1_MASK) {
		gint diff;
		gint height = vadj->page_size;
		gint yy = y - vadj->value;

		if (((yy < 0) || (yy > height)) &&
		    (imhtml->scroll_timer == 0) &&
		    (vadj->upper > vadj->page_size)) {
			imhtml->scroll_timer = gtk_timeout_add (100,
								(GtkFunction) scroll_timeout,
								imhtml);
			diff = (yy < 0) ? (yy >> 1) : ((yy - height) >> 1);
			gtk_adjustment_set_value (vadj,
						  MIN (vadj->value + diff, vadj->upper - height + 20));
		}

		if (imhtml->selection) {
			imhtml->sel_endx = MAX (x, 0);
			imhtml->sel_endy = MAX (y, 0);
			gtk_imhtml_select_bits (imhtml);
		}
	} else {
		GList *urls = imhtml->urls;
		struct url_widget *uw;

		while (urls) {
			uw = (struct url_widget *) urls->data;
			if ((x > uw->x) && (x < uw->x + uw->width) &&
			    (y > uw->y) && (y < uw->y + uw->height)) {
				gdk_window_set_cursor (imhtml->layout.bin_window, imhtml->hand_cursor);
				return TRUE;
			}
			urls = g_list_next (urls);
		}
	}

	gdk_window_set_cursor (imhtml->layout.bin_window, imhtml->arrow_cursor);

	return TRUE;
}

static gint
gtk_imhtml_button_press_event (GtkWidget      *widget,
			       GdkEventButton *event)
{
	GtkIMHtml *imhtml = GTK_IMHTML (widget);
	GtkAdjustment *vadj = GTK_LAYOUT (widget)->vadjustment;
	GtkAdjustment *hadj = GTK_LAYOUT (widget)->hadjustment;
	gint x, y;

	if (event->button == 1) {
		x = event->x + hadj->value;
		y = event->y + vadj->value;

		imhtml->sel_startx = x;
		imhtml->sel_starty = y;
		imhtml->selection = TRUE;
		gtk_imhtml_select_none (imhtml);
	}

	return TRUE;
}

static gint
gtk_imhtml_button_release_event (GtkWidget      *widget,
				 GdkEventButton *event)
{
	GtkIMHtml *imhtml = GTK_IMHTML (widget);
	GtkAdjustment *vadj = GTK_LAYOUT (widget)->vadjustment;
	GtkAdjustment *hadj = GTK_LAYOUT (widget)->hadjustment;
	gint x, y;

	if ((event->button == 1) && imhtml->selection) {
		x = event->x + hadj->value;
		y = event->y + vadj->value;

		if ((x == imhtml->sel_startx) && (y == imhtml->sel_starty)) {
			imhtml->sel_startx = imhtml->sel_starty = 0;
			imhtml->selection = FALSE;
			gtk_imhtml_select_none (imhtml);
		} else {
			imhtml->sel_endx = MAX (x, 0);
			imhtml->sel_endy = MAX (y, 0);
			gtk_imhtml_select_bits (imhtml);
		}

		gtk_selection_owner_set (widget, GDK_SELECTION_PRIMARY, event->time);
	}

	if ((event->button == 1) && (imhtml->selected_text->len == 0)) {
		GList *urls = imhtml->urls;
		struct url_widget *uw;

		while (urls) {
			uw = (struct url_widget *) urls->data;
			if ((x > uw->x) && (x < uw->x + uw->width) &&
			    (y > uw->y) && (y < uw->y + uw->height)) {
				gtk_signal_emit (GTK_OBJECT (imhtml), signals [URL_CLICKED], uw->url);
				return TRUE;
			}
			urls = g_list_next (urls);
		}
	}

	return TRUE;
}

static void
gtk_imhtml_selection_get (GtkWidget        *widget,
			  GtkSelectionData *sel_data,
			  guint             sel_info,
			  guint32           time)
{
	GtkIMHtml *imhtml;
	gchar *string;
	gint length;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMHTML (widget));
	g_return_if_fail (sel_data->selection == GDK_SELECTION_PRIMARY);

	imhtml = GTK_IMHTML (widget);

	g_return_if_fail (imhtml->selected_text != NULL);
	g_return_if_fail (imhtml->selected_text->str != NULL);

	if (imhtml->selected_text->len <= 0) {
		string = NULL;
		length = 0;
	} else {
		string = g_strdup (imhtml->selected_text->str);
		length = strlen (string);
	}

	if (sel_info == TARGET_STRING) {
		gtk_selection_data_set (sel_data,
					GDK_SELECTION_TYPE_STRING,
					8 * sizeof (gchar),
					(guchar *) string,
					length);
	} else if ((sel_info == TARGET_TEXT) || (sel_info == TARGET_COMPOUND_TEXT)) {
		guchar *text;
		GdkAtom encoding;
		gint format;
		gint new_length;

		gdk_string_to_compound_text (string, &encoding, &format, &text, &new_length);
		gtk_selection_data_set (sel_data, encoding, format, text, new_length);
		gdk_free_compound_text (text);
	}

	if (string)
		g_free (string);
}

static gint
gtk_imhtml_selection_clear_event (GtkWidget         *widget,
				  GdkEventSelection *event)
{
	GtkIMHtml *imhtml;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_IMHTML (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	g_return_val_if_fail (event->selection == GDK_SELECTION_PRIMARY, TRUE);

	if (!gtk_selection_clear (widget, event))
		return FALSE;

	imhtml = GTK_IMHTML (widget);

	gtk_imhtml_select_none (imhtml);

	return TRUE;
}

static void
gtk_imhtml_set_scroll_adjustments (GtkLayout     *layout,
				   GtkAdjustment *hadj,
				   GtkAdjustment *vadj)
{
	if (parent_class->set_scroll_adjustments)
		(* parent_class->set_scroll_adjustments) (layout, hadj, vadj);
}

static void
gtk_imhtml_class_init (GtkIMHtmlClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkLayoutClass *layout_class;

	object_class = (GtkObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	layout_class = (GtkLayoutClass*) class;

	parent_class = gtk_type_class (GTK_TYPE_LAYOUT);

	signals [URL_CLICKED] =
		gtk_signal_new ("url_clicked",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkIMHtmlClass, url_clicked),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = gtk_imhtml_destroy;

	widget_class->realize = gtk_imhtml_realize;
	widget_class->draw = gtk_imhtml_draw;
	widget_class->style_set = gtk_imhtml_style_set;
	widget_class->expose_event  = gtk_imhtml_expose_event;
	widget_class->size_allocate = gtk_imhtml_size_allocate;
	widget_class->motion_notify_event = gtk_imhtml_motion_notify_event;
	widget_class->button_press_event = gtk_imhtml_button_press_event;
	widget_class->button_release_event = gtk_imhtml_button_release_event;
	widget_class->selection_get = gtk_imhtml_selection_get;
	widget_class->selection_clear_event = gtk_imhtml_selection_clear_event;

	layout_class->set_scroll_adjustments = gtk_imhtml_set_scroll_adjustments;
}

static GdkFont*
gtk_imhtml_font_load (GtkIMHtml *imhtml,
		      gchar     *name,
		      gboolean   bold,
		      gboolean   italics,
		      gint       fontsize)
{
	gchar buf [16 * 1024];
	GdkFont *font;
	gint size = fontsize ? font_sizes [MIN (fontsize, MAX_SIZE) - 1] : 120;

	g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
		    name ? name : DEFAULT_FONT_NAME,
		    bold ? "bold" : "medium",
		    italics ? 'i' : 'r',
		    size);
	font = gdk_font_load (buf);
	if (font)
		return font;

	if (italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
			    name ? name : DEFAULT_FONT_NAME,
			    bold ? "bold" : "medium",
			    'o',
			    size);
		font = gdk_font_load (buf);
		if (font)
			return font;

		if (bold) {
			g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
				    name ? name : DEFAULT_FONT_NAME,
				    "bold",
				    'r',
				    size);
			font = gdk_font_load (buf);
			if (font)
				return font;
		}
	}

	if (bold) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
			    name ? name : DEFAULT_FONT_NAME,
			    "medium",
			    italics ? 'i' : 'r',
			    size);
		font = gdk_font_load (buf);
		if (font)
			return font;

		if (italics) {
			g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
				    name ? name : DEFAULT_FONT_NAME,
				    "medium",
				    'o',
				    size);
			font = gdk_font_load (buf);
			if (font)
				return font;
		}
	}

	if (!bold && !italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-medium-r-*-*-*-%d-*-*-*-*-*-*",
			    name ? name : DEFAULT_FONT_NAME,
			    size);
		font = gdk_font_load (buf);
		if (font)
			return font;
	}

	g_snprintf (buf, sizeof (buf), "-*-%s-medium-r-*-*-*-%d-*-*-*-*-*-*",
		    DEFAULT_FONT_NAME,
		    size);
	font = gdk_font_load (buf);
	if (font)
		return font;

	if (imhtml->default_font)
		return gdk_font_ref (imhtml->default_font);

	return NULL;
}

static void
gtk_imhtml_init (GtkIMHtml *imhtml)
{
	static const GtkTargetEntry targets [] = {
		{ "STRING", 0, TARGET_STRING },
		{ "TEXT", 0, TARGET_TEXT },
		{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT }
	};

	imhtml->default_font = gtk_imhtml_font_load (imhtml, NULL, FALSE, FALSE, 0);
	if (imhtml->default_font == NULL)
		g_warning ("GtkIMHtml: Could not load default font!");
	imhtml->default_fg_color = gdk_color_copy (&GTK_WIDGET (imhtml)->style->black);
	imhtml->hand_cursor = gdk_cursor_new (GDK_HAND2);
	imhtml->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (imhtml), GTK_CAN_FOCUS);
	gtk_selection_add_targets (GTK_WIDGET (imhtml), GDK_SELECTION_PRIMARY, targets, 3);
}

GtkType
gtk_imhtml_get_type (void)
{
	static GtkType imhtml_type = 0;

	if (!imhtml_type) {
		static const GtkTypeInfo imhtml_info = {
			"GtkIMHtml",
			sizeof (GtkIMHtml),
			sizeof (GtkIMHtmlClass),
			(GtkClassInitFunc) gtk_imhtml_class_init,
			(GtkObjectInitFunc) gtk_imhtml_init,
			NULL,
			NULL,
			NULL
		};

		imhtml_type = gtk_type_unique (GTK_TYPE_LAYOUT, &imhtml_info);
	}

	return imhtml_type;
}

static void
gtk_imhtml_init_smiley_hash (GtkIMHtml *imhtml)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	imhtml->smiley_hash = g_hash_table_new (g_str_hash, g_str_equal);

	gtk_imhtml_associate_smiley (imhtml, ":)", smile_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-)", smile_xpm);

	gtk_imhtml_associate_smiley (imhtml, ":(", sad_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-(", sad_xpm);

	gtk_imhtml_associate_smiley (imhtml, ";)", wink_xpm);
	gtk_imhtml_associate_smiley (imhtml, ";-)", wink_xpm);

	gtk_imhtml_associate_smiley (imhtml, ":-p", tongue_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-P", tongue_xpm);

	gtk_imhtml_associate_smiley (imhtml, "=-O", scream_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-*", kiss_xpm);
	gtk_imhtml_associate_smiley (imhtml, ">:o", yell_xpm);
	gtk_imhtml_associate_smiley (imhtml, "8-)", smile8_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-$", moneymouth_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-!", burp_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-[", embarrassed_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":'(", cry_xpm);

	gtk_imhtml_associate_smiley (imhtml, ":-/", think_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-\\", think_xpm);

	gtk_imhtml_associate_smiley (imhtml, ":-X", crossedlips_xpm);
	gtk_imhtml_associate_smiley (imhtml, ":-D", bigsmile_xpm);
	gtk_imhtml_associate_smiley (imhtml, "O:-)", angel_xpm);
}

GtkWidget*
gtk_imhtml_new (GtkAdjustment *hadj,
		GtkAdjustment *vadj)
{
	GtkIMHtml *imhtml = gtk_type_new (GTK_TYPE_IMHTML);

	gtk_imhtml_set_adjustments (imhtml, hadj, vadj);

	imhtml->bits = NULL;
	imhtml->urls = NULL;

	imhtml->x = BORDER_SIZE;
	imhtml->y = BORDER_SIZE + 10;
	imhtml->llheight = 0;
	imhtml->llascent = 0;
	imhtml->line = NULL;

	imhtml->selected_text = g_string_new ("");
	imhtml->scroll_timer = 0;

	imhtml->img = NULL;

	imhtml->smileys = TRUE;
	imhtml->comments = FALSE;

	imhtml->smin = G_MAXINT;
	imhtml->smax = 0;
	gtk_imhtml_init_smiley_hash (imhtml);

	return GTK_WIDGET (imhtml);
}

void
gtk_imhtml_set_adjustments (GtkIMHtml     *imhtml,
			    GtkAdjustment *hadj,
			    GtkAdjustment *vadj)
{
	gtk_layout_set_hadjustment (GTK_LAYOUT (imhtml), hadj);
	gtk_layout_set_vadjustment (GTK_LAYOUT (imhtml), vadj);
}

void
gtk_imhtml_set_defaults (GtkIMHtml *imhtml,
			 GdkFont   *font,
			 GdkColor  *fg_color)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	if (font) {
		if (imhtml->default_font)
			gdk_font_unref (imhtml->default_font);
		imhtml->default_font = gdk_font_ref (font);
	}

	if (fg_color) {
		if (imhtml->default_fg_color)
			gdk_color_free (imhtml->default_fg_color);
		imhtml->default_fg_color = gdk_color_copy (fg_color);
	}
}

void
gtk_imhtml_set_img_handler (GtkIMHtml      *imhtml,
			    GtkIMHtmlImage  handler)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	imhtml->img = handler;
}

void
gtk_imhtml_associate_smiley (GtkIMHtml  *imhtml,
			     gchar      *text,
			     gchar     **xpm)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));
	g_return_if_fail (text != NULL);

	if (strlen (text) < imhtml->smin)
		imhtml->smin = strlen (text);

	if (strlen (text) > imhtml->smax)
		imhtml->smax = strlen (text);

	if (xpm == NULL)
		g_hash_table_remove (imhtml->smiley_hash, text);
	else
		g_hash_table_insert (imhtml->smiley_hash, text, xpm);
}

/*
static gint
draw_line (GtkWidget *widget,
	   GdkEvent  *event,
	   gpointer   data)
{
	GtkIMHtmlBit *bit;
	GdkDrawable *drawable;
	GdkColormap *cmap;
	GdkGC *gc;
	guint max_width;
	guint max_height;

	bit = data;
	drawable = widget->window;
	cmap = gdk_colormap_new (gdk_visual_get_best (), FALSE);
	gc = gdk_gc_new (drawable);

	if (bit->bg != NULL) {
		gdk_color_alloc (cmap, bit->bg);
		gdk_gc_set_foreground (gc, bit->bg);

		gdk_draw_rectangle (widget->window, gc, TRUE, 0, 0,
				    widget->allocation.width,
				    widget->allocation.height);
	}

	gdk_gc_copy (gc, widget->style->black_gc);

	max_width = widget->allocation.width;
	max_height = widget->allocation.height / 2;

	gdk_draw_rectangle (drawable, gc,
			    TRUE,
			    0, max_height / 2,
			    max_width, max_height);

	gdk_colormap_unref (cmap);
	gdk_gc_unref (gc);

	return TRUE;
}
*/

static void
new_line (GtkIMHtml *imhtml)
{
	GList *last = g_list_last (imhtml->line);
	struct line_info *li;

	if (last) {
		li = last->data;
		if (li->x + li->width != imhtml->xsize - BORDER_SIZE)
			li->width = imhtml->xsize - BORDER_SIZE - li->x;
	}

	last = imhtml->line;
	if (last) {
		li = last->data;
		if (li->height < MIN_HEIGHT) {
			while (last) {
				gint diff;
				li = last->data;
				diff = MIN_HEIGHT - li->height;
				li->height = MIN_HEIGHT;
				li->ascent += diff >> 1;
				last = g_list_next (last);
			}
			imhtml->llheight = MIN_HEIGHT;
		}
	}

	g_list_free (imhtml->line);
	imhtml->line = NULL;

	imhtml->x = BORDER_SIZE;
	imhtml->y += imhtml->llheight;
}

static void
backwards_update (GtkIMHtml    *imhtml,
		  GtkIMHtmlBit *bit,
		  gint          height,
		  gint          ascent)
{
	gint diff;
	GList *ls = NULL;
	struct line_info *li;
	struct url_widget *uw;

	if (height > imhtml->llheight) {
		diff = height - imhtml->llheight;

		ls = imhtml->line;
		while (ls) {
			li = ls->data;
			li->height += diff;
			if (ascent)
				li->ascent = ascent;
			else
				li->ascent += diff >> 1;
			ls = g_list_next (ls);
		}

		ls = imhtml->urls;
		while (ls) {
			uw = ls->data;
			if (uw->y + diff > imhtml->y)
				uw->y += diff;
			ls = g_list_next (ls);
		}

		imhtml->llheight = height;
		if (ascent)
			imhtml->llascent = ascent;
		else
			imhtml->llascent += diff >> 1;
	}
}

/*
static GtkTooltips *tips = NULL;
*/

static void
add_text_renderer (GtkIMHtml    *imhtml,
		   GtkIMHtmlBit *bit,
		   gchar        *text)
{
	struct line_info *li;
	struct url_widget *uw;
	gint width;

	if (text)
		width = gdk_string_width (bit->font, text);
	else
		width = 0;

	li = g_new0 (struct line_info, 1);
	li->x = imhtml->x;
	li->y = imhtml->y;
	li->width = width;
	li->height = imhtml->llheight;
	if (text)
		li->ascent = MAX (imhtml->llascent, bit->font->ascent);
	else
		li->ascent = 0;
	li->text = text;
	li->bit = bit;

	if (bit->url) {
		/* FIXME
		eventbox = gtk_event_box_new ();
		gtk_layout_put (GTK_LAYOUT (imhtml), eventbox, imhtml->x, imhtml->y);
		gtk_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
				    GTK_SIGNAL_FUNC (click_event_box), imhtml);
		gtk_widget_show (eventbox);

		gtk_container_add (GTK_CONTAINER (eventbox), darea);
		*/

		uw = g_new0 (struct url_widget, 1);
		uw->x = imhtml->x;
		uw->y = imhtml->y;
		uw->width = width;
		uw->height = imhtml->llheight;
		uw->url = bit->url;
		imhtml->urls = g_list_append (imhtml->urls, uw);

		/*
		if (!tips)
			tips = gtk_tooltips_new ();
		gtk_tooltips_set_tip (tips, eventbox, bit->url, "");
		*/
	}

	bit->chunks = g_list_append (bit->chunks, li);
	imhtml->line = g_list_append (imhtml->line, li);
}

static void
add_img_renderer (GtkIMHtml    *imhtml,
		  GtkIMHtmlBit *bit)
{
	struct line_info *li;
	struct url_widget *uw;
	gint width;

	gdk_window_get_size (bit->pm, &width, NULL);

	li = g_new0 (struct line_info, 1);
	li->x = imhtml->x;
	li->y = imhtml->y;
	li->width = width;
	li->height = imhtml->llheight;
	li->ascent = 0;
	li->bit = bit;

	if (bit->url) {
		/* FIXME
		eventbox = gtk_event_box_new ();
		gtk_layout_put (GTK_LAYOUT (imhtml), eventbox, imhtml->x, imhtml->y);
		gtk_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
				    GTK_SIGNAL_FUNC (click_event_box), imhtml);
		gtk_widget_show (eventbox);

		gtk_container_add (GTK_CONTAINER (eventbox), darea);
		*/

		uw = g_new0 (struct url_widget, 1);
		uw->x = imhtml->x;
		uw->y = imhtml->y;
		uw->width = width;
		uw->height = imhtml->llheight;
		uw->url = bit->url;
		imhtml->urls = g_list_append (imhtml->urls, uw);

		/*
		if (!tips)
			tips = gtk_tooltips_new ();
		gtk_tooltips_set_tip (tips, eventbox, bit->url, "");
		*/
	}

	bit->chunks = g_list_append (bit->chunks, li);
	imhtml->line = g_list_append (imhtml->line, li);

	imhtml->x += width;
}

static void
gtk_imhtml_draw_bit (GtkIMHtml    *imhtml,
		     GtkIMHtmlBit *bit)
{
	gint width, height;
	GdkWindow *window;
	GdkGC *gc;

	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));
	g_return_if_fail (bit != NULL);

	window = GTK_LAYOUT (imhtml)->bin_window;
	gc = gdk_gc_new (window);

	if ( (bit->type == TYPE_TEXT) ||
	    ((bit->type == TYPE_SMILEY) && !imhtml->smileys) ||
	    ((bit->type == TYPE_COMMENT) && imhtml->comments)) {
		gchar *copy = g_strdup (bit->text);
		gint pos = 0;
		gboolean seenspace = FALSE;
		gchar *tmp;

		height = bit->font->ascent + bit->font->descent;
		width = gdk_string_width (bit->font, bit->text);

		if ((imhtml->x != BORDER_SIZE) &&
				((imhtml->x + width + BORDER_SIZE + BORDER_SIZE + 5) > imhtml->xsize)) {
			gint remain = imhtml->xsize - imhtml->x - BORDER_SIZE - BORDER_SIZE - 5;
			while (gdk_text_width (bit->font, copy, pos) < remain) {
				if (copy [pos] == ' ')
					seenspace = TRUE;
				pos++;
			}
			if (seenspace) {
				while (copy [pos - 1] != ' ') pos--;

				tmp = g_strndup (copy, pos);

				backwards_update (imhtml, bit, height, bit->font->ascent);
				add_text_renderer (imhtml, bit, tmp);
			} else
				pos = 0;
			seenspace = FALSE;
			new_line (imhtml);
			imhtml->llheight = 0;
			imhtml->llascent = 0;
		}

		backwards_update (imhtml, bit, height, bit->font->ascent);

		while (pos < strlen (bit->text)) {
			width = gdk_string_width (bit->font, copy + pos);
			if (imhtml->x + width + BORDER_SIZE + BORDER_SIZE + 5 > imhtml->xsize) {
				gint newpos = 0;
				gint remain = imhtml->xsize - imhtml->x - BORDER_SIZE - BORDER_SIZE - 5;
				while (gdk_text_width (bit->font, copy + pos, newpos) < remain) {
					if (copy [pos + newpos] == ' ')
						seenspace = TRUE;
					newpos++;
				}

				if (seenspace)
					while (copy [pos + newpos - 1] != ' ') newpos--;

				if (newpos == 0)
					break;

				tmp = g_strndup (copy + pos, newpos);
				pos += newpos;

				add_text_renderer (imhtml, bit, tmp);

				seenspace = FALSE;
				new_line (imhtml);
			} else {
				tmp = g_strdup (copy + pos);

				add_text_renderer (imhtml, bit, tmp);

				pos = strlen (bit->text);

				imhtml->x += width;
			}
		}

		g_free (copy);
	} else if ((bit->type == TYPE_SMILEY) || (bit->type == TYPE_IMG)) {
		gdk_window_get_size (bit->pm, &width, &height);

		if ((imhtml->x != BORDER_SIZE) &&
				((imhtml->x + width + BORDER_SIZE + BORDER_SIZE + 5) > imhtml->xsize)) {
			new_line (imhtml);
			imhtml->llheight = 0;
			imhtml->llascent = 0;
		} else
			backwards_update (imhtml, bit, height, height * 3 / 4);

		add_img_renderer (imhtml, bit);
	} else if (bit->type == TYPE_BR) {
		new_line (imhtml);
		imhtml->llheight = 0;
		imhtml->llascent = 0;
		add_text_renderer (imhtml, bit, NULL);
	} else if (bit->type == TYPE_SEP) {
		if (imhtml->llheight) {
			new_line (imhtml);
			imhtml->llheight = 0;
			imhtml->llascent = 0;
		}
		/* FIXME
		darea = gtk_drawing_area_new ();
		gtk_widget_set_usize (darea, imhtml->xsize - (BORDER_SIZE * 2), HR_HEIGHT * 2);
		gtk_layout_put (GTK_LAYOUT (imhtml), darea, imhtml->x, imhtml->y);
		gtk_signal_connect (GTK_OBJECT (darea), "expose_event",
				    GTK_SIGNAL_FUNC (draw_line), bit);
		gtk_widget_show (darea);
		*/
		imhtml->llheight = HR_HEIGHT * 2;
		new_line (imhtml);
		imhtml->llheight = 0;
		imhtml->llascent = 0;
		add_text_renderer (imhtml, bit, NULL);
	}

	gtk_layout_set_size (GTK_LAYOUT (imhtml), imhtml->xsize, imhtml->y + 5);

	gdk_gc_destroy (gc);
}

void
gtk_imhtml_show_smileys (GtkIMHtml *imhtml,
			 gboolean   show)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	imhtml->smileys = show;

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (imhtml)))
		gtk_imhtml_redraw_all (imhtml);
}

void
gtk_imhtml_show_comments (GtkIMHtml *imhtml,
			  gboolean   show)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	imhtml->comments = show;

	if (GTK_WIDGET_VISIBLE (GTK_WIDGET (imhtml)))
		gtk_imhtml_redraw_all (imhtml);
}

static GdkColor *
gtk_imhtml_get_color (const gchar *color)
{
	GdkColor c;

	if (!gdk_color_parse (color, &c))
		return NULL;

	return gdk_color_copy (&c);
}

static gint
gtk_imhtml_is_smiley (GtkIMHtml   *imhtml,
		      const gchar *text)
{
	gchar *tmp;
	gint i;

	g_return_val_if_fail (imhtml != NULL, 0);
	g_return_val_if_fail (GTK_IS_IMHTML (imhtml), 0);
	g_return_val_if_fail (text != NULL, 0);

	tmp = g_malloc (imhtml->smax + 1);

	for (i = imhtml->smin; i <= imhtml->smax; i++) {
		if (strlen (text) < i) {
			g_free (tmp);
			return 0;
		}
		g_snprintf (tmp, i + 1, "%s", text);
		if (g_hash_table_lookup (imhtml->smiley_hash, tmp)) {
			g_free (tmp);
			return i;
		}
	}

	g_free (tmp);
	return 0;
}

static GtkIMHtmlBit *
gtk_imhtml_new_bit (GtkIMHtml  *imhtml,
		    gint        type,
		    gchar      *text,
		    gint        bold,
		    gint        italics,
		    gint        underline,
		    gint        strike,
		    FontDetail *font,
		    GdkColor   *bg,
		    gchar      *url)
{
	GtkIMHtmlBit *bit = NULL;

	g_return_val_if_fail (imhtml != NULL, NULL);
	g_return_val_if_fail (GTK_IS_IMHTML (imhtml), NULL);

	if ((type == TYPE_TEXT) && ((text == NULL) || (strlen (text) == 0)))
		return NULL;

	bit = g_new0 (GtkIMHtmlBit, 1);
	bit->type = type;

	if ((text != NULL) && (strlen (text) != 0))
		bit->text = g_strdup (text);

	if ((font != NULL) || bold || italics) {
		if (font && (bold || italics || font->size || font->face)) {
			bit->font = gtk_imhtml_font_load (imhtml, font->face, bold, italics, font->size);
		} else if (bold || italics) {
			bit->font = gtk_imhtml_font_load (imhtml, NULL, bold, italics, 0);
		}

		if (font && (type != TYPE_BR)) {
			if (font->fore != NULL)
				bit->fore = gdk_color_copy (font->fore);

			if (font->back != NULL)
				bit->back = gdk_color_copy (font->back);
		}
	}

	if (((bit->type == TYPE_TEXT) || (bit->type == TYPE_SMILEY) || (bit->type == TYPE_COMMENT)) &&
	    (bit->font == NULL))
		bit->font = gdk_font_ref (imhtml->default_font);

	if (bg != NULL)
		bit->bg = gdk_color_copy (bg);

	bit->underline = underline;
	bit->strike = strike;

	if (url != NULL)
		bit->url = g_strdup (url);

	if (type == TYPE_SMILEY) {
		GdkColor *clr;

		if ((font != NULL) && (font->back != NULL))
			clr = font->back;
		else
			clr = (bg != NULL) ? bg : &GTK_WIDGET (imhtml)->style->white;

		bit->pm = gdk_pixmap_create_from_xpm_d (GTK_WIDGET (imhtml)->window,
							&bit->bm,
							clr,
							g_hash_table_lookup (imhtml->smiley_hash, text));
	}

	return bit;
}

#define NEW_TEXT_BIT    gtk_imhtml_new_bit (imhtml, TYPE_TEXT, ws, bold, italics, underline, strike, \
				fonts ? fonts->data : NULL, bg, url)
#define NEW_SMILEY_BIT  gtk_imhtml_new_bit (imhtml, TYPE_SMILEY, ws, bold, italics, underline, strike, \
				fonts ? fonts->data : NULL, bg, url)
#define NEW_SEP_BIT     gtk_imhtml_new_bit (imhtml, TYPE_SEP, NULL, 0, 0, 0, 0, NULL, bg, NULL)
#define NEW_BR_BIT      gtk_imhtml_new_bit (imhtml, TYPE_BR, NULL, 0, 0, 0, 0, \
				fonts ? fonts->data : NULL, bg, NULL)
#define NEW_COMMENT_BIT gtk_imhtml_new_bit (imhtml, TYPE_COMMENT, ws, bold, italics, underline, strike, \
				fonts ? fonts->data : NULL, bg, url)

#define NEW_BIT(bit) { GtkIMHtmlBit *tmp = bit; if (tmp != NULL) \
				newbits = g_list_append (newbits, tmp); }

#define UPDATE_BG_COLORS \
	{ \
		GdkColormap *cmap; \
		GList *rev; \
		cmap = gdk_colormap_new (gdk_visual_get_best (), FALSE); \
		rev = g_list_last (newbits); \
		while (rev) { \
			GtkIMHtmlBit *bit = rev->data; \
			if (bit->type == TYPE_BR) \
				break; \
			if (bit->bg) \
				gdk_color_free (bit->bg); \
			bit->bg = gdk_color_copy (bg); \
			rev = g_list_previous (rev); \
		} \
		if (!rev) { \
			rev = g_list_last (imhtml->bits); \
			while (rev) { \
				GtkIMHtmlBit *bit = rev->data; \
				if (bit->type == TYPE_BR) \
					break; \
				if (bit->bg) \
					gdk_color_free (bit->bg); \
				bit->bg = gdk_color_copy (bg); \
				gdk_color_alloc (cmap, bit->bg); \
				rev = g_list_previous (rev); \
			} \
			gdk_colormap_unref (cmap); \
		} \
	}

GString*
gtk_imhtml_append_text (GtkIMHtml        *imhtml,
			const gchar      *text,
			GtkIMHtmlOptions  options)
{
	const gchar *c;
	gboolean intag = FALSE;
	gboolean tagquote = FALSE;
	gboolean incomment = FALSE;
	gchar *ws;
	gchar *tag;
	gint wpos = 0;
	gint tpos = 0;
	int smilelen;
	GList *newbits = NULL;

	guint	bold = 0,
		italics = 0,
		underline = 0,
		strike = 0,
		sub = 0,
		sup = 0,
		title = 0;
	GSList *fonts = NULL;
	GdkColor *bg = NULL;
	gchar *url = NULL;

	GtkAdjustment *vadj;
	gboolean scrolldown = TRUE;

	GString *retval = NULL;

	g_return_val_if_fail (imhtml != NULL, NULL);
	g_return_val_if_fail (GTK_IS_IMHTML (imhtml), NULL);
	g_return_val_if_fail (text != NULL, NULL);

	if (options & GTK_IMHTML_RETURN_LOG)
		retval = g_string_new ("");

	vadj = GTK_LAYOUT (imhtml)->vadjustment;
	if ((vadj->value < imhtml->y + 5 - GTK_WIDGET (imhtml)->allocation.height) &&
	    (vadj->upper >= GTK_WIDGET (imhtml)->allocation.height))
		scrolldown = FALSE;

	c = text;
	ws = g_malloc (strlen (text) + 1);
	tag = g_malloc (strlen (text) + 1);

	ws [0] = '\0';

	while (*c) {
		if (*c == '<') {
			if (intag) {
				ws [wpos] = 0;
				tag [tpos] = 0;
				tpos = 0;
				strcat (ws, tag);
				wpos = strlen (ws);
			}

			if (incomment) {
				ws [wpos++] = *c++;
				continue;
			}

			if (!g_strncasecmp (c, "<!--", strlen ("<!--"))) {
				if (!(options & GTK_IMHTML_NO_COMMENTS)) {
					ws [wpos] = 0;
					wpos = 0;
					tag [tpos] = 0;
					strcat (tag, ws);
					incomment = TRUE;
					intag = FALSE;
				}
				ws [wpos++] = *c++;
				ws [wpos++] = *c++;
				ws [wpos++] = *c++;
				ws [wpos++] = *c++;
				continue;
			}

			tag [tpos++] = *c++;
			intag = TRUE;
		} else if (incomment && (*c == '-') && !g_strncasecmp (c, "-->", strlen ("-->"))) {
			gchar *tmp;
			ws [wpos] = 0;
			wpos = 0;
			tmp = g_strdup (ws);
			ws [wpos] = 0;
			strcat (ws, tag);
			NEW_BIT (NEW_TEXT_BIT);
			ws [wpos] = 0;
			strcat (ws, tmp + strlen ("<!--"));
			g_free (tmp);
			NEW_BIT (NEW_COMMENT_BIT);
			incomment = FALSE;
			c += strlen ("-->");
		} else if (*c == '>' && intag && !tagquote) {
			gboolean got_tag = FALSE;
			tag [tpos++] = *c++;
			tag [tpos] = 0;
			ws [wpos] = 0;

			if (!g_strcasecmp (tag, "<B>") || !g_strcasecmp (tag, "<BOLD>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				bold++;
			} else if (!g_strcasecmp (tag, "</B>") || !g_strcasecmp (tag, "</BOLD>")) {
				got_tag = TRUE;
				if (bold) {
					NEW_BIT (NEW_TEXT_BIT);
					bold--;
				}
			} else if (!g_strcasecmp (tag, "<I>") || !g_strcasecmp (tag, "<ITALIC>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				italics++;
			} else if (!g_strcasecmp (tag, "</I>") || !g_strcasecmp (tag, "</ITALIC>")) {
				got_tag = TRUE;
				if (italics) {
					NEW_BIT (NEW_TEXT_BIT);
					italics--;
				}
			} else if (!g_strcasecmp (tag, "<U>") || !g_strcasecmp (tag, "<UNDERLINE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				underline++;
			} else if (!g_strcasecmp (tag, "</U>") || !g_strcasecmp (tag, "</UNDERLINE>")) {
				got_tag = TRUE;
				if (underline) {
					NEW_BIT (NEW_TEXT_BIT);
					underline--;
				}
			} else if (!g_strcasecmp (tag, "<S>") || !g_strcasecmp (tag, "<STRIKE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				strike++;
			} else if (!g_strcasecmp (tag, "</S>") || !g_strcasecmp (tag, "</STRIKE>")) {
				got_tag = TRUE;
				if (strike) {
					NEW_BIT (NEW_TEXT_BIT);
					strike--;
				}
			} else if (!g_strcasecmp (tag, "<SUB>")) {
				got_tag = TRUE;
				sub++;
			} else if (!g_strcasecmp (tag, "</SUB>")) {
				got_tag = TRUE;
				if (sub) {
					sub--;
				}
			} else if (!g_strcasecmp (tag, "<SUP>")) {
				got_tag = TRUE;
				sup++;
			} else if (!g_strcasecmp (tag, "</SUP>")) {
				got_tag = TRUE;
				if (sup) {
					sup--;
				}
			} else if (!g_strcasecmp (tag, "<TITLE>")) {
				if (options & GTK_IMHTML_NO_TITLE) {
					got_tag = TRUE;
					title++;
				} else {
					intag = FALSE;
					tpos = 0;
					continue;
				}
			} else if (!g_strcasecmp (tag, "</TITLE>")) {
				if (title) {
					got_tag = TRUE;
					wpos = 0;
					ws [wpos] = '\0';
					title--;
				} else {
					intag = FALSE;
					tpos = 0;
					continue;
				}
			} else if (!g_strcasecmp (tag, "<BR>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				NEW_BIT (NEW_BR_BIT);
			} else if (!g_strcasecmp (tag, "<HR>") ||
				   !g_strncasecmp (tag, "<HR ", strlen ("<HR "))) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				NEW_BIT (NEW_SEP_BIT);
			} else if (!g_strncasecmp (tag, "<FONT ", strlen ("<FONT "))) {
				gchar *t, *e, *a, *value;
				FontDetail *font = NULL;
				GdkColor *clr;
				gint saw;
				gint i;

				t = tag + strlen ("<FONT ");

				while (*t != '\0') {
					value = NULL;
					saw = 0;

					while (g_strncasecmp (t, "COLOR=", strlen ("COLOR="))
					    && g_strncasecmp (t, "BACK=", strlen ("BACK="))
					    && g_strncasecmp (t, "FACE=", strlen ("FACE="))
					    && g_strncasecmp (t, "SIZE=", strlen ("SIZE="))) {
						gboolean quote = FALSE;
						if (*t == '\0') break;
						while (*t && !((*t == ' ') && !quote)) {
							if (*t == '\"')
								quote = ! quote;
							t++;
						}
						while (*t && (*t == ' ')) t++;
					}

					if (!g_strncasecmp (t, "COLOR=", strlen ("COLOR="))) {
						t += strlen ("COLOR=");
						saw = 1;
					} else if (!g_strncasecmp (t, "BACK=", strlen ("BACK="))) {
						t += strlen ("BACK=");
						saw = 2;
					} else if (!g_strncasecmp (t, "FACE=", strlen ("FACE="))) {
						t += strlen ("FACE=");
						saw = 3;
					} else if (!g_strncasecmp (t, "SIZE=", strlen ("SIZE="))) {
						t += strlen ("SIZE=");
						saw = 4;
					}

					if (!saw)
						continue;

					if ((*t == '\"') || (*t == '\'')) {
						e = a = ++t;
						while (*e && (*e != *(t - 1))) e++;
						if (*e != '\0') {
							*e = '\0';
							t = e + 1;
							value = g_strdup (a);
						} else {
							*t = '\0';
						}
					} else {
						e = a = t;
						while (*e && !isspace ((gint) *e)) e++;
						if (*e == '\0') e--;
						*e = '\0';
						t = e + 1;
						value = g_strdup (a);
					}

					if (value == NULL)
						continue;

					if (font == NULL)
						font = g_new0 (FontDetail, 1);

					switch (saw) {
					case 1:
						clr = gtk_imhtml_get_color (value);
						if (clr != NULL) {
							if ( (font->fore == NULL) &&
							    !(options & GTK_IMHTML_NO_COLOURS))
								font->fore = clr;
						}
						break;
					case 2:
						clr = gtk_imhtml_get_color (value);
						if (clr != NULL) {
							if ( (font->back == NULL) &&
							    !(options & GTK_IMHTML_NO_COLOURS))
								font->back = clr;
						}
						break;
					case 3:
						if ( (font->face == NULL) &&
						    !(options & GTK_IMHTML_NO_FONTS))
							font->face = g_strdup (value);
						break;
					case 4:
						if ((font->size != 0) ||
						    (options & GTK_IMHTML_NO_SIZES))
							break;

						if (isdigit ((gint) value [0])) {
							for (i = 0; i < strlen (value); i++)
								if (!isdigit ((gint) value [i]))
									break;
							if (i != strlen (value))
								break;

							sscanf (value, "%hd", &font->size);
							break;
						}

						if ((value [0] == '+') && (value [1] != '\0')) {
							for (i = 1; i < strlen (value); i++)
								if (!isdigit ((gint) value [i]))
									break;
							if (i != strlen (value))
								break;

							sscanf (value + 1, "%hd", &font->size);
							font->size += 3;
							break;
						}

						if ((value [0] == '-') && (value [1] != '\0')) {
							for (i = 1; i < strlen (value); i++)
								if (!isdigit ((gint) value [i]))
									break;
							if (i != strlen (value))
								break;

							sscanf (value + 1, "%hd", &font->size);
							font->size = MIN (font->size, 2);
							font->size = 3 - font->size;
							break;
						}

						break;
					}

					g_free (value);
				}

				if (!font) {
					intag = FALSE;
					tpos = 0;
					continue;
				}

				if (!(font->size || font->face || font->fore || font->back)) {
					g_free (font);
					intag = FALSE;
					tpos = 0;
					continue;
				}

				NEW_BIT (NEW_TEXT_BIT);

				if (fonts) {
					FontDetail *oldfont = fonts->data;
					if (!font->size)
						font->size = oldfont->size;
					if (!font->face)
						font->face = g_strdup (oldfont->face);
					if (!font->fore && oldfont->fore)
						font->fore = gdk_color_copy (oldfont->fore);
					if (!font->back && oldfont->back)
						font->back = gdk_color_copy (oldfont->back);
				} else {
					if (!font->size)
						font->size = 3;
					if (!font->face)
						font->face = g_strdup ("helvetica");
				}

				fonts = g_slist_prepend (fonts, font);
				got_tag = TRUE;
			} else if (!g_strcasecmp (tag, "</FONT>")) {
				FontDetail *font;

				if (fonts) {
					got_tag = TRUE;
					NEW_BIT (NEW_TEXT_BIT);
					font = fonts->data;
					fonts = g_slist_remove (fonts, font);
					g_free (font->face);
					if (font->fore)
						gdk_color_free (font->fore);
					if (font->back)
						gdk_color_free (font->back);
					g_free (font);
				} else {
					intag = FALSE;
					tpos = 0;
					continue;
				}
			} else if (!g_strncasecmp (tag, "<BODY ", strlen ("<BODY "))) {
				gchar *t, *e, *color = NULL;
				GdkColor *tmp;

				got_tag = TRUE;

				if (!(options & GTK_IMHTML_NO_COLOURS)) {
					t = tag + strlen ("<BODY");
					do {
						gboolean quote = FALSE;
						if (*t == '\0') break;
						while (*t && !((*t == ' ') && !quote)) {
							if (*t == '\"')
								quote = ! quote;
							t++;
						}
						while (*t && (*t == ' ')) t++;
					} while (g_strncasecmp (t, "BGCOLOR=", strlen ("BGCOLOR=")));

					if (!g_strncasecmp (t, "BGCOLOR=", strlen ("BGCOLOR="))) {
						t += strlen ("BGCOLOR=");
						if ((*t == '\"') || (*t == '\'')) {
							e = ++t;
							while (*e && (*e != *(t - 1))) e++;
							if (*e != '\0') {
								*e = '\0';
								color = g_strdup (t);
							}
						} else {
							e = t;
							while (*e && !isspace ((gint) *e)) e++;
							if (*e == '\0') e--;
							*e = '\0';
							color = g_strdup (t);
						}

						if (color != NULL) {
							tmp = gtk_imhtml_get_color (color);
							g_free (color);
							if (tmp != NULL) {
								NEW_BIT (NEW_TEXT_BIT);
								bg = tmp;
								UPDATE_BG_COLORS;
							}
						}
					}
				}
			} else if (!g_strncasecmp (tag, "<A ", strlen ("<A "))) {
				gchar *t, *e;

				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);

				if (url != NULL)
					g_free (url);
				url = NULL;

				t = tag + strlen ("<A");
				do {
					gboolean quote = FALSE;
					if (*t == '\0') break;
					while (*t && !((*t == ' ') && !quote)) {
						if (*t == '\"')
							quote = ! quote;
						t++;
					}
					while (*t && (*t == ' ')) t++;
				} while (g_strncasecmp (t, "HREF=", strlen ("HREF=")));

				if (!g_strncasecmp (t, "HREF=", strlen ("HREF="))) {
					t += strlen ("HREF=");
					if ((*t == '\"') || (*t == '\'')) {
						e = ++t;
						while (*e && (*e != *(t - 1))) e++;
						if (*e != '\0') {
							*e = '\0';
							url = g_strdup (t);
						}
					} else {
						e = t;
						while (*e && !isspace ((gint) *e)) e++;
						if (*e == '\0') e--;
						*e = '\0';
						url = g_strdup (t);
					}
				}
			} else if (!g_strcasecmp (tag, "</A>")) {
				got_tag = TRUE;
				if (url != NULL) {
					NEW_BIT (NEW_TEXT_BIT);
					g_free (url);
				}
				url = NULL;
			} else if (!g_strncasecmp (tag, "<IMG ", strlen ("<IMG "))) {
				gchar *t, *e, *src = NULL;
				gchar *copy = g_strdup (tag);
				gchar **xpm;
				GdkColor *clr = NULL;
				GtkIMHtmlBit *bit;

				intag = FALSE;
				tpos = 0;

				if (imhtml->img == NULL) {
					ws [wpos] = 0;
					strcat (ws, copy);
					wpos = strlen (ws);
					g_free (copy);
					continue;
				}

				t = tag + strlen ("<IMG");
				do {
					gboolean quote = FALSE;
					if (*t == '\0') break;
					while (*t && !((*t == ' ') && !quote)) {
						if (*t == '\"')
							quote = ! quote;
						t++;
					}
					while (*t && (*t == ' ')) t++;
				} while (g_strncasecmp (t, "SRC=", strlen ("SRC=")));

				if (!g_strncasecmp (t, "SRC=", strlen ("SRC="))) {
					t += strlen ("SRC=");
					if ((*t == '\"') || (*t == '\'')) {
						e = ++t;
						while (*e && (*e != *(t - 1))) e++;
						if (*e != '\0') {
							*e = '\0';
							src = g_strdup (t);
						}
					} else {
						e = t;
						while (*e && !isspace ((gint) *e)) e++;
						if (*e == '\0') e--;
						*e = '\0';
						src = g_strdup (t);
					}
				}

				if (src == NULL) {
					ws [wpos] = 0;
					strcat (ws, copy);
					wpos = strlen (ws);
					g_free (copy);
					continue;
				}

				xpm = (* imhtml->img) (src);
				if (xpm == NULL) {
					g_free (src);
					ws [wpos] = 0;
					strcat (ws, copy);
					wpos = strlen (ws);
					g_free (copy);
					continue;
				}

				g_free (copy);

				if (!fonts || ((clr = ((FontDetail *)fonts->data)->back) == NULL))
					clr = (bg != NULL) ? bg : &GTK_WIDGET (imhtml)->style->white;

				if (!GTK_WIDGET_REALIZED (imhtml))
					gtk_widget_realize (GTK_WIDGET (imhtml));

				bit = g_new0 (GtkIMHtmlBit, 1);
				bit->type = TYPE_IMG;
				bit->pm = gdk_pixmap_create_from_xpm_d (GTK_WIDGET (imhtml)->window,
									&bit->bm,
									clr,
									xpm);
				if (url)
					bit->url = g_strdup (url);

				NEW_BIT (bit);

				g_free (src);

				continue;
			} else if (!g_strcasecmp (tag, "<P>") ||
				   !g_strcasecmp (tag, "</P>") ||
				   !g_strncasecmp (tag, "<P ", strlen ("<P ")) ||
				   !g_strcasecmp (tag, "<PRE>") ||
				   !g_strcasecmp (tag, "</PRE>") ||
				   !g_strcasecmp (tag, "<H3>") ||
				   !g_strcasecmp (tag, "<H3 ") ||
				   !g_strcasecmp (tag, "</H3>") ||
				   !g_strcasecmp (tag, "<HTML>") ||
				   !g_strcasecmp (tag, "</HTML>") ||
				   !g_strcasecmp (tag, "<BODY>") ||
				   !g_strcasecmp (tag, "</BODY>") ||
				   !g_strcasecmp (tag, "<FONT>") ||
				   !g_strcasecmp (tag, "<HEAD>") ||
				   !g_strcasecmp (tag, "</HEAD>")) {
				intag = FALSE;
				tpos = 0;
				continue;
			}

			if (!got_tag) {
				ws [wpos] = 0;
				strcat (ws, tag);
				wpos = strlen (ws);
			} else {
				wpos = 0;
			}
			intag = FALSE;
			tpos = 0;
		} else if (*c == '&' && !intag) {
			if (!g_strncasecmp (c, "&amp;", 5)) {
				ws [wpos++] = '&';
				c += 5;
			} else if (!g_strncasecmp (c, "&lt;", 4)) {
				ws [wpos++] = '<';
				c += 4;
			} else if (!g_strncasecmp (c, "&gt;", 4)) {
				ws [wpos++] = '>';
				c += 4;
			} else if (!g_strncasecmp (c, "&nbsp;", 6)) {
				ws [wpos++] = ' ';
				c += 6;
			} else if (!g_strncasecmp (c, "&copy;", 6)) {
				ws [wpos++] = '©';
				c += 6;
			} else if (!g_strncasecmp (c, "&quot;", 6)) {
				ws [wpos++] = '\"';
				c += 6;
			} else if (!g_strncasecmp (c, "&reg;", 5)) {
				ws [wpos++] = '®';
				c += 5;
			} else if (*(c + 1) == '#') {
				gint pound = 0;
				if (sscanf (c, "&#%d;", &pound) == 1) {
					if (*(c + 3 + (gint)log10 (pound)) != ';') {
						ws [wpos++] = *c++;
						continue;
					}
					ws [wpos++] = (gchar)pound;
					c += 2;
					while (isdigit ((gint) *c)) c++;
					if (*c == ';') c++;
				} else {
					ws [wpos++] = *c++;
				}
			} else {
				ws [wpos++] = *c++;
			}
		} else if (intag) {
			if (*c == '\"')
				tagquote = !tagquote;
			tag [tpos++] = *c++;
		} else if (incomment) {
			ws [wpos++] = *c++;
		} else if (((smilelen = gtk_imhtml_is_smiley (imhtml, c)) != 0)) {
			ws [wpos] = 0;
			wpos = 0;
			NEW_BIT (NEW_TEXT_BIT);
			g_snprintf (ws, smilelen + 1, "%s", c);
			NEW_BIT (NEW_SMILEY_BIT);
			c += smilelen;
		} else if (*c == '\n') {
			if (!(options & GTK_IMHTML_NO_NEWLINE)) {
				ws [wpos] = 0;
				wpos = 0;
				NEW_BIT (NEW_TEXT_BIT);
				NEW_BIT (NEW_BR_BIT);
			}
			c++;
		} else {
			ws [wpos++] = *c++;
		}
	}

	if (intag) {
		tag [tpos] = 0;
		c = tag;
		while (*c) {
			if ((smilelen = gtk_imhtml_is_smiley (imhtml, c)) != 0) {
				ws [wpos] = 0;
				wpos = 0;
				NEW_BIT (NEW_TEXT_BIT);
				g_snprintf (ws, smilelen + 1, "%s", c);
				NEW_BIT (NEW_SMILEY_BIT);
				c += smilelen;
			} else {
				ws [wpos++] = *c++;
			}
		}
	} else if (incomment) {
		ws [wpos] = 0;
		wpos = 0;
		strcat (tag, ws);
		ws [wpos] = 0;
		c = tag;
		while (*c) {
			if ((smilelen = gtk_imhtml_is_smiley (imhtml, c)) != 0) {
				ws [wpos] = 0;
				wpos = 0;
				NEW_BIT (NEW_TEXT_BIT);
				g_snprintf (ws, smilelen + 1, "%s", c);
				NEW_BIT (NEW_SMILEY_BIT);
				c += smilelen;
			} else {
				ws [wpos++] = *c++;
			}
		}
	}

	ws [wpos] = 0;
	NEW_BIT (NEW_TEXT_BIT);

	while (newbits) {
		GtkIMHtmlBit *bit = newbits->data;
		imhtml->bits = g_list_append (imhtml->bits, bit);
		newbits = g_list_remove (newbits, bit);
		gtk_imhtml_draw_bit (imhtml, bit);
	}

	gtk_widget_set_usize (GTK_WIDGET (imhtml), -1, imhtml->y + 5);

	if (!(options & GTK_IMHTML_NO_SCROLL) &&
	    scrolldown &&
	    (imhtml->y + 5 >= GTK_WIDGET (imhtml)->allocation.height))
		gtk_adjustment_set_value (vadj, imhtml->y + 5 - GTK_WIDGET (imhtml)->allocation.height);

	if (url) {
		g_free (url);
		if (retval)
			retval = g_string_append (retval, "</A>");
	}
	if (bg)
		gdk_color_free (bg);
	while (fonts) {
		FontDetail *font = fonts->data;
		fonts = g_slist_remove (fonts, font);
		g_free (font->face);
		if (font->fore)
			gdk_color_free (font->fore);
		if (font->back)
			gdk_color_free (font->back);
		g_free (font);
		if (retval)
			retval = g_string_append (retval, "</FONT>");
	}
	if (retval) {
		while (bold) {
			retval = g_string_append (retval, "</B>");
			bold--;
		}
		while (italics) {
			retval = g_string_append (retval, "</I>");
			italics--;
		}
		while (underline) {
			retval = g_string_append (retval, "</U>");
			underline--;
		}
		while (strike) {
			retval = g_string_append (retval, "</S>");
			strike--;
		}
		while (sub) {
			retval = g_string_append (retval, "</SUB>");
			sub--;
		}
		while (sup) {
			retval = g_string_append (retval, "</SUP>");
			sup--;
		}
		while (title) {
			retval = g_string_append (retval, "</TITLE>");
			title--;
		}
	}
	g_free (ws);
	g_free (tag);

	return retval;
}
