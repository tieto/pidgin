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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "gtkimhtml.h"
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#include <locale.h>
#endif

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
#define MAX_FONTS 32767

gint font_sizes [] = { 80, 100, 120, 140, 200, 300, 400 };

#define BORDER_SIZE 2
#define TOP_BORDER 10
#define MIN_HEIGHT 20
#define HR_HEIGHT 2
#define TOOLTIP_TIMEOUT 500

#define DIFF(a, b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))
#define COLOR_MOD  0x8000
#define COLOR_DIFF 0x800

#define TYPE_TEXT     0
#define TYPE_SMILEY   1
#define TYPE_IMG      2
#define TYPE_SEP      3
#define TYPE_BR       4
#define TYPE_COMMENT  5

#define DRAW_IMG(x) (((x)->type == TYPE_IMG) || (imhtml->smileys && ((x)->type == TYPE_SMILEY)))

typedef struct _GtkIMHtmlBit GtkIMHtmlBit;
typedef struct _FontDetail   FontDetail;

struct _GtkSmileyTree {
	GString *values;
	GtkSmileyTree **children;
	gchar **image;
};

static gchar* getcharset()
{
	static gchar charset[64];
#ifdef HAVE_LANGINFO_CODESET
	gchar *ch = nl_langinfo(CODESET);
	if (strncasecmp(ch, "iso-", 4) == 0)
		g_snprintf(charset, sizeof(charset), "iso%s", ch + 4);
	else
		g_snprintf(charset, sizeof(charset), ch);
#else
	g_snprintf(charset, sizeof(charset), "iso8859-*");
#endif
	return charset;
}

static GtkSmileyTree*
gtk_smiley_tree_new ()
{
	return g_new0 (GtkSmileyTree, 1);
}

static void
gtk_smiley_tree_insert (GtkSmileyTree *tree,
			const gchar   *text,
			gchar        **image)
{
	GtkSmileyTree *t = tree;
	const gchar *x = text;

	if (!strlen (x))
		return;

	while (*x) {
		gchar *pos;
		gint index;

		if (!t->values)
			t->values = g_string_new ("");

		pos = strchr (t->values->str, *x);
		if (!pos) {
			t->values = g_string_append_c (t->values, *x);
			index = t->values->len - 1;
			t->children = g_realloc (t->children, t->values->len * sizeof (GtkSmileyTree *));
			t->children [index] = g_new0 (GtkSmileyTree, 1);
		} else
			index = (int) pos - (int) t->values->str;

		t = t->children [index];

		x++;
	}

	t->image = image;
}

static void
gtk_smiley_tree_remove (GtkSmileyTree *tree,
			const gchar   *text)
{
	GtkSmileyTree *t = tree;
	const gchar *x = text;
	gint len = 0;

	while (*x) {
		gchar *pos;

		if (t->image) {
			t->image = NULL;
			return;
		}

		if (!t->values)
			return;

		pos = strchr (t->values->str, *x);
		if (pos)
			t = t->children [(int) pos - (int) t->values->str];
		else
			return;

		x++; len++;
	}

	if (t->image)
		t->image = NULL;
}

static gint
gtk_smiley_tree_lookup (GtkSmileyTree *tree,
			const gchar   *text)
{
	GtkSmileyTree *t = tree;
	const gchar *x = text;
	gint len = 0;

	while (*x) {
		gchar *pos;

		if (t->image)
			return len;

		if (!t->values)
			return 0;

		pos = strchr (t->values->str, *x);
		if (pos)
			t = t->children [(int) pos - (int) t->values->str];
		else
			return 0;

		x++; len++;
	}

	if (t->image)
		return len;

	return 0;
}

static gchar**
gtk_smiley_tree_image (GtkSmileyTree *tree,
		       const gchar   *text)
{
	GtkSmileyTree *t = tree;
	const gchar *x = text;

	while (*x) {
		gchar *pos;

		if (!t->values)
			return NULL;

		pos = strchr (t->values->str, *x);
		if (pos) {
			t = t->children [(int) pos - (int) t->values->str];
		} else
			return NULL;

		x++;
	}

	return t->image;
}

static void
gtk_smiley_tree_destroy (GtkSmileyTree *tree)
{
	GSList *list = g_slist_append (NULL, tree);

	while (list) {
		GtkSmileyTree *t = list->data;
		gint i;
		list = g_slist_remove(list, t);
		if (t->values) {
			for (i = 0; i < t->values->len; i++)
				list = g_slist_append (list, t->children [i]);
			g_string_free (t->values, TRUE);
			g_free (t->children);
		}
		g_free (t);
	}
}

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
	GtkIMHtmlBit *bit;
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

	if (imhtml->tip_timer) {
		gtk_timeout_remove (imhtml->tip_timer);
		imhtml->tip_timer = 0;
	}
	if (imhtml->tip_window) {
		gtk_widget_destroy (imhtml->tip_window);
		imhtml->tip_window = NULL;
	}
	imhtml->tip_bit = NULL;

	if (imhtml->default_font)
		gdk_font_unref (imhtml->default_font);
	if (imhtml->default_fg_color)
		gdk_color_free (imhtml->default_fg_color);
	if (imhtml->default_bg_color)
		gdk_color_free (imhtml->default_bg_color);

	gdk_cursor_destroy (imhtml->hand_cursor);
	gdk_cursor_destroy (imhtml->arrow_cursor);

	gtk_smiley_tree_destroy (imhtml->smiley_data);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_imhtml_realize (GtkWidget *widget)
{
	GtkIMHtml *imhtml;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMHTML (widget));

	imhtml = GTK_IMHTML (widget);
	GTK_WIDGET_SET_FLAGS (imhtml, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);

	attributes.x = widget->style->klass->xthickness + BORDER_SIZE;
	attributes.y = widget->style->klass->xthickness + BORDER_SIZE;
	attributes.width = MAX (1, (gint) widget->allocation.width - (gint) attributes.x * 2);
	attributes.height = MAX (1, (gint) widget->allocation.height - (gint) attributes.y * 2);
	attributes.event_mask = gtk_widget_get_events (widget)
				| GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
				| GDK_POINTER_MOTION_MASK | GDK_EXPOSURE_MASK | GDK_LEAVE_NOTIFY_MASK;

	GTK_LAYOUT (imhtml)->bin_window = gdk_window_new (widget->window,
							  &attributes, attributes_mask);
	gdk_window_set_user_data (GTK_LAYOUT (imhtml)->bin_window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gdk_window_set_cursor (widget->window, imhtml->arrow_cursor);

	imhtml->default_font = gdk_font_ref (GTK_WIDGET (imhtml)->style->font);

	gdk_window_set_background (widget->window, &widget->style->base [GTK_STATE_NORMAL]);
	gdk_window_set_background (GTK_LAYOUT (imhtml)->bin_window,
				   &widget->style->base [GTK_STATE_NORMAL]);

	imhtml->default_fg_color = gdk_color_copy (&GTK_WIDGET (imhtml)->style->fg [GTK_STATE_NORMAL]);
	imhtml->default_bg_color = gdk_color_copy (&GTK_WIDGET (imhtml)->style->base [GTK_STATE_NORMAL]);

	gdk_window_show (GTK_LAYOUT (imhtml)->bin_window);
}

static gboolean
similar_colors (GdkColor *bg,
		GdkColor *fg)
{
	if ((DIFF (bg->red, fg->red) < COLOR_DIFF) &&
	    (DIFF (bg->green, fg->green) < COLOR_DIFF) &&
	    (DIFF (bg->blue, fg->blue) < COLOR_DIFF)) {
		fg->red = (0xff00 - COLOR_MOD > bg->red) ?
			bg->red + COLOR_MOD : bg->red - COLOR_MOD;
		fg->green = (0xff00 - COLOR_MOD > bg->green) ?
			bg->green + COLOR_MOD : bg->green - COLOR_MOD;
		fg->blue = (0xff00 - COLOR_MOD > bg->blue) ?
			bg->blue + COLOR_MOD : bg->blue - COLOR_MOD;
		return TRUE;
	}
	return FALSE;
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
	GdkColor *bg, *fg;

	if (GTK_LAYOUT (imhtml)->freeze_count)
		return;

	bit = line->bit;
	gc = gdk_gc_new (window);
	cmap = gtk_widget_get_colormap (GTK_WIDGET (imhtml));
	xoff = GTK_LAYOUT (imhtml)->hadjustment->value;
	yoff = GTK_LAYOUT (imhtml)->vadjustment->value;

	if (bit->bg != NULL) {
		gdk_color_alloc (cmap, bit->bg);
		gdk_gc_set_foreground (gc, bit->bg);
		bg = bit->bg;
	} else {
		gdk_color_alloc (cmap, imhtml->default_bg_color);
		gdk_gc_set_foreground (gc, imhtml->default_bg_color);
		bg = imhtml->default_bg_color;
	}

	gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff,
			    line->width ? line->width : imhtml->xsize, line->height);

	if (!line->text) {
		gdk_gc_unref (gc);
		return;
	}

	if (bit->back != NULL) {
		gdk_color_alloc (cmap, bit->back);
		gdk_gc_set_foreground (gc, bit->back);
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff,
				    gdk_string_width (bit->font, line->text), line->height);
		bg = bit->back;
	}

	bg = gdk_color_copy (bg);

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
		fg = gdk_color_copy (tc);
		gdk_color_free (tc);
	} else if (bit->fore) {
		gdk_color_alloc (cmap, bit->fore);
		gdk_gc_set_foreground (gc, bit->fore);
		fg = gdk_color_copy (bit->fore);
	} else {
		gdk_color_alloc (cmap, imhtml->default_fg_color);
		gdk_gc_set_foreground (gc, imhtml->default_fg_color);
		fg = gdk_color_copy (imhtml->default_fg_color);
	}

	if (similar_colors (bg, fg)) {
		gdk_color_alloc (cmap, fg);
		gdk_gc_set_foreground (gc, fg);
	}
	gdk_color_free (bg);
	gdk_color_free (fg);

	gdk_draw_string (window, bit->font, gc, line->x - xoff,
			 line->y - yoff + line->ascent, line->text);

	if (bit->underline || bit->url)
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff + line->ascent + 1,
				    gdk_string_width (bit->font, line->text), 1);
	if (bit->strike)
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff,
				    line->y - yoff + line->ascent - (bit->font->ascent / 2),
				    gdk_string_width (bit->font, line->text), 1);

	gdk_gc_unref (gc);
}

static void
draw_img (GtkIMHtml        *imhtml,
	  struct line_info *line)
{
	GtkIMHtmlBit *bit;
	GdkGC *gc;
	GdkColormap *cmap;
	gint width, height, hoff;
	GdkWindow *window = GTK_LAYOUT (imhtml)->bin_window;
	gfloat xoff, yoff;

	if (GTK_LAYOUT (imhtml)->freeze_count)
		return;

	bit = line->bit;
	gdk_window_get_size (bit->pm, &width, &height);
	hoff = (line->height - height) / 2;
	xoff = GTK_LAYOUT (imhtml)->hadjustment->value;
	yoff = GTK_LAYOUT (imhtml)->vadjustment->value;
	gc = gdk_gc_new (window);
	cmap = gtk_widget_get_colormap (GTK_WIDGET (imhtml));

	if (bit->bg != NULL) {
		gdk_color_alloc (cmap, bit->bg);
		gdk_gc_set_foreground (gc, bit->bg);
	} else {
		gdk_color_alloc (cmap, imhtml->default_bg_color);
		gdk_gc_set_foreground (gc, imhtml->default_bg_color);
	}

	gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff, line->width, line->height);

	if (bit->back != NULL) {
		gdk_color_alloc (cmap, bit->back);
		gdk_gc_set_foreground (gc, bit->back);
		gdk_draw_rectangle (window, gc, TRUE, line->x - xoff, line->y - yoff,
				    width, line->height);
	}

	gdk_draw_pixmap (window, gc, bit->pm, 0, 0, line->x - xoff, line->y - yoff + hoff, -1, -1);

	gdk_gc_unref (gc);
}

static void
draw_line (GtkIMHtml        *imhtml,
	   struct line_info *line)
{
	GtkIMHtmlBit *bit;
	GdkDrawable *drawable;
	GdkColormap *cmap;
	GdkGC *gc;
	guint line_height;
	gfloat xoff, yoff;

	if (GTK_LAYOUT (imhtml)->freeze_count)
		return;

	xoff = GTK_LAYOUT (imhtml)->hadjustment->value;
	yoff = GTK_LAYOUT (imhtml)->vadjustment->value;
	bit = line->bit;
	drawable = GTK_LAYOUT (imhtml)->bin_window;
	cmap = gtk_widget_get_colormap (GTK_WIDGET (imhtml));
	gc = gdk_gc_new (drawable);

	if (bit->bg != NULL) {
		gdk_color_alloc (cmap, bit->bg);
		gdk_gc_set_foreground (gc, bit->bg);

		gdk_draw_rectangle (drawable, gc, TRUE, line->x - xoff, line->y - yoff,
				    line->width, line->height);
	}

	gdk_color_alloc (cmap, imhtml->default_fg_color);
	gdk_gc_set_foreground (gc, imhtml->default_fg_color);

	line_height = line->height / 2;

	gdk_draw_rectangle (drawable, gc, TRUE, line->x - xoff, line->y - yoff + line_height / 2,
			    line->width, line_height);

	gdk_gc_unref (gc);
}

static void
gtk_imhtml_draw_focus (GtkWidget *widget)
{
	GtkIMHtml *imhtml;
	gint x = 0,
	     y = 0,
	     w = 0,
	     h = 0;
	
	imhtml = GTK_IMHTML (widget);

	if (!GTK_WIDGET_DRAWABLE (widget))
		return;

	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		gtk_paint_focus (widget->style, widget->window, NULL, widget, "text", 0, 0,
				 widget->allocation.width - 1, widget->allocation.height - 1);
		x = 1; y = 1; w = 2; h = 2;
	}

	gtk_paint_shadow (widget->style, widget->window, GTK_STATE_NORMAL,
			  GTK_SHADOW_IN, NULL, widget, "text", x, y,
			  widget->allocation.width - w, widget->allocation.height - h);
}

static void
gtk_imhtml_draw_exposed (GtkIMHtml *imhtml)
{
	GList *bits;
	GtkIMHtmlBit *bit;
	GList *chunks;
	struct line_info *line;
	gfloat x, y;
	gint width, height;

	x = GTK_LAYOUT (imhtml)->hadjustment->value;
	y = GTK_LAYOUT (imhtml)->vadjustment->value;
	gdk_window_get_size (GTK_LAYOUT (imhtml)->bin_window, &width, &height);

	bits = imhtml->bits;

	while (bits) {
		bit = bits->data;
		chunks = bit->chunks;
		if (DRAW_IMG (bit)) {
			if (chunks) {
				line = chunks->data;
				if ((line->x <= x + width) &&
				    (line->y <= y + height) &&
				    (x <= line->x + line->width) &&
				    (y <= line->y + line->height))
					draw_img (imhtml, line);
			}
		} else if (bit->type == TYPE_SEP) {
			if (chunks) {
				line = chunks->data;
				if ((line->x <= x + width) &&
				    (line->y <= y + height) &&
				    (x <= line->x + line->width) &&
				    (y <= line->y + line->height))
					draw_line (imhtml, line);

				line = chunks->next->data;
				if ((line->x <= x + width) &&
				    (line->y <= y + height) &&
				    (x <= line->x + line->width) &&
				    (y <= line->y + line->height))
					draw_text (imhtml, line);
			}
		} else {
			while (chunks) {
				line = chunks->data;
				if ((line->x <= x + width) &&
				    (line->y <= y + height) &&
				    (x <= line->x + line->width) &&
				    (y <= line->y + line->height))
					draw_text (imhtml, line);
				chunks = g_list_next (chunks);
			}
		}
		bits = g_list_next (bits);
	}

	gtk_imhtml_draw_focus (GTK_WIDGET (imhtml));
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

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, style);

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	imhtml = GTK_IMHTML (widget);
	gtk_imhtml_draw_exposed (imhtml);
}

static gint
gtk_imhtml_expose_event (GtkWidget      *widget,
			 GdkEventExpose *event)
{
	GtkIMHtml *imhtml;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_IMHTML (widget), FALSE);

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
	gint oldy;

	vadj = GTK_LAYOUT (imhtml)->vadjustment;
	oldvalue = vadj->value / vadj->upper;
	oldy = imhtml->y;

	gtk_layout_freeze (GTK_LAYOUT (imhtml));

	g_list_free (imhtml->line);
	imhtml->line = NULL;

	while (imhtml->urls) {
		g_free (imhtml->urls->data);
		imhtml->urls = g_list_remove (imhtml->urls, imhtml->urls->data);
	}

	imhtml->x = 0;
	imhtml->y = TOP_BORDER;
	imhtml->llheight = 0;
	imhtml->llascent = 0;

	if (GTK_LAYOUT (imhtml)->yoffset < TOP_BORDER)
		gdk_window_clear_area (GTK_LAYOUT (imhtml)->bin_window, 0, 0,
				       imhtml->xsize, TOP_BORDER - GTK_LAYOUT (imhtml)->yoffset);

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
		gtk_imhtml_draw_bit (imhtml, bit);
	}

	GTK_LAYOUT (imhtml)->height = imhtml->y;
	GTK_LAYOUT (imhtml)->vadjustment->upper = imhtml->y;
	gtk_signal_emit_by_name (GTK_OBJECT (GTK_LAYOUT (imhtml)->vadjustment), "changed");

	gtk_widget_set_usize (GTK_WIDGET (imhtml), -1, imhtml->y);
	gtk_adjustment_set_value (vadj, vadj->upper * oldvalue);

	if (GTK_LAYOUT (imhtml)->bin_window && (imhtml->y < oldy)) {
		GdkGC *gc;
		GdkColormap *cmap;

		gc = gdk_gc_new (GTK_LAYOUT (imhtml)->bin_window);
		cmap = gtk_widget_get_colormap (GTK_WIDGET (imhtml));

		gdk_color_alloc (cmap, imhtml->default_bg_color);
		gdk_gc_set_foreground (gc, imhtml->default_bg_color);

		gdk_draw_rectangle (GTK_LAYOUT (imhtml)->bin_window, gc, TRUE,
				    0, imhtml->y - GTK_LAYOUT (imhtml)->vadjustment->value,
				    GTK_WIDGET (imhtml)->allocation.width,
				    oldy - imhtml->y);

		gdk_gc_unref (gc);
	}

	gtk_layout_thaw (GTK_LAYOUT (imhtml));
	gtk_imhtml_draw_focus (GTK_WIDGET (imhtml));
}

static void
gtk_imhtml_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
	GtkIMHtml *imhtml;
	GtkLayout *layout;
	gint new_xsize, new_ysize;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_IMHTML (widget));
	g_return_if_fail (allocation != NULL);

	imhtml = GTK_IMHTML (widget);
	layout = GTK_LAYOUT (widget);

	widget->allocation = *allocation;

	new_xsize = MAX (1, (gint) allocation->width -
			    (gint) (widget->style->klass->xthickness + BORDER_SIZE) * 2);
	new_ysize = MAX (1, (gint) allocation->height -
			    (gint) (widget->style->klass->ythickness + BORDER_SIZE) * 2);

	if (GTK_WIDGET_REALIZED (widget)) {
		gint x = widget->style->klass->xthickness + BORDER_SIZE;
		gint y = widget->style->klass->ythickness + BORDER_SIZE;
		gdk_window_move_resize (widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);
		gdk_window_move_resize (layout->bin_window,
					x, y, new_xsize, new_ysize);
	}

	layout->hadjustment->page_size = new_xsize;
	layout->hadjustment->page_increment = new_xsize / 2;
	layout->hadjustment->lower = 0;
	layout->hadjustment->upper = imhtml->x;

	layout->vadjustment->page_size = new_ysize;
	layout->vadjustment->page_increment = new_ysize / 2;
	layout->vadjustment->lower = 0;
	layout->vadjustment->upper = imhtml->y;

	gtk_signal_emit_by_name (GTK_OBJECT (layout->hadjustment), "changed");
	gtk_signal_emit_by_name (GTK_OBJECT (layout->vadjustment), "changed");

	if (new_xsize == imhtml->xsize) {
		if ((GTK_LAYOUT (imhtml)->vadjustment->value > imhtml->y - new_ysize)) {
			if (imhtml->y > new_ysize)
				gtk_adjustment_set_value (GTK_LAYOUT (imhtml)->vadjustment,
							  imhtml->y - new_ysize);
			else
				gtk_adjustment_set_value (GTK_LAYOUT (imhtml)->vadjustment, 0);
		}
		return;
	}

	imhtml->xsize = new_xsize;

	if (GTK_WIDGET_REALIZED (widget))
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
				else if ((bit->type == TYPE_SEP) && (bit->chunks->data == chunk))
					draw_line (imhtml, chunk);
				else if (chunk->width)
					draw_text (imhtml, chunk);
			}

			chunks = g_list_next (chunks);
		}

		bits = g_list_next (bits);
	}
	imhtml->sel_endchunk = NULL;
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
			if (width < total + (char_width / 2))
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
						imhtml->sel_endchunk = chunk;
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
						imhtml->sel_endchunk = chunk;
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
					imhtml->sel_endchunk = chunk;
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
				else if ((bit->type == TYPE_SEP) && (bit->chunks->data == chunk))
					draw_line (imhtml, chunk);
				else if (chunk->width)
					draw_text (imhtml, chunk);
				redraw = FALSE;
			}

			chunks = g_list_next (chunks);
		}

		bits = g_list_next (bits);
	}
}

static void
gtk_imhtml_select_in_chunk (GtkIMHtml *imhtml,
			    struct line_info *chunk)
{
	GtkIMHtmlBit *bit = chunk->bit;
	gchar *new_pos;
	guint endx = imhtml->sel_endx;
	guint startx = imhtml->sel_startx;
	guint starty = imhtml->sel_starty;
	gboolean smileys = imhtml->smileys;
	gboolean redraw = FALSE;

	new_pos = get_position (chunk, endx, smileys);
	if ((starty < chunk->y) ||
	    ((starty < chunk->y + chunk->height) && (startx < endx))) {
		if (chunk->sel_end != new_pos)
			redraw = TRUE;
		chunk->sel_end = new_pos;
	} else {
		if (chunk->sel_start != new_pos)
			redraw = TRUE;
		chunk->sel_start = new_pos;
	}

	if (redraw) {
		if (DRAW_IMG (bit))
			draw_img (imhtml, chunk);
		else if ((bit->type == TYPE_SEP) && 
			 (bit->chunks->data == chunk))
			draw_line (imhtml, chunk);
		else if (chunk->width)
			draw_text (imhtml, chunk);
	}
}

static gint
scroll_timeout (GtkIMHtml *imhtml)
{
	GdkEventMotion event;
	gint x, y;
	GdkModifierType mask;

	imhtml->scroll_timer = 0;

	gdk_window_get_pointer (GTK_LAYOUT (imhtml)->bin_window, &x, &y, &mask);

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
gtk_imhtml_tip_paint (GtkIMHtml *imhtml)
{
	GtkStyle *style;
	gint y, baseline_skip, gap;

	style = imhtml->tip_window->style;

	gap = (style->font->ascent + style->font->descent) / 4;
	if (gap < 2)
		gap = 2;
	baseline_skip = style->font->ascent + style->font->descent + gap;

	if (!imhtml->tip_bit)
		return FALSE;

	gtk_paint_flat_box (style, imhtml->tip_window->window, GTK_STATE_NORMAL, GTK_SHADOW_OUT,
			   NULL, imhtml->tip_window, "tooltip", 0, 0, -1, -1);

	y = style->font->ascent + 4;
	gtk_paint_string (style, imhtml->tip_window->window, GTK_STATE_NORMAL, NULL,
			  imhtml->tip_window, "tooltip", 4, y, imhtml->tip_bit->url);

	return FALSE;
}

static gint
gtk_imhtml_tip (gpointer data)
{
	GtkIMHtml *imhtml = data;
	GtkWidget *widget = GTK_WIDGET (imhtml);
	GtkStyle *style;
	gint gap, x, y, w, h, scr_w, scr_h, baseline_skip;

	if (!imhtml->tip_bit || !GTK_WIDGET_DRAWABLE (widget)) {
		imhtml->tip_timer = 0;
		return FALSE;
	}

	if (imhtml->tip_window)
		gtk_widget_destroy (imhtml->tip_window);

	imhtml->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_app_paintable (imhtml->tip_window, TRUE);
	gtk_window_set_policy (GTK_WINDOW (imhtml->tip_window), FALSE, FALSE, TRUE);
	gtk_widget_set_name (imhtml->tip_window, "gtk-tooltips");
	gtk_signal_connect_object (GTK_OBJECT (imhtml->tip_window), "expose_event",
				   GTK_SIGNAL_FUNC (gtk_imhtml_tip_paint), GTK_OBJECT (imhtml));
	gtk_signal_connect_object (GTK_OBJECT (imhtml->tip_window), "draw",
				   GTK_SIGNAL_FUNC (gtk_imhtml_tip_paint), GTK_OBJECT (imhtml));

	gtk_widget_ensure_style (imhtml->tip_window);
	style = imhtml->tip_window->style;

	scr_w = gdk_screen_width ();
	scr_h = gdk_screen_height ();

	gap = (style->font->ascent + style->font->descent) / 4;
	if (gap < 2)
		gap = 2;
	baseline_skip = style->font->ascent + style->font->descent + gap;

	w = 8 + gdk_string_width (style->font, imhtml->tip_bit->url);
	h = 8 - gap + baseline_skip;

	gdk_window_get_pointer (NULL, &x, &y, NULL);
	if (GTK_WIDGET_NO_WINDOW (widget))
		y += widget->allocation.y;

	x -= ((w >> 1) + 4);

	if ((x + w) > scr_w)
		x -= (x + w) - scr_w;
	else if (x < 0)
		x = 0;

	if ((y + h + + 4) > scr_h)
		y = y - imhtml->tip_bit->font->ascent + imhtml->tip_bit->font->descent;
	else 
		if (imhtml->tip_bit->font)
			y = y + imhtml->tip_bit->font->ascent + imhtml->tip_bit->font->descent;
		else
			y = y + style->font->ascent + style->font->descent;

	gtk_widget_set_usize (imhtml->tip_window, w, h);
	gtk_widget_set_uposition (imhtml->tip_window, x, y);
	gtk_widget_show (imhtml->tip_window);

	imhtml->tip_timer = 0;
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
			diff = (yy < 0) ? (yy / 2) : ((yy - height) / 2);
			gtk_adjustment_set_value (vadj,
						  MIN (vadj->value + diff, vadj->upper - height));
		}

		if (imhtml->selection) {
			struct line_info *chunk = imhtml->sel_endchunk;
			imhtml->sel_endx = MAX (x, 0);
			imhtml->sel_endy = MAX (y, 0);
			if ((chunk == NULL) ||
			    (x < chunk->x) ||
			    (x > chunk->x + chunk->width) ||
			    (y < chunk->y) ||
			    (y > chunk->y + chunk->height))
				gtk_imhtml_select_bits (imhtml);
			else
				gtk_imhtml_select_in_chunk (imhtml, chunk);
		}
	} else {
		GList *urls = imhtml->urls;
		struct url_widget *uw;

		while (urls) {
			uw = (struct url_widget *) urls->data;
			if ((x > uw->x) && (x < uw->x + uw->width) &&
			    (y > uw->y) && (y < uw->y + uw->height)) {
				if (imhtml->tip_bit != uw->bit) {
					imhtml->tip_bit = uw->bit;
					if (imhtml->tip_timer != 0)
						gtk_timeout_remove (imhtml->tip_timer);
					if (imhtml->tip_window) {
						gtk_widget_destroy (imhtml->tip_window);
						imhtml->tip_window = NULL;
					}
					imhtml->tip_timer = gtk_timeout_add (TOOLTIP_TIMEOUT,
									     gtk_imhtml_tip,
									     imhtml);
				}
				gdk_window_set_cursor (GTK_LAYOUT (imhtml)->bin_window,
						       imhtml->hand_cursor);
				return TRUE;
			}
			urls = g_list_next (urls);
		}
	}

	if (imhtml->tip_timer) {
		gtk_timeout_remove (imhtml->tip_timer);
		imhtml->tip_timer = 0;
	}
	if (imhtml->tip_window) {
		gtk_widget_destroy (imhtml->tip_window);
		imhtml->tip_window = NULL;
	}
	imhtml->tip_bit = NULL;

	gdk_window_set_cursor (GTK_LAYOUT (imhtml)->bin_window, imhtml->arrow_cursor);

	return TRUE;
}

static gint
gtk_imhtml_leave_notify_event (GtkWidget        *widget,
			       GdkEventCrossing *event)
{
	GtkIMHtml *imhtml = GTK_IMHTML (widget);

	if (imhtml->tip_timer) {
		gtk_timeout_remove (imhtml->tip_timer);
		imhtml->tip_timer = 0;
	}
	if (imhtml->tip_window) {
		gtk_widget_destroy (imhtml->tip_window);
		imhtml->tip_window = NULL;
	}
	imhtml->tip_bit = NULL;

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

	x = event->x + hadj->value;
	y = event->y + vadj->value;

	if ((event->button == 1) && imhtml->selection) {
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

	if ((event->button == 1) && (imhtml->sel_startx == 0)) {
		GList *urls = imhtml->urls;
		struct url_widget *uw;

		while (urls) {
			uw = (struct url_widget *) urls->data;
			if ((x > uw->x) && (x < uw->x + uw->width) &&
			    (y > uw->y) && (y < uw->y + uw->height)) {
				gtk_signal_emit (GTK_OBJECT (imhtml), signals [URL_CLICKED],
						 uw->bit->url);
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

	if (imhtml->selected_text->len <= 0)
		return;

	string = g_strdup (imhtml->selected_text->str);
	length = strlen (string);

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
gtk_imhtml_adjustment_changed (GtkAdjustment *adjustment,
			       GtkIMHtml     *imhtml)
{
	GtkLayout *layout = GTK_LAYOUT (imhtml);

	layout->xoffset = (gint) layout->hadjustment->value;
	layout->yoffset = (gint) layout->vadjustment->value;

	if (!GTK_WIDGET_MAPPED (imhtml) || !GTK_WIDGET_REALIZED (imhtml))
		return;

	if (layout->freeze_count)
		return;

	if (layout->yoffset < TOP_BORDER)
		gdk_window_clear_area (layout->bin_window, 0, 0,
				       imhtml->xsize, TOP_BORDER - layout->yoffset);

	gtk_imhtml_draw_exposed (imhtml);
}

static void
gtk_imhtml_set_scroll_adjustments (GtkLayout     *layout,
				   GtkAdjustment *hadj,
				   GtkAdjustment *vadj)
{
	gboolean need_adjust = FALSE;

	g_return_if_fail (layout != NULL);
	g_return_if_fail (GTK_IS_IMHTML (layout));

	if (hadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
	else
		hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	if (vadj)
		g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
	else
		vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (layout->hadjustment && (layout->hadjustment != hadj)) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (layout->hadjustment), layout);
		gtk_object_unref (GTK_OBJECT (layout->hadjustment));
	}

	if (layout->vadjustment && (layout->vadjustment != vadj)) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (layout->vadjustment), layout);
		gtk_object_unref (GTK_OBJECT (layout->vadjustment));
	}

	if (layout->hadjustment != hadj) {
		layout->hadjustment = hadj;
		gtk_object_ref (GTK_OBJECT (layout->hadjustment));
		gtk_object_sink (GTK_OBJECT (layout->hadjustment));

		gtk_signal_connect (GTK_OBJECT (layout->hadjustment), "value_changed",
				    (GtkSignalFunc) gtk_imhtml_adjustment_changed, layout);
		need_adjust = TRUE;
	}
	
	if (layout->vadjustment != vadj) {
		layout->vadjustment = vadj;
		gtk_object_ref (GTK_OBJECT (layout->vadjustment));
		gtk_object_sink (GTK_OBJECT (layout->vadjustment));

		gtk_signal_connect (GTK_OBJECT (layout->vadjustment), "value_changed",
				    (GtkSignalFunc) gtk_imhtml_adjustment_changed, layout);
		need_adjust = TRUE;
	}

	if (need_adjust)
		gtk_imhtml_adjustment_changed (NULL, GTK_IMHTML (layout));
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
	widget_class->draw_focus = gtk_imhtml_draw_focus;
	widget_class->style_set = gtk_imhtml_style_set;
	widget_class->expose_event  = gtk_imhtml_expose_event;
	widget_class->size_allocate = gtk_imhtml_size_allocate;
	widget_class->motion_notify_event = gtk_imhtml_motion_notify_event;
	widget_class->leave_notify_event = gtk_imhtml_leave_notify_event;
	widget_class->button_press_event = gtk_imhtml_button_press_event;
	widget_class->button_release_event = gtk_imhtml_button_release_event;
	widget_class->selection_get = gtk_imhtml_selection_get;
	widget_class->selection_clear_event = gtk_imhtml_selection_clear_event;

	layout_class->set_scroll_adjustments = gtk_imhtml_set_scroll_adjustments;
}

static gchar**
get_font_names ()
{
	gint num_fonts = 0;
	gchar **xfontnames;
	static gchar **fonts = NULL;
	gint i;

	if (fonts)
		return fonts;

	xfontnames = XListFonts (GDK_DISPLAY (), "-*", MAX_FONTS, &num_fonts);

	if (!num_fonts) {
		XFreeFontNames(xfontnames);
		return g_new0 (char *, 1);
	}

	fonts = g_new0 (char *, num_fonts + 1);

	for (i = 0; i < num_fonts; i++) {
		gint countdown = 1, num_dashes = 1;
		const gchar *t1 = xfontnames [i];
		const gchar *t2;

		while (*t1 && (countdown >= 0))
			if (*t1++ == '-')
				countdown--;

		for (t2 = t1; *t2; t2++)
			if (*t2 == '-' && --num_dashes == 0)
				break;

		fonts [i] = g_strndup (t1, (long) t2 - (long) t1);
	}

	XFreeFontNames(xfontnames);
	return fonts;
}

static GdkFont*
gtk_imhtml_font_load (GtkIMHtml *imhtml,
		      gchar     *name,
		      gboolean   bold,
		      gboolean   italics,
		      gint       fontsize)
{
	gchar buf [16 * 1024];
	GdkFont *font = NULL;
	XFontStruct *xfs;
	static gchar **fontnames = NULL;
	gchar *choice = NULL;
	gint size = fontsize ? font_sizes [MIN (fontsize, MAX_SIZE) - 1] : 120;
	gint i, j;

	if (!fontnames)
		fontnames = get_font_names ();

	if (name) {
		gchar **choices = g_strsplit (name, ",", -1);

		for (i = 0; choices [i]; i++) {
			for (j = 0; fontnames [j]; j++)
				if (!g_strcasecmp (fontnames [j], choices [i]))
					break;
			if (fontnames [j])
				break;
		}

		if (choices [i])
			choice = g_strdup (choices [i]);

		g_strfreev (choices);
	} else if (!bold && !italics && !fontsize && imhtml->default_font)
		return gdk_font_ref (imhtml->default_font);

	if (!choice) {
		for (i = 0; fontnames [i]; i++)
			if (!g_strcasecmp (fontnames [i], DEFAULT_FONT_NAME))
				break;
		if (fontnames [i])
			choice = g_strdup (DEFAULT_FONT_NAME);
	}

	if (!choice) {
		if (imhtml->default_font)
			return gdk_font_ref (imhtml->default_font);
		return gdk_fontset_load ("-*-*-*-*-*-*-*-*-*-*-*-*-*-*,*");
	}

	g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-%s",
		    choice,
		    bold ? "bold" : "medium",
		    italics ? 'i' : 'r',
		    size,
		    getcharset());
	font = gdk_font_load (buf);

	if (!font && italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-o-*-*-*-%d-*-*-*-*-%s",
			    choice,
			    bold ? "bold" : "medium",
			    size,
			    getcharset());
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-*-*-*-*-*-%s",
			    choice,
			    bold ? "bold" : "medium",
			    italics ? 'i' : 'r',
			    getcharset());
		font = gdk_font_load (buf);
	}

	if (!font && italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-o-*-*-*-*-*-*-*-*-%s",
			    choice,
			    bold ? "bold" : "medium",
			    getcharset());
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-*-%c-*-*-*-*-*-*-*-*-%s",
			    choice,
			    italics ? 'i' : 'r',
			    getcharset());
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-*-%c-*-*-*-*-*-*-*-*-%s",
			    choice,
			    italics ? 'o' : '*',
			    getcharset());
		font = gdk_font_load (buf);
	}

	if (!font && italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-*-*-*-*-*-*-*-*-*-*-%s",
			    choice,
			    getcharset());
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
			    choice,
			    bold ? "bold" : "medium",
			    italics ? 'i' : 'r',
			    size);
		font = gdk_font_load (buf);
	}

	if (!font && italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-o-*-*-*-%d-*-*-*-*-*-*",
			    choice,
			    bold ? "bold" : "medium",
			    size);
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-%c-*-*-*-*-*-*-*-*-*-*",
			    choice,
			    bold ? "bold" : "medium",
			    italics ? 'i' : 'r');
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-%s-o-*-*-*-*-*-*-*-*-*-*",
			    choice,
			    bold ? "bold" : "medium");
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-*-%c-*-*-*-*-*-*-*-*-*-*",
			    choice,
			    italics ? 'i' : 'r');
		font = gdk_font_load (buf);
	}

	if (!font) {
		g_snprintf (buf, sizeof (buf), "-*-%s-*-%c-*-*-*-*-*-*-*-*-*-*",
			    choice,
			    italics ? 'o' : '*');
		font = gdk_font_load (buf);
	}

	if (!font && italics) {
		g_snprintf (buf, sizeof (buf), "-*-%s-*-*-*-*-*-*-*-*-*-*-*-*",
			    choice);
		font = gdk_font_load (buf);
	}

	g_free (choice);

	if (!font && imhtml->default_font)
		return gdk_font_ref (imhtml->default_font);

	xfs = font ? GDK_FONT_XFONT (font) : NULL;
	if (xfs && (xfs->min_byte1 != 0 || xfs->max_byte1 != 0)) {
		gchar *tmp_name;

		gdk_font_unref (font);
		tmp_name = g_strconcat (buf, ",*", NULL);
		font = gdk_fontset_load (tmp_name);
		g_free (tmp_name);
	}

	if (!font)
		return gdk_fontset_load ("-*-*-*-*-*-*-*-*-*-*-*-*-*-*,*");


	return font;
}

static void
gtk_imhtml_init (GtkIMHtml *imhtml)
{
	static const GtkTargetEntry targets [] = {
		{ "STRING", 0, TARGET_STRING },
		{ "TEXT", 0, TARGET_TEXT },
		{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT }
	};

	imhtml->default_font = gtk_imhtml_font_load (imhtml, DEFAULT_FONT_NAME, FALSE, FALSE, 0);
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
gtk_imhtml_init_smileys (GtkIMHtml *imhtml)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	imhtml->smiley_data = gtk_smiley_tree_new ();

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

	imhtml->x = 0;
	imhtml->y = TOP_BORDER;
	imhtml->llheight = 0;
	imhtml->llascent = 0;
	imhtml->line = NULL;

	imhtml->selected_text = g_string_new ("");
	imhtml->scroll_timer = 0;

	imhtml->img = NULL;

	imhtml->smileys = TRUE;
	imhtml->comments = FALSE;

	gtk_imhtml_init_smileys (imhtml);

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
			 GdkColor  *fg_color,
			 GdkColor  *bg_color)
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

	if (bg_color) {
		if (imhtml->default_bg_color)
			gdk_color_free (imhtml->default_bg_color);
		imhtml->default_bg_color = gdk_color_copy (bg_color);
		gdk_window_set_background (GTK_LAYOUT (imhtml)->bin_window, imhtml->default_bg_color);
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

	if (xpm == NULL)
		gtk_smiley_tree_remove (imhtml->smiley_data, text);
	else
		gtk_smiley_tree_insert (imhtml->smiley_data, text, xpm);
}

static void
new_line (GtkIMHtml *imhtml)
{
	GList *last = g_list_last (imhtml->line);
	struct line_info *li;

	if (last) {
		li = last->data;
		if (li->x + li->width != imhtml->xsize)
			li->width = imhtml->xsize - li->x;
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
				li->ascent += diff / 2;
				last = g_list_next (last);
			}
			imhtml->llheight = MIN_HEIGHT;
		}
	}

	g_list_free (imhtml->line);
	imhtml->line = NULL;

	imhtml->x = 0;
	imhtml->y += imhtml->llheight;
	imhtml->llheight = 0;
	imhtml->llascent = 0;
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
				li->ascent += diff / 2;
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
			imhtml->llascent += diff / 2;
	}
}

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
		uw = g_new0 (struct url_widget, 1);
		uw->x = imhtml->x;
		uw->y = imhtml->y;
		uw->width = width;
		uw->height = imhtml->llheight;
		uw->bit = bit;
		imhtml->urls = g_list_append (imhtml->urls, uw);
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
		uw = g_new0 (struct url_widget, 1);
		uw->x = imhtml->x;
		uw->y = imhtml->y;
		uw->width = width;
		uw->height = imhtml->llheight;
		uw->bit = bit;
		imhtml->urls = g_list_append (imhtml->urls, uw);
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

	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));
	g_return_if_fail (bit != NULL);

	if ( (bit->type == TYPE_TEXT) ||
	    ((bit->type == TYPE_SMILEY) && !imhtml->smileys) ||
	    ((bit->type == TYPE_COMMENT) && imhtml->comments)) {
		gchar *copy = g_strdup (bit->text);
		gint pos = 0;
		gboolean seenspace = FALSE;
		gchar *tmp;

		height = bit->font->ascent + bit->font->descent;
		width = gdk_string_width (bit->font, bit->text);

		if ((imhtml->x != 0) && ((imhtml->x + width) > imhtml->xsize)) {
			gint remain = imhtml->xsize - imhtml->x;
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
		}

		backwards_update (imhtml, bit, height, bit->font->ascent);

		while (pos < strlen (bit->text)) {
			width = gdk_string_width (bit->font, copy + pos);
			if (imhtml->x + width > imhtml->xsize) {
				gint newpos = 0;
				gint remain = imhtml->xsize - imhtml->x;
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

				backwards_update (imhtml, bit, height, bit->font->ascent);
				add_text_renderer (imhtml, bit, tmp);

				seenspace = FALSE;
				new_line (imhtml);
			} else {
				tmp = g_strdup (copy + pos);

				backwards_update (imhtml, bit, height, bit->font->ascent);
				add_text_renderer (imhtml, bit, tmp);

				pos = strlen (bit->text);

				imhtml->x += width;
			}
		}

		g_free (copy);
	} else if ((bit->type == TYPE_SMILEY) || (bit->type == TYPE_IMG)) {
		gdk_window_get_size (bit->pm, &width, &height);

		if ((imhtml->x != 0) && ((imhtml->x + width) > imhtml->xsize))
			new_line (imhtml);
		else
			backwards_update (imhtml, bit, height, 0);

		add_img_renderer (imhtml, bit);
	} else if (bit->type == TYPE_BR) {
		new_line (imhtml);
		add_text_renderer (imhtml, bit, NULL);
	} else if (bit->type == TYPE_SEP) {
		struct line_info *li;
		if (imhtml->llheight)
			new_line (imhtml);

		li = g_new0 (struct line_info, 1);
		li->x = imhtml->x;
		li->y = imhtml->y;
		li->width = imhtml->xsize;
		li->height = HR_HEIGHT * 2;
		li->ascent = 0;
		li->text = NULL;
		li->bit = bit;

		bit->chunks = g_list_append (bit->chunks, li);

		imhtml->llheight = HR_HEIGHT * 2;
		new_line (imhtml);
		add_text_renderer (imhtml, bit, NULL);
	}
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
	return gtk_smiley_tree_lookup (imhtml->smiley_data, text);
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
		    gchar      *url,
		    gint	pre)
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

	if ((font != NULL) || bold || italics || pre) {
		if (font && (bold || italics || font->size || font->face || pre)) {
			if (pre) {
				bit->font = gtk_imhtml_font_load (imhtml, "courier", bold, italics, font->size);
			} else {
				bit->font = gtk_imhtml_font_load (imhtml, font->face, bold, italics, font->size);
			}
		} else if (bold || italics || pre) {
			if (pre) {
				bit->font = gtk_imhtml_font_load (imhtml, "Courier", bold, italics, 0);
			} else {
				bit->font = gtk_imhtml_font_load (imhtml, NULL, bold, italics, 0);
			}
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
			clr = (bg != NULL) ? bg : imhtml->default_bg_color;

		bit->pm = gdk_pixmap_create_from_xpm_d (GTK_WIDGET (imhtml)->window,
							&bit->bm,
							clr,
							gtk_smiley_tree_image (imhtml->smiley_data, text));
	}

	return bit;
}

#define NEW_TEXT_BIT    gtk_imhtml_new_bit (imhtml, TYPE_TEXT, ws, bold, italics, underline, strike, \
				fonts ? fonts->data : NULL, bg, url, pre)
#define NEW_SMILEY_BIT  gtk_imhtml_new_bit (imhtml, TYPE_SMILEY, ws, bold, italics, underline, strike, \
				fonts ? fonts->data : NULL, bg, url, pre)
#define NEW_SEP_BIT     gtk_imhtml_new_bit (imhtml, TYPE_SEP, NULL, 0, 0, 0, 0, NULL, bg, NULL, 0)
#define NEW_BR_BIT      gtk_imhtml_new_bit (imhtml, TYPE_BR, NULL, 0, 0, 0, 0, \
				fonts ? fonts->data : NULL, bg, NULL, 0)
#define NEW_COMMENT_BIT gtk_imhtml_new_bit (imhtml, TYPE_COMMENT, ws, bold, italics, underline, strike, \
				fonts ? fonts->data : NULL, bg, url, pre)

#define NEW_BIT(bit) { GtkIMHtmlBit *tmp = bit; if (tmp != NULL) \
				newbits = g_list_append (newbits, tmp); }

#define UPDATE_BG_COLORS \
	{ \
		GdkColormap *cmap; \
		GList *rev; \
		cmap = gtk_widget_get_colormap (GTK_WIDGET (imhtml)); \
		rev = g_list_last (newbits); \
		while (rev) { \
			GtkIMHtmlBit *bit = rev->data; \
			if (bit->bg) \
				gdk_color_free (bit->bg); \
			bit->bg = gdk_color_copy (bg); \
			if (bit->type == TYPE_BR) \
				break; \
			rev = g_list_previous (rev); \
		} \
		if (!rev) { \
			rev = g_list_last (imhtml->bits); \
			while (rev) { \
				GtkIMHtmlBit *bit = rev->data; \
				if (bit->bg) \
					gdk_color_free (bit->bg); \
				bit->bg = gdk_color_copy (bg); \
				gdk_color_alloc (cmap, bit->bg); \
				if (bit->type == TYPE_BR) \
					break; \
				rev = g_list_previous (rev); \
			} \
		} \
	}

static gboolean
is_amp_escape (const gchar *string,
	       gchar       *replace,
	       gint        *length)
{
	g_return_val_if_fail (string != NULL, FALSE);
	g_return_val_if_fail (replace != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (!g_strncasecmp (string, "&amp;", 5)) {
		*replace = '&';
		*length = 5;
	} else if (!g_strncasecmp (string, "&lt;", 4)) {
		*replace = '<';
		*length = 4;
	} else if (!g_strncasecmp (string, "&gt;", 4)) {
		*replace = '>';
		*length = 4;
	} else if (!g_strncasecmp (string, "&nbsp;", 6)) {
		*replace = ' ';
		*length = 6;
	} else if (!g_strncasecmp (string, "&copy;", 6)) {
		*replace = '©';
		*length = 6;
	} else if (!g_strncasecmp (string, "&quot;", 6)) {
		*replace = '\"';
		*length = 6;
	} else if (!g_strncasecmp (string, "&reg;", 5)) {
		*replace = '®';
		*length = 5;
	} else if (*(string + 1) == '#') {
		guint pound = 0;
		if (sscanf (string, "&#%u;", &pound) == 1) {
			if (*(string + 3 + (gint)log10 (pound)) != ';')
				return FALSE;
			*replace = (gchar)pound;
			*length = 2;
			while (isdigit ((gint) string [*length])) (*length)++;
			if (string [*length] == ';') (*length)++;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	return TRUE;
}

GString*
gtk_imhtml_append_text (GtkIMHtml        *imhtml,
			const gchar      *text,
			GtkIMHtmlOptions  options)
{
	const gchar *c;
	gboolean intag = FALSE;
	gint tagquote = 0;
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
		title = 0,
		pre = 0;
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
	if ((vadj->value < imhtml->y - GTK_WIDGET (imhtml)->allocation.height) &&
	    (vadj->upper >= GTK_WIDGET (imhtml)->allocation.height))
		scrolldown = FALSE;

	c = text;
	ws = g_malloc (strlen (text) + 1);
	tag = g_malloc (strlen (text) + 1);

	ws [0] = '\0';

	while (*c) {
		if (*c == '<') {
			if (intag && (tagquote != 1)) {
				char *d;
				tag [tpos] = 0;
				d = tag;
				while (*d) {
					if ((smilelen = gtk_imhtml_is_smiley (imhtml, d)) != 0) {
						ws [wpos] = 0;
						wpos = 0;
						NEW_BIT (NEW_TEXT_BIT);
						g_snprintf (ws, smilelen + 1, "%s", d);
						NEW_BIT (NEW_SMILEY_BIT);
						d += smilelen;
					} else if (*d == '&') {
						gchar replace;
						gint length;
						if (is_amp_escape (d, &replace, &length)) {
							ws [wpos++] = replace;
							d += length;
						} else {
							ws [wpos++] = *d++;
						}
					} else if (*d == '\n') {
						if (!(options & GTK_IMHTML_NO_NEWLINE)) {
							ws [wpos] = 0;
							wpos = 0;
							NEW_BIT (NEW_TEXT_BIT);
							NEW_BIT (NEW_BR_BIT);
						}
						d++;
					} else {
						ws [wpos++] = *d++;
					}
				}
				tpos = 0;
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
			tagquote = 0;
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
		} else if (*c == '>' && intag && (tagquote != 1)) {
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
				NEW_BIT (NEW_TEXT_BIT);
				if (bold) {
					bold--;
				}
			} else if (!g_strcasecmp (tag, "<I>") || !g_strcasecmp (tag, "<ITALIC>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				italics++;
			} else if (!g_strcasecmp (tag, "</I>") || !g_strcasecmp (tag, "</ITALIC>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				if (italics) {
					italics--;
				}
			} else if (!g_strcasecmp (tag, "<U>") || !g_strcasecmp (tag, "<UNDERLINE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				underline++;
			} else if (!g_strcasecmp (tag, "</U>") || !g_strcasecmp (tag, "</UNDERLINE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				if (underline) {
					underline--;
				}
			} else if (!g_strcasecmp (tag, "<S>") || !g_strcasecmp (tag, "<STRIKE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				strike++;
			} else if (!g_strcasecmp (tag, "</S>") || !g_strcasecmp (tag, "</STRIKE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				if (strike) {
					strike--;
				}
			} else if (!g_strcasecmp (tag, "<SUB>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				sub++;
			} else if (!g_strcasecmp (tag, "</SUB>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				if (sub) {
					sub--;
				}
			} else if (!g_strcasecmp (tag, "<SUP>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				sup++;
			} else if (!g_strcasecmp (tag, "</SUP>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				if (sup) {
					sup--;
				}
			} else if (!g_strcasecmp (tag, "<PRE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				pre++;
			} else if (!g_strcasecmp (tag, "</PRE>")) {
				got_tag = TRUE;
				NEW_BIT (NEW_TEXT_BIT);
				if (pre) {
					pre--;
				}
			} else if (!g_strcasecmp (tag, "<TITLE>")) {
				if (options & GTK_IMHTML_NO_TITLE) {
					got_tag = TRUE;
					NEW_BIT (NEW_TEXT_BIT);
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

				if (!font || !(font->size || font->face || font->fore || font->back)) {
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
						font->face = g_strdup (DEFAULT_FONT_NAME);
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
				if (url != NULL) {
					got_tag = TRUE;
					NEW_BIT (NEW_TEXT_BIT);
					g_free (url);
					url = NULL;
				} else {
					intag = FALSE;
					tpos = 0;
					continue;
				}
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
					clr = (bg != NULL) ? bg : imhtml->default_bg_color;

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
				   !g_strcasecmp (tag, "<H3>") ||
				   !g_strncasecmp (tag, "<H3 ", strlen ("<H3 ")) ||
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
				char *d;
				tag [tpos] = 0;
				d = tag;
				while (*d) {
					if ((smilelen = gtk_imhtml_is_smiley (imhtml, d)) != 0) {
						ws [wpos] = 0;
						wpos = 0;
						NEW_BIT (NEW_TEXT_BIT);
						g_snprintf (ws, smilelen + 1, "%s", d);
						NEW_BIT (NEW_SMILEY_BIT);
						d += smilelen;
					} else if (*d == '&') {
						gchar replace;
						gint length;
						if (is_amp_escape (d, &replace, &length)) {
							ws [wpos++] = replace;
							d += length;
						} else {
							ws [wpos++] = *d++;
						}
					} else if (*d == '\n') {
						if (!(options & GTK_IMHTML_NO_NEWLINE)) {
							ws [wpos] = 0;
							wpos = 0;
							NEW_BIT (NEW_TEXT_BIT);
							NEW_BIT (NEW_BR_BIT);
						}
						d++;
					} else {
						ws [wpos++] = *d++;
					}
				}
				tpos = 0;
			} else {
				wpos = 0;
			}
			intag = FALSE;
			tpos = 0;
		} else if (*c == '&' && !intag) {
			gchar replace;
			gint length;
			if (is_amp_escape (c, &replace, &length)) {
				ws [wpos++] = replace;
				c += length;
			} else {
				ws [wpos++] = *c++;
			}
		} else if (intag) {
			if (*c == '\"')
				tagquote++;
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
			} else if (*c == '&') {
				gchar replace;
				gint length;
				if (is_amp_escape (c, &replace, &length)) {
					ws [wpos++] = replace;
					c += length;
				} else {
					ws [wpos++] = *c++;
				}
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
			} else if (*c == '&') {
				gchar replace;
				gint length;
				if (is_amp_escape (c, &replace, &length)) {
					ws [wpos++] = replace;
					c += length;
				} else {
					ws [wpos++] = *c++;
				}
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
	}

	ws [wpos] = 0;
	NEW_BIT (NEW_TEXT_BIT);

	while (newbits) {
		GtkIMHtmlBit *bit = newbits->data;
		imhtml->bits = g_list_append (imhtml->bits, bit);
		newbits = g_list_remove (newbits, bit);
		gtk_imhtml_draw_bit (imhtml, bit);
	}

	GTK_LAYOUT (imhtml)->height = imhtml->y;
	GTK_LAYOUT (imhtml)->vadjustment->upper = imhtml->y;
	gtk_signal_emit_by_name (GTK_OBJECT (GTK_LAYOUT (imhtml)->vadjustment), "changed");

	gtk_widget_set_usize (GTK_WIDGET (imhtml), -1, imhtml->y);

	if (!(options & GTK_IMHTML_NO_SCROLL) &&
	    scrolldown &&
	    (imhtml->y >= MAX (1,
			       (GTK_WIDGET (imhtml)->allocation.height -
				(GTK_WIDGET (imhtml)->style->klass->ythickness + BORDER_SIZE) * 2))))
		gtk_adjustment_set_value (vadj, imhtml->y -
					  MAX (1, (GTK_WIDGET (imhtml)->allocation.height - 
						   (GTK_WIDGET (imhtml)->style->klass->ythickness +
						    BORDER_SIZE) * 2)));

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
		while (pre) {
			retval = g_string_append (retval, "</PRE>");
			pre--;
		}
	}
	g_free (ws);
	g_free (tag);

	return retval;
}

void
gtk_imhtml_clear (GtkIMHtml *imhtml)
{
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

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

	if (imhtml->selected_text) {
		g_string_free (imhtml->selected_text, TRUE);
		imhtml->selected_text = g_string_new ("");
	}

	if (imhtml->tip_timer) {
		gtk_timeout_remove (imhtml->tip_timer);
		imhtml->tip_timer = 0;
	}
	if (imhtml->tip_window) {
		gtk_widget_destroy (imhtml->tip_window);
		imhtml->tip_window = NULL;
	}
	imhtml->tip_bit = NULL;

	gdk_window_set_cursor (GTK_LAYOUT (imhtml)->bin_window, imhtml->arrow_cursor);

	imhtml->x = 0;
	imhtml->y = TOP_BORDER;
	imhtml->llheight = 0;
	imhtml->llascent = 0;
	imhtml->line = NULL;

	if (GTK_WIDGET_REALIZED (GTK_WIDGET (imhtml)))
		gdk_window_clear (GTK_LAYOUT (imhtml)->bin_window);
}

void
gtk_imhtml_page_up (GtkIMHtml *imhtml)
{
	GtkAdjustment *vadj;

	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	vadj = GTK_LAYOUT (imhtml)->vadjustment;
	gtk_adjustment_set_value (vadj, MAX (vadj->value - vadj->page_increment,
					     vadj->lower));
	gtk_signal_emit_by_name (GTK_OBJECT (vadj), "changed");
}

void
gtk_imhtml_page_down (GtkIMHtml *imhtml)
{
	GtkAdjustment *vadj;

	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	vadj = GTK_LAYOUT (imhtml)->vadjustment;
	gtk_adjustment_set_value (vadj, MIN (vadj->value + vadj->page_increment,
					     vadj->upper - vadj->page_size));
	gtk_signal_emit_by_name (GTK_OBJECT (vadj), "changed");
}
