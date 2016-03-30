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

#include "tls-certificate.h"
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
} certmgr;

/* Pretty much Xerox of gtkcertmgr */

/* Add certificate */
static void
tls_peers_mgmt_import_ok2_cb(gpointer data, const char *result)
{
	GTlsCertificate *crt = data;
	const char *id = result;
	GError *error = NULL;

	/* TODO: Perhaps prompt if you're overwriting a cert? */

	if (purple_tls_certificate_trust(id, crt, &error)) {
		gnt_tree_add_row_last(GNT_TREE(certmgr.tree), g_strdup(id),
				gnt_tree_create_row(GNT_TREE(certmgr.tree), id),
				NULL);
	} else {
		purple_debug_error("gntcertmgr/tls_peers_mgmt",
				"Failure trusting peer certificate '%s': %s",
				id, error->message);
		g_clear_error(&error);
	}

	g_object_unref(crt);
}

static void
tls_peers_mgmt_import_ok_cb(gpointer data, const char *filename)
{
	GTlsCertificate *crt;
	GError *error = NULL;

	crt = g_tls_certificate_new_from_file(filename, &error);

	if (crt != NULL) {
		gchar *default_hostname;
		PurpleTlsCertificateInfo *info;

		info = purple_tls_certificate_get_info(crt);
		default_hostname = purple_tls_certificate_info_get_subject_name(info);
		purple_tls_certificate_info_free(info);

		purple_request_input(NULL,
				_("Certificate Import"),
				_("Specify a hostname"),
				_("Type the host name this certificate is for."),
				default_hostname, FALSE, FALSE, NULL,
				_("OK"), G_CALLBACK(tls_peers_mgmt_import_ok2_cb),
				_("Cancel"), G_CALLBACK(g_object_unref),
				NULL, crt);
		g_free(default_hostname);
	} else {
		gchar * secondary;

		purple_debug_error("gntcertmgr/tls_peers_mgmt",
				"Failed to import certificate '%s': %s",
				filename, error->message);
		g_clear_error(&error);

		secondary = g_strdup_printf(_("File %s could not be imported.\nMake sure that the file is readable and in PEM format.\n"), filename);
		purple_notify_error(NULL,
				_("Certificate Import Error"),
				_("X.509 certificate import failed"),
				secondary, NULL);
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
			NULL, NULL );
}

/* Save certs in some file */
static void
tls_peers_mgmt_export_ok_cb(gpointer data, const char *filename)
{
	GTlsCertificate *crt = data;
	gchar *pem = NULL;
	GError *error = NULL;

	g_object_get(crt, "certificate-pem", &pem, NULL);
	g_return_if_fail(crt != NULL);

	if (!g_file_set_contents(filename, pem, -1, &error)) {
		gchar * secondary;

		purple_debug_error("gntcertmgr/tls_peers_mgmt",
				"Failed to export certificate '%s': %s",
				filename, error->message);
		g_clear_error(&error);

		secondary = g_strdup_printf(_("Export to file %s failed.\nCheck that you have write permission to the target path\n"), filename);
		purple_notify_error(NULL,
				    _("Certificate Export Error"),
				    _("X.509 certificate export failed"),
				    secondary, NULL);
		g_free(secondary);
	}

	g_free(pem);
	g_object_unref(crt);
}

static void
save_cert_cb(GntWidget *button, gpointer null)
{
	GTlsCertificate *crt;
	const char *key;
	GError *error = NULL;

	if (!certmgr.window)
		return;

	key = gnt_tree_get_selection_data(GNT_TREE(certmgr.tree));
	if (!key)
		return;

	crt = purple_tls_certificate_new_from_id(key, &error);

	if (!crt) {
		purple_debug_error("gntcertmgr/tls_peers_mgmt",
				"Failed to fetch trusted certificate '%s': %s",
				key, error->message);
		g_clear_error(&error);
		return;
	}

	purple_request_file((void*)key,
			_("PEM X.509 Certificate Export"),
			"certificate.pem", TRUE,
			G_CALLBACK(tls_peers_mgmt_export_ok_cb),
			G_CALLBACK(g_object_unref),
			NULL, crt);
}

/* Show information about a cert */
static void
info_cert_cb(GntWidget *button, gpointer null)
{
	const char *key;
	GTlsCertificate *crt;
	PurpleTlsCertificateInfo *info;
	gchar *subject;
	GByteArray *fpr_sha1;
	gchar *fpr_sha1_asc;
	gchar *primary, *secondary;

	if (!certmgr.window)
		return;

	key = gnt_tree_get_selection_data(GNT_TREE(certmgr.tree));
	if (!key)
		return;

	crt = purple_tls_certificate_new_from_id(key, NULL);
	g_return_if_fail(crt);

	primary = g_strdup_printf(_("Certificate for %s"), key);

	fpr_sha1 = purple_tls_certificate_get_fingerprint_sha1(crt);
	fpr_sha1_asc = purple_base16_encode_chunked(fpr_sha1->data,
						    fpr_sha1->len);

	info = purple_tls_certificate_get_info(crt);
	subject = purple_tls_certificate_info_get_subject_name(info);
	purple_tls_certificate_info_free(info);

	secondary = g_strdup_printf(_("Common name: %s\n\nSHA1 fingerprint:\n%s"), subject, fpr_sha1_asc);

	purple_notify_info(NULL,
			   _("SSL Host Certificate"), primary, secondary, NULL);

	g_free(primary);
	g_free(secondary);
	g_byte_array_free(fpr_sha1, TRUE);
	g_free(fpr_sha1_asc);
	g_free(subject);
	g_object_unref(crt);
}

/* Delete a cert */
static void
tls_peers_mgmt_delete_confirm_cb(gchar *id, gint dontcare)
{
	GError *error = NULL;

	if (!purple_tls_certificate_distrust(id, &error)) {
		purple_debug_warning("gntcertmgr/tls_peers_mgmt",
				"Deletion failed on id '%s': %s\n",
				id, error->message);
		g_clear_error(&error);
	};

	purple_request_close_with_handle((void*)id);
	gnt_tree_remove(GNT_TREE(certmgr.tree), (void*)id);

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
			NULL,
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

	idlist = purple_tls_certificate_list_ids();
	for (l = idlist; l; l = l->next) {
		gnt_tree_add_row_last(GNT_TREE(certmgr.tree), g_strdup(l->data),
				gnt_tree_create_row(GNT_TREE(certmgr.tree), l->data), NULL);
	}
	purple_tls_certificate_free_ids(idlist);
}

void finch_certmgr_show(void)
{
	GntWidget *win, *tree, *box, *button;

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

	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(purple_signals_disconnect_by_handle), NULL);

	gnt_widget_show(certmgr.window);
}

