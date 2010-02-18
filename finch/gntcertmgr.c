/**
 * @file gntcertmgr.c GNT Certificate Manager API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
 *
 */

#include <internal.h>
#include "finch.h"

#include "certificate.h"
#include "debug.h"
#include "notify.h"
#include "request.h"

#include "gntcertmgr.h"

#include "gntbutton.h"
#include "gntlabel.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"

struct {
	GntWidget *window;
	GntWidget *tree;
	PurpleCertificatePool *pool;
} certmgr;

/* Pretty much Xerox of gtkcertmgr */

/* Add certificate */
static void
tls_peers_mgmt_import_ok2_cb(gpointer data, const char *result)
{
	PurpleCertificate *crt = (PurpleCertificate *) data;
	const char *id = result;

	/* TODO: Perhaps prompt if you're overwriting a cert? */

	purple_certificate_pool_store(purple_certificate_find_pool("x509", "tls_peers"), id, crt);
	purple_certificate_destroy(crt);
}

static void
tls_peers_mgmt_import_cancel2_cb(gpointer data, const char *result)
{
	PurpleCertificate *crt = (PurpleCertificate *) data;
	purple_certificate_destroy(crt);
}

static void
tls_peers_mgmt_import_ok_cb(gpointer data, const char *filename)
{
	PurpleCertificateScheme *x509;
	PurpleCertificate *crt;

	x509 = purple_certificate_pool_get_scheme(purple_certificate_find_pool("x509", "tls_peers"));

	crt = purple_certificate_import(x509, filename);

	if (crt != NULL) {
		gchar *default_hostname;
		default_hostname = purple_certificate_get_subject_name(crt);
		purple_request_input(NULL,
				_("Certificate Import"),
				_("Specify a hostname"),
				_("Type the host name this certificate is for."),
				default_hostname, FALSE, FALSE, NULL,
				_("OK"), G_CALLBACK(tls_peers_mgmt_import_ok2_cb),
				_("Cancel"), G_CALLBACK(tls_peers_mgmt_import_cancel2_cb),
				NULL, NULL, NULL,
				crt);
		g_free(default_hostname);
	} else {
		gchar * secondary;
		secondary = g_strdup_printf(_("File %s could not be imported.\nMake sure that the file is readable and in PEM format.\n"), filename);
		purple_notify_error(NULL,
				_("Certificate Import Error"),
				_("X.509 certificate import failed"),
				secondary);
		g_free(secondary);
	}
}

static void
add_cert_cb(GntWidget *button, gpointer null)
{
	purple_request_file(NULL,
			_("Select a PEM certificate"),
			"certificate.pem",
			FALSE,
			G_CALLBACK(tls_peers_mgmt_import_ok_cb),
			NULL,
			NULL, NULL, NULL, NULL );
}

/* Save certs in some file */
static void
tls_peers_mgmt_export_ok_cb(gpointer data, const char *filename)
{
	PurpleCertificate *crt = (PurpleCertificate *) data;

	if (!purple_certificate_export(filename, crt)) {
		gchar * secondary;

		secondary = g_strdup_printf(_("Export to file %s failed.\nCheck that you have write permission to the target path\n"), filename);
		purple_notify_error(NULL,
				    _("Certificate Export Error"),
				    _("X.509 certificate export failed"),
				    secondary);
		g_free(secondary);
	}

	purple_certificate_destroy(crt);
}

static void
save_cert_cb(GntWidget *button, gpointer null)
{
	PurpleCertificate *crt;
	const char *key;

	if (!certmgr.window)
		return;

	key = gnt_tree_get_selection_data(GNT_TREE(certmgr.tree));
	if (!key)
		return;

	crt = purple_certificate_pool_retrieve(certmgr.pool, key);
	if (!crt) {
		purple_debug_error("gntcertmgr/tls_peers_mgmt",
				"Id %s was not in the peers cache?!\n", key);
		return;
	}

	purple_request_file((void*)key,
			_("PEM X.509 Certificate Export"),
			"certificate.pem", TRUE,
			G_CALLBACK(tls_peers_mgmt_export_ok_cb),
			G_CALLBACK(purple_certificate_destroy),
			NULL, NULL, NULL,
			crt);
}

/* Show information about a cert */
static void
info_cert_cb(GntWidget *button, gpointer null)
{
	const char *key;
	PurpleCertificate *crt;
	gchar *subject;
	GByteArray *fpr_sha1;
	gchar *fpr_sha1_asc;
	gchar *primary, *secondary;

	if (!certmgr.window)
		return;

	key = gnt_tree_get_selection_data(GNT_TREE(certmgr.tree));
	if (!key)
		return;

	crt = purple_certificate_pool_retrieve(certmgr.pool, key);
	g_return_if_fail(crt);

	primary = g_strdup_printf(_("Certificate for %s"), key);

	fpr_sha1 = purple_certificate_get_fingerprint_sha1(crt);
	fpr_sha1_asc = purple_base16_encode_chunked(fpr_sha1->data,
						    fpr_sha1->len);
	subject = purple_certificate_get_subject_name(crt);

	secondary = g_strdup_printf(_("Common name: %s\n\nSHA1 fingerprint:\n%s"), subject, fpr_sha1_asc);
	
	purple_notify_info(NULL,
			   _("SSL Host Certificate"), primary, secondary);

	g_free(primary);
	g_free(secondary);
	g_byte_array_free(fpr_sha1, TRUE);
	g_free(fpr_sha1_asc);
	g_free(subject);
	purple_certificate_destroy(crt);
}

/* Delete a cert */
static void
tls_peers_mgmt_delete_confirm_cb(gchar *id, gint dontcare)
{
	if (!purple_certificate_pool_delete(certmgr.pool, id)) {
		purple_debug_warning("gntcertmgr/tls_peers_mgmt",
				"Deletion failed on id %s\n", id);
	};

	g_free(id);
}

static void
delete_cert_cb(GntWidget *button, gpointer null)
{
	gchar *primary;
	const char *key;

	if (!certmgr.window)
		return;

	key = gnt_tree_get_selection_data(GNT_TREE(certmgr.tree));
	if (!key)
		return;

	primary = g_strdup_printf(_("Really delete certificate for %s?"), key);

	purple_request_close_with_handle((void *)key);
	purple_request_yes_no((void *)key, _("Confirm certificate delete"),
			primary, NULL,
			0,
			NULL, NULL, NULL,
			g_strdup(key),
			tls_peers_mgmt_delete_confirm_cb,
			g_free);

	g_free(primary);
}

/* populate the list */
static void
populate_cert_list(void)
{
	GList *idlist, *l;

	if (!certmgr.window)
		return;

	gnt_tree_remove_all(GNT_TREE(certmgr.tree));

	idlist = purple_certificate_pool_get_idlist(purple_certificate_find_pool("x509", "tls_peers"));
	for (l = idlist; l; l = l->next) {
		gnt_tree_add_row_last(GNT_TREE(certmgr.tree), g_strdup(l->data),
				gnt_tree_create_row(GNT_TREE(certmgr.tree), l->data), NULL);
	}
	purple_certificate_pool_destroy_idlist(idlist);
}

static void
cert_list_added(PurpleCertificatePool *pool, const char *id, gpointer null)
{
	g_return_if_fail(certmgr.window);
	gnt_tree_add_row_last(GNT_TREE(certmgr.tree), g_strdup(id),
			gnt_tree_create_row(GNT_TREE(certmgr.tree), id), NULL);
}

static void
cert_list_removed(PurpleCertificatePool *pool, const char *id, gpointer null)
{
	g_return_if_fail(certmgr.window);
	purple_request_close_with_handle((void*)id);
	gnt_tree_remove(GNT_TREE(certmgr.tree), (void*)id);
}

void finch_certmgr_show(void)
{
	GntWidget *win, *tree, *box, *button;
	PurpleCertificatePool *pool;

	if (certmgr.window) {
		gnt_window_present(certmgr.window);
		return;
	}

	certmgr.window = win = gnt_vwindow_new(FALSE);
	gnt_box_set_title(GNT_BOX(win), _("Certificate Manager"));
	gnt_box_set_pad(GNT_BOX(win), 0);

	certmgr.tree = tree = gnt_tree_new();
	gnt_tree_set_hash_fns(GNT_TREE(tree), g_str_hash, g_str_equal, g_free);
	gnt_tree_set_column_title(GNT_TREE(tree), 0, _("Hostname"));
	gnt_tree_set_show_title(GNT_TREE(tree), TRUE);

	gnt_box_add_widget(GNT_BOX(win), tree);

	box = gnt_hbox_new(FALSE);
	gnt_box_add_widget(GNT_BOX(win), box);

	button = gnt_button_new(_("Add"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(add_cert_cb), NULL);
	gnt_util_set_trigger_widget(GNT_WIDGET(tree), GNT_KEY_INS, button);

	button = gnt_button_new(_("Save"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(save_cert_cb), NULL);

	button = gnt_button_new(_("Info"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(info_cert_cb), NULL);

	button = gnt_button_new(_("Delete"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect(G_OBJECT(button), "activate", G_CALLBACK(delete_cert_cb), NULL);
	gnt_util_set_trigger_widget(GNT_WIDGET(tree), GNT_KEY_DEL, button);

	button = gnt_button_new(_("Close"));
	gnt_box_add_widget(GNT_BOX(box), button);
	g_signal_connect_swapped(G_OBJECT(button), "activate", G_CALLBACK(gnt_widget_destroy), win);

	g_signal_connect_swapped(G_OBJECT(win), "destroy", G_CALLBACK(g_nullify_pointer), &certmgr.window);

	populate_cert_list();

	pool = certmgr.pool = purple_certificate_find_pool("x509", "tls_peers");
	purple_signal_connect(pool, "certificate-stored",
			      win, PURPLE_CALLBACK(cert_list_added), NULL);
	purple_signal_connect(pool, "certificate-deleted",
			      win, PURPLE_CALLBACK(cert_list_removed), NULL);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(purple_signals_disconnect_by_handle), NULL);

	gnt_widget_show(certmgr.window);
}

