
/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <ctype.h>

#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif

#include "gaim.h"
#include "gtkhtml.h"

#include "pixmaps/aol_icon.xpm"
#include "pixmaps/admin_icon.xpm"
#include "pixmaps/free_icon.xpm"
#include "pixmaps/dt_icon.xpm"
#define MAX_SIZE                 7
#define MIN_HTML_WIDTH_LINES     20
#define MIN_HTML_HEIGHT_LINES    10
#define BORDER_WIDTH             2
#define SCROLL_TIME              100
#define SCROLL_PIXELS            5
#define KEY_SCROLL_PIXELS        10

int font_sizes[] = { 80, 100, 120, 140, 200, 300, 400 };

/*
GdkFont *fixed_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *fixed_bold_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *fixed_italic_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *fixed_bold_italic_font[] =
	{ NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *prop_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *prop_bold_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *prop_italic_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
GdkFont *prop_bold_italic_font[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
*/

GData * font_cache;
static gboolean cache_init = FALSE;

struct font_state
{
	int size;
	int owncolor;
	int ownbg;
	gchar font[1024];
	GdkColor *color;
	GdkColor *bgcol;
	struct font_state *next;
};

struct font_state *push_state(struct font_state *current)
{
	struct font_state *tmp;
	tmp = (struct font_state *) g_new0(struct font_state, 1);
	tmp->next = current;
	tmp->color = current->color;
	tmp->bgcol = current->bgcol;
	tmp->size = current->size;
	tmp->owncolor = 0;
	tmp->ownbg = 0;
	strcpy( tmp->font, current->font );
	return tmp;
}

enum
{
	ARG_0,
	ARG_HADJUSTMENT,
	ARG_VADJUSTMENT,
};


enum
{
	TARGET_STRING,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT
};


static void gtk_html_class_init(GtkHtmlClass * klass);
static void gtk_html_set_arg(GtkObject * object, GtkArg * arg, guint arg_id);
static void gtk_html_get_arg(GtkObject * object, GtkArg * arg, guint arg_id);
static void gtk_html_init(GtkHtml * html);
static void gtk_html_destroy(GtkObject * object);
static void gtk_html_finalize(GtkObject * object);
static void gtk_html_realize(GtkWidget * widget);
static void gtk_html_unrealize(GtkWidget * widget);
static void gtk_html_style_set(GtkWidget * widget, GtkStyle * previous_style);
static void gtk_html_draw_focus(GtkWidget * widget);
static void gtk_html_size_request(GtkWidget * widget,
								  GtkRequisition * requisition);
static void gtk_html_size_allocate(GtkWidget * widget,
								   GtkAllocation * allocation);
static void gtk_html_adjustment(GtkAdjustment * adjustment, GtkHtml * html);
static void gtk_html_disconnect(GtkAdjustment * adjustment, GtkHtml * html);
static void gtk_html_add_seperator(GtkHtml * html);
// static void gtk_html_add_pixmap(GtkHtml * html, GdkPixmap * pm, gint fit);
static void gtk_html_add_text(GtkHtml * html,
							  GdkFont * font,
							  GdkColor * fore,
							  GdkColor * back,
							  gchar * chars,
							  gint length,
							  gint uline, gint strike, gchar * url);
static void gtk_html_draw_bit(GtkHtml * html,
							  GtkHtmlBit * htmlbit, gint redraw);
static void gtk_html_selection_get(GtkWidget * widget,
								   GtkSelectionData * selection_data,
								   guint sel_info, guint32 time);
static gint gtk_html_selection_clear(GtkWidget * widget,
									 GdkEventSelection * event);
static gint gtk_html_visibility_notify(GtkWidget * widget,
									   GdkEventVisibility * event);


/* Event handlers */
static void gtk_html_draw(GtkWidget * widget, GdkRectangle * area);
static gint gtk_html_expose(GtkWidget * widget, GdkEventExpose * event);
static gint gtk_html_button_press(GtkWidget * widget, GdkEventButton * event);
static gint gtk_html_button_release(GtkWidget * widget, GdkEventButton * event);
static gint gtk_html_motion_notify(GtkWidget * widget, GdkEventMotion * event);
static gint gtk_html_key_press(GtkWidget * widget, GdkEventKey * event);
static gint gtk_html_leave_notify(GtkWidget * widget, GdkEventCrossing * event);

static gint gtk_html_tooltip_timeout(gpointer data);


static void clear_area(GtkHtml * html, GdkRectangle * area);
static void expose_html(GtkHtml * html, GdkRectangle * area, gboolean cursor);
static void scroll_down(GtkHtml * html, gint diff0);
static void scroll_up(GtkHtml * html, gint diff0);

static void adjust_adj(GtkHtml * html, GtkAdjustment * adj);
static void resize_html(GtkHtml * html);
static gint html_bit_is_onscreen(GtkHtml * html, GtkHtmlBit * hb);
static void draw_cursor(GtkHtml * html);
static void undraw_cursor(GtkHtml * html);

static GtkWidgetClass *parent_class = NULL;

GtkType gtk_html_get_type(void)
{
	static GtkType html_type = 0;

	if (!html_type)
	{
		static const GtkTypeInfo html_info = {
			"GtkHtml",
			sizeof(GtkHtml),
			sizeof(GtkHtmlClass),
			(GtkClassInitFunc) gtk_html_class_init,
			(GtkObjectInitFunc) gtk_html_init,
			NULL,
			NULL,
			NULL,
		};
		html_type = gtk_type_unique(GTK_TYPE_WIDGET, &html_info);
	}
	return html_type;
}


static void gtk_html_class_init(GtkHtmlClass * class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	parent_class = gtk_type_class(GTK_TYPE_WIDGET);


	gtk_object_add_arg_type("GtkHtml::hadjustment",
							GTK_TYPE_ADJUSTMENT,
							GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
							ARG_HADJUSTMENT);

	gtk_object_add_arg_type("GtkHtml::vadjustment",
							GTK_TYPE_ADJUSTMENT,
							GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
							ARG_VADJUSTMENT);

	object_class->set_arg = gtk_html_set_arg;
	object_class->get_arg = gtk_html_get_arg;
	object_class->destroy = gtk_html_destroy;
	object_class->finalize = gtk_html_finalize;

	widget_class->realize = gtk_html_realize;
	widget_class->unrealize = gtk_html_unrealize;
	widget_class->style_set = gtk_html_style_set;
	widget_class->draw_focus = gtk_html_draw_focus;
	widget_class->size_request = gtk_html_size_request;
	widget_class->size_allocate = gtk_html_size_allocate;
	widget_class->draw = gtk_html_draw;
	widget_class->expose_event = gtk_html_expose;
	widget_class->button_press_event = gtk_html_button_press;
	widget_class->button_release_event = gtk_html_button_release;
	widget_class->motion_notify_event = gtk_html_motion_notify;
	widget_class->leave_notify_event = gtk_html_leave_notify;
	widget_class->selection_get = gtk_html_selection_get;
	widget_class->selection_clear_event = gtk_html_selection_clear;
	widget_class->key_press_event = gtk_html_key_press;
	widget_class->visibility_notify_event = gtk_html_visibility_notify;


	widget_class->set_scroll_adjustments_signal =
		gtk_signal_new("set_scroll_adjustments",
					   GTK_RUN_LAST,
					   object_class->type,
					   GTK_SIGNAL_OFFSET(GtkHtmlClass, set_scroll_adjustments),
					   gtk_marshal_NONE__POINTER_POINTER,
					   GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT,
					   GTK_TYPE_ADJUSTMENT);


	class->set_scroll_adjustments = gtk_html_set_adjustments;

}

static void gtk_html_set_arg(GtkObject * object, GtkArg * arg, guint arg_id)
{
	GtkHtml *html;

	html = GTK_HTML(object);

	switch (arg_id)
	{
	case ARG_HADJUSTMENT:
		gtk_html_set_adjustments(html, GTK_VALUE_POINTER(*arg), html->vadj);
		break;
	case ARG_VADJUSTMENT:
		gtk_html_set_adjustments(html, html->hadj, GTK_VALUE_POINTER(*arg));
		break;
	default:
		break;
	}
}

static void gtk_html_get_arg(GtkObject * object, GtkArg * arg, guint arg_id)
{
	GtkHtml *html;

	html = GTK_HTML(object);

	switch (arg_id)
	{
	case ARG_HADJUSTMENT:
		GTK_VALUE_POINTER(*arg) = html->hadj;
		break;
	case ARG_VADJUSTMENT:
		GTK_VALUE_POINTER(*arg) = html->vadj;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void gtk_html_init(GtkHtml * html)
{
	static const GtkTargetEntry targets[] = {
		{"STRING", 0, TARGET_STRING},
		{"TEXT", 0, TARGET_TEXT},
		{"COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT}
	};

	static const gint n_targets = sizeof(targets) / sizeof(targets[0]);

	GTK_WIDGET_SET_FLAGS(html, GTK_CAN_FOCUS);

	html->html_area = NULL;
	html->hadj = NULL;
	html->vadj = NULL;
	html->current_x = 0;
	html->current_y = 0;
	html->start_sel = html->end_sel = NULL;
	html->start_sel_x = html->start_sel_y = -1;
	html->num_end = html->num_start = -1;

	html->html_bits = NULL;
	html->urls = NULL;
	html->selected_text = NULL;
	html->tooltip_hb = NULL;
	html->tooltip_timer = -1;
	html->tooltip_window = NULL;
	html->cursor_hb = NULL;
	html->cursor_pos = 0;

	html->pm = NULL;

	html->editable = 0;
	html->transparent = 0;

	html->frozen = 0;

	gtk_selection_add_targets(GTK_WIDGET(html), GDK_SELECTION_PRIMARY,
							  targets, n_targets);



}


GtkWidget *gtk_html_new(GtkAdjustment * hadj, GtkAdjustment * vadj)
{
	GtkWidget *html;
	if(!cache_init)
	{
		g_datalist_init(&font_cache);
		cache_init = TRUE;
	}

	if (hadj)
		g_return_val_if_fail(GTK_IS_ADJUSTMENT(hadj), NULL);
	if (vadj)
		g_return_val_if_fail(GTK_IS_ADJUSTMENT(vadj), NULL);

	html = gtk_widget_new(GTK_TYPE_HTML,
						  "hadjustment", hadj, "vadjustment", vadj, NULL);

	return html;
}


void gtk_html_set_editable(GtkHtml * html, gboolean is_editable)
{
	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));


	html->editable = (is_editable != FALSE);

	if (is_editable)
		draw_cursor(html);
	else
		undraw_cursor(html);

}

void gtk_html_set_transparent(GtkHtml * html, gboolean is_transparent)
{
	GdkRectangle rect;
	gint width,
	  height;
	GtkWidget *widget;

	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));


	widget = GTK_WIDGET(html);
	html->transparent = (is_transparent != FALSE);

	if (!GTK_WIDGET_REALIZED(widget))
		return;

	html->bg_gc = NULL;
	gdk_window_get_size(widget->window, &width, &height);
	rect.x = 0;
	rect.y = 0;
	rect.width = width;
	rect.height = height;
	gdk_window_clear_area(widget->window, rect.x, rect.y, rect.width,
						  rect.height);

	expose_html(html, &rect, FALSE);
	gtk_html_draw_focus((GtkWidget *) html);
}


void gtk_html_set_adjustments(GtkHtml * html,
							  GtkAdjustment * hadj, GtkAdjustment * vadj)
{
	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));
	if (hadj)
		g_return_if_fail(GTK_IS_ADJUSTMENT(hadj));
	else
		hadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
	if (vadj)
		g_return_if_fail(GTK_IS_ADJUSTMENT(vadj));
	else
		vadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (html->hadj && (html->hadj != hadj))
	{
		gtk_signal_disconnect_by_data(GTK_OBJECT(html->hadj), html);
		gtk_object_unref(GTK_OBJECT(html->hadj));
	}

	if (html->vadj && (html->vadj != vadj))
	{
		gtk_signal_disconnect_by_data(GTK_OBJECT(html->vadj), html);
		gtk_object_unref(GTK_OBJECT(html->vadj));
	}

	if (html->hadj != hadj)
	{
		html->hadj = hadj;
		gtk_object_ref(GTK_OBJECT(html->hadj));
		gtk_object_sink(GTK_OBJECT(html->hadj));

		gtk_signal_connect(GTK_OBJECT(html->hadj), "changed",
						   (GtkSignalFunc) gtk_html_adjustment, html);
		gtk_signal_connect(GTK_OBJECT(html->hadj), "value_changed",
						   (GtkSignalFunc) gtk_html_adjustment, html);
		gtk_signal_connect(GTK_OBJECT(html->hadj), "disconnect",
						   (GtkSignalFunc) gtk_html_disconnect, html);
		gtk_html_adjustment(hadj, html);
	}

	if (html->vadj != vadj)
	{
		html->vadj = vadj;
		gtk_object_ref(GTK_OBJECT(html->vadj));
		gtk_object_sink(GTK_OBJECT(html->vadj));

		gtk_signal_connect(GTK_OBJECT(html->vadj), "changed",
						   (GtkSignalFunc) gtk_html_adjustment, html);
		gtk_signal_connect(GTK_OBJECT(html->vadj), "value_changed",
						   (GtkSignalFunc) gtk_html_adjustment, html);
		gtk_signal_connect(GTK_OBJECT(html->vadj), "disconnect",
						   (GtkSignalFunc) gtk_html_disconnect, html);
		gtk_html_adjustment(vadj, html);
	}
}



GdkColor *get_color(int colorv, GdkColormap * map)
{
	GdkColor *color;
#if 0
	fprintf(stdout, "color is %x\n", colorv);
#endif
	color = (GdkColor *) g_new0(GdkColor, 1);
	color->red = ((colorv & 0xff0000) >> 16) * 256;
	color->green = ((colorv & 0xff00) >> 8) * 256;
	color->blue = ((colorv & 0xff)) * 256;
#if 0
	fprintf(stdout, "Colors are %d, %d, %d\n", color->red, color->green,
			color->blue);
#endif
	gdk_color_alloc(map, color);
	return color;
}


int load_font_with_cache(const char *name, const char *weight, char slant,
	int size, GdkFont **font_return)
{
	gchar font_spec[1024];

	if (size > 0)
		g_snprintf(font_spec, sizeof font_spec,
			"-*-%s-%s-%c-*-*-*-%d-*-*-*-*-*-*",
			name, weight, slant, size);
	else
		g_snprintf(font_spec, sizeof font_spec,
			"-*-*-*-*-*-*-*-*-*-*-*-*-*-*");

	if((*font_return = g_datalist_id_get_data(&font_cache,
				g_quark_from_string(font_spec)))) {
		return TRUE;
	} else if ((*font_return = gdk_font_load(font_spec))) {
		g_datalist_id_set_data(&font_cache,
			g_quark_from_string(font_spec), *font_return);
		return TRUE;
	} else {
		return FALSE;
	}
}


GdkFont *getfont(const char *font, int bold, int italic, int fixed, int size)
{
	GdkFont *my_font = NULL;
	gchar *weight, slant;

	if (!font || !strlen(font)) font = fixed ? "courier" : "helvetica";
	weight = bold ? "bold" : "medium";
	slant = italic ? 'i' : 'r';

	if (size > MAX_SIZE) size = MAX_SIZE;
	if (size < 1) size = 1;
	size = font_sizes[size-1];
	
	/* try both 'i'talic and 'o'blique for italic fonts, and keep
	 * increasing the size until we get one that works. */	

	while (size <= 720) {
		if (load_font_with_cache(font, weight, slant, size, &my_font))
			return my_font;
		if (italic && load_font_with_cache(font, weight, 'o', size, &my_font))
			return my_font;
		size += 10;
	}

	/* since we couldn't get any size up to 72, fall back to the
	 * default fonts. */

	font = fixed ? "courier" : "helvetica";
	size = 120;
	while (size <= 720) {
		if (load_font_with_cache(font, weight, slant, size, &my_font))
			return my_font;
		size += 10;
	}
	
	font = fixed ? "helvetica" : "courier";
	size = 120;
	while (size <= 720) {
		if (load_font_with_cache(font, weight, slant, size, &my_font))
			return my_font;
		size += 10;
	}

	/* whoops, couldn't do any of those. maybe they have a default outgoing
	 * font? maybe we can use that. */
	if (font_options & OPT_FONT_FACE) {
		if (fontname != NULL) {
			/* woohoo! */
			size = 120;
			while (size <= 720) {
				if (load_font_with_cache(fontname, "medium", 'r', size, &my_font))
					return my_font;
				size += 10;
			}
		}
	}

	/* ok, now we're in a pickle. if we can't do any of the above, let's
	 * try doing the most boring font we can find. */
	size = 120;
	while (size <= 720) {
		if (load_font_with_cache("courier", "medium", 'r', size, &my_font))
			return my_font;
		size += 10;
	}

	size = 120;
	while (size <= 720) {
		if (load_font_with_cache("helvetica", "medium", 'r', size, &my_font))
			return my_font;
		size += 10;
	}

	size = 120;
	while (size <= 720) {
		if (load_font_with_cache("times", "medium", 'r', size, &my_font))
			return my_font;
		size += 10;
	}

	/* my god, how did we end up here. is there a 'generic font' function
	 * in gdk? that would be incredibly useful here. there's gotta be a
	 * better way to do this. */
	
	/* well, if they can't do any of the fonts above, they'll take whatever
	 * they can get, and be happy about it, damn it. :) */
	load_font_with_cache("*", "*", '*', -1, &my_font);
	return my_font;
}


/* 'Borrowed' from ETerm */
GdkWindow *get_desktop_window(GtkWidget * widget)
{
#ifndef _WIN32
	GdkAtom prop,
	  type,
	  prop2;
	int format;
	gint length;
	guchar *data;
	GtkWidget *w;

	prop = gdk_atom_intern("_XROOTPMAP_ID", 1);
	prop2 = gdk_atom_intern("_XROOTCOLOR_PIXEL", 1);

	if (prop == None && prop2 == None)
	{
		return NULL;
	}



	for (w = widget; w; w = w->parent)
	{

		if (prop != None)
		{
			gdk_property_get(w->window, prop, AnyPropertyType, 0L, 1L, 0,
							 &type, &format, &length, &data);
		}
		else if (prop2 != None)
		{
			gdk_property_get(w->window, prop2, AnyPropertyType, 0L, 1L, 0,
							 &type, &format, &length, &data);
		}
		else
		{
			continue;
		}
		if (type != None)
		{
			return (w->window);
		}
	}
#endif
	return NULL;

}



GdkPixmap *get_desktop_pixmap(GtkWidget * widget)
{
#ifndef _WIN32
	GdkPixmap *p;
	GdkAtom prop,
	  type,
	  prop2;
	int format;
	gint length;
	guint32 id;
	guchar *data;

	prop = gdk_atom_intern("_XROOTPMAP_ID", 1);
	prop2 = gdk_atom_intern("_XROOTCOLOR_PIXEL", 1);


	if (prop == None && prop2 == None)
	{
		return NULL;
	}

	if (prop != None)
	{
		gdk_property_get(get_desktop_window(widget), prop, AnyPropertyType, 0L,
						 1L, 0, &type, &format, &length, &data);
		if (type == XA_PIXMAP)
		{
			id = data[0];
			id += data[1] << 8;
			id += data[2] << 16;
			id += data[3] << 24;
			p = gdk_pixmap_foreign_new(id);
			return p;
		}
	}
	if (prop2 != None)
	{

/*                XGetWindowProperty(Xdisplay, desktop_window, prop2, 0L, 1L, False, AnyPropertyType,
                                   &type, &format, &length, &after, &data);*/

/*                if (type == XA_CARDINAL) {*/
		/*
		 * D_PIXMAP(("  Solid color not yet supported.\n"));
		 */

/*                        return NULL;
                }*/
	}
	/*
	 * D_PIXMAP(("No suitable attribute found.\n"));
	 */
#endif
	return NULL;
}


static void clear_focus_area(GtkHtml * html,
							 gint area_x,
							 gint area_y, gint area_width, gint area_height)
{
	GtkWidget *widget = GTK_WIDGET(html);
	gint x,
	  y;

	gint ythick = BORDER_WIDTH + widget->style->klass->ythickness;
	gint xthick = BORDER_WIDTH + widget->style->klass->xthickness;

	gint width,
	  height;

	if (html->frozen > 0)
		return;

	if (html->transparent)
	{
		if (html->pm == NULL)
			html->pm = get_desktop_pixmap(widget);

		if (html->pm == NULL)
			return;

		if (html->bg_gc == NULL)
		{
			GdkGCValues values;

			values.tile = html->pm;
			values.fill = GDK_TILED;

			html->bg_gc = gdk_gc_new_with_values(html->html_area, &values,
												 GDK_GC_FILL | GDK_GC_TILE);

		}

		gdk_window_get_deskrelative_origin(widget->window, &x, &y);

		gdk_draw_pixmap(widget->window, html->bg_gc, html->pm,
						x + area_x, y + area_y, area_x, area_y, area_width,
						area_height);


	}
	else
	{
		gdk_window_get_size(widget->style->bg_pixmap[GTK_STATE_NORMAL], &width,
							&height);

		gdk_gc_set_ts_origin(html->bg_gc,
							 (-html->xoffset + xthick) % width,
							 (-html->yoffset + ythick) % height);

		gdk_draw_rectangle(widget->window, html->bg_gc, TRUE,
						   area_x, area_y, area_width, area_height);
	}
}

static void gtk_html_draw_focus(GtkWidget * widget)
{
	GtkHtml *html;
	gint width,
	  height;
	gint x,
	  y;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));

	html = GTK_HTML(widget);

	if (GTK_WIDGET_DRAWABLE(widget))
	{
		gint ythick = widget->style->klass->ythickness;
		gint xthick = widget->style->klass->xthickness;
		gint xextra = BORDER_WIDTH;
		gint yextra = BORDER_WIDTH;

		x = 0;
		y = 0;
		width = widget->allocation.width;
		height = widget->allocation.height;

		if (GTK_WIDGET_HAS_FOCUS(widget))
		{
			x += 1;
			y += 1;
			width -= 2;
			height -= 2;
			xextra -= 1;
			yextra -= 1;

			gtk_paint_focus(widget->style, widget->window,
							NULL, widget, "text",
							0, 0,
							widget->allocation.width - 1,
							widget->allocation.height - 1);
		}

		gtk_paint_shadow(widget->style, widget->window,
						 GTK_STATE_NORMAL, GTK_SHADOW_IN,
						 NULL, widget, "text", x, y, width, height);

		x += xthick;
		y += ythick;
		width -= 2 * xthick;
		height -= 2 * ythick;


		if (widget->style->bg_pixmap[GTK_STATE_NORMAL] || html->transparent)
		{
			/*
			 * top rect 
			 */
			clear_focus_area(html, x, y, width, yextra);
			/*
			 * left rect 
			 */
			clear_focus_area(html, x, y + yextra,
							 xextra, y + height - 2 * yextra);
			/*
			 * right rect 
			 */
			clear_focus_area(html, x + width - xextra, y + yextra,
							 xextra, height - 2 * ythick);
			/*
			 * bottom rect 
			 */
			clear_focus_area(html, x, x + height - yextra, width, yextra);
		}
	}
}

static void gtk_html_size_request(GtkWidget * widget,
								  GtkRequisition * requisition)
{
	gint xthickness;
	gint ythickness;
	gint char_height;
	gint char_width;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));
	g_return_if_fail(requisition != NULL);

	xthickness = widget->style->klass->xthickness + BORDER_WIDTH;
	ythickness = widget->style->klass->ythickness + BORDER_WIDTH;

	char_height = MIN_HTML_HEIGHT_LINES * (widget->style->font->ascent +
										   widget->style->font->descent);

	char_width = MIN_HTML_WIDTH_LINES * (gdk_text_width(widget->style->font,
														"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
														26) / 26);

	requisition->width = char_width + xthickness * 2;
	requisition->height = char_height + ythickness * 2;
}

static void gtk_html_size_allocate(GtkWidget * widget,
								   GtkAllocation * allocation)
{
	GtkHtml *html;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));
	g_return_if_fail(allocation != NULL);

	html = GTK_HTML(widget);

	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED(widget))
	{
		gdk_window_move_resize(widget->window,
							   allocation->x, allocation->y,
							   allocation->width, allocation->height);

		gdk_window_move_resize(html->html_area,
							   widget->style->klass->xthickness + BORDER_WIDTH,
							   widget->style->klass->ythickness + BORDER_WIDTH,
							   MAX(1, (gint) widget->allocation.width -
								   (gint) (widget->style->klass->xthickness +
										   (gint) BORDER_WIDTH) * 2),
							   MAX(1, (gint) widget->allocation.height -
								   (gint) (widget->style->klass->ythickness +
										   (gint) BORDER_WIDTH) * 2));

		resize_html(html);
	}
}

static void gtk_html_draw(GtkWidget * widget, GdkRectangle * area)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));
	g_return_if_fail(area != NULL);

	if (GTK_WIDGET_DRAWABLE(widget))
	{
		expose_html(GTK_HTML(widget), area, TRUE);
		gtk_widget_draw_focus(widget);
	}
}


static gint gtk_html_expose(GtkWidget * widget, GdkEventExpose * event)
{
	GtkHtml *html;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_HTML(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	html = GTK_HTML(widget);

	if (event->window == html->html_area)
	{
		expose_html(html, &event->area, TRUE);
	}
	else if (event->count == 0)
	{
		gtk_widget_draw_focus(widget);
	}

	return FALSE;

}


static gint gtk_html_selection_clear(GtkWidget * widget,
									 GdkEventSelection * event)
{
	GtkHtml *html;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_HTML(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	/*
	 * Let the selection handling code know that the selection
	 * * has been changed, since we've overriden the default handler 
	 */
	if (!gtk_selection_clear(widget, event))
		return FALSE;

	html = GTK_HTML(widget);

	if (event->selection == GDK_SELECTION_PRIMARY)
	{
		if (html->selected_text)
		{
			GList *hbits = html->html_bits;
			GtkHtmlBit *hb;

			g_free(html->selected_text);
			html->selected_text = NULL;
			html->start_sel = NULL;
			html->end_sel = NULL;
			html->num_start = 0;
			html->num_end = 0;
			while (hbits)
			{
				hb = (GtkHtmlBit *) hbits->data;
				if (hb->was_selected)
					gtk_html_draw_bit(html, hb, 1);
				hbits = hbits->prev;
			}
			hbits = g_list_last(html->html_bits);
		}
	}

	return TRUE;
}



static void gtk_html_selection_get(GtkWidget * widget,
								   GtkSelectionData * selection_data,
								   guint sel_info, guint32 time)
{
	gchar *str;
	gint len;
	GtkHtml *html;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));

	html = GTK_HTML(widget);


	if (selection_data->selection != GDK_SELECTION_PRIMARY)
		return;

	str = html->selected_text;

	if (!str)
		return;

	len = strlen(str);

	if (sel_info == TARGET_STRING)
	{
		gtk_selection_data_set(selection_data,
							   GDK_SELECTION_TYPE_STRING,
							   8 * sizeof(gchar), (guchar *) str, len);
	}
	else if ((sel_info == TARGET_TEXT) || (sel_info == TARGET_COMPOUND_TEXT))
	{
		guchar *text;
		GdkAtom encoding;
		gint format;
		gint new_length;

		gdk_string_to_compound_text(str, &encoding, &format, &text,
									&new_length);
		gtk_selection_data_set(selection_data, encoding, format, text,
							   new_length);
		gdk_free_compound_text(text);
	}



}

static void do_select(GtkHtml * html, int x, int y)
{
	GList *hbits = g_list_last(html->html_bits);
	int epos,
	  spos;
	GtkHtmlBit *hb;

	if (!hbits)
		return;

	hb = (GtkHtmlBit *) hbits->data;

	while (hbits)
	{
		hb = (GtkHtmlBit *) hbits->data;
		if (hb->type == HTML_BIT_TEXT)
			break;
		hbits = hbits->prev;
	}

	if (!hb)
		return;


	if (y > hb->y)
	{
		html->num_end = strlen(hb->text) - 1;
		html->end_sel = hb;
	}
	else if (y < 0)
	{
		html->num_end = 0;
		html->end_sel = (GtkHtmlBit *) html->html_bits->data;
	}
	else
		while (hbits)
		{
			hb = (GtkHtmlBit *) hbits->data;
			if ((y < hb->y && y > (hb->y - hb->height)) &&
				(x > hb->x + hb->width))
			{
				if (hb->type != HTML_BIT_TEXT)
				{
					html->num_end = 0;
					html->end_sel = hb;
					break;
				}

				html->num_end = strlen(hb->text) - 1;
				html->end_sel = hb;
				break;
			}
			else if ((x > hb->x && x < (hb->x + hb->width)) &&
					 (y < hb->y && y > (hb->y - hb->height)))
			{
				int i,
				  len;
				int w = x - hb->x;

				if (hb->type != HTML_BIT_TEXT)
				{
					html->num_end = 0;
					html->end_sel = hb;
					break;
				}

				len = strlen(hb->text);

				for (i = 1; i <= len; i++)
				{
					if (gdk_text_measure(hb->font, hb->text, i) > w)
					{
						html->num_end = i - 1;
						html->end_sel = hb;
						break;
					}
				}
				break;
			}
			hbits = hbits->prev;
		}

	if (html->end_sel == NULL)
		return;
	if (html->start_sel == NULL)
	{
		html->start_sel = html->end_sel;
		html->num_start = html->num_end;
	}

	epos = g_list_index(html->html_bits, html->end_sel);
	spos = g_list_index(html->html_bits, html->start_sel);
	g_free(html->selected_text);
	html->selected_text = NULL;

	if (epos == spos)
	{
		char *str;
		if (html->start_sel->type != HTML_BIT_TEXT)
		{
			html->selected_text = NULL;
			return;
		}
		if (html->num_end == html->num_start)
		{
			str = g_malloc(2);
			if (strlen(html->start_sel->text))
				str[0] = html->start_sel->text[html->num_end];
			else
				str[0] = 0;
			str[1] = 0;
			gtk_html_draw_bit(html, html->start_sel, 0);
			html->selected_text = str;
		}
		else
		{
			size_t st,
			  en;
			char *str;
			if (html->num_end > html->num_start)
			{
				en = html->num_end;
				st = html->num_start;
			}
			else
			{
				en = html->num_start;
				st = html->num_end;
			}

			str = g_malloc(en - st + 2);
			strncpy(str, html->start_sel->text + st, (en - st + 1));
			str[en - st + 1] = 0;
			gtk_html_draw_bit(html, html->start_sel, 0);
			html->selected_text = str;

		}
	}
	else
	{
		GtkHtmlBit *shb,
		 *ehb;
		size_t en,
		  st;
		int len,
		  nlen;
		char *str;
		if (epos > spos)
		{
			shb = html->start_sel;
			ehb = html->end_sel;
			en = html->num_end;
			st = html->num_start;
		}
		else
		{
			shb = html->end_sel;
			ehb = html->start_sel;
			en = html->num_start;
			st = html->num_end;
		}

		hbits = g_list_find(html->html_bits, shb);

		if (!hbits)
			return;

		if (shb->type == HTML_BIT_TEXT)
		{
			len = strlen(shb->text) - st + 1;
			str = g_malloc(len);
			strcpy(str, shb->text + st);
			str[len - 1] = 0;
			gtk_html_draw_bit(html, shb, 0);
			if (shb->newline)
			{
				len += 1;
				str = g_realloc(str, len);
				str[len - 2] = '\n';
				str[len - 1] = 0;
			}
		}
		else
		{
			len = 1;
			str = g_malloc(1);
			str[0] = 0;
		}
		if (hbits->next == NULL)
		{
			html->selected_text = str;
			return;
		}


		hbits = hbits->next;
		while (1)
		{						/*
								 * Yah I know is dangerous :P 
								 */
			hb = (GtkHtmlBit *) hbits->data;
			if (hb->type != HTML_BIT_TEXT)
			{
				if (hb == ehb)
					break;
				hbits = hbits->next;
				continue;
			}
			if (hb != ehb)
			{
				nlen = len + strlen(hb->text);
				str = g_realloc(str, nlen);
				strcpy(str + (len - 1), hb->text);
				len = nlen;
				str[len - 1] = 0;
				gtk_html_draw_bit(html, hb, 0);
				if (hb->newline)
				{
					len += 1;
					str = g_realloc(str, len);
					str[len - 2] = '\n';
					str[len - 1] = 0;
				}
			}
			else
			{
				nlen = len + en + 1;
				str = g_realloc(str, nlen);
				strncpy(str + (len - 1), hb->text, en + 1);
				len = nlen;
				str[len - 1] = 0;

				gtk_html_draw_bit(html, hb, 0);
				if (hb->newline && en == strlen(hb->text))
				{
					len += 1;
					str = g_realloc(str, len);
					str[len - 2] = '\n';
					str[len - 1] = 0;
				}
				break;
			}
			hbits = hbits->next;
		}
		html->selected_text = str;
	}

}

static gint scroll_timeout(GtkHtml * html)
{
	GdkEventMotion event;
	gint x,
	  y;
	GdkModifierType mask;

	html->timer = 0;
	gdk_window_get_pointer(html->html_area, &x, &y, &mask);

	if (mask & GDK_BUTTON1_MASK)
	{
		event.is_hint = 0;
		event.x = x;
		event.y = y;
		event.state = mask;

		gtk_html_motion_notify(GTK_WIDGET(html), &event);
	}

	return FALSE;

}


static gint gtk_html_tooltip_paint_window(GtkHtml * html)
{
	GtkStyle *style;
	gint y,
	  baseline_skip,
	  gap;

	style = html->tooltip_window->style;

	gap = (style->font->ascent + style->font->descent) / 4;
	if (gap < 2)
		gap = 2;
	baseline_skip = style->font->ascent + style->font->descent + gap;

	if (!html->tooltip_hb)
		return FALSE;

	gtk_paint_flat_box(style, html->tooltip_window->window,
					   GTK_STATE_NORMAL, GTK_SHADOW_OUT,
					   NULL, GTK_WIDGET(html->tooltip_window), "tooltip",
					   0, 0, -1, -1);

	y = style->font->ascent + 4;

	gtk_paint_string(style, html->tooltip_window->window,
					 GTK_STATE_NORMAL,
					 NULL, GTK_WIDGET(html->tooltip_window), "tooltip",
					 4, y, "HTML Link:");
	y += baseline_skip;
	gtk_paint_string(style, html->tooltip_window->window,
					 GTK_STATE_NORMAL,
					 NULL, GTK_WIDGET(html->tooltip_window), "tooltip",
					 4, y, html->tooltip_hb->url);

	return FALSE;


}

static gint gtk_html_tooltip_timeout(gpointer data)
{
	GtkHtml *html = (GtkHtml *) data;


	GDK_THREADS_ENTER();

	if (html->tooltip_hb && GTK_WIDGET_DRAWABLE(GTK_WIDGET(html)))
	{
		GtkWidget *widget;
		GtkStyle *style;
		gint gap,
		  x,
		  y,
		  w,
		  h,
		  scr_w,
		  scr_h,
		  baseline_skip;

		if (html->tooltip_window)
			gtk_widget_destroy(html->tooltip_window);

		html->tooltip_window = gtk_window_new(GTK_WINDOW_POPUP);
		gtk_widget_set_app_paintable(html->tooltip_window, TRUE);
		gtk_window_set_policy(GTK_WINDOW(html->tooltip_window), FALSE, FALSE,
							  TRUE);
		gtk_widget_set_name(html->tooltip_window, "gtk-tooltips");
		gtk_signal_connect_object(GTK_OBJECT(html->tooltip_window),
								  "expose_event",
								  GTK_SIGNAL_FUNC
								  (gtk_html_tooltip_paint_window),
								  GTK_OBJECT(html));
		gtk_signal_connect_object(GTK_OBJECT(html->tooltip_window), "draw",
								  GTK_SIGNAL_FUNC
								  (gtk_html_tooltip_paint_window),
								  GTK_OBJECT(html));

		gtk_widget_ensure_style(html->tooltip_window);
		style = html->tooltip_window->style;

		widget = GTK_WIDGET(html);

		scr_w = gdk_screen_width();
		scr_h = gdk_screen_height();

		gap = (style->font->ascent + style->font->descent) / 4;
		if (gap < 2)
			gap = 2;
		baseline_skip = style->font->ascent + style->font->descent + gap;

		w = 8 + MAX(gdk_string_width(style->font, _("HTML Link:")),
					gdk_string_width(style->font, html->tooltip_hb->url));
		;
		h = 8 - gap;
		h += (baseline_skip * 2);

		gdk_window_get_pointer(NULL, &x, &y, NULL);
		/*
		 * gdk_window_get_origin (widget->window, NULL, &y);
		 */
		if (GTK_WIDGET_NO_WINDOW(widget))
			y += widget->allocation.y;

		x -= ((w >> 1) + 4);

		if ((x + w) > scr_w)
			x -= (x + w) - scr_w;
		else if (x < 0)
			x = 0;

		if ((y + h + 4) > scr_h)
			y =
				y - html->tooltip_hb->font->ascent +
				html->tooltip_hb->font->descent;
		else
			y =
				y + html->tooltip_hb->font->ascent +
				html->tooltip_hb->font->descent;

		gtk_widget_set_usize(html->tooltip_window, w, h);
		gtk_widget_popup(html->tooltip_window, x, y);

	}

	html->tooltip_timer = -1;

	GDK_THREADS_LEAVE();

	return FALSE;
}


static gint gtk_html_leave_notify(GtkWidget * widget, GdkEventCrossing * event)
{
	GtkHtml *html;

	html = GTK_HTML(widget);

	if (html->tooltip_timer != -1)
		gtk_timeout_remove(html->tooltip_timer);
	if (html->tooltip_window)
	{
		gtk_widget_destroy(html->tooltip_window);
		html->tooltip_window = NULL;
	}


	html->tooltip_hb = NULL;
	return TRUE;
}


static gint gtk_html_motion_notify(GtkWidget * widget, GdkEventMotion * event)
{
	int x,
	  y;
	gint width,
	  height;
	GdkModifierType state;
	int realx,
	  realy;
	GtkHtml *html = GTK_HTML(widget);

	if (event->is_hint)
		gdk_window_get_pointer(event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	gdk_window_get_size(html->html_area, &width, &height);

	realx = x;
	realy = y + html->yoffset;


	if (state & GDK_BUTTON1_MASK)
	{
		if (realx != html->start_sel_x || realy != html->start_sel_y)
		{
			char *tmp = NULL;

			if (y < 0 || y > height)
			{
				int diff;
				if (html->timer == 0)
				{
					html->timer = gtk_timeout_add(100,
												  (GtkFunction) scroll_timeout,
												  html);
					if (y < 0)
						diff = y / 2;
					else
						diff = (y - height) / 2;

					if (html->vadj->value + diff >
						html->vadj->upper - height + 20)
						gtk_adjustment_set_value(html->vadj,
												 html->vadj->upper - height +
												 20);
					else
						gtk_adjustment_set_value(html->vadj,
												 html->vadj->value + diff);

				}
			}

			if (html->selected_text != NULL)
				tmp = g_strdup(html->selected_text);
			do_select(html, realx, realy);
			if (tmp)
			{
				if (!html->selected_text || strcmp(tmp, html->selected_text))
				{
					GtkHtmlBit *hb;
					GList *hbits = html->html_bits;
					while (hbits)
					{
						hb = (GtkHtmlBit *) hbits->data;
						if (hb->was_selected)
							gtk_html_draw_bit(html, hb, 0);
						hbits = hbits->next;
					}
				}
				g_free(tmp);
			}
		}
	}
	else
	{
		GtkHtmlBit *hb;
		GList *urls;

		urls = html->urls;
		while (urls)
		{
			hb = (GtkHtmlBit *) urls->data;
			if ((realx > hb->x && realx < (hb->x + hb->width)) &&
				(realy < hb->y && realy > (hb->y - hb->height)))
			{
				GdkCursor *cursor = NULL;

				if (html->tooltip_hb != hb)
				{
					html->tooltip_hb = hb;
					if (html->tooltip_timer != -1)
						gtk_timeout_remove(html->tooltip_timer);
					if (html->tooltip_window)
					{
						gtk_widget_destroy(html->tooltip_window);
						html->tooltip_window = NULL;
					}
					html->tooltip_timer =
						gtk_timeout_add(HTML_TOOLTIP_DELAY,
										gtk_html_tooltip_timeout, html);
				}

				cursor = gdk_cursor_new(GDK_HAND2);
				gdk_window_set_cursor(html->html_area, cursor);
				gdk_cursor_destroy(cursor);

				return TRUE;
			}
			urls = urls->next;
		}
		if (html->tooltip_timer != -1)
			gtk_timeout_remove(html->tooltip_timer);
		if (html->tooltip_window)
		{
			gtk_widget_destroy(html->tooltip_window);
			html->tooltip_window = NULL;
		}


		html->tooltip_hb = NULL;
		gdk_window_set_cursor(html->html_area, NULL);


	}

	return TRUE;
}

static gint gtk_html_button_release(GtkWidget * widget, GdkEventButton * event)
{
	GtkHtml *html;

	html = GTK_HTML(widget);

	if (html->frozen > 0)
		return TRUE;

	if (event->button == 1)
	{
		int realx,
		  realy;
		GtkHtmlBit *hb;
		GList *urls = html->urls;

		realx = event->x;
		realy = event->y + html->yoffset;
		if (realx != html->start_sel_x || realy != html->start_sel_y)
		{
			if (gtk_selection_owner_set(widget,
										GDK_SELECTION_PRIMARY, event->time))
			{
			}
			else
			{
			}
		}
		else
		{
			if (gdk_selection_owner_get(GDK_SELECTION_PRIMARY) ==
				widget->window)
				gtk_selection_owner_set(NULL, GDK_SELECTION_PRIMARY,
										event->time);


			while (urls)
			{
				void open_url_nw(GtkWidget * w, char *url);
				hb = (GtkHtmlBit *) urls->data;
				if ((realx > hb->x && realx < (hb->x + hb->width)) &&
					(realy < hb->y && realy > (hb->y - hb->height)))
				{
					open_url_nw(NULL, hb->url);
					// else
					//         open_url(NULL, hb->url);
					break;
				}
				urls = urls->next;
			}
		}
	}
	return TRUE;
}



static gint gtk_html_button_press(GtkWidget * widget, GdkEventButton * event)
{
	GtkHtml *html;
	gfloat value;


	html = GTK_HTML(widget);
	value = html->vadj->value;

	if (html->frozen > 0)
		return TRUE;

	if (event->button == 4)
	{
		value -= html->vadj->step_increment;
		if (value < html->vadj->lower)
			value = html->vadj->lower;
		gtk_adjustment_set_value(html->vadj, value);
	}
	else if (event->button == 5)
	{
		value += html->vadj->step_increment;
		if (value > html->vadj->upper)
			value = html->vadj->upper;
		gtk_adjustment_set_value(html->vadj, value);

	}
	else if (event->button == 1)
	{
		GList *hbits = g_list_last(html->html_bits);
		int realx,
		  realy;
		GtkHtmlBit *hb;

		realx = event->x;
		realy = event->y + html->yoffset;

		html->start_sel_x = realx;
		html->start_sel_y = realy;

		if (!hbits)
			return TRUE;

		if (html->selected_text)
		{
			g_free(html->selected_text);
			html->selected_text = NULL;
			html->start_sel = NULL;
			html->end_sel = NULL;
			html->num_start = 0;
			html->num_end = 0;
			while (hbits)
			{
				hb = (GtkHtmlBit *) hbits->data;
				if (hb->was_selected)
					gtk_html_draw_bit(html, hb, 1);
				hbits = hbits->prev;
			}
			hbits = g_list_last(html->html_bits);
		}

		hb = (GtkHtmlBit *) hbits->data;
		if (realy > hb->y)
		{
			if (hb->text)
				html->num_start = strlen(hb->text) - 1;
			else
				html->num_start = 0;
			html->start_sel = hb;
		}
		else
			while (hbits)
			{
				hb = (GtkHtmlBit *) hbits->data;
				if ((realy < hb->y && realy > (hb->y - hb->height)) &&
					(realx > hb->x + hb->width))
				{
					if (hb->type != HTML_BIT_TEXT)
					{
						html->num_end = 0;
						html->end_sel = hb;
						break;
					}

					if (hb->text)
						html->num_start = strlen(hb->text) - 1;
					else
						html->num_start = 0;

					html->start_sel = hb;
					break;
				}
				else if ((realx > hb->x && realx < (hb->x + hb->width)) &&
						 (realy < hb->y && realy > (hb->y - hb->height)))
				{
					int i,
					  len;
					int w = realx - hb->x;

					if (hb->type != HTML_BIT_TEXT)
					{
						html->num_end = 0;
						html->end_sel = hb;
						break;
					}

					if (hb->text)
						len = strlen(hb->text);
					else
						len = 0;

					for (i = 1; i <= len; i++)
					{
						if (gdk_text_measure(hb->font, hb->text, i) > w)
						{
							html->num_start = i - 1;
							html->start_sel = hb;
							break;
						}
					}
					break;
				}
				hbits = hbits->prev;
			}
	}
	else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
	{
		GtkHtmlBit *hb = NULL;
		int realx,
		  realy;
		GList *urls;

		realx = event->x;
		realy = event->y + html->yoffset;

		urls = html->urls;
		while (urls)
		{
			hb = (GtkHtmlBit *) urls->data;
			if ((realx > hb->x && realx < (hb->x + hb->width)) &&
				(realy < hb->y && realy > (hb->y - hb->height)))
			{
				break;
			}
			urls = urls->next;
			hb = NULL;
		}

		if (hb != NULL)
		{
			
			  GtkWidget *menu, *button;
			   
			   menu = gtk_menu_new();
			   
			   if (web_browser == BROWSER_NETSCAPE) {
			   
			   button = gtk_menu_item_new_with_label(_("Open URL in existing window"));
			   gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(open_url), hb->url);
			   gtk_menu_append(GTK_MENU(menu), button);
			   gtk_widget_show(button);
			   
			   }
			   
			   
			   button = gtk_menu_item_new_with_label(_("Open URL in new window"));
			   gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(open_url_nw), hb->url);
			   gtk_menu_append(GTK_MENU(menu), button);
			   gtk_widget_show(button);
			   
			   if (web_browser == BROWSER_NETSCAPE) {
			   
			   button = gtk_menu_item_new_with_label(_("Add URL as bookmark"));
			   gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(add_bookmark), hb->url);
			   gtk_menu_append(GTK_MENU(menu), button);
			   gtk_widget_show(button);
			   
			   }
			   
			   gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			   event->button, event->time);                 
			   
		}
	}

	return TRUE;
}


static void gtk_html_draw_bit(GtkHtml * html, GtkHtmlBit * hb, int redraw)
{
	int mypos,
	  epos,
	  spos;
	GdkGC *gc = html->gc;
	int shift;
	GtkStateType selected_state;
	GtkWidget *widget = GTK_WIDGET(html);
	GdkRectangle area;

	if (html->frozen > 0)
		return;

	if (hb->type == HTML_BIT_TEXT)
	{

		if (!strlen(hb->text))
			return;

		mypos = g_list_index(html->html_bits, hb);
		epos = g_list_index(html->html_bits, html->end_sel);
		spos = g_list_index(html->html_bits, html->start_sel);

		if (((html->end_sel == NULL) || (html->start_sel == NULL)) ||
			((epos < mypos) && (spos < mypos)) ||
			((epos > mypos) && (spos > mypos)))
		{
			selected_state = GTK_STATE_NORMAL;
		}
		else
		{
			selected_state = GTK_STATE_SELECTED;
		}


		gdk_text_extents(hb->font, hb->text, 1, &shift, NULL, NULL, NULL, NULL);

		if (selected_state == GTK_STATE_SELECTED)
		{
			int schar = 0,
			  echar = 0;
			int startx = 0,
			  xwidth = 0;

			if (epos > spos ||
				(epos == spos && html->num_end >= html->num_start))
			{
				if (mypos == epos)
				{
					echar = html->num_end;
					xwidth =
						gdk_text_width(hb->font, hb->text, html->num_end + 1);
				}
				else
				{
					echar = strlen(hb->text);
					xwidth = hb->width;
				}
				if (mypos == spos)
				{
					schar = html->num_start;
					startx =
						gdk_text_width(hb->font, hb->text, html->num_start);
					xwidth -= startx;
				}
			}
			else
			{
				if (mypos == spos)
				{
					echar = html->num_start;
					xwidth =
						gdk_text_width(hb->font, hb->text,
										 html->num_start + 1);
				}
				else
				{
					echar = strlen(hb->text);
					xwidth = hb->width;
				}
				if (mypos == epos)
				{
					schar = html->num_end;
					startx =
						gdk_text_width(hb->font, hb->text, html->num_end);
					xwidth -= startx;
				}
			}

			if (!redraw && echar == hb->sel_e && schar == hb->sel_s)
				return;

			hb->sel_e = echar;
			hb->sel_s = schar;

			startx += hb->x;


			area.x = hb->x - html->xoffset;
			area.y = hb->y - hb->height + 3 - html->yoffset;
			area.width = hb->width + 2;
			area.height = hb->height;
			clear_area(html, &area);

			gtk_paint_flat_box(widget->style, html->html_area,
							   selected_state, GTK_SHADOW_NONE,
							   NULL, widget, "text",
							   startx,
							   hb->y - hb->height + 3 - html->yoffset,
							   xwidth + 2, hb->height);
			hb->was_selected = 1;
		}
		else if (hb->was_selected)
		{
			area.x = hb->x - html->xoffset;
			area.y = hb->y - hb->height + 3 - html->yoffset;
			area.width = hb->width + 2;
			area.height = hb->height;
			clear_area(html, &area);

			hb->sel_e = -1;
			hb->sel_s = -1;

			hb->was_selected = 0;
		}




		if (selected_state == GTK_STATE_SELECTED && (mypos == epos
													 || mypos == spos))
		{
			char *s = hb->text;
			int num = 0,
			  width = 0,
			  fsel = 0,
			  esel = strlen(hb->text);
			int lbearing,
			  rbearing,
			  w;

			if (epos > spos ||
				(epos == spos && html->num_end >= html->num_start))
			{
				if (mypos == epos)
					esel = html->num_end;
				if (mypos == spos)
					fsel = html->num_start;
			}
			else
			{
				if (mypos == spos)
					esel = html->num_start;
				if (mypos == epos)
					fsel = html->num_end;
			}

			while (*s)
			{

				if (num < fsel || num > esel)
					selected_state = GTK_STATE_NORMAL;
				else
					selected_state = GTK_STATE_SELECTED;
				if (hb->fore != NULL)
					gdk_gc_set_foreground(gc, hb->fore);
				else
					gdk_gc_set_foreground(gc,
										  &widget->style->fg[selected_state]);
				if (hb->back != NULL)
					gdk_gc_set_background(gc, hb->back);
				else
					gdk_gc_set_background(gc,
										  &widget->style->bg[selected_state]);


				gdk_gc_set_font(gc, hb->font);

				gdk_text_extents(hb->font, s, 1, &lbearing, &rbearing, &w, NULL,
								 NULL);

				gdk_draw_text(html->html_area, hb->font, gc,
							  shift + hb->x + width, hb->y - html->yoffset, s,
							  1);

				if (hb->uline)
					gdk_draw_line(html->html_area, gc, shift + hb->x + width,
								  hb->y - html->yoffset,
								  shift + hb->x + width + w,
								  hb->y - html->yoffset);

				if (hb->strike)
					gdk_draw_line(html->html_area, gc, shift + hb->x + width,
								  hb->y - html->yoffset - (hb->height / 3),
								  shift + hb->x + width + w,
								  hb->y - html->yoffset - (hb->height / 3));

				width += w;

				s++;
				num++;
			}


		}
		else
		{
			/*my stuff here*/
			
			if(!hb->was_selected)
			{
				area.x = hb->x - html->xoffset;
				area.y = hb->y - hb->height + 3 - html->yoffset;
				area.width = hb->width + 2;
				area.height = hb->height;
				clear_area(html, &area);
			}

			/*end my stuff*/


			if (hb->fore != NULL)
				gdk_gc_set_foreground(gc, hb->fore);
			else
				gdk_gc_set_foreground(gc, &widget->style->fg[selected_state]);
			if (hb->back != NULL)
				gdk_gc_set_background(gc, hb->back);
			else
				gdk_gc_set_background(gc, &widget->style->bg[selected_state]);


			gdk_gc_set_font(gc, hb->font);


			gdk_draw_string(html->html_area, hb->font, gc, shift + hb->x,
							hb->y - html->yoffset, hb->text);
			if (hb->uline)
				gdk_draw_line(html->html_area, gc, shift + hb->x,
							  hb->y - html->yoffset,
							  hb->x + gdk_string_measure(hb->font, hb->text),
							  hb->y - html->yoffset);

			if (hb->strike)
				gdk_draw_line(html->html_area, gc, shift + hb->x,
							  hb->y - html->yoffset - (hb->height / 3),
							  hb->x + gdk_string_measure(hb->font, hb->text),
							  hb->y - html->yoffset - (hb->height / 3));

		}
	}
	else if (hb->type == HTML_BIT_SEP)
	{

		gdk_draw_line(html->html_area, gc, hb->x + 2,
					  hb->y - html->yoffset - (hb->height / 2 - 1),
					  hb->x + hb->width,
					  hb->y - html->yoffset - (hb->height / 2 - 1));

	}
	else if (hb->type == HTML_BIT_PIXMAP)
	{
		gdk_gc_set_background(gc, &widget->style->base[GTK_STATE_NORMAL]);
		gdk_draw_pixmap(html->html_area, gc, hb->pm, 0, 0, hb->x,
						hb->y - html->yoffset - (hb->height) + 4, -1, -1);
	}
}



gint compare_types(GtkHtmlBit * hb, GtkHtmlBit * hb2)
{
	/*
	 * In this function, it's OK to accidently return a
	 * * 0, but will cause problems on an accidental 1 
	 */

	if (!hb || !hb2)
		return 0;


	if (hb->uline != hb2->uline)
		return 0;
	if (hb->strike != hb2->strike)
		return 0;
	if (hb->font && hb2->font)
	{
		if (!gdk_font_equal(hb->font, hb2->font))
			return 0;
	}
	else if (hb->font && !hb2->font)
	{
		return 0;
	}
	else if (!hb->font && hb2->font)
	{
		return 0;
	}
	if (hb->type != hb2->type)
		return 0;

	if (hb->fore && hb2->fore)
	{
		if (!gdk_color_equal(hb->fore, hb2->fore))
			return 0;
	}
	else if (hb->fore && !hb2->fore)
	{
		return 0;
	}
	else if (!hb->fore && hb2->fore)
	{
		return 0;
	}

	if (hb->back && hb2->back)
	{
		if (!gdk_color_equal(hb->back, hb2->back))
			return 0;
	}
	else if (hb->back && !hb2->back)
	{
		return 0;
	}
	else if (!hb->back && hb2->back)
	{
		return 0;
	}

	if ((hb->url != NULL && hb2->url == NULL) ||
		(hb->url == NULL && hb2->url != NULL))
		return 0;

	if (hb->url != NULL && hb2->url != NULL)
		if (strcasecmp(hb->url, hb2->url))
			return 0;

	return 1;
}

static gint html_bit_is_onscreen(GtkHtml * html, GtkHtmlBit * hb)
{
	gint width,
	  height;

	gdk_window_get_size(html->html_area, &width, &height);

	if (hb->y < html->yoffset)
	{
		return 0;
	}

	if ((hb->y - hb->height) > (html->yoffset + height))
	{
		return 0;
	}
	return 1;
}

static void draw_cursor(GtkHtml * html)
{
	if (html->editable &&
		html->cursor_hb &&
		GTK_WIDGET_DRAWABLE(html) &&
		html_bit_is_onscreen(html, html->cursor_hb))
	{
		gint x,
		  y;
		gint width;

		GdkFont *font = html->cursor_hb->font;

		gdk_text_extents(font, html->cursor_hb->text, html->cursor_pos, NULL,
						 NULL, &width, NULL, NULL);

		gdk_gc_set_foreground(html->gc,
							  &GTK_WIDGET(html)->style->text[GTK_STATE_NORMAL]);

		y = html->cursor_hb->y - html->yoffset;
		x = html->cursor_hb->x + width;


		gdk_draw_line(html->html_area, html->gc, x, y, x, y - font->ascent);

	}
}

static void undraw_cursor(GtkHtml * html)
{
	if (html->editable &&
		html->cursor_hb &&
		GTK_WIDGET_DRAWABLE(html) &&
		html_bit_is_onscreen(html, html->cursor_hb))
	{
		gint x,
		  y;
		gint width;
		GdkRectangle area;

		GdkFont *font = html->cursor_hb->font;

		gdk_text_extents(font, html->cursor_hb->text, html->cursor_pos, NULL,
						 NULL, &width, NULL, NULL);

		y = html->cursor_hb->y - html->yoffset;
		x = html->cursor_hb->x + width;

		area.x = x;
		area.y = y - font->ascent;
		area.height = font->ascent + 1;
		area.width = 1;


		clear_area(html, &area);

		gtk_html_draw_bit(html, html->cursor_hb, 1);


	}
}


static void expose_html(GtkHtml * html, GdkRectangle * area, gboolean cursor)
{
	GList *hbits;
	GtkHtmlBit *hb;
	gint width,
	  height;
	gint realy;


	if (html->frozen > 0)
		return;


	hbits = html->html_bits;

	gdk_window_get_size(html->html_area, &width, &height);

	realy = area->y + html->yoffset;

	clear_area(html, area);

	while (hbits)
	{

		hb = (GtkHtmlBit *) hbits->data;

		if (html_bit_is_onscreen(html, hb))
			gtk_html_draw_bit(html, hb, 1);


		hbits = hbits->next;
	}
}

static void resize_html(GtkHtml * html)
{
	GList *hbits = html->html_bits;
	GList *html_bits = html->html_bits;
	GtkHtmlBit *hb,
	 *hb2;
	char *str;
	gint height;

	if (!hbits)
		return;


	html->html_bits = NULL;

	html->current_x = 0;
	html->current_y = 0;

	html->vadj->upper = 0;

	gtk_html_freeze(html);

	while (hbits)
	{
		hb = (GtkHtmlBit *) hbits->data;
		if (hb->type == HTML_BIT_SEP)
		{

			gtk_html_add_seperator(html);

			g_free(hb);

			hbits = hbits->next;
			continue;
		}
		if (hb->type == HTML_BIT_PIXMAP)
		{

			gtk_html_add_pixmap(html, hb->pm, hb->fit, hb->newline);

			g_free(hb);

			hbits = hbits->next;
			continue;
		}

		if (hb->newline)
		{
			int i;

			if (!hb->text)
			{
				hb->text = g_malloc(1);
				hb->text[0] = 0;
			}
			for (i = 0; i < hb->newline; i++)
			{
				str = hb->text;
				hb->text = g_strconcat(str, "\n", NULL);
				if (str) g_free(str);
			}
		}

		if (hbits->next)
		{
			hb2 = (GtkHtmlBit *) hbits->next->data;
		}
		else
		{
			hb2 = NULL;
		}



		if (!hb->newline && compare_types(hb, hb2))
		{
			str = hb2->text;
			hb2->text = g_strconcat(hb->text, hb2->text, NULL);
			if (str) g_free(str);
			hb2 = NULL;
		}
		else if (hb->text)
		{
			gtk_html_add_text(html, hb->font, hb->fore, hb->back,
							  hb->text, strlen(hb->text), hb->uline, hb->strike,
							  hb->url);
		}



		/*
		 * Font stays, so do colors (segfaults if I free) 
		 */
		if (hb->fore)
			gdk_color_free(hb->fore);
		if (hb->back)
			gdk_color_free(hb->back);
		if (hb->text)
			g_free(hb->text);
		if (hb->url)
			g_free(hb->url);

		g_free(hb);

		hbits = hbits->next;
	}

	g_list_free(html_bits);


	gtk_html_thaw(html);

	gdk_window_get_size(html->html_area, NULL, &height);
	gtk_adjustment_set_value(html->vadj, html->vadj->upper - height);

}

static GdkGC *create_bg_gc(GtkHtml * html)
{
	GdkGCValues values;

	values.tile = GTK_WIDGET(html)->style->bg_pixmap[GTK_STATE_NORMAL];
	values.fill = GDK_TILED;

	return gdk_gc_new_with_values(html->html_area, &values,
								  GDK_GC_FILL | GDK_GC_TILE);
}

static void clear_area(GtkHtml * html, GdkRectangle * area)
{
	GtkWidget *widget = GTK_WIDGET(html);
	gint x,
	  y;


	if (html->transparent)
	{
		if (html->pm == NULL)
			html->pm = get_desktop_pixmap(widget);

		if (html->pm == NULL)
			return;

		if (html->bg_gc == NULL)
		{
			GdkGCValues values;

			values.tile = html->pm;
			values.fill = GDK_TILED;

			html->bg_gc = gdk_gc_new_with_values(html->html_area, &values,
												 GDK_GC_FILL | GDK_GC_TILE);

		}

		gdk_window_get_deskrelative_origin(html->html_area, &x, &y);

		gdk_draw_pixmap(html->html_area, html->bg_gc, html->pm,
						x + area->x, y + area->y, area->x, area->y, area->width,
						area->height);

		return;

	}
	if (html->bg_gc)
	{

		gint width,
		  height;

		gdk_window_get_size(widget->style->bg_pixmap[GTK_STATE_NORMAL], &width,
							&height);

		gdk_gc_set_ts_origin(html->bg_gc,
							 (-html->xoffset) % width,
							 (-html->yoffset) % height);

		gdk_draw_rectangle(html->html_area, html->bg_gc, TRUE,
						   area->x, area->y, area->width, area->height);
	}
	else
		gdk_window_clear_area(html->html_area, area->x, area->y, area->width,
							  area->height);
}




static void gtk_html_destroy(GtkObject * object)
{
	GtkHtml *html;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GTK_IS_HTML(object));

	html = (GtkHtml *) object;


	gtk_signal_disconnect_by_data(GTK_OBJECT(html->hadj), html);
	gtk_signal_disconnect_by_data(GTK_OBJECT(html->vadj), html);

	if (html->timer)
	{
		gtk_timeout_remove(html->timer);
		html->timer = 0;
	}

	if (html->tooltip_timer)
	{
		gtk_timeout_remove(html->tooltip_timer);
		html->tooltip_timer = -1;
	}


	GTK_OBJECT_CLASS(parent_class)->destroy(object);

}

static void gtk_html_finalize(GtkObject * object)
{
	GList *hbits;
	GtkHtml *html;
	GtkHtmlBit *hb;


	g_return_if_fail(object != NULL);
	g_return_if_fail(GTK_IS_HTML(object));

	html = (GtkHtml *) object;

	gtk_object_unref(GTK_OBJECT(html->hadj));
	gtk_object_unref(GTK_OBJECT(html->vadj));

	hbits = html->html_bits;

	while (hbits)
	{
		hb = (GtkHtmlBit *) hbits->data;
		if (hb->fore)
			gdk_color_free(hb->fore);
		if (hb->back)
			gdk_color_free(hb->back);
		if (hb->text)
			g_free(hb->text);
		if (hb->url)
			g_free(hb->url);
		if (hb->pm)
			gdk_pixmap_unref(hb->pm);

		g_free(hb);
		hbits = hbits->next;
	}
	if (html->html_bits)
		g_list_free(html->html_bits);

	if (html->urls)
		g_list_free(html->urls);

	if (html->selected_text)
		g_free(html->selected_text);

	if (html->gc)
		gdk_gc_destroy(html->gc);

	if (html->bg_gc)
		gdk_gc_destroy(html->bg_gc);

	if (html->tooltip_window)
		gtk_widget_destroy(html->tooltip_window);

	GTK_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gtk_html_realize(GtkWidget * widget)
{
	GtkHtml *html;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));

	html = GTK_HTML(widget);
	GTK_WIDGET_SET_FLAGS(html, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK |
							  GDK_BUTTON_PRESS_MASK |
							  GDK_BUTTON_RELEASE_MASK |
							  GDK_BUTTON_MOTION_MASK |
							  GDK_ENTER_NOTIFY_MASK |
							  GDK_LEAVE_NOTIFY_MASK |
							  GDK_POINTER_MOTION_MASK |
							  GDK_POINTER_MOTION_HINT_MASK |
							  GDK_VISIBILITY_NOTIFY_MASK | GDK_KEY_PRESS_MASK);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window =
		gdk_window_new(gtk_widget_get_parent_window(widget), &attributes,
					   attributes_mask);
	gdk_window_set_user_data(widget->window, html);

	attributes.x = (widget->style->klass->xthickness + BORDER_WIDTH);
	attributes.y = (widget->style->klass->ythickness + BORDER_WIDTH);
	attributes.width =
		MAX(1, (gint) widget->allocation.width - (gint) attributes.x * 2);
	attributes.height =
		MAX(1, (gint) widget->allocation.height - (gint) attributes.y * 2);

	html->html_area =
		gdk_window_new(widget->window, &attributes, attributes_mask);
	gdk_window_set_user_data(html->html_area, html);

	widget->style = gtk_style_attach(widget->style, widget->window);

	/*
	 * Can't call gtk_style_set_background here because it's handled specially 
	 */
	gdk_window_set_background(widget->window,
							  &widget->style->base[GTK_STATE_NORMAL]);
	gdk_window_set_background(html->html_area,
							  &widget->style->base[GTK_STATE_NORMAL]);

	if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
		html->bg_gc = create_bg_gc(html);

	html->gc = gdk_gc_new(html->html_area);
	gdk_gc_set_exposures(html->gc, TRUE);
	gdk_gc_set_foreground(html->gc, &widget->style->text[GTK_STATE_NORMAL]);

	gdk_window_show(html->html_area);

}

static void gtk_html_style_set(GtkWidget * widget, GtkStyle * previous_style)
{
	GtkHtml *html;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));

	html = GTK_HTML(widget);
	if (GTK_WIDGET_REALIZED(widget))
	{
		gdk_window_set_background(widget->window,
								  &widget->style->base[GTK_STATE_NORMAL]);
		gdk_window_set_background(html->html_area,
								  &widget->style->base[GTK_STATE_NORMAL]);

		if (html->bg_gc)
		{
			gdk_gc_destroy(html->bg_gc);
			html->bg_gc = NULL;
		}

		if (widget->style->bg_pixmap[GTK_STATE_NORMAL])
		{
			html->bg_gc = create_bg_gc(html);
		}

	}
}

static void gtk_html_unrealize(GtkWidget * widget)
{
	GtkHtml *html;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_HTML(widget));

	html = GTK_HTML(widget);

	gdk_window_set_user_data(html->html_area, NULL);
	gdk_window_destroy(html->html_area);
	html->html_area = NULL;

	gdk_gc_destroy(html->gc);
	html->gc = NULL;

	if (html->bg_gc)
	{
		gdk_gc_destroy(html->bg_gc);
		html->bg_gc = NULL;
	}

	if (GTK_WIDGET_CLASS(parent_class)->unrealize)
		(*GTK_WIDGET_CLASS(parent_class)->unrealize) (widget);
}


void gtk_html_add_pixmap(GtkHtml * html, GdkPixmap * pm, int fit, int newline)
{
	GtkHtmlBit *last_hb;
	GtkHtmlBit *hb = g_new0(GtkHtmlBit, 1);
	GdkWindowPrivate *private = (GdkWindowPrivate *) pm;
	int width, height;

	last_hb = (GtkHtmlBit *) g_list_last(html->html_bits)->data;

	/* wrap pixmaps */
	gdk_window_get_size(html->html_area, &width, &height);
	if ((html->current_x + private->width) >= width) {
		html->current_x = 0;
	}
			
	hb->fit = fit;
	html->current_x += 2;
	hb->x = html->current_x;
	hb->y = html->current_y;
	if (fit)
		hb->height = last_hb->height;
	else
		hb->height = private->height;
	hb->type = HTML_BIT_PIXMAP;
	hb->width = private->width;
	hb->text = NULL;
	hb->url = NULL;
	hb->fore = NULL;
	hb->back = NULL;
	hb->font = NULL;
	hb->uline = 0;
	hb->strike = 0;
	hb->was_selected = 0;
	hb->newline = newline;
	hb->pm = pm;

	if (html->current_x == BORDER_WIDTH)
	{
		html->current_y += hb->height + 3;
		hb->y += hb->height + 3;
	}


	html->current_x += hb->width + 1;

	gtk_html_draw_bit(html, hb, 1);

	if (hb->newline)
		html->current_x = 0;

	html->html_bits = g_list_append(html->html_bits, hb);


}

static void gtk_html_add_seperator(GtkHtml * html)
{
	GtkHtmlBit *hb = g_new0(GtkHtmlBit, 1);
	gint width,
	  height;

	html->current_x = 0;
	html->current_y += 5;

	gdk_window_get_size(html->html_area, &width, &height);

	hb->x = html->current_x;
	hb->y = html->current_y;
	hb->height = 5;
	hb->type = HTML_BIT_SEP;
	hb->width =
		width -
		GTK_SCROLLED_WINDOW(GTK_WIDGET(html)->parent)->vscrollbar->allocation.
		width - 10;
	hb->text = NULL;
	hb->url = NULL;
	hb->fore = NULL;
	hb->back = NULL;
	hb->font = NULL;
	hb->uline = 0;
	hb->strike = 0;
	hb->was_selected = 0;
	hb->newline = 0;
	hb->pm = NULL;

	gtk_html_draw_bit(html, hb, 1);

	html->html_bits = g_list_append(html->html_bits, hb);

}


static void gtk_html_add_text(GtkHtml * html,
							  GdkFont * cfont,
							  GdkColor * fore,
							  GdkColor * back,
							  char *chars,
							  gint length, gint uline, gint strike, char *url)
{
	char *nextline = NULL,
	 *c,
	 *text,
	 *tmp;
	GdkGC *gc;
	int nl = 0,
	  nl2 = 0;
	int maxwidth;
	gint lb;
	GList *hbits;
	size_t num = 0;
	int i,
	  height;
	GtkHtmlBit *hb;
	gint hwidth,
	  hheight;

	if (length == 1 && chars[0] == '\n')
	{
		GtkHtmlBit *h;
		hbits = g_list_last(html->html_bits);
		if (!hbits)
			return;
		/*
		 * I realize this loses a \n sometimes
		 * * if it's the first thing in the widget.
		 * * so fucking what. 
		 */

		h = (GtkHtmlBit *) hbits->data;
		h->newline++;
		if (html->current_x > 0)
			html->current_x = 0;
		else
			html->current_y += cfont->ascent + cfont->descent + 5;
		return;
	}



	c = text = g_malloc(length + 2);
	strncpy(text, chars, length);
	text[length] = 0;


	gc = html->gc;

	if (gc == NULL)
		gc = html->gc = gdk_gc_new(html->html_area);

	gdk_gc_set_font(gc, cfont);


	while (*c)
	{
		if (*c == '\n')
		{
			if (*(c + 1) == '\0')
			{
				nl = 1;
				length--;
				c[0] = '\0';
				break;
			}
			if (*c)
			{
				gtk_html_add_text(html, cfont, fore, back, text, num + 1, uline,
								  strike, url);
				tmp = text;
				length -= (num + 1);
				text = g_malloc(length + 2);
				strncpy(text, (c + 1), length);
				text[length] = 0;
				c = text;
				num = 0;
				g_free(tmp);
				continue;
			}
		}

		num++;
		c++;
	}

	/*
	 * Note, yG is chosen because G is damn high, and y is damn low, 
	 */
	/*
	 * it should be just fine. :) 
	 */

	gdk_window_get_size(html->html_area, &hwidth, &hheight);

	num = strlen(text);

	while (GTK_WIDGET(html)->allocation.width < 20)
	{
		while (gtk_events_pending())
			gtk_main_iteration();
	}

	maxwidth = (hwidth - html->current_x - 8);
	/*
	 * HTK_SCROLLED_WINDOW(GTK_WIDGET(layout)->parent)->vscrollbar->allocation.width) - 8; 
	 */

	while (gdk_text_measure(cfont, text, num) > maxwidth)
	{
		if (num > 1)
			num--;
		else
		{
			if (html->current_x != 0) {
				html->current_x = 0;
				if (nl) {
					text[length] = '\n';
					length++;
				}
				gtk_html_add_text(html, cfont, fore, back, text, length, uline, strike, url);
				g_free(text);
				return;
			} else {
				num = strlen (text);
				break;
			}
		}

	}

	height = cfont->ascent + cfont->descent + 2;


	if ((int) (html->vadj->upper - html->current_y) < (int) (height * 2))
	{
		int val;
		val = (height * 2) + html->current_y;
		html->vadj->upper = val;
		adjust_adj(html, html->vadj);
	}


	if (html->current_x == 0)
	{
		html->current_y += height + 3;
		gdk_text_extents(cfont, text, 1, &lb, NULL, NULL, NULL, NULL);
		html->current_x += (2 - lb);
	}
	else if ((hbits = g_list_last(html->html_bits)) != NULL)
	{
		int diff,
		  y;
		hb = (GtkHtmlBit *) hbits->data;
		if (height > hb->height)
		{
			diff = height - hb->height;
			y = hb->y;
			html->current_y += diff;
			while (hbits)
			{
				hb = (GtkHtmlBit *) hbits->data;
				if (hb->y != y)
					break;
				if (hb->type != HTML_BIT_PIXMAP)
					hb->height = height;
				hb->y += diff;  ////////////my thing here /////////////////
				gtk_html_draw_bit(html, hb, FALSE);

				hbits = hbits->prev;
			}
		}
	}




	if (num != strlen(text))
	{
		/*
		 * This is kinda cheesy but it may make things
		 * * much better lookin 
		 */

		for (i=2; (num - i > 0); i++) {
			if (text[num - i] == ' ') {
				num = num - (i - 1);
				nl2 = 1;
				break;
			}
		}

		nextline = g_malloc(length - num + 2);
		strncpy(nextline, (char *) (text + num), length - num);
		nextline[length - num] = 0;
		if (nl)
		{
			nextline[length - num] = '\n';
			nextline[length - num + 1] = 0;
			nl = 0;
		}


		text[num] = 0;
	}


	if (url != NULL) {
		fore = get_color(3355647, gdk_window_get_colormap(html->html_area));
	}

	hb = g_new0(GtkHtmlBit, 1);

	hb->text = g_strdup(text);

	if (fore)
		hb->fore = gdk_color_copy(fore);
	else
		hb->fore = NULL;
	
	if (back)
		hb->back = gdk_color_copy(back);
	else
		hb->back = NULL;
	hb->font = cfont;
	hb->uline = uline;
	hb->strike = strike;
	hb->height = height;
	gdk_text_extents(cfont, text, num, &lb, NULL, &hb->width, NULL, NULL);
	hb->x = html->current_x;
	hb->y = html->current_y;
	hb->type = HTML_BIT_TEXT;
	hb->pm = NULL;
	if (url != NULL)
	{
		uline = 1;
		hb->uline = 1;
		hb->url = g_strdup(url);
	}
	else
	{
		hb->url = NULL;
	}
	html->current_x += hb->width;

	html->html_bits = g_list_append(html->html_bits, hb);
	if (url != NULL)
	{
		html->urls = g_list_append(html->urls, hb);
	}



	gtk_html_draw_bit(html, hb, 1);

	if (nl || nl2)
	{
		if (nl)
			hb->newline = 1;
		html->current_x = 0;
	}
	else
		hb->newline = 0;


	if (nextline != NULL)
	{
		gtk_html_add_text(html, cfont, fore, back, nextline, strlen(nextline),
						  uline, strike, url);
		g_free(nextline);
	}

	g_free(text);
	if (url != NULL)
		g_free(fore);
}

static char * html_strtok( char * input, char delim )
{
	static char * end;
	static char * curr_offset;
	int i;
	int num_quotes=0;

	if( input != NULL)
	{
		curr_offset = input;
		end = input+strlen(input);
	}
	else
	{
		if( curr_offset + strlen(curr_offset) < end )
		{
			curr_offset += strlen(curr_offset) + 1;
		}
		else
		{
			return NULL;
		}
	}
	for( i=0; curr_offset+i < end && 
			  (curr_offset[i] != delim || num_quotes != 0)
		  ; i++ )
	{
		if( curr_offset[i] == '\"' )
		{
			num_quotes = (num_quotes+1)%2;
		}
	}
	curr_offset[i] = '\0';
	return curr_offset;
}


void gtk_html_append_text(GtkHtml * html, char *text, gint options)
{
	GdkColormap *map;
	GdkFont *cfont;
	GdkRectangle area;
	char ws[BUF_LONG],
	  tag[BUF_LONG],
	 *c,
	 *url = NULL;
	gint intag = 0,
	  wpos = 0,
	  tpos = 0;
	static gint colorv,
	  bold = 0,
	  italic = 0,
	  fixed = 0,
	  uline = 0,
	  strike = 0,
	  title = 0,
	  height;
	static struct font_state *current = NULL,
	 *tmp;
	static struct font_state def_state = { 3, 0, 0, "", NULL, NULL, NULL };

	if (text == NULL) {
		while (current->next)
		{
			if (current->ownbg)
				g_free(current->bgcol);
			if (current->owncolor)
				g_free(current->color);
			tmp = current;
			current = current->next;
			g_free(tmp);
		}
		return;
	}

	if (!current) current = &def_state;
	map = gdk_window_get_colormap(html->html_area);
	cfont = getfont(current->font, bold, italic, fixed, current->size);
	c = text;


	while (*c)
	{
		if (*c == '<')
		{
			if (!intag)
			{
				ws[wpos] = 0;
				if (wpos)
				{
					if (title)
					{
						if (html->title)
							g_free(html->title);
						html->title = g_strdup(ws);
					}
					else
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, ws, strlen(ws), uline,
										  strike, url);
				}
				wpos = 0;
				intag = 1;
			}
			else
			{
				/*
				 * Assuming you NEVER have nested tags
				 * * (and I mean <tag <tag>> by this, not
				 * * <tag><tag2></tag2><tag>..
				 */
				tag[tpos] = 0;
				gtk_html_add_text(html, cfont, current->color, current->bgcol,
								  "<", 1, 0, 0, NULL);
				gtk_html_add_text(html, cfont, current->color, current->bgcol,
								  tag, strlen(tag), 0, 0, NULL);
				tpos = 0;

				tag[0] = *c;
			}
		}
		else if (*c == '>')
		{
			if (intag)
			{
				tag[tpos] = 0;
				if (!strcasecmp(tag, "B"))
					bold = 1;
				else if (!strcasecmp(tag, "STRIKE"))
					strike = 1;
				else if (!strcasecmp(tag, "I"))
					italic = 1;
				else if (!strcasecmp(tag, "U"))
					uline = 1;
				else if (!strcasecmp(tag, "PRE"))
					fixed = 1;
				else if (!strcasecmp(tag, "HR"))
					gtk_html_add_seperator(html);
				else if (!strcasecmp(tag, "/B"))
					bold = 0;
				else if (!strcasecmp(tag, "/STRIKE"))
					strike = 0;
				else if (!strcasecmp(tag, "/I"))
					italic = 0;
				else if (!strcasecmp(tag, "/U"))
					uline = 0;
				else if (!strcasecmp(tag, "/PRE"))
					fixed = 0;
				else if (!strcasecmp(tag, "TITLE"))
					title = 1;
				else if (!strcasecmp(tag, "/TITLE"))
					title = 0;
				else if (!strncasecmp(tag, "IMG", 3))
				{
					GdkPixmap *legend_i;
					GdkBitmap *legend_m;

					if (strstr(tag, "SRC=\"aol_icon.gif\"") != NULL)
					{
						legend_i = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(html)->window, &legend_m, NULL, aol_icon_xpm);
						gtk_html_add_pixmap(html, legend_i, 0, 0);
					}

					if (strstr(tag, "SRC=\"admin_icon.gif\"") != NULL)
					{
						legend_i = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(html)->window, &legend_m, NULL, admin_icon_xpm);
						gtk_html_add_pixmap(html, legend_i, 0, 0);
					}
					if (strstr(tag, "SRC=\"dt_icon.gif\"") != NULL)
					{
						legend_i = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(html)->window, &legend_m, NULL, dt_icon_xpm);
						gtk_html_add_pixmap(html, legend_i, 0, 0);
					}
					if (strstr(tag, "SRC=\"free_icon.gif\"") != NULL)
					{
						legend_i = gdk_pixmap_create_from_xpm_d(GTK_WIDGET(html)->window, &legend_m, NULL, free_icon_xpm);
						gtk_html_add_pixmap(html, legend_i, 0, 0);
					}
				}
				else if (!strcasecmp(tag, "H3"))
				{
					current = push_state(current);
					current->size = 4;
				}
				else if (!strcasecmp(tag, "/H3"))
				{
					gtk_html_add_text(html, cfont, current->color,
									  current->bgcol, "\n", 1, 0, 0, NULL);

					if (current->next)
					{
						if (current->ownbg)
							g_free(current->bgcol);
						if (current->owncolor)
							g_free(current->color);
						tmp = current;
						current = current->next;
						g_free(tmp);
					}
				}
				else if (!strcasecmp(tag, "TABLE"))
				{
				}
				else if (!strcasecmp(tag, "/TABLE"))
				{
				}
				else if (!strcasecmp(tag, "TR"))
				{
				}
				else if (!strcasecmp(tag, "/TR"))
				{
				}
				else if (!strcasecmp(tag, "/TD"))
				{
				}
				else if (!strcasecmp(tag, "TD"))
				{
					gtk_html_add_text(html, cfont, current->color,
									  current->bgcol, "  ", 2, 0, 0, NULL);
				}
				else if (!strncasecmp(tag, "A ", 2))
				{
					char *d;
					char *temp = d = g_strdup(tag);
					int flag = 0;
					strtok(tag, " ");
					while ((d = strtok(NULL, " ")))
					{
						if (strlen(d) < 7)
							break;
						if (!strncasecmp(d, "HREF=\"", strlen("HREF=\"")))
						{
							d += strlen("HREF=\"");
							d[strlen(d) - 1] = 0;
							url = g_malloc(strlen(d) + 1);
							strcpy(url, d);
							flag = 1;
						}
					}
					g_free(temp);
					if (!flag)
					{
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, "<", 1, 0, 0, NULL);
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, tag, strlen(tag), 0,
										  0, NULL);
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, ">", 1, 0, 0, NULL);
					}
				}
				else if (!strcasecmp(tag, "/A"))
				{
					if (url)
					{
						g_free(url);
						url = NULL;
					}
				}
				else if (!strncasecmp(tag, "FONT", strlen("FONT")))
				{
					char *d;
					/*
					 * Push a new state onto the stack, based on the old state 
					 */
					current = push_state(current);
					html_strtok(tag, ' ');
					while ((d = html_strtok(NULL, ' ')))
					{
						if (!strncasecmp(d, "COLOR=", strlen("COLOR=")))
						{
							d += strlen("COLOR=");
							if (*d == '\"')
							{
								d++;
							}
							if (*d == '#')
								d++;
							if (d[strlen(d) - 1] == '\"')
								d[strlen(d) - 1] = 0;
							if (sscanf(d, "%x", &colorv)
								&& !(options & HTML_OPTION_NO_COLOURS))
							{
								current->color = get_color(colorv, map);
								current->owncolor = 1;
							}
							else
							{
							}
						}
						if (!strncasecmp(d, "FACE=", strlen("FACE=")))
						{
							d += strlen("FACE=");
							if (*d == '\"')
							{
								d++;
							}
							if (d[strlen(d) - 1] == '\"')
								d[strlen(d) - 1] = 0;
							strcpy(current->font, d);
						}
						else if (!strncasecmp(d, "BACK=", strlen("BACK=")))
						{
							d += strlen("BACK=");
							if (*d == '\"')
								d++;
							if (*d == '#')
								d++;
							if (d[strlen(d) - 1] == '\"')
								d[strlen(d) - 1] = 0;
							if (sscanf(d, "%x", &colorv)
								&& !(options & HTML_OPTION_NO_COLOURS))
							{
								current->bgcol = get_color(colorv, map);
								current->ownbg = 1;
							}
							else
							{
							}
						}
						else if (!strncasecmp(d, "SIZE=", strlen("SIZE=")))
						{
							d += strlen("SIZE=");
							if (*d == '\"')
								d++;
							if (*d == '+')
								d++;
							if (sscanf(d, "%d", &colorv))
							{
								current->size = colorv;
							}
							else
							{
							}
						}
						else if (strncasecmp(d, "PTSIZE=", strlen("PTSIZE=")))
						{
						}
					}
				}
				else
					if (!strncasecmp
						(tag, "BODY BGCOLOR", strlen("BODY BGCOLOR")))
				{

					/*
					 * Ditch trailing \" 
					 */
					tag[strlen(tag) - 1] = 0;
					if (sscanf(tag + strlen("BODY BGCOLOR=\"#"), "%x", &colorv)
						&& !(options & HTML_OPTION_NO_COLOURS))
					{
						current->bgcol = get_color(colorv, map);
						current->ownbg = 1;
					}
				}
				else if (!strncasecmp(tag, "/FONT", strlen("/FONT")))
				{
					/*
					 * Pop a font state off the list if possible, freeing
					 * any resources it used 
					 */
					if (current->next)
					{
						if (current->ownbg)
							g_free(current->bgcol);
						if (current->owncolor)
							g_free(current->color);
						tmp = current;
						current = current->next;
						g_free(tmp);
					}

				}
				else if (!strcasecmp(tag, "/BODY"))
				{
					if (current->next)
					{
						if (current->ownbg)
							g_free(current->bgcol);
						if (current->owncolor)
							g_free(current->color);
						tmp = current;
						current = current->next;
						g_free(tmp);
					}			/*
								 * tags we ignore below 
								 */
				}
				else if (!strncasecmp(tag, "BR", 2))
				{
					gtk_html_add_text(html, cfont, current->color,
									  current->bgcol, "\n", 1, 0, 0, NULL);
				}
				else if (strncasecmp(tag, "HTML", 4)
						 && strncasecmp(tag, "/HTML", 5)
						 && strncasecmp(tag, "BODY", 4)
						 && strncasecmp(tag, "/BODY", 5)
						 && strncasecmp(tag, "P", 1)
						 && strncasecmp(tag, "/P", 2)
						 && strncasecmp(tag, "HEAD", 4)
						 && strncasecmp(tag, "/HEAD", 5))
				{
					if (tpos)
					{
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, "<", 1, 0, 0, NULL);
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, tag, strlen(tag), 0,
										  0, NULL);
						gtk_html_add_text(html, cfont, current->color,
										  current->bgcol, ">", 1, 0, 0, NULL);

					}
				}
				cfont = getfont(current->font, bold, italic, fixed, current->size);
				tpos = 0;
				intag = 0;
			}
			else
			{
				ws[wpos++] = *c;
			}
		}
		else if (!intag && *c == '&')
		{
			if (!strncasecmp(c, "&amp;", 5))
			{
				ws[wpos++] = '&';
				c += 4;
			}
			else if (!strncasecmp(c, "&lt;", 4))
			{
				ws[wpos++] = '<';
				c += 3;
			}
			else if (!strncasecmp(c, "&gt;", 4))
			{
				ws[wpos++] = '>';
				c += 3;
			}
			else if (!strncasecmp(c, "&nbsp;", 6))
			{
				ws[wpos++] = ' ';
				c += 5;
			}
			else if (!strncasecmp(c, "&copy;", 6))
			{
				ws[wpos++] = '©';
				c += 5;
			}
			else if (*(c + 1) == '#')
			{
				int pound = 0;
				debug_print("got &#;\n");
				if (sscanf(c, "&#%d;", &pound) > 0) {
					ws[wpos++] = (char)pound;
					c += 2;
					while (isdigit(*c)) c++;
					if (*c != ';') c--;
				} else {
					ws[wpos++] = *c;
				}
			}
			else
			{
				ws[wpos++] = *c;
			}
		}
		else
		{
			if (intag)
			{
				tag[tpos++] = *c;
			}
			else
			{
				ws[wpos++] = *c;
			}
		}
		c++;
	}
	ws[wpos] = 0;
	tag[tpos] = 0;
	if (wpos)
	{
		gtk_html_add_text(html, cfont, current->color, current->bgcol, ws,
						  strlen(ws), uline, strike, url);
	}
	if (tpos)
	{
		gtk_html_add_text(html, cfont, current->color, current->bgcol, "<", 1,
						  0, 0, NULL);
		gtk_html_add_text(html, cfont, current->color, current->bgcol, tag,
						  strlen(tag), 0, 0, NULL);
/*		gtk_html_add_text(html, cfont, current->color, current->bgcol, ">", 1,
						  0, 0, NULL);
*/	}



	gdk_window_get_size(html->html_area, NULL, &height);
	area.height = height;
	gtk_adjustment_set_value(html->vadj, html->vadj->upper - area.height);

	return;
}


static void adjust_adj(GtkHtml * html, GtkAdjustment * adj)
{
	gint height;

	gdk_window_get_size(html->html_area, NULL, &height);

	adj->step_increment = MIN(adj->upper, (float) SCROLL_PIXELS);
	adj->page_increment = MIN(adj->upper, height - (float) KEY_SCROLL_PIXELS);
	adj->page_size = MIN(adj->upper, height);
	adj->value = MIN(adj->value, adj->upper - adj->page_size);
	adj->value = MAX(adj->value, 0.0);

	gtk_signal_emit_by_name(GTK_OBJECT(adj), "changed");
}


static void scroll_down(GtkHtml * html, gint diff0)
{
	GdkRectangle rect;
	gint width,
	  height;

	html->yoffset += diff0;

	gdk_window_get_size(html->html_area, &width, &height);

	if (html->transparent)
	{
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;
	}
	else
	{


		if (height > diff0 && !html->transparent)
			gdk_draw_pixmap(html->html_area,
							html->gc,
							html->html_area,
							0, diff0, 0, 0, width, height - diff0);

		rect.x = 0;
		rect.y = MAX(0, height - diff0);
		rect.width = width;
		rect.height = MIN(height, diff0);
	}

	expose_html(html, &rect, FALSE);
	gtk_html_draw_focus((GtkWidget *) html);

}

static void scroll_up(GtkHtml * html, gint diff0)
{
	GdkRectangle rect;
	gint width,
	  height;

	html->yoffset -= diff0;


	gdk_window_get_size(html->html_area, &width, &height);

	if (html->transparent)
	{
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;
	}
	else
	{

		if (height > diff0)
			gdk_draw_pixmap(html->html_area,
							html->gc,
							html->html_area,
							0, 0, 0, diff0, width, height - diff0);

		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = MIN(height, diff0);
	}

	expose_html(html, &rect, FALSE);
	gtk_html_draw_focus((GtkWidget *) html);

}



static void gtk_html_adjustment(GtkAdjustment * adjustment, GtkHtml * html)
{
	g_return_if_fail(adjustment != NULL);
	g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));

	/*
	 * Just ignore it if we haven't been size-allocated and realized yet 
	 */
	if (html->html_area == NULL)
		return;

	if (adjustment == html->hadj)
	{
		g_warning("horizontal scrolling not implemented");
	}
	else
	{
		gint diff = ((gint) adjustment->value) - html->last_ver_value;

		if (diff != 0)
		{
			/*
			 * undraw_cursor (text, FALSE);
			 */

			if (diff > 0)
			{
				scroll_down(html, diff);
			}
			else
			{					/*
								 * if (diff < 0) 
								 */
				scroll_up(html, -diff);
			}
			/*
			 * draw_cursor (text, FALSE); 
			 */

			html->last_ver_value = adjustment->value;
		}
	}
}

static gint gtk_html_visibility_notify(GtkWidget * widget,
									   GdkEventVisibility * event)
{
	GtkHtml *html;
	GdkRectangle rect;
	gint width,
	  height;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_HTML(widget), FALSE);

	html = GTK_HTML(widget);

	if (GTK_WIDGET_REALIZED(widget) && html->transparent)
	{
		gdk_window_get_size(html->html_area, &width, &height);
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;
		expose_html(html, &rect, FALSE);
		gtk_html_draw_focus((GtkWidget *) html);
	}
	else
	{
	}


	return FALSE;
}



static void gtk_html_disconnect(GtkAdjustment * adjustment, GtkHtml * html)
{
	g_return_if_fail(adjustment != NULL);
	g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));

	if (adjustment == html->hadj)
		gtk_html_set_adjustments(html, NULL, html->vadj);
	if (adjustment == html->vadj)
		gtk_html_set_adjustments(html, html->hadj, NULL);
}

static void move_cursor_ver(GtkHtml * html, int count)
{
	GList *hbits = g_list_find(html->html_bits, html->cursor_hb);
	GtkHtmlBit *hb = NULL,
	 *hb2 = NULL;
	gint y;
	size_t len,
	  len2 = 0;

	undraw_cursor(html);

	if (!html->html_bits)
		return;

	if (!html->cursor_hb)
		html->cursor_hb = (GtkHtmlBit *) html->html_bits->data;

	hb = html->cursor_hb;

	len = html->cursor_pos;
	hbits = hbits->prev;
	while (hbits)
	{
		hb2 = (GtkHtmlBit *) hbits->data;

		if (hb2->y != hb->y)
			break;

		len += strlen(hb2->text);

		hbits = hbits->prev;
	}

	hbits = g_list_find(html->html_bits, html->cursor_hb);

	if (count < 0)
	{
		while (hbits)
		{
			hb2 = (GtkHtmlBit *) hbits->data;

			if (hb2->y != hb->y)
				break;

			hbits = hbits->prev;
		}
		if (!hbits)
		{
			draw_cursor(html);
			return;
		}
		y = hb2->y;
		hb = hb2;
		while (hbits)
		{
			hb2 = (GtkHtmlBit *) hbits->data;

			if (hb2->y != y)
				break;

			hb = hb2;

			hbits = hbits->prev;
		}
		hbits = g_list_find(html->html_bits, hb);
		while (hbits)
		{
			hb2 = (GtkHtmlBit *) hbits->data;

			if (hb->y != hb2->y)
			{
				html->cursor_hb = hb;
				html->cursor_pos = strlen(hb->text);
				break;
			}


			if (len < len2 + strlen(hb2->text))
			{
				html->cursor_hb = hb2;
				html->cursor_pos = len - len2;
				break;
			}

			len2 += strlen(hb2->text);

			hb = hb2;

			hbits = hbits->next;
		}
	}
	else
	{
		while (hbits)
		{
			hb2 = (GtkHtmlBit *) hbits->data;

			if (hb2->y != hb->y)
				break;

			hbits = hbits->next;
		}
		if (!hbits)
		{
			draw_cursor(html);
			return;
		}
		hb = hb2;
		while (hbits)
		{
			hb2 = (GtkHtmlBit *) hbits->data;

			if (hb->y != hb2->y)
			{
				html->cursor_hb = hb;
				html->cursor_pos = strlen(hb->text);
				break;
			}


			if (len < len2 + strlen(hb2->text))
			{
				html->cursor_hb = hb2;
				html->cursor_pos = len - len2;
				break;
			}

			len2 += strlen(hb2->text);

			hb = hb2;

			hbits = hbits->next;
		}
	}

	draw_cursor(html);

}

static void move_cursor_hor(GtkHtml * html, int count)
{
	GList *hbits = g_list_find(html->html_bits, html->cursor_hb);
	GtkHtmlBit *hb,
	 *hb2;

	undraw_cursor(html);

	if (!html->html_bits)
		return;

	if (!html->cursor_hb)
		html->cursor_hb = (GtkHtmlBit *) html->html_bits->data;

	html->cursor_pos += count;

	if (html->cursor_pos < 0)
	{
		if (hbits->prev)
		{
			gint diff;
			hb = html->cursor_hb;
			hb2 = (GtkHtmlBit *) hbits->prev->data;
			diff = html->cursor_pos + strlen(hb2->text) + 1;
			if (hb->y == hb2->y)
				--diff;

			html->cursor_pos = diff;

			html->cursor_hb = (GtkHtmlBit *) hbits->prev->data;
		}
		else
		{
			html->cursor_pos = 0;
		}
	}
	else if ((unsigned) html->cursor_pos > strlen(html->cursor_hb->text))
	{
		if (hbits->next)
		{
			gint diff;
			hb = html->cursor_hb;
			hb2 = (GtkHtmlBit *) hbits->next->data;

			diff = html->cursor_pos - strlen(html->cursor_hb->text) - 1;
			if (hb->y == hb2->y)
				++diff;
			html->cursor_pos = diff;
			html->cursor_hb = (GtkHtmlBit *) hbits->next->data;
		}
		else
		{
			html->cursor_pos = strlen(html->cursor_hb->text);
		}

	}

	draw_cursor(html);
}

static void move_beginning_of_line(GtkHtml * html)
{
	GList *hbits = g_list_find(html->html_bits, html->cursor_hb);
	GtkHtmlBit *hb = NULL;
	gint y;

	undraw_cursor(html);

	if (!html->html_bits)
		return;

	if (!html->cursor_hb)
		html->cursor_hb = (GtkHtmlBit *) html->html_bits->data;

	y = html->cursor_hb->y;

	while (hbits)
	{
		hb = (GtkHtmlBit *) hbits->data;

		if (y != hb->y)
		{
			hb = (GtkHtmlBit *) hbits->next->data;
			break;
		}

		hbits = hbits->prev;
	}
	if (!hbits)
		html->cursor_hb = (GtkHtmlBit *) html->html_bits->data;
	else
		html->cursor_hb = hb;

	html->cursor_pos = 0;


	draw_cursor(html);


}

static void move_end_of_line(GtkHtml * html)
{
	GList *hbits = g_list_find(html->html_bits, html->cursor_hb);
	GtkHtmlBit *hb = NULL;
	gint y;

	undraw_cursor(html);

	if (!html->html_bits)
		return;

	if (!html->cursor_hb)
		html->cursor_hb = (GtkHtmlBit *) html->html_bits->data;

	y = html->cursor_hb->y;

	while (hbits)
	{
		hb = (GtkHtmlBit *) hbits->data;

		if (y != hb->y)
		{
			hb = (GtkHtmlBit *) hbits->prev->data;
			break;
		}

		hbits = hbits->next;
	}
	if (!hbits)
		html->cursor_hb = (GtkHtmlBit *) g_list_last(html->html_bits)->data;
	else
		html->cursor_hb = hb;

	html->cursor_pos = strlen(html->cursor_hb->text);


	draw_cursor(html);


}



static gint gtk_html_key_press(GtkWidget * widget, GdkEventKey * event)
{
	GtkHtml *html;
	gchar key;
	gint return_val;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_HTML(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	return_val = FALSE;

	html = GTK_HTML(widget);

	key = event->keyval;
	return_val = TRUE;


	if (html->editable == FALSE)
	{
		/*
		 * switch (event->keyval) {
		 * case GDK_Home:
		 * if (event->state & GDK_CONTROL_MASK)
		 * scroll_int (text, -text->vadj->value);
		 * else
		 * return_val = FALSE;
		 * break;
		 * case GDK_End:
		 * if (event->state & GDK_CONTROL_MASK)
		 * scroll_int (text, +text->vadj->upper);
		 * else
		 * return_val = FALSE;
		 * break;
		 * case GDK_Page_Up:   scroll_int (text, -text->vadj->page_increment); break;
		 * case GDK_Page_Down: scroll_int (text, +text->vadj->page_increment); break;
		 * case GDK_Up:        scroll_int (text, -KEY_SCROLL_PIXELS); break;
		 * case GDK_Down:      scroll_int (text, +KEY_SCROLL_PIXELS); break;
		 * case GDK_Return:
		 * if (event->state & GDK_CONTROL_MASK)
		 * gtk_signal_emit_by_name (GTK_OBJECT (text), "activate");
		 * else
		 * return_val = FALSE;
		 * break;
		 * default:
		 * return_val = FALSE;
		 * break;
		 * }
		 */
	}
	else
	{

		switch (event->keyval)
		{
		case GDK_Home:
			move_beginning_of_line(html);
			break;
		case GDK_End:
			move_end_of_line(html);
			break;
			/*
			 * case GDK_Page_Up:
			 * move_cursor_page_ver (html, -1);
			 * break;
			 * case GDK_Page_Down:
			 * move_cursor_page_ver (html, +1);
			 * break;
			 */
			/*
			 * CUA has Ctrl-Up/Ctrl-Down as paragraph up down 
			 */
		case GDK_Up:
			move_cursor_ver(html, -1);
			break;
		case GDK_Down:
			move_cursor_ver(html, +1);
			break;
		case GDK_Left:
			move_cursor_hor(html, -1);
			break;
		case GDK_Right:
			move_cursor_hor(html, +1);
			break;
#if 0
		case GDK_BackSpace:
			if (event->state & GDK_CONTROL_MASK)
				gtk_text_delete_backward_word(text);
			else
				gtk_text_delete_backward_character(text);
			break;
		case GDK_Clear:
			gtk_text_delete_line(text);
			break;
		case GDK_Insert:
			if (event->state & GDK_SHIFT_MASK)
			{
				extend_selection = FALSE;
				gtk_editable_paste_clipboard(editable);
			}
			else if (event->state & GDK_CONTROL_MASK)
			{
				gtk_editable_copy_clipboard(editable);
			}
			else
			{
				/*
				 * gtk_toggle_insert(text) -- IMPLEMENT 
				 */
			}
			break;
		case GDK_Delete:
			if (event->state & GDK_CONTROL_MASK)
				gtk_text_delete_forward_word(text);
			else if (event->state & GDK_SHIFT_MASK)
			{
				extend_selection = FALSE;
				gtk_editable_cut_clipboard(editable);
			}
			else
				gtk_text_delete_forward_character(text);
			break;
		case GDK_Tab:
			position = text->point.index;
			gtk_editable_insert_text(editable, "\t", 1, &position);
			break;
		case GDK_Return:
			if (event->state & GDK_CONTROL_MASK)
				gtk_signal_emit_by_name(GTK_OBJECT(text), "activate");
			else
			{
				position = text->point.index;
				gtk_editable_insert_text(editable, "\n", 1, &position);
			}
			break;
		case GDK_Escape:
			/*
			 * Don't insert literally 
			 */
			return_val = FALSE;
			break;
#endif
		default:
			return_val = FALSE;

#if 0
			if (event->state & GDK_CONTROL_MASK)
			{
				if ((key >= 'A') && (key <= 'Z'))
					key -= 'A' - 'a';

				if ((key >= 'a') && (key <= 'z')
					&& control_keys[(int) (key - 'a')])
				{
					(*control_keys[(int) (key - 'a')]) (editable, event->time);
					return_val = TRUE;
				}

				break;
			}
			else if (event->state & GDK_MOD1_MASK)
			{
				if ((key >= 'A') && (key <= 'Z'))
					key -= 'A' - 'a';

				if ((key >= 'a') && (key <= 'z') && alt_keys[(int) (key - 'a')])
				{
					(*alt_keys[(int) (key - 'a')]) (editable, event->time);
					return_val = TRUE;
				}
				break;
			}
#endif
			/*
			 * if (event->length > 0) {
			 * html->cursor_pos++;
			 * gtk_editable_insert_text (editable, event->string, event->length, &position);
			 * 
			 * return_val = TRUE;
			 * }
			 * else
			 * return_val = FALSE;
			 */
		}

	}

	return return_val;
}

void gtk_html_freeze(GtkHtml * html)
{
	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));

	html->frozen++;
}

void gtk_html_thaw(GtkHtml * html)
{
	GdkRectangle area;

	g_return_if_fail(html != NULL);
	g_return_if_fail(GTK_IS_HTML(html));

	gtk_html_append_text(html, NULL, 0);

	html->frozen--;

	if (html->frozen < 0)
		html->frozen = 0;

	if (html->frozen == 0)
	{
		if (html->html_area)
		{
			gint width,
			  height;
			area.x = 0;
			area.y = 0;

			gdk_window_get_size(html->html_area, &width, &height);

			area.width = width;
			area.height = height;

			expose_html(html, &area, TRUE);
		}
	}
}
