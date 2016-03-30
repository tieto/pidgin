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
#include "ciphers/sha1hash.h"
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

/* Called when a GTlsConnection (which this handler has been connected to)
 *   has an error validating its certificate.
 * Returns TRUE if the certificate is already trusted, so the connection
 *   can continue.
 * Returns FALSE if the certificate is not trusted, causing the
 *   connection's handshake to fail.
 */
static gboolean
accept_certificate_cb(GTlsConnection *conn, GTlsCertificate *peer_cert,
		GTlsCertificateFlags errors, gpointer user_data)
{
	GTlsCertificate *trusted_cert;
	GSocketConnectable *connectable;
	const gchar *identity;

	g_return_val_if_fail(G_IS_TLS_CLIENT_CONNECTION(conn), FALSE);
	g_return_val_if_fail(G_IS_TLS_CERTIFICATE(peer_cert), FALSE);

	/* Get the certificate identity from the GTlsClientConnection */

	connectable = g_tls_client_connection_get_server_identity(
			G_TLS_CLIENT_CONNECTION(conn));

	g_return_val_if_fail(G_IS_SOCKET_CONNECTABLE(connectable), FALSE);

	/* identity is owned by the connectable */
	if (G_IS_NETWORK_ADDRESS(connectable)) {
		identity = g_network_address_get_hostname(
				G_NETWORK_ADDRESS(connectable));
	} else if (G_IS_NETWORK_SERVICE(connectable)) {
		identity = g_network_service_get_domain(
				G_NETWORK_SERVICE(connectable));
	} else {
		g_return_val_if_reached(FALSE);
	}

	/* See if a trusted certificate matching the peer certificate exists */

	trusted_cert = purple_tls_certificate_new_from_id(identity, NULL);

	if (trusted_cert != NULL &&
			g_tls_certificate_is_same(peer_cert, trusted_cert)) {
		/* It's manually trusted. Accept certificate */
		g_object_unref(trusted_cert);
		return TRUE;
	}

	/* Certificate failed and isn't trusted. Fail certificate */

	g_clear_object(&trusted_cert);
	return FALSE;
}

gpointer
purple_tls_certificate_attach_to_tls_connection(GTlsConnection *conn)
{
	return g_object_connect(conn, "signal::accept-certificate",
			accept_certificate_cb, NULL, NULL);
}

/* Called when GSocketClient signals an event.
 * Calls purple_tls_certificate_attach_to_tls_connection() on the client's
 * connection when it's about to handshake.
 */
static void
socket_client_event_cb(GSocketClient *client, GSocketClientEvent event,
		GSocketConnectable *connectable, GIOStream *connection,
		gpointer user_data)
{
	if (event == G_SOCKET_CLIENT_TLS_HANDSHAKING) {
		/* Attach libpurple's certificate subsystem to the
		 * GTlsConnection right before it starts the handshake
		 */
		purple_tls_certificate_attach_to_tls_connection(
				G_TLS_CONNECTION(connection));
	}
}

gpointer
purple_tls_certificate_attach_to_socket_client(GSocketClient *client)
{
	return g_object_connect(client, "signal::event",
			socket_client_event_cb, NULL, NULL);
}

#define DER_TYPE_CLASS(type)		(type & 0xc0)

#define DER_TYPE_CLASS_UNIVERSAL	0x00
#define DER_TYPE_CLASS_APPLICATION	0x40
#define DER_TYPE_CLASS_CONTEXT_SPECIFIC	0x80
#define DER_TYPE_CLASS_PRIVATE		0xc0

#define DER_TYPE_TAG(type) (type & 0x1f)

#define DER_TYPE_IS_CONSTRUCTED(type) ((type & 0x20) ? TRUE : FALSE)

#define DER_TYPE_TAG_IS_LONG_FORM(type) (DER_TYPE_TAG(type) == 0x1f)

#define DER_LENGTH_IS_LONG_FORM(byte) ((byte & 0x80) ? TRUE : FALSE)
#define DER_LENGTH_LONG_FORM_SIZE(byte) (byte & 0x7f)

typedef struct {
	guint8 type_class;
	gboolean constructed;
	guint type;
	GBytes *content;
	GSList *children;
} DerNodeData;

static void der_node_data_children_list_free(GSList *children);

static void
der_node_data_free(DerNodeData *node_data)
{
	g_return_if_fail(node_data != NULL);

	g_clear_pointer(&node_data->content, g_bytes_unref);
	g_clear_pointer(&node_data->children,
			der_node_data_children_list_free);

	g_free(node_data);
}

static void
der_node_data_children_list_free(GSList *children)
{
	g_return_if_fail(children != NULL);

	g_slist_free_full(children, (GDestroyNotify)der_node_data_free);
}

/* Parses DER encoded data into a GSList of DerNodeData instances */
static GSList *
der_parse(GBytes *data_bytes)
{
	const guint8 *data;
	gsize size = 0;
	gsize offset = 0;
	GSList *nodes = NULL;
	DerNodeData *node = NULL;

	data = g_bytes_get_data(data_bytes, &size);

	/* Parse data */
	while (offset < size) {
		guint8 byte;
		gsize length;

		/* Parse type */

		byte = *(data + offset++);
		node = g_new0(DerNodeData, 1);
		node->type_class = DER_TYPE_CLASS(byte);
		node->constructed = DER_TYPE_IS_CONSTRUCTED(byte);

		if (DER_TYPE_TAG_IS_LONG_FORM(byte)) {
			/* Long-form type encoding */
			/* TODO: Handle long-form encoding.
			 * Maiku: The certificates I tested didn't do this.
			 */
			g_return_val_if_reached(NULL);
		} else {
			/* Short-form type encoding */
			node->type = DER_TYPE_TAG(byte);
		}

		/* Parse content length */

		if (offset >= size) {
			purple_debug_error("tls-certificate",
					"Not enough remaining data when "
					"parsing DER chunk length byte: "
					"read (%" G_GSIZE_FORMAT ") "
					"available: ""(%" G_GSIZE_FORMAT ")",
					offset, size);
			break;
		}

		byte = *(data + offset++);

		if (DER_LENGTH_IS_LONG_FORM(byte)) {
			/* Long-form length encoding */
			guint num_len_bytes = DER_LENGTH_LONG_FORM_SIZE(byte);
			guint i;

			/* Guard against overflowing the integer */
			if (num_len_bytes > sizeof(guint)) {
				purple_debug_error("tls-certificate",
						"Number of long-form length "
						"bytes greater than guint "
						"size: %u > %" G_GSIZE_FORMAT,
						num_len_bytes, sizeof(guint));
				break;
			}

			/* Guard against reading past the end of the buffer */
			if (offset + num_len_bytes > size) {
				purple_debug_error("tls-certificate",
						"Not enough remaining data "
						"when parsing DER chunk "
						"long-form length bytes: "
						"read (%" G_GSIZE_FORMAT ") "
						"available: ""(%"
						G_GSIZE_FORMAT ")",
						offset, size);
				break;
			}

			length = 0;

			for (i = 0; i < num_len_bytes; ++i) {
				length = length << 8;
				length |= *(data + offset++);
			}
		} else {
			/* Short-form length encoding */
			length = byte;
		}
		
		/* Parse content */

		if (offset + length > size) {
			purple_debug_error("tls-certificate",
					"Not enough remaining data when "
					"parsing DER chunk content: "
					"content size (%" G_GSIZE_FORMAT ") "
					"available: ""(%" G_GSIZE_FORMAT ")",
					length, size - offset);
			break;
		}

		node->content = g_bytes_new_from_bytes(data_bytes,
				offset, length);
		offset += length;

		/* Maybe recurse */
		if (node->constructed) {
			node->children = der_parse(node->content);

			if (node->children == NULL) {
				/* No children on a constructed type
				 * should an error. If this happens, it
				 * outputs debug info inside der_parse().
				 */
				break;
			}
		}

		nodes = g_slist_append(nodes, node);
		node = NULL;
	}

	if (node != NULL) {
		/* There was an error. Free parsing data. */
		der_node_data_free(node);
		g_clear_pointer(&nodes, der_node_data_children_list_free);
		/* FIXME: Report error to calling function ala GError? */
	}

	return nodes;
}

static gchar *
der_parse_string(DerNodeData *node)
{
	const gchar *str;
	gsize length = 0;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(node->content != NULL, NULL);

	str = g_bytes_get_data(node->content, &length);
	return g_strndup(str, length);
}

typedef struct {
	gchar *oid;
	gchar *value;
} DerOIDValue;

static void
der_oid_value_free(DerOIDValue *data)
{
	g_return_if_fail(data != NULL);

	g_clear_pointer(&data->oid, g_free);
	g_clear_pointer(&data->value, g_free);

	g_free(data);
}

static void
der_oid_value_slist_free(GSList *list)
{
	g_return_if_fail(list != NULL);

	g_slist_free_full(list, (GDestroyNotify)der_oid_value_free);
}

static const gchar *
der_oid_value_slist_get_value_by_oid(GSList *list, const gchar *oid)
{
	for (; list != NULL; list = g_slist_next(list)) {
		DerOIDValue *value = list->data;

		if (!strcmp(oid, value->oid)) {
			return value->value;
		}
	}

	return NULL;
}

static gchar *
der_parse_oid(DerNodeData *node)
{
	const gchar *oid_data;
	gsize length = 0;
	gsize offset = 0;
	guint8 byte;
	GString *ret;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(node->content != NULL, NULL);

	oid_data = g_bytes_get_data(node->content, &length);
	/* Most OIDs used for certificates aren't larger than 9 bytes */
	ret = g_string_sized_new(9);

	/* First byte is encoded as num1 * 40 + num2 */
	if (length > 0) {
		byte = *(oid_data + offset++);
		g_string_append_printf(ret, "%u.%u", byte / 40, byte % 40);
	}

	/* Subsequent numbers are in base 128 format (the most
	 * significant bit being set adds another 7 bits to the number)
	 */
	while (offset < length) {
		guint value = 0;

		do {
			byte = *(oid_data + offset++);
			value = (value << 7) + (byte & 0x7f);
		} while (byte & 0x80 && offset < length);

		g_string_append_printf(ret, ".%u", value);
	}

	return g_string_free(ret, FALSE);
}

/* Parses X.509 Issuer and Subject name structures
 * into a GSList of DerOIDValue.
 */
static GSList *
der_parse_name(DerNodeData *name_node)
{
	GSList *list;
	GSList *ret = NULL;
	DerOIDValue *value;

	g_return_val_if_fail(name_node != NULL, NULL);

	/* Iterate over items in the name sequence */
	list = name_node->children;

	while (list != NULL) {
		DerNodeData *child_node;
		GSList *child_list;

		value = g_new(DerOIDValue, 1);

		/* Each item in the name sequence is a set containing
		 * a sequence of an ObjectID and a String-like value
		 */

		/* Get the DerNode containing set data */
		if ((child_node = g_slist_nth_data(list, 0)) == NULL) {
			break;
		}

		/* Get the DerNode containing its sequence data */
		if (child_node == NULL ||
				(child_node = g_slist_nth_data(
				child_node->children, 0)) == NULL) {
			break;
		}

		/* Get the GSList item containing the ObjectID DerNode  */
		if ((child_list = child_node->children) == NULL) {
			break;
		}

		/* Get the DerNode containing the ObjectID */
		if ((child_node = child_list->data) == NULL) {
			break;
		}

		/* Parse ObjectID */
		value->oid = der_parse_oid(child_node);

		/* Get the GSList item containing the String-like value */
		if ((child_list = g_slist_next(child_list)) == NULL) {
			break;
		}

		/* Get the DerNode containing the String-like value */
		if ((child_node = child_list->data) == NULL) {
			break;
		}

		/* Parse String-like value */
		value->value = der_parse_string(child_node);

		ret = g_slist_prepend(ret, value);
		list = g_slist_next(list);
		value = NULL;
	}

	if (value != NULL) {
		der_oid_value_free(value);
		der_oid_value_slist_free(ret);
	}

	return g_slist_reverse(ret);
}

static GDateTime *
der_parse_time(DerNodeData *node)
{
	gchar *time;
	gchar *c;
	gint time_parts[7];
	gint time_part_idx = 0;
	int length;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(node->content != NULL, NULL);

	time = der_parse_string(node);

	/* For the purposes of X.509
	 * UTCTime format is "YYMMDDhhmmssZ" (YY >= 50 ? 19YY : 20YY) and
	 * GeneralizedTime format is "YYYYMMDDhhmmssZ"
	 * According to RFC2459, they both are GMT, which is weird
	 * considering one is named UTC, but for the purposes of display,
	 * for which this is used, it shouldn't matter.
	 */

	length = strlen(time);
	if (length == 13) {
		/* UTCTime: Skip the first part as it's calculated later */
		time_part_idx = 1;
	} else if (length == 15) {
		/* Generalized Time */
		/* TODO: Handle generalized time
		 * Maiku: None of the certificates I tested used this
		 */
		g_free(time);
		g_return_val_if_reached(NULL);
	} else {
		purple_debug_error("tls-certificate",
				"Unrecognized time format (length: %i)",
				length);
		g_free(time);
		return NULL;
	}

	c = time;

	while (c - time < length) {
		if (*c == 'Z') {
			break;
		}

		if (!g_ascii_isdigit(*c) || !g_ascii_isdigit(*(c + 1))) {
			purple_debug_error("tls-certificate",
					"Error parsing time. next characters "
					"aren't both digits: '%c%c'",
					*c, *(c + 1));
			break;
		}

		time_parts[time_part_idx++] =
				g_ascii_digit_value(*c) * 10 +
				g_ascii_digit_value(*(c + 1));
		c += 2;
	}

	if (length == 13) {
		if (time_parts[1] >= 50) {
			time_parts[0] = 19;
		} else {
			time_parts[0] = 20;
		}
	}

	return g_date_time_new_utc(
			time_parts[0] * 100 + time_parts[1],	/* year */
			time_parts[2],				/* month */
			time_parts[3],				/* day */
			time_parts[4],				/* hour */	
			time_parts[5],				/* minute */
			time_parts[6]);				/* seconds */
}

/* This structure contains the data which is in an X.509 certificate.
 * Only the values actually parsed/used are here. The remaining commented
 * out values are informative placeholders for the remaining data that
 * could be in a standard certificate.
 */
struct _PurpleTlsCertificateInfo {
	GTlsCertificate *cert;

	/* version (Optional, defaults to version 1 (version = value + 1)) */
	/* serialNumber */
	/* signature */
	GSList *issuer;
	GDateTime *notBefore;
	GDateTime *notAfter;
	GSList *subject;
	/* subjectPublicKeyInfo */
	/* issuerUniqueIdentifier (Optional, requires version 2 or 3) */
	/* subjectUniqueIdentifier (Optional, requires version 2 or 3) */
	/* extensions (Optional, requires version 3) */
};

/* TODO: Make better API for this? */
PurpleTlsCertificateInfo *
purple_tls_certificate_get_info(GTlsCertificate *certificate)
{
	GByteArray *der_array = NULL;
	GBytes *root;
	GSList *nodes;
	DerNodeData *node;
	DerNodeData *cert_node;
	DerNodeData *valid_node;
	PurpleTlsCertificateInfo *info;

	g_return_val_if_fail(G_IS_TLS_CERTIFICATE(certificate), NULL);

	/* Get raw bytes from DER formatted certificate */
	g_object_get(certificate, "certificate", &der_array, NULL);

	/* Parse raw bytes into DerNode tree */
	root = g_byte_array_free_to_bytes(der_array);
	nodes = der_parse(root);
	g_bytes_unref(root);

	if (nodes == NULL) {
		purple_debug_warning("tls-certificate",
				"Error parsing certificate");
		return NULL;
	}

	/* Set up PurpleTlsCertificateInfo struct with initial data */
	info = g_new0(PurpleTlsCertificateInfo, 1);
	info->cert = g_object_ref(certificate);

	/* Get certificate root sequence GSList item */
	node = g_slist_nth_data(nodes, 0);
	if (node == NULL || node->children == NULL) {
		purple_debug_warning("tls-certificate",
				"Error parsing certificate root node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	/* Get certificate sequence GSList DerNode */
	cert_node = g_slist_nth_data(node->children, 0);
	if (cert_node == NULL || cert_node->children == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	/* Check for optional certificate version */

	node = g_slist_nth_data(cert_node->children, 0);
	if (node == NULL || node->children == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate version node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	if (node->type_class != DER_TYPE_CLASS_CONTEXT_SPECIFIC) {
		/* Include optional version so indices work right */
		/* TODO: Actually set default version value? */
		cert_node->children =
				g_slist_prepend(cert_node->children, NULL);
	}

	/* Get certificate issuer */

	node = g_slist_nth_data(cert_node->children, 3);
	if (node == NULL || node->children == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate issuer node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	info->issuer = der_parse_name(node);

	/* Get certificate validity */

	valid_node = g_slist_nth_data(cert_node->children, 4);
	if (valid_node == NULL || valid_node->children == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate validity node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	/* Get certificate validity (notBefore) */
	node = g_slist_nth_data(valid_node->children, 0);
	if (node == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate valid "
				"notBefore node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	info->notBefore = der_parse_time(node);

	/* Get certificate validity (notAfter) */
	node = g_slist_nth_data(valid_node->children, 1);
	if (node == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate valid "
				"notAfter node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	info->notAfter = der_parse_time(node);

	/* Get certificate subject */

	node = g_slist_nth_data(cert_node->children, 5);
	if (node == NULL || node->children == NULL) {
		purple_debug_warning("tls-certificate",
				"Error to parsing certificate subject node");
		purple_tls_certificate_info_free(info);
		return NULL;
	}

	info->subject = der_parse_name(node);

	/* Clean up */
	der_node_data_children_list_free(nodes);

	return info;
}

void
purple_tls_certificate_info_free(PurpleTlsCertificateInfo *info)
{
	g_return_if_fail(info != NULL);

	g_clear_object(&info->cert);

	g_clear_pointer(&info->issuer, der_oid_value_slist_free);
	g_clear_pointer(&info->notBefore, g_date_time_unref);
	g_clear_pointer(&info->notAfter, g_date_time_unref);
	g_clear_pointer(&info->subject, der_oid_value_slist_free);

	g_free(info);
}

/* Looks up the relative distinguished name (RDN) from an ObjectID */
static const gchar *
lookup_rdn_name_by_oid(const gchar *oid)
{
	static GHashTable *ht = NULL;

	if (G_UNLIKELY(ht == NULL)) {
		ht = g_hash_table_new_full(g_str_hash, g_str_equal,
				NULL, NULL);

		/* commonName */
		g_hash_table_insert(ht, "2.5.4.3", "CN");
		/* countryName */
		g_hash_table_insert(ht, "2.5.4.6", "C");
		/* localityName */
		g_hash_table_insert(ht, "2.5.4.7", "L");
		/* stateOrProvinceName */
		g_hash_table_insert(ht, "2.5.4.8", "ST");
		/* organizationName */
		g_hash_table_insert(ht, "2.5.4.10", "O");
		/* organizationalUnitName */
		g_hash_table_insert(ht, "2.5.4.11", "OU");
	}

	return g_hash_table_lookup(ht, oid);
}

/* Makes a distinguished name (DN) from
 * a list of relative distinguished names (RDN).
 * Order matters.
 */
static gchar *
make_dn_from_oid_value_slist(GSList *list)
{
	GString *str = g_string_new(NULL);

	for (; list != NULL; list = g_slist_next(list)) {
		DerOIDValue *value = list->data;
		const gchar *name;
		gchar *new_value;

		if (value == NULL) {
			purple_debug_error("tls-certificate",
					"DerOIDValue data missing from GSList");
			continue;
		}

		name = lookup_rdn_name_by_oid(value->oid);
		/* Escape commas in value as that's the DN separator */
		new_value = purple_strreplace(value->value, ",", "\\,");
		g_string_append_printf(str, "%s=%s,", name, new_value);
		g_free(new_value);
	}

	/* Remove trailing comma */
	g_string_truncate(str, str->len - 1);

	return g_string_free(str, FALSE);
}

static gchar *
purple_tls_certificate_info_get_issuer_dn(PurpleTlsCertificateInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);
	g_return_val_if_fail(info->issuer != NULL, NULL);

	return make_dn_from_oid_value_slist(info->issuer);
}

gchar *
purple_tls_certificate_info_get_display_string(PurpleTlsCertificateInfo *info)
{
	gchar *subject_name;
	gchar *issuer_name = NULL;
	GByteArray *sha1_bytes;
	gchar *sha1_str = NULL;
	gchar *activation_time;
	gchar *expiration_time;
	gchar *ret;

	g_return_val_if_fail(info != NULL, NULL);

	/* Getting the commonName of a CA supposedly doesn't work, but we
	 * shouldn't be dealing with those here anyway.
	 */
	subject_name = purple_tls_certificate_info_get_subject_name(info);

	issuer_name = purple_tls_certificate_info_get_issuer_dn(info);

	sha1_bytes = purple_tls_certificate_get_fingerprint_sha1(info->cert);
	if (sha1_bytes != NULL) {
		sha1_str = purple_base16_encode_chunked(sha1_bytes->data,
				sha1_bytes->len);
		g_byte_array_unref(sha1_bytes);
	}

	activation_time = g_date_time_format(info->notBefore, "%c");
	expiration_time = g_date_time_format(info->notAfter, "%c");

	ret = g_strdup_printf(
			_("Common name: %s\n\n"
			  "Issued by: %s\n\n"
			  "Fingerprint (SHA1): %s\n\n"
			  "Activation date: %s\n"
			  "Expiriation date: %s\n"),
			subject_name,
			issuer_name,
			sha1_str,
			activation_time,
			expiration_time);

	g_free(subject_name);
	g_free(issuer_name);
	g_free(sha1_str);
	g_free(activation_time);
	g_free(expiration_time);

	return ret;
}

/* TODO: Make better API for this? */
gchar *
purple_tls_certificate_info_get_subject_name(PurpleTlsCertificateInfo *info)
{
	g_return_val_if_fail(info != NULL, NULL);
	g_return_val_if_fail(info->subject != NULL, NULL);

	/* commonName component of the subject */
	return g_strdup(der_oid_value_slist_get_value_by_oid(info->subject,
			"2.5.4.3"));
}

/* TODO: Make better API for this? */
GByteArray *
purple_tls_certificate_get_fingerprint_sha1(GTlsCertificate *certificate)
{
	PurpleHash *hash;
	GByteArray *der = NULL;
	guint8 *data = NULL;
	gsize buf_size = 0;

	g_return_val_if_fail(G_IS_TLS_CERTIFICATE(certificate), NULL);

	g_object_get(certificate, "certificate", &der, NULL);

	g_return_val_if_fail(der != NULL, NULL);

	hash = purple_sha1_hash_new();

	buf_size = purple_hash_get_digest_size(hash);
	data = g_malloc(buf_size);

	purple_hash_append(hash, der->data, der->len);
	g_byte_array_unref(der);

	purple_hash_digest(hash, data, buf_size);
	g_object_unref(hash);

	return g_byte_array_new_take(data, buf_size);
}

