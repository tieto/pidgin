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

#ifndef __GTK_IMHTML_H
#define __GTK_IMHTML_H

#include <gdk/gdk.h>
#include <gtk/gtktextview.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_TYPE_IMHTML            (gtk_imhtml_get_type ())
#define GTK_IMHTML(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_IMHTML, GtkIMHtml))
#define GTK_IMHTML_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_IMHTML, GtkIMHtmlClass))
#define GTK_IS_IMHTML(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_IMHTML))
#define GTK_IS_IMHTML_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_IMHTML))

typedef gchar** (*GtkIMHtmlImage) (gchar *url);

typedef struct _GtkSmileyTree  GtkSmileyTree;

typedef struct _GtkIMHtml      GtkIMHtml;
typedef struct _GtkIMHtmlClass GtkIMHtmlClass;

struct _GtkIMHtml {
	GtkTextView text_view;
	GtkTextBuffer *text_buffer;
	GtkTextMark *end;
	gboolean comments, smileys;
	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
};

struct _GtkIMHtmlClass {
	GtkTextViewClass parent_class;

	void (*url_clicked) (GtkIMHtml *, const gchar *);
};

typedef enum
{
	GTK_IMHTML_NO_COLOURS   = 1 << 0,
	GTK_IMHTML_NO_FONTS     = 1 << 1,
	GTK_IMHTML_NO_COMMENTS  = 1 << 2,
	GTK_IMHTML_NO_TITLE     = 1 << 3,
	GTK_IMHTML_NO_NEWLINE   = 1 << 4,
	GTK_IMHTML_NO_SIZES	= 1 << 5,
	GTK_IMHTML_NO_SCROLL	= 1 << 6,
	GTK_IMHTML_RETURN_LOG	= 1 << 7
} GtkIMHtmlOptions;

GtkType    gtk_imhtml_get_type         (void);
GtkWidget* gtk_imhtml_new              (void *, void *);

void       gtk_imhtml_set_adjustments  (GtkIMHtml        *imhtml,
					GtkAdjustment    *hadj,
					GtkAdjustment    *vadj);

void       gtk_imhtml_set_defaults     (GtkIMHtml            *imhtml,
					PangoFontDescription *font,
					GdkColor             *fg_color,
					GdkColor             *bg_color);

void       gtk_imhtml_set_img_handler  (GtkIMHtml        *imhtml,
					GtkIMHtmlImage    handler);

void       gtk_imhtml_associate_smiley (GtkIMHtml        *imhtml,
					gchar            *text,
					gchar           **xpm);

void 	   gtk_imhtml_init_smileys     (GtkIMHtml *imhtml);

void       gtk_imhtml_remove_smileys   (GtkIMHtml        *imhtml);

void       gtk_imhtml_reset_smileys   (GtkIMHtml        *imhtml);

void       gtk_imhtml_show_smileys     (GtkIMHtml        *imhtml,
					gboolean          show);

void       gtk_imhtml_show_comments    (GtkIMHtml        *imhtml,
					gboolean          show);

GString*   gtk_imhtml_append_text      (GtkIMHtml        *imhtml,
					const gchar      *text,
					gint              len,
					GtkIMHtmlOptions  options);

void       gtk_imhtml_clear            (GtkIMHtml        *imhtml);

void       gtk_imhtml_page_up          (GtkIMHtml        *imhtml);

void       gtk_imhtml_page_down        (GtkIMHtml        *imhtml);
void       gtk_imhtml_to_bottom        (GtkIMHtml        *imhtml);

#ifdef __cplusplus
}
#endif

#endif
