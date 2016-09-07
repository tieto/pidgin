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

#ifndef _PURPLE_TLS_CERTIFICATE_INFO_H
#define _PURPLE_TLS_CERTIFICATE_INFO_H
/**
 * SECTION:tls-certificate-info
 * @section_id: libpurple-tls-certificate-info
 * @short_description: TLS certificate information parsing
 * @title: TLS Certificate Info API
 *
 * The TLS certificate info API provides information parsing functions
 * for #GTlsCertificate's. This information can then be presented to the
 * user, usually to help them decide whether or not to trust a given
 * certificate.
 */

#include <gio/gio.h>

/**
 * PurpleTlsCertificateInfo
 *
 * An opaque structure to contain parsed certificate info, which
 * can subsequently be accessed by purple_tls_certificate_info_*
 * functions.
 */
typedef struct _PurpleTlsCertificateInfo PurpleTlsCertificateInfo;

#define PURPLE_TYPE_TLS_CERTIFICATE_INFO \
		(purple_tls_certificate_info_get_type())

/**
 * purple_tls_certificate_info_get_type:
 *
 * Returns: The #GType for the #PurpleTlsCertificateInfo boxed structure.
 */
GType purple_tls_certificate_info_get_type(void);

/**
 * purple_tls_certificate_get_info:
 * @certificate: Certificate from which to parse the info
 *
 * Returns a #PurpleTlsCertificateInfo containing parsed information
 * of the certificate.
 *
 * Returns: #PurpleTlsCertificateInfo parsed from the certificate
 */
PurpleTlsCertificateInfo *
purple_tls_certificate_get_info(GTlsCertificate *certificate);

/**
 * purple_tls_certificate_info_free:
 * @info: #PurpleTlsCertificateInfo object to free
 *
 * Frees @info.
 */
void
purple_tls_certificate_info_free(PurpleTlsCertificateInfo *info);

/**
 * purple_tls_certificate_info_get_display_string:
 * @info: #PurpleTlsCertificateInfo from which to generate a display string
 *
 * Generates a user readable string to display information from @info
 *
 * Returns: A user readable string suitable to display to the user
 */
gchar *
purple_tls_certificate_info_get_display_string(PurpleTlsCertificateInfo *info);

/**
 * purple_tls_certificate_get_subject_name:
 * @certificate: Certificate from which to get the subject name
 *
 * Returns the common subject name of the cert
 *
 * Returns: The subject name of the cert
 */
gchar *
purple_tls_certificate_info_get_subject_name(PurpleTlsCertificateInfo *info);

/**
 * purple_tls_certificate_get_fingerprint_sha1:
 * @certificate: Certificate from which to get the SHA1 fingerprint
 *
 * Returns the SHA1 fingerprint of the cert
 *
 * Returns: (transfer full): The SHA1 fingerprint of the cert
 */
GByteArray *
purple_tls_certificate_get_fingerprint_sha1(GTlsCertificate *certificate);

G_END_DECLS

#endif /* _PURPLE_TLS_CERTIFICATE_INFO_H */
