/*
 * @file gtkcertmgr.c GTK+ Certificate Manager API
 * @ingroup pidgin
 *
 * pidgin
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
 *
 */

#include <glib.h>

#include "core.h"
#include "internal.h"
#include "pidgin.h"

#include "certificate.h"
#include "debug.h"
#include "notify.h"

#include "gtkblist.h"
#include "gtkutils.h"

#include "gtkcertmgr.h"

/*****************************************************************************
 * X.509 tls_peers management interface                                      *
 *****************************************************************************/

typedef struct {
	GtkWidget *mgmt_widget;
	GtkTreeView *listview;
	GtkTreeSelection *listselect;
	GtkWidget *importbutton;
	GtkWidget *exportbutton;
	GtkWidget *infobutton;
	GtkWidget *deletebutton;
	PurpleCertificatePool *tls_peers;
} tls_peers_mgmt_data;

tls_peers_mgmt_data *tpm_dat = NULL;

/* Columns
   See http://developer.gnome.org/doc/API/2.0/gtk/TreeWidget.html */
enum
{
	TPM_HOSTNAME_COLUMN,
	TPM_N_COLUMNS
};

static void
tls_peers_mgmt_destroy(GtkWidget *mgmt_widget, gpointer data)
{
	purple_debug_info("certmgr",
			  "tls peers self-destructs\n");

	purple_signals_disconnect_by_handle(tpm_dat);
	g_free(tpm_dat); tpm_dat = NULL;
}

static void
tls_peers_mgmt_repopulate_list(void)
{
	GtkTreeView *listview = tpm_dat->listview;
	PurpleCertificatePool *tls_peers;
	GList *idlist, *l;
	
	GtkListStore *store = GTK_LIST_STORE(
		gtk_tree_view_get_model(GTK_TREE_VIEW(listview)));
	
	/* First, delete everything in the list */
	gtk_list_store_clear(store);

	/* Locate the "tls_peers" pool */
	tls_peers = purple_certificate_find_pool("x509", "tls_peers");
	g_return_if_fail(tls_peers);

	/* Grab the loaded certificates */
	idlist = purple_certificate_pool_get_idlist(tls_peers);

	/* Populate the listview */
	for (l = idlist; l; l = l->next) {
		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);

		gtk_list_store_set(GTK_LIST_STORE(store), &iter,
				   TPM_HOSTNAME_COLUMN, l->data,
				   -1);
	}
	purple_certificate_pool_destroy_idlist(idlist);
}

static void
tls_peers_mgmt_mod_cb(PurpleCertificatePool *pool, const gchar *id, gpointer data)
{
	g_assert (pool == tpm_dat->tls_peers);

	tls_peers_mgmt_repopulate_list();
}

static void
tls_peers_mgmt_delete_cb(GtkWidget *button, gpointer data)
{
	GtkTreeSelection *select = tpm_dat->listselect;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* See if things are selected */
	if (gtk_tree_selection_get_selected(select, &model, &iter)) {

		gchar *id;

		/* Retrieve the selected hostname */
		gtk_tree_model_get(model, &iter, TPM_HOSTNAME_COLUMN, &id, -1);

		/* Now delete the thing */
		g_assert(purple_certificate_pool_delete(tpm_dat->tls_peers, id));
		
		g_free(id);
	} else {
		purple_debug_warning("gtkcertmgr/tls_peers_mgmt",
				     "Delete clicked with no selection?\n");
		return;
	}
}

static GtkWidget *
tls_peers_mgmt_build(void)
{
	GtkWidget *bbox;
	GtkListStore *store;

	/* This block of variables will end up in tpm_dat */
	GtkTreeView *listview;
	GtkWidget *importbutton;
	GtkWidget *exportbutton;
	GtkWidget *infobutton;
	GtkWidget *deletebutton;
	/** Element to return to the Certmgr window to put in the Notebook */
	GtkWidget *mgmt_widget;

	/* Create a struct to store context information about this window */
	tpm_dat = g_new0(tls_peers_mgmt_data, 1);
	
	tpm_dat->mgmt_widget = mgmt_widget =
		gtk_hbox_new(FALSE, /* Non-homogeneous */
			     0);    /* No spacing */
	gtk_widget_show(mgmt_widget);

	/* Ensure that everything gets cleaned up when the dialog box
	   is closed */
	g_signal_connect(G_OBJECT(mgmt_widget), "destroy",
			 G_CALLBACK(tls_peers_mgmt_destroy), NULL);

	/* List view */
	store = gtk_list_store_new(TPM_N_COLUMNS, G_TYPE_STRING);
	
	tpm_dat->listview = listview =
		GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
	
	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		GtkTreeSelection *select;

		/* Set up the display columns */
		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
			"Hostname",
			renderer,
			"text", TPM_HOSTNAME_COLUMN,
			NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);

		/* Get the treeview selector into the struct */
		tpm_dat->listselect = select =
			gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
		
		/* Force the selection mode */
		gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	}
	
	gtk_box_pack_start(GTK_BOX(mgmt_widget), GTK_WIDGET(listview),
			   TRUE, TRUE, /* Take up lots of space */
			   0); /* TODO: this padding is wrong */
	gtk_widget_show(GTK_WIDGET(listview));

	/* Fill the list for the first time */
	tls_peers_mgmt_repopulate_list();
	
	/* Right-hand side controls box */
	bbox = gtk_vbutton_box_new();
	gtk_box_pack_end(GTK_BOX(mgmt_widget), bbox,
			 FALSE, FALSE, /* Do not take up space */
			 0); /* TODO: this padding is probably wrong */
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_START);
	gtk_widget_show(bbox);

	/* Import button */
	/* TODO: This is the wrong stock button */
	tpm_dat->importbutton = importbutton =
		gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_box_pack_start(GTK_BOX(bbox), importbutton, FALSE, FALSE, 0);
	gtk_widget_show(importbutton);

	/* Export button */
	/* TODO: This is the wrong stock button */
	tpm_dat->exportbutton = exportbutton =
		gtk_button_new_from_stock(GTK_STOCK_SAVE);
	gtk_box_pack_start(GTK_BOX(bbox), exportbutton, FALSE, FALSE, 0);
	gtk_widget_show(exportbutton);

	/* Info button */
	tpm_dat->infobutton = infobutton =
		gtk_button_new_from_stock(GTK_STOCK_INFO);
	gtk_box_pack_start(GTK_BOX(bbox), infobutton, FALSE, FALSE, 0);
	gtk_widget_show(infobutton);

	/* Delete button */
	tpm_dat->deletebutton = deletebutton =
		gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_box_pack_start(GTK_BOX(bbox), deletebutton, FALSE, FALSE, 0);
	gtk_widget_show(deletebutton);
	g_signal_connect(G_OBJECT(deletebutton), "clicked",
			 G_CALLBACK(tls_peers_mgmt_delete_cb), NULL);

	/* Disable all the buttons */
	gtk_widget_set_sensitive(importbutton, FALSE);
	gtk_widget_set_sensitive(exportbutton, FALSE);
	gtk_widget_set_sensitive(infobutton, FALSE);
	gtk_widget_set_sensitive(deletebutton, FALSE);

	/* Bind us to the tls_peers pool */
	tpm_dat->tls_peers = purple_certificate_find_pool("x509", "tls_peers");
	
	/**** libpurple signals ****/
	/* Respond to certificate add/remove by just reloading everything */
	purple_signal_connect(tpm_dat->tls_peers, "certificate-stored",
			      tpm_dat, PURPLE_CALLBACK(tls_peers_mgmt_mod_cb),
			      NULL);
	purple_signal_connect(tpm_dat->tls_peers, "certificate-deleted",
			      tpm_dat, PURPLE_CALLBACK(tls_peers_mgmt_mod_cb),
			      NULL);
	
	return mgmt_widget;
}

PidginCertificateManager tls_peers_mgmt = {
	tls_peers_mgmt_build, /* Widget creation function */
	N_("SSL Servers")
};

/*****************************************************************************
 * GTK+ main certificate manager                                             *
 *****************************************************************************/
typedef struct
{
	GtkWidget *window;
	GtkWidget *notebook;

	GtkWidget *closebutton;
} CertMgrDialog;

/* If a certificate manager window is open, this will point to it.
   So if it is set, don't open another one! */
CertMgrDialog *certmgr_dialog = NULL;

static void
certmgr_close_cb(GtkWidget *w, CertMgrDialog *dlg)
{
	/* TODO: Ignoring the arguments to this function may not be ideal,
	   but there *should* only be "one dialog to rule them all" at a time*/
	pidgin_certmgr_hide();
}

void
pidgin_certmgr_show(void)
{
	CertMgrDialog *dlg;
	GtkWidget *win;
	GtkWidget *vbox;
	GtkWidget *bbox;

	/* Enumerate all the certificates on file */
	{
		GList *idlist, *poollist;

		for ( poollist = purple_certificate_get_pools();
		      poollist;
		      poollist = poollist->next ) {
			PurpleCertificatePool *pool = poollist->data;
			GList *l;
			
			purple_debug_info("gtkcertmgr",
					  "Pool %s found for scheme %s -"
					  "Enumerating certificates:\n",
					  pool->name, pool->scheme_name);

			idlist = purple_certificate_pool_get_idlist(pool);

			for (l=idlist; l; l = l->next) {
				purple_debug_info("gtkcertmgr",
						  "- %s\n",
						  (gchar *) l->data);
			} /* idlist */
			purple_certificate_pool_destroy_idlist(idlist);
		} /* poollist */
	}

	
	/* If the manager is already open, bring it to the front */
	if (certmgr_dialog != NULL) {
		gtk_window_present(GTK_WINDOW(certmgr_dialog->window));
		return;
	}

	/* Create the dialog, and set certmgr_dialog so we never create
	   more than one at a time */
	dlg = certmgr_dialog = g_new0(CertMgrDialog, 1);

	win = dlg->window =
		pidgin_create_window(_("Certificate Manager"),/* Title */
				     PIDGIN_HIG_BORDER, /*Window border*/
				     "certmgr",         /* Role */
				     TRUE); /* Allow resizing */
	g_signal_connect(G_OBJECT(win), "delete_event",
			 G_CALLBACK(certmgr_close_cb), dlg);

	
	/* TODO: Retrieve the user-set window size and use it */
	gtk_window_set_default_size(GTK_WINDOW(win), 400, 400);

	/* Main vbox */
	vbox = gtk_vbox_new( FALSE, PIDGIN_HIG_BORDER );
	gtk_container_add(GTK_CONTAINER(win), vbox);
	gtk_widget_show(vbox);

	/* Notebook of various certificate managers */
	dlg->notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), dlg->notebook,
			   TRUE, TRUE, /* Notebook should take extra space */
			   0);
	gtk_widget_show(dlg->notebook);

	/* Box for the close button */
	bbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(bbox), PIDGIN_HIG_BOX_SPACE);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, TRUE, 0);
	gtk_widget_show(bbox);

	/* Close button */
	dlg->closebutton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_box_pack_start(GTK_BOX(bbox), dlg->closebutton, FALSE, FALSE, 0);
	gtk_widget_show(dlg->closebutton);
	g_signal_connect(G_OBJECT(dlg->closebutton), "clicked",
			 G_CALLBACK(certmgr_close_cb), dlg);

	/* Add the defined certificate managers */
	/* TODO: Find a way of determining whether each is shown or not */
	/* TODO: Implement this correctly */
	gtk_notebook_append_page(GTK_NOTEBOOK (dlg->notebook),
				 (tls_peers_mgmt.build)(),
				 gtk_label_new(_(tls_peers_mgmt.label)) );

	gtk_widget_show(win);
}

void
pidgin_certmgr_hide(void)
{
	/* If it isn't open, do nothing */
	if (certmgr_dialog == NULL) {
		return;
	}

	purple_signals_disconnect_by_handle(certmgr_dialog);
	purple_prefs_disconnect_by_handle(certmgr_dialog);

	gtk_widget_destroy(certmgr_dialog->window);
	g_free(certmgr_dialog);
	certmgr_dialog = NULL;

	/* If this was the only window left, quit */
	if (PIDGIN_BLIST(purple_get_blist())->window == NULL &&
		purple_connections_get_all() == NULL) {

		purple_core_quit();
	}
}
