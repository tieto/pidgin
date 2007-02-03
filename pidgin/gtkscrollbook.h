/*
 * @file gtkscrollbook  GTK+ Scrolling notebook Widget
 * @ingroup gtkui
 *
 * gaim
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */

#ifndef __GTK_GAIM_SCROLL_BOOK_H__
#define __GTK_GAIM_SCROLL_BOOK_H__

#include <gtk/gtk.h>

#if !GTK_CHECK_VERSION(2,4,0)
#include "gaimcombobox.h"
#endif

G_BEGIN_DECLS

#define GTK_GAIM_TYPE_SCROLL_BOOK             (gtk_gaim_scroll_book_get_type ())
#define GTK_GAIM_SCROLL_BOOK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_GAIM_TYPE_SCROLL_BOOK, GtkGaimScrollBook))
#define GTK_GAIM_SCROLL_BOOK_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), GTK_GAIM_TYPE_SCROLL_BOOK, GtkGaimScrollBookClass))
#define GTK_GAIM_IS_SCROLL_BOOK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_GAIM_TYPE_SCROLL_BOOK))
#define GTK_GAIM_IS_SCROLL_BOOK_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), GTK_GAIM_TYPE_SCROLL_BOOK))
#define GTK_GAIM_SCROLL_BOOK_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), GTK_GAIM_TYPE_SCROLL_BOOK, GtkGaimScrollBookClass))

typedef struct _GtkGaimScrollBook      GtkGaimScrollBook;
typedef struct _GtkGaimScrollBookClass GtkGaimScrollBookClass;

struct _GtkGaimScrollBook
{
	GtkVBox parent_instance;

	GtkWidget *notebook;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *left_arrow;
	GtkWidget *right_arrow;
	
	/* Padding for future expansion */
	void (*_gtk_reserved0) (void);
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);

};


struct _GtkGaimScrollBookClass
{
	GtkComboBoxClass parent_class;

	/* Padding for future expansion */
	void (*_gtk_reserved0) (void);
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);
};


GType         gtk_gaim_scroll_book_get_type         (void) G_GNUC_CONST;
GtkWidget    *gtk_gaim_scroll_book_new              (void);

G_END_DECLS

#endif  /* __GTK_GAIM_SCROLL_BOOK_H__ */
