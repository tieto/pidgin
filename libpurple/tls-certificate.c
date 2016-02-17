/*
 *
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#include "internal.h"
#include "tls-certificate.h"
#include "debug.h"
#include "util.h"

/* Makes a filename path for a certificate. If id is NULL,
 * just return the directory
 */
static gchar *
make_certificate_path(const gchar *id)
{
	return g_build_filename(purple_user_dir(),
				"certificates", "tls",
				id != NULL ? purple_escape_filename(id) : NULL,
				NULL);
}

/* Creates the certificate directory if it doesn't exist,
 * returns TRUE if it's successful or it already exists,
 * returns FALSE if there was an error.
 */
static gboolean
ensure_certificate_dir(GError **error)
{
	gchar *dir = make_certificate_path(NULL);
	gboolean ret = TRUE;

	if (purple_build_dir(dir, 0700) != 0) {
		g_set_error_literal(error, G_FILE_ERROR,
				g_file_error_from_errno(errno),
				g_strerror(errno));
		ret = FALSE;
	}

	g_free(dir);
	return ret;
}

GList *
purple_tls_certificate_list_ids()
{
	gchar *dir_path;
	GDir *dir;
	const gchar *entry;
	GList *idlist = NULL;
	GError *error = NULL;

	/* Ensure certificate directory exists */

	if (!ensure_certificate_dir(&error)) {
		purple_debug_error("tls-certificate",
				"Error creating certificate directory: %s",
				error->message);
		g_clear_error(&error);
		return NULL;
	}

	/* Open certificate directory */

	dir_path = make_certificate_path(NULL);
	dir = g_dir_open(dir_path, 0, &error);

	if (dir == NULL) {
		purple_debug_error("tls-certificate",
				"Error opening certificate directory (%s): %s",
				dir_path, error->message);
		g_free(dir_path);
		g_clear_error(&error);
		return NULL;
	}

	g_free(dir_path);

	/* Traverse the directory listing and create an idlist */

	while ((entry = g_dir_read_name(dir)) != NULL) {
		/* Unescape the filename
		 * (GLib owns original string)
		 */
		const char *unescaped = purple_unescape_filename(entry);

		/* Copy the entry name into our list
		 * (Purple own the escaped string)
		 */
		idlist = g_list_prepend(idlist, g_strdup(unescaped));
	}

	g_dir_close(dir);

	return idlist;
}

void
purple_tls_certificate_free_ids(GList *ids)
{
	g_list_free_full(ids, g_free);
}

GTlsCertificate *
purple_tls_certificate_new_from_id(const gchar *id, GError **error)
{
	GTlsCertificate *cert;
	gchar *path;

	g_return_val_if_fail(id != NULL && id[0] != '\0', NULL);

	/* Load certificate from file if it exists */

	path = make_certificate_path(id);
	cert = g_tls_certificate_new_from_file(path, error);
	g_free(path);

	return cert;
}

gboolean
purple_tls_certificate_trust(const gchar *id, GTlsCertificate *certificate,
		GError **error)
{
	gchar *path;
	gchar *pem = NULL;
	gboolean ret;

	g_return_val_if_fail(id != NULL && id[0] != '\0', FALSE);
	g_return_val_if_fail(G_IS_TLS_CERTIFICATE(certificate), FALSE);

	/* Ensure certificate directory exists */

	if (!ensure_certificate_dir(error)) {
		return FALSE;
	}

	/* Get the text representation of the certificate */

	g_object_get(certificate, "certificate-pem", &pem, NULL);
	g_return_val_if_fail(pem != NULL, FALSE);

	/* Save certificate text to a fail */

	path = make_certificate_path(id);
	ret = g_file_set_contents(path, pem, -1, error);
	g_free(path);
	g_free(pem);

	return ret;
}

gboolean
purple_tls_certificate_distrust(const gchar *id, GError **error)
{
	gchar *path;
	gboolean ret = TRUE;

	g_return_val_if_fail(id != NULL && id[0] != '\0', FALSE);

	/* Delete certificate file if it exists */

	path = make_certificate_path(id);

	if (g_unlink(path) != 0) {
		g_set_error_literal(error, G_FILE_ERROR,
				g_file_error_from_errno(errno),
				g_strerror(errno));
		ret = FALSE;
	}

	g_free(path);

	return ret;
}

