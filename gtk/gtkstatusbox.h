/*
 * @file gtkstatusbox.c GTK+ Status Selection Widget
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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


#ifndef __GTK_GAIM_STATUS_BOX_H__
#define __GTK_GAIM_STATUS_BOX_H__

#include <gtk/gtk.h>
#include "gtkimhtml.h"
#include "account.h"
#include "savedstatuses.h"
#include "status.h"
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

#define GTK_GAIM_TYPE_STATUS_BOX             (gtk_gaim_status_box_get_type ())
#define GTK_GAIM_STATUS_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_GAIM_TYPE_STATUS_BOX, GtkGaimStatusBox))
#define GTK_GAIM_STATUS_BOX_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), GTK_GAIM_TYPE_STATUS_BOX, GtkGaimStatusBoxClass))
#define GTK_GAIM_IS_STATUS_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_GAIM_TYPE_STATUS_BOX))
#define GTK_GAIM_IS_STATUS_BOX_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), GTK_GAIM_TYPE_STATUS_BOX))
#define GTK_GAIM_STATUS_BOX_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), GTK_GAIM_TYPE_STATUS_BOX, GtkGaimStatusBoxClass))

/**
 * This is a hidden field in the GtkStatusBox that identifies the
 * item in the list store.  The item could be a normal
 * GaimStatusPrimitive, or it could be something special like the
 * "Custom..." item, or "Saved..." or a GtkSeparator.
 */
typedef enum
{
	GTK_GAIM_STATUS_BOX_TYPE_SEPARATOR,
	GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE,
	GTK_GAIM_STATUS_BOX_TYPE_POPULAR,
	GTK_GAIM_STATUS_BOX_TYPE_CUSTOM,
	GTK_GAIM_STATUS_BOX_TYPE_SAVED,
	GTK_GAIM_STATUS_BOX_NUM_TYPES
} GtkGaimStatusBoxItemType;

typedef struct _GtkGaimStatusBox      GtkGaimStatusBox;
typedef struct _GtkGaimStatusBoxClass GtkGaimStatusBoxClass;

struct _GtkGaimStatusBox
{
	GtkComboBox parent_instance;

	/**
	 * This GtkListStore contains only one row--the currently selected status.
	 */
	GtkListStore *store;

	/**
	 * This is the dropdown GtkListStore that contains the available statuses,
	 * plus some recently used statuses, plus the "Custom..." and "Saved..."
	 * options.
	 */
	GtkListStore *dropdown_store;

	GaimAccount *account;

	GtkWidget *vbox, *sw;
	GtkWidget *imhtml;

	char      *buddy_icon_path;
	GdkPixbuf *buddy_icon;
	GdkPixbuf *buddy_icon_hover;
	GtkWidget *buddy_icon_sel;
	GtkWidget *icon;
	GtkWidget *icon_box;
	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
	int icon_size;

	gboolean imhtml_visible;

	GtkWidget *cell_view;
	GtkCellRenderer *icon_rend;
	GtkCellRenderer *text_rend;

	GdkPixbuf *error_pixbuf;
	int connecting_index;
	GdkPixbuf *connecting_pixbufs[4];
	int typing_index;
	GdkPixbuf *typing_pixbufs[4];

	gboolean connecting;
	guint typing;

	GtkTreeIter iter;
	char *error;

	gulong status_changed_signal;

	/*
	 * These widgets are made for renderin'
	 * That's just what they'll do
	 * One of these days these widgets
	 * Are gonna render all over you
	 */
	GtkWidget *hbox;
	GtkWidget *toggle_button;
	GtkWidget *vsep;
	GtkWidget *arrow;
};

struct _GtkGaimStatusBoxClass
{
	GtkComboBoxClass parent_class;

	/* signals */
	void     (* changed)          (GtkComboBox *combo_box);

	/* Padding for future expansion */
	void (*_gtk_reserved0) (void);
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);
};


GType         gtk_gaim_status_box_get_type         (void) G_GNUC_CONST;
GtkWidget    *gtk_gaim_status_box_new              (void);
GtkWidget    *gtk_gaim_status_box_new_with_account (GaimAccount *);

void
gtk_gaim_status_box_add(GtkGaimStatusBox *status_box, GtkGaimStatusBoxItemType type, GdkPixbuf *pixbuf, const char *text, const char *sec_text, gpointer data);

void
gtk_gaim_status_box_add_separator(GtkGaimStatusBox *status_box);

void
gtk_gaim_status_box_set_connecting(GtkGaimStatusBox *status_box, gboolean connecting);

void
gtk_gaim_status_box_pulse_connecting(GtkGaimStatusBox *status_box);

void
gtk_gaim_status_box_set_buddy_icon(GtkGaimStatusBox *status_box, const char *filename);

const char *
gtk_gaim_status_box_get_buddy_icon(GtkGaimStatusBox *status_box);

char *gtk_gaim_status_box_get_message(GtkGaimStatusBox *status_box);

G_END_DECLS

#endif /* __GTK_GAIM_GTK_STATUS_COMBO_BOX_H__ */
