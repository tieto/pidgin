/**
 * @file gtkaccount.c Account Editor dialog
 * @ingroup gtkui
 *
 * gaim
 *
 * Copyright (C) 2002-2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "gtkaccount.h"
#include "account.h"
#include "prefs.h"
#include "stock.h"

typedef struct
{
	GtkWidget *window;

} AccountsDialog;

static AccountsDialog *accounts_dialog = NULL;

static gint
__window_destroy_cb(GtkWidget *w, GdkEvent *event, void *unused)
{
	g_free(accounts_dialog);
	accounts_dialog = NULL;

	return FALSE;
}

static gboolean
__configure_cb(GtkWidget *w, GdkEventConfigure *event, AccountsDialog *dialog)
{
	if (GTK_WIDGET_VISIBLE(w)) {
		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/width",  event->width);
		gaim_prefs_set_int("/gaim/gtk/accounts/dialog/height", event->height);
	}

	return FALSE;
}

static GtkWidget *
__create_accounts_list(void)
{
	return NULL;
}

void
gaim_gtk_account_dialog_show(void)
{
	AccountsDialog *dialog;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;
	GtkWidget *sw;
	GtkWidget *sep;
	GtkWidget *button;
	int width, height;

	if (accounts_dialog != NULL)
		return;

	dialog = g_new0(AccountsDialog, 1);

	width  = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/width");
	height = gaim_prefs_get_int("/gaim/gtk/accounts/dialog/height");

	win = accounts_dialog->window;

	GAIM_DIALOG(win);
	gtk_window_set_default_size(GTK_WINDOW(win), width, height);
	gtk_window_set_role(GTK_WINDOW(win), "accounts");
	gtk_window_set_title(GTK_WINDOW(win), "Accounts");
	gtk_container_set_border_width(GTK_CONTAINER(win), 12);

	g_signal_connect(G_OBJECT(win), "delete_event",
					 G_CALLBACK(__window_destroy_cb), accounts_dialog);
	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(__configure_cb), accounts_dialog);

	/* Setup the vbox */
	vbox = gtk_vbox_new(FALSE, 12);
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Setup the scrolled window that will contain the list of accounts. */
	sw = __create_accounts_list();
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	/* Separator... */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
	gtk_widget_show(sep);

	/* Button box. */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), 6);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(vbox);

	/* Add button */
	button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* Modify button */
	button = gtk_button_new_from_stock(GAIM_STOCK_MODIFY);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* Delete button */
	button = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(button), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	/* Close button */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(button), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	gtk_widget_show(win);
}

