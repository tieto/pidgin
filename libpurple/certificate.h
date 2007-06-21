/**
 * @file certificate.h Public-Key Certificate API
 * @ingroup core
 */

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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PURPLE_CERTIFICATE_H
#define _PURPLE_CERTIFICATE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


typedef enum
{
	PURPLE_CERTIFICATE_INVALID = 0,
	PURPLE_CERTIFICATE_VALID = 1
} PurpleCertificateVerificationStatus;

typedef struct _PurpleCertificate PurpleCertificate;
typedef struct _PurpleCertificateScheme PurpleCertificateScheme;
typedef struct _PurpleCertificateVerifier PurpleCertificateVerifier;
typedef struct _PurpleCertificateVerificationRequest PurpleCertificateVerificationRequest;

/** A certificate instance
 *
 *  An opaque data structure representing a single certificate under some
 *  CertificateScheme
 */
struct _PurpleCertificate
{
	/** Scheme this certificate is under */
	PurpleCertificateScheme * scheme;
	/** Opaque pointer to internal data */
	gpointer data;
};

/** A certificate type
 *
 *  A CertificateScheme must implement all of the fields in the structure,
 *  and register it using TODO:purple_register_certscheme()
 *
 *  There may be only ONE CertificateScheme provided for each certificate
 *  type, as specified by the "name" field.
 */
struct _PurpleCertificateScheme
{
	/** Name of the certificate type
	 *  ex: "x509", "pgp", etc.
	 *  This must be globally unique - you may not register more than one
	 *  CertificateScheme of the same name at a time.
	 */
	gchar * name;

	/** User-friendly name for this type
	 *  ex: N_("X.509 Certificates")
	 *  When this is displayed anywhere, it should be i18ned
	 *  ex: _(scheme->name)
	 */
	gchar * fullname;

	/** Imports a certificate from a file
	 *
	 *  @param filename   File to import the certificate from
	 *  @return           Pointer to the newly allocated Certificate struct
	 *                    or NULL on failure.
	 */
	PurpleCertificate * (* import_certificate)(const gchar * filename);

	/** Destroys and frees a Certificate structure
	 *
	 *  Destroys a Certificate's internal data structures and calls
	 *  free(crt)
	 *
	 *  @param crt  Certificate instance to be destroyed. It WILL NOT be
	 *              destroyed if it is not of the correct
	 *              CertificateScheme. Can be NULL
	 */
	void (* destroy_certificate)(PurpleCertificate * crt);
	
	/* TODO: Fill out this structure */
};

/** A set of operations used to provide logic for verifying a Certificate's
 *  authenticity.
 */
struct _PurpleCertificateVerifier
{	
	/** Scheme this Verifier operates on */
	PurpleCertificateScheme *scheme;

	/** Internal name used for lookups
	 *
	 * Case insensitive
	 */
	gchar * name;
};

/** Structure for a single certificate request
 *
 *  Useful for keeping track of the state of a verification that involves
 *  several steps
 */
struct _PurpleCertificateVerificationRequest
{
	/** Reference to the verification logic used */
	PurpleCertificateVerifier *verifier;

	/** List of certificates in the chain to be verified.
	 *
	 * This is most relevant for X.509 certificates used in SSL sessions.
	 */
	GList *cert_chain;
	
	/** Internal data used by the Verifier code */
	gpointer *data;
};

/*****************************************************************************/
/** @name PurpleCertificate Subsystem API                                    */
/*****************************************************************************/
/*@{*/

/** Look up a registered CertificateScheme by name
 * @param name   The scheme name. Case insensitive.
 * @return Pointer to the located Scheme, or NULL if it isn't found.
 */
PurpleCertificateScheme *
purple_certificate_find_scheme(const gchar *name);

/** Register a CertificateScheme with libpurple
 *
 * No two schemes can be registered with the same name; this function enforces
 * that.
 *
 * @param scheme  Pointer to the scheme to register.
 * @return TRUE if the scheme was successfully added, otherwise FALSE
 */
gboolean
purple_certificate_register_scheme(PurpleCertificateScheme *scheme);

/** Unregister a CertificateScheme from libpurple
 *
 * @param scheme    Scheme to unregister.
 *                  If the scheme is not registered, this is a no-op.
 *
 * @return TRUE if the unregister completed successfully
 */
gboolean
purple_certificate_unregister_scheme(PurpleCertificateScheme *scheme);

/* TODO: ADD STUFF HERE */

/*@}*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PURPLE_CERTIFICATE_H */
