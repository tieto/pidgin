/*
 * GtkIMHtmlToolbar
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#ifndef _PIDGINIMHTMLTOOLBAR_H_
#define _PIDGINIMHTMLTOOLBAR_H_

#include <gtk/gtk.h>
#include "gtkimhtml.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_FONT_FACE "Helvetica 12"

#define GTK_TYPE_IMHTMLTOOLBAR            (gtk_imhtmltoolbar_get_type())
#define GTK_IMHTMLTOOLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_IMHTMLTOOLBAR, GtkIMHtmlToolbar))
#define GTK_IMHTMLTOOLBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_IMHTMLTOOLBAR, GtkIMHtmlToolbarClass))
#define GTK_IS_IMHTMLTOOLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_IMHTMLTOOLBAR))
#define GTK_IS_IMHTMLTOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_IMHTMLTOOLBAR))

typedef struct _GtkIMHtmlToolbar		GtkIMHtmlToolbar;
typedef struct _GtkIMHtmlToolbarClass		GtkIMHtmlToolbarClass;

struct _GtkIMHtmlToolbar {
	GtkHBox box;

	GtkWidget *imhtml;

#if GTK_CHECK_VERSION(2,12,0)
	gpointer depr1;
#else
	GtkTooltips *tooltips;
#endif

	GtkWidget *bold;
	GtkWidget *italic;
	GtkWidget *underline;

	GtkWidget *larger_size;
	GtkWidget *normal_size;
	GtkWidget *smaller_size;

	GtkWidget *font;
	GtkWidget *fgcolor;
	GtkWidget *bgcolor;

	GtkWidget *clear;

	GtkWidget *image;
	GtkWidget *link;
	GtkWidget *smiley;
	GtkWidget *attention;

	GtkWidget *font_dialog;
	GtkWidget *fgcolor_dialog;
	GtkWidget *bgcolor_dialog;
	GtkWidget *link_dialog;
	GtkWidget *smiley_dialog;
	GtkWidget *image_dialog;

	char *sml;
	GtkWidget *strikethrough;
	GtkWidget *insert_hr;
	GtkWidget *call;
};

struct _GtkIMHtmlToolbarClass {
	GtkHBoxClass parent_class;

};

GType      gtk_imhtmltoolbar_get_type         (void);
GtkWidget* gtk_imhtmltoolbar_new              (void);

void gtk_imhtmltoolbar_attach    (GtkIMHtmlToolbar *toolbar, GtkWidget *imhtml);
void gtk_imhtmltoolbar_associate_smileys (GtkIMHtmlToolbar *toolbar, const char *proto_id);

void gtk_imhtmltoolbar_switch_active_conversation(GtkIMHtmlToolbar *toolbar,
	PurpleConversation *conv);

#ifdef __cplusplus
}
#endif

#endif /* _PIDGINIMHTMLTOOLBAR_H_ */
