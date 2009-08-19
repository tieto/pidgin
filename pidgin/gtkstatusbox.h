/*
 * @file gtkstatusbox.c GTK+ Status Selection Widget
 * @ingroup pidgin
 */

/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */


#ifndef __PIDGIN_STATUS_BOX_H__
#define __PIDGIN_STATUS_BOX_H__

#include <gtk/gtk.h>
#include "gtkimhtml.h"
#include "account.h"
#include "imgstore.h"
#include "savedstatuses.h"
#include "status.h"

G_BEGIN_DECLS

#define PIDGIN_TYPE_STATUS_BOX             (pidgin_status_box_get_type ())
#define PIDGIN_STATUS_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_STATUS_BOX, PidginStatusBox))
#define PIDGIN_STATUS_BOX_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), PIDGIN_TYPE_STATUS_BOX, PidginStatusBoxClass))
#define PIDGIN_IS_STATUS_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_STATUS_BOX))
#define PIDGIN_IS_STATUS_BOX_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), PIDGIN_TYPE_STATUS_BOX))
#define PIDGIN_STATUS_BOX_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), PIDGIN_TYPE_STATUS_BOX, PidginStatusBoxClass))

/**
 * This is a hidden field in the GtkStatusBox that identifies the
 * item in the list store.  The item could be a normal
 * PurpleStatusPrimitive, or it could be something special like the
 * "Custom..." item, or "Saved..." or a GtkSeparator.
 */
typedef enum
{
	PIDGIN_STATUS_BOX_TYPE_SEPARATOR,
	PIDGIN_STATUS_BOX_TYPE_PRIMITIVE,
	PIDGIN_STATUS_BOX_TYPE_POPULAR,
	PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR,
	PIDGIN_STATUS_BOX_TYPE_CUSTOM,
	PIDGIN_STATUS_BOX_TYPE_SAVED,
	PIDGIN_STATUS_BOX_NUM_TYPES
} PidginStatusBoxItemType;

typedef struct _PidginStatusBox      PidginStatusBox;
typedef struct _PidginStatusBoxClass PidginStatusBoxClass;

struct _PidginStatusBox
{
	GtkContainer parent_instance;

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

	PurpleAccount *account;

	/* This will be non-NULL and contain a sample account
	 * when all enabled accounts use the same statuses */
	PurpleAccount *token_status_account;

	GtkWidget *vbox, *sw;
	GtkWidget *imhtml;

	PurpleStoredImage *buddy_icon_img;
	GdkPixbuf *buddy_icon;
	GdkPixbuf *buddy_icon_hover;
	GtkWidget *buddy_icon_sel;
	GtkWidget *icon;
	GtkWidget *icon_box;
	GtkWidget *icon_box_menu;
	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
        int icon_size;
        gboolean icon_opaque;

	gboolean imhtml_visible;

	GtkWidget *cell_view;
	GtkCellRenderer *icon_rend;
	GtkCellRenderer *text_rend;

	GdkPixbuf *error_pixbuf;
	int connecting_index;
	GdkPixbuf *connecting_pixbufs[9];
	int typing_index;
	GdkPixbuf *typing_pixbufs[6];

	gboolean network_available;
	gboolean connecting;
	guint typing;

	GtkTreeIter iter;
	char *error;

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

	GtkWidget *popup_window;
	GtkWidget *popup_frame;
	GtkWidget *scrolled_window;
	GtkWidget *cell_view_frame;
	GtkTreeViewColumn *column;
	GtkWidget *tree_view;
	gboolean popup_in_progress;
	GtkTreeRowReference *active_row;
};

struct _PidginStatusBoxClass
{
	GtkContainerClass parent_class;

	/* signals */
	void     (* changed)          (GtkComboBox *combo_box);

	/* Padding for future expansion */
	void (*_gtk_reserved0) (void);
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);
};


GType         pidgin_status_box_get_type         (void) G_GNUC_CONST;
GtkWidget    *pidgin_status_box_new              (void);
GtkWidget    *pidgin_status_box_new_with_account (PurpleAccount *);

void
pidgin_status_box_add(PidginStatusBox *status_box, PidginStatusBoxItemType type, GdkPixbuf *pixbuf, const char *text, const char *sec_text, gpointer data);

void
pidgin_status_box_add_separator(PidginStatusBox *status_box);

void
pidgin_status_box_set_network_available(PidginStatusBox *status_box, gboolean available);

void
pidgin_status_box_set_connecting(PidginStatusBox *status_box, gboolean connecting);

void
pidgin_status_box_pulse_connecting(PidginStatusBox *status_box);

void
pidgin_status_box_set_buddy_icon(PidginStatusBox *status_box, PurpleStoredImage *img);

char *pidgin_status_box_get_message(PidginStatusBox *status_box);

G_END_DECLS

#endif /* __GTK_PIDGIN_STATUS_COMBO_BOX_H__ */
