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
