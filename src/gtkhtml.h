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

#ifndef __GTK_HTML_H__
#define __GTK_HTML_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
/*extern "C" {*/
#endif /* __cplusplus */

#define GTK_TYPE_HTML            (gtk_html_get_type())
#define GTK_HTML(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_HTML, GtkHtml))
#define GTK_HTML_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_HTML, GtkHtmlClass))
#define GTK_IS_HTML(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_HTML))
#define GTK_IS_HTML_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_HTML)
        
typedef struct _GtkHtml          GtkHtml;
typedef struct _GtkHtmlClass     GtkHtmlClass;
typedef struct _GtkHtmlBit       GtkHtmlBit;


struct _GtkHtmlBit {
	int type;
	GdkColor *fore;
	GdkColor *back;
	GdkFont *font;
        int uline;
        int strike;
        int width, height;
	int x, y;
	char *url;
	int was_selected;
	int sel_s, sel_e;
	int newline;
        char *text;
	GdkPixmap *pm;
        int fit;
};


struct _GtkHtml {
	GtkWidget widget;
	
	GdkWindow *html_area;

	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	gint xoffset;
	gint yoffset;
	
	int current_x;
	int current_y;
        GdkGC *gc;
        GdkGC *bg_gc;
	GList *html_bits;
	GList *urls;
	int start_sel_x, start_sel_y;
	GtkHtmlBit *start_sel, *end_sel;
	int num_start, num_end;
	char *selected_text;
        gint editable;
        gint transparent;
	gint timer;
	gint last_ver_value;
        char *title;
        gint frozen;
        GtkHtmlBit *cursor_hb;
        GtkWidget *tooltip_window;
        GtkHtmlBit *tooltip_hb;
        int tooltip_timer;
        int cursor_pos;
        GdkPixmap *pm;
        
};


struct _GtkHtmlClass {
	GtkWidgetClass parent_class;

	void  (*set_scroll_adjustments)   (GtkHtml        *html,
					   GtkAdjustment  *hadjustment,
					   GtkAdjustment  *vadjustment);

};


#define HTML_BIT_TEXT 0
#define HTML_BIT_PIXMAP 1
#define HTML_BIT_SEP 2


#define HTML_OPTION_NO_COLOURS   0x01
#define HTML_OPTION_NO_FONTS     0x02

#define STYLE_ITALIC 0x01000000
#define STYLE_BOLD 0x020000000

#define FIXED_FONT "-*-courier-medium-r-*-*-*-%d-*-*-*-*-*-*"
#define FIXED_BOLD_FONT "-*-courier-bold-r-*-*-*-%d-*-*-*-*-*-*"
#define FIXED_ITALIC_FONT "-*-courier-medium-o-*-*-*-%d-*-*-*-*-*-*"
#define FIXED_BOLD_ITALIC_FONT "-*-courier-bold-o-*-*-*-%d-*-*-*-*-*-*"
#define PROP_FONT "-*-helvetica-medium-r-*-*-*-%d-*-*-*-*-*-*"
#define PROP_BOLD_FONT "-*-helvetica-bold-r-*-*-*-%d-*-*-*-*-*-*"
#define PROP_ITALIC_FONT "-*-helvetica-medium-o-*-*-*-%d-*-*-*-*-*-*"
#define PROP_BOLD_ITALIC_FONT "-*-helvetica-bold-o-*-*-*-%d-*-*-*-*-*-*"



#define HTML_TOOLTIP_DELAY 500

        

GtkType    gtk_html_get_type         (void);
GtkWidget* gtk_html_new              (GtkAdjustment *hadj,
				      GtkAdjustment *vadj);
void       gtk_html_set_editable     (GtkHtml *html,
                                      gboolean is_editable);
void       gtk_html_set_transparent  (GtkHtml *html,
                                      gboolean is_transparent);
void       gtk_html_set_adjustments  (GtkHtml       *html,
                                      GtkAdjustment *hadj,
				      GtkAdjustment *vadj);
void       gtk_html_append_text      (GtkHtml      *html,
                                      char *text,
                                      gint options);
void       gtk_html_freeze           (GtkHtml      *html);
void       gtk_html_thaw             (GtkHtml      *html);

void	gtk_html_add_pixmap	(GtkHtml * html, GdkPixmap *pm, gint fint, gint newline);


#ifdef __cplusplus
/*}*/
#endif /* __cplusplus */

#endif /* __GTK_HTML_H__ */



