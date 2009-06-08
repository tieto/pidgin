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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#ifndef PIDGIN_XMPP_DISCO_UI_H
#define PIDGIN_XMPP_DISCO_UI_H

typedef struct _PidginDiscoDialog PidginDiscoDialog;
typedef struct _PidginDiscoList PidginDiscoList;

#include "xmppdisco.h"

struct _PidginDiscoDialog {
	GtkWidget *window;
	GtkWidget *account_widget;

	GtkWidget *sw;
	GtkWidget *progress;

	GtkWidget *stop_button;
	GtkWidget *browse_button;
	GtkWidget *register_button;
	GtkWidget *add_button;
	GtkWidget *close_button;
	XmppDiscoService *selected;

	PurpleAccount *account;
	PidginDiscoList *discolist;
};

struct _PidginDiscoList {
	PurpleConnection *pc;
	gboolean in_progress;
	const gchar *server;

	gint ref;
	guint fetch_count;

	PidginDiscoDialog *dialog;
	GtkTreeStore *model;
	GtkWidget *tree;
	GHashTable *services;
};

/**
 * Shows a new service discovery dialog.
 */
PidginDiscoDialog *pidgin_disco_dialog_new(void);

/**
 * Destroy all the open dialogs (called when unloading the plugin).
 */
void pidgin_disco_dialogs_destroy_all(void);
void pidgin_disco_signed_off_cb(PurpleConnection *pc);

void pidgin_disco_add_service(PidginDiscoList *list, XmppDiscoService *service,
                              XmppDiscoService *parent);

PidginDiscoList *pidgin_disco_list_ref(PidginDiscoList *list);
void pidgin_disco_list_unref(PidginDiscoList *list);

void pidgin_disco_list_set_in_progress(PidginDiscoList *list, gboolean in_progress);
#endif /* PIDGIN_XMPP_DISCO_UI_H */
