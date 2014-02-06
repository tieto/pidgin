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
/**
 * SECTION:certificate
 * @section_id: libpurple-certificate
 * @short_description: <filename>certificate.h</filename>
 * @title: Public-Key Certificate API
 * @see_also: <link linkend="chapter-signals-certificate">Certificate signals</link>
 */

#ifndef _PURPLE_CERTIFICATE_H
#define _PURPLE_CERTIFICATE_H

#include <time.h>

#include <glib.h>

/**
 * PurpleCertificateVerificationStatus:
 * @PURPLE_CERTIFICATE_UNKNOWN_ERROR: Unknown error.
 * @PURPLE_CERTIFICATE_VALID: Not an error.
 * @PURPLE_CERTIFICATE_NON_FATALS_MASK: Non-fatal.
 * @PURPLE_CERTIFICATE_SELF_SIGNED: The certificate is self-signed.
 * @PURPLE_CERTIFICATE_CA_UNKNOWN: The CA is not in libpurple's pool of
 *                     certificates.
 * @PURPLE_CERTIFICATE_NOT_ACTIVATED: The current time is before the
 *                     certificate's specified activation time.
 * @PURPLE_CERTIFICATE_EXPIRED: The current time is after the certificate's
 *                     specified expiration time.
 * @PURPLE_CERTIFICATE_NAME_MISMATCH: The certificate's subject name doesn't
 *                     match the expected.
 * @PURPLE_CERTIFICATE_NO_CA_POOL: No CA pool was found. This shouldn't happen.
 * @PURPLE_CERTIFICATE_FATALS_MASK: Fatal
 * @PURPLE_CERTIFICATE_INVALID_CHAIN: The signature chain could not be
 *                     validated. Due to limitations in the the current API,
 *                     this also indicates one of the CA certificates in the
 *                     chain is expired (or not yet activated).
 * @PURPLE_CERTIFICATE_REVOKED: The signature has been revoked.
 * @PURPLE_CERTIFICATE_REJECTED: The certificate was rejected by the user.
 */
/* FIXME 3.0.0 PURPLE_CERTIFICATE_INVALID_CHAIN -- see description */
typedef enum
{
	PURPLE_CERTIFICATE_UNKNOWN_ERROR = -1,
	PURPLE_CERTIFICATE_VALID = 0,
	PURPLE_CERTIFICATE_NON_FATALS_MASK = 0x0000FFFF,
	PURPLE_CERTIFICATE_SELF_SIGNED = 0x01,
	PURPLE_CERTIFICATE_CA_UNKNOWN = 0x02,
	PURPLE_CERTIFICATE_NOT_ACTIVATED = 0x04,
	PURPLE_CERTIFICATE_EXPIRED = 0x08,
	PURPLE_CERTIFICATE_NAME_MISMATCH = 0x10,
	PURPLE_CERTIFICATE_NO_CA_POOL = 0x20,
	PURPLE_CERTIFICATE_FATALS_MASK = 0xFFFF0000,
	PURPLE_CERTIFICATE_INVALID_CHAIN = 0x10000,
	PURPLE_CERTIFICATE_REVOKED = 0x20000,
	PURPLE_CERTIFICATE_REJECTED = 0x40000,

	/*< private >*/
	PURPLE_CERTIFICATE_LAST = 0x80000,
} PurpleCertificateVerificationStatus;

#define PURPLE_TYPE_CERTIFICATE   (purple_certificate_get_type())
typedef struct _PurpleCertificate PurpleCertificate;

#define PURPLE_TYPE_CERTIFICATE_POOL  (purple_certificate_pool_get_type())
typedef struct _PurpleCertificatePool PurpleCertificatePool;

typedef struct _PurpleCertificateScheme PurpleCertificateScheme;
typedef struct _PurpleCertificateVerifier PurpleCertificateVerifier;
typedef struct _PurpleCertificateVerificationRequest PurpleCertificateVerificationRequest;

/**
 * PurpleCertificateVerifiedCallback:
 * @st:       Status code
 * @userdata: User-defined data
 *
 * Callback function for the results of a verification check
 */
typedef void (*PurpleCertificateVerifiedCallback)(PurpleCertificateVerificationStatus st,
		gpointer userdata);

/**
 * PurpleCertificate:
 * @scheme: Scheme this certificate is under
 * @data:   Opaque pointer to internal data
 *
 * A certificate instance
 *
 * An opaque data structure representing a single certificate under some
 * CertificateScheme
 */
struct _PurpleCertificate
{
	PurpleCertificateScheme * scheme;
	gpointer data;
};

/**
 * PurpleCertificatePool:
 * @scheme_name:  Scheme this Pool operates for
 * @name:         Internal name to refer to the pool by
 * @fullname:     User-friendly name for this type. ex: N_("SSL Servers"). When
 *                this is displayed anywhere, it should be i18ned.
 *                ex: _(pool->fullname)
 * @data:         Internal pool data
 * @init:         Set up the Pool's internal state
 *                <sbr/>Upon calling purple_certificate_register_pool() , this
 *                function will be called. May be %NULL.
 *                <sbr/>Returns: %TRUE if the initialization succeeded,
 *                               otherwise %FALSE.
 * @uninit:       Uninit the Pool's internal state. Will be called by
 *                purple_certificate_unregister_pool(). May be %NULL.
 * @cert_in_pool: Check for presence of a certificate in the pool using unique
 *                ID
 * @get_cert:     Retrieve a PurpleCertificate from the pool
 * @put_cert:     Add a certificate to the pool. Must overwrite any other
 *                certificates sharing the same ID in the pool.
 *                <sbr/>Returns: %TRUE if the operation succeeded, otherwise
 *                               %FALSE.
 * @delete_cert:  Delete a certificate from the pool
 * @get_idlist:   Returns a list of IDs stored in the pool
 *
 * Database for retrieval or storage of Certificates
 *
 * More or less a hash table; all lookups and writes are controlled by a string
 * key.
 */
struct _PurpleCertificatePool
{
	gchar *scheme_name;
	gchar *name;
	gchar *fullname;
	gpointer data;

	gboolean (* init)(void);
	void (* uninit)(void);

	gboolean (* cert_in_pool)(const gchar *id);
	PurpleCertificate * (* get_cert)(const gchar *id);
	gboolean (* put_cert)(const gchar *id, PurpleCertificate *crt);
	gboolean (* delete_cert)(const gchar *id);

	GList * (* get_idlist)(void);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleCertificateScheme:
 * @name: Name of the certificate type. ex: "x509", "pgp", etc.
 *        <sbr/> This must be globally unique - you may not register more than
 *        one CertificateScheme of the same name at a time.
 * @fullname: User-friendly name for this type. ex: N_("X.509 Certificates")
 *            <sbr/> When this is displayed anywhere, it should be i18ned. ex:
 *            _(scheme->fullname)
 * @import_certificate: Imports a certificate from a file
 *                      <sbr/> @filename: File to import the certificate from
 *                      <sbr/> Returns: Pointer to the newly allocated
 *                      Certificate struct or NULL on failure.
 * @export_certificate: Exports a certificate to a file.
 *                      <sbr/>See purple_certificate_export().
 *                      <sbr/>@filename: File to export the certificate to
 *                      <sbr/>@crt:      Certificate to export
 *                      <sbr/>Returns:   %TRUE if the export succeeded,
 *                                       otherwise %FALSE
 * @copy_certificate: Duplicates a certificate
 *                    <sbr/>Certificates are generally assumed to be read-only,
 *                    so feel free to do any sort of reference-counting magic
 *                    you want here. If this ever changes, please remember to
 *                    change the magic accordingly.
 *                    <sbr/>Returns: Reference to the new copy
 * @destroy_certificate: Destroys and frees a Certificate structure
 *                       <sbr/> Destroys a Certificate's internal data
 *                       structures and calls free(@crt)
 *                       <sbr/> @crt:  Certificate instance to be destroyed.
 *                       It <emphasis>WILL NOT</emphasis> be destroyed if it is
 *                       not of the correct CertificateScheme. Can be %NULL.
 * @signed_by: Find whether "crt" has a valid signature from issuer "issuer".
 *             <sbr/>See purple_certificate_signed_by().
 * @get_fingerprint_sha1: Retrieves the certificate public key fingerprint using
 *                        SHA1
 *                        <sbr/>@crt:    Certificate instance
 *                        <sbr/>Returns: Binary representation of SHA1 hash -
 *                                       must be freed using g_byte_array_free().
 * @get_unique_id: Retrieves a unique certificate identifier
 *                 <sbr/>@crt:    Certificate instance
 *                 <sbr/>Returns: Newly allocated string that can be used to
 *                                uniquely identify the certificate.
 * @get_issuer_unique_id: Retrieves a unique identifier for the certificate's
 *                        issuer
 *                        <sbr/>@crt:    Certificate instance
 *                        <sbr/>Returns: Newly allocated string that can be used
 *                                       to uniquely identify the issuer's
 *                                       certificate.
 * @get_subject_name: Gets the certificate subject's name
 *                    <sbr/>For X.509, this is the "Common Name" field, as we're
 *                    only using it for hostname verification at the moment.
 *                    <sbr/>See purple_certificate_get_subject_name().
 *                    <sbr/>@crt:    Certificate instance
 *                    <sbr/>Returns: Newly allocated string with the certificate
 *                                   subject.
 * @check_subject_name: Check the subject name against that on the certificate
 *                      <sbr/>See purple_certificate_check_subject_name().
 *                      <sbr/>Returns: %TRUE if it is a match, else %FALSE
 * @get_times: Retrieve the certificate activation/expiration times
 * @import_certificates: Imports certificates from a file
 *                       <sbr/> @filename: File to import the certificates from
 *                       <sbr/> Returns:   #GSList of pointers to the newly
 *                                         allocated Certificate structs or
 *                                         %NULL on failure.
 * @get_der_data: Retrieves the certificate data in DER form
 *                <sbr/>@crt:    Certificate instance
 *                <sbr/>Returns: Binary DER representation of certificate - must
 *                               be freed using g_byte_array_free().
 * @get_display_string: Retrieves a string representation of the certificate
 *                      suitable for display
 *                      <sbr/>@crt:   Certificate instance
 *                      <sbr/>Returns: User-displayable string representation of
 *                                     certificate - must be freed using
 *                                     g_free().
 *
 * A certificate type.
 *
 * A CertificateScheme must implement all of the fields in the structure,
 * and register it using purple_certificate_register_scheme().
 *
 * There may be only <emphasis>ONE</emphasis> CertificateScheme provided for
 * each certificate type, as specified by the "name" field.
 */
struct _PurpleCertificateScheme
{
	gchar * name;
	gchar * fullname;

	PurpleCertificate * (* import_certificate)(const gchar * filename);
	gboolean (* export_certificate)(const gchar *filename, PurpleCertificate *crt);

	PurpleCertificate * (* copy_certificate)(PurpleCertificate *crt);
	void (* destroy_certificate)(PurpleCertificate * crt);

	gboolean (*signed_by)(PurpleCertificate *crt, PurpleCertificate *issuer);
	GByteArray * (* get_fingerprint_sha1)(PurpleCertificate *crt);
	gchar * (* get_unique_id)(PurpleCertificate *crt);
	gchar * (* get_issuer_unique_id)(PurpleCertificate *crt);

	gchar * (* get_subject_name)(PurpleCertificate *crt);
	gboolean (* check_subject_name)(PurpleCertificate *crt, const gchar *name);

	gboolean (* get_times)(PurpleCertificate *crt, gint64 *activation, gint64 *expiration);

	GSList * (* import_certificates)(const gchar * filename);
	GByteArray * (* get_der_data)(PurpleCertificate *crt);

	gchar * (* get_display_string)(PurpleCertificate *crt);

	/*< private >*/
	void (*_purple_reserved1)(void);
};

/**
 * PurpleCertificateVerifier:
 * @scheme_name:        Name of the scheme this Verifier operates on. The scheme
 *                      will be looked up by name when a Request is generated
 *                      using this Verifier.
 * @name:               Name of the Verifier - case insensitive.
 * @start_verification: Start the verification process. To be called from
 *                      purple_certificate_verify() once it has constructed the
 *                      request. This will use the information in the given
 *                      VerificationRequest to check the certificate and
 *                      callback the requester with the verification results.
 *                      <sbr/>@vrq: The request to process.
 * @destroy_request:    Destroy a completed Request under this Verifier. The
 *                      function pointed to here is only responsible for
 *                      cleaning up whatever
 *                      #PurpleCertificateVerificationRequest.data points to.
 *                      It should not call free(@vrq).
 *                      <sbr/>@vrq: The request to destroy.
 *
 * A set of operations used to provide logic for verifying a Certificate's
 * authenticity.
 *
 * A Verifier provider must fill out these fields, then register it using
 * purple_certificate_register_verifier().
 *
 * The (scheme_name, name) value must be unique for each Verifier - you may not
 * register more than one Verifier of the same name for each Scheme.
 */
struct _PurpleCertificateVerifier
{
	gchar *scheme_name;
	gchar *name;

	void (* start_verification)(PurpleCertificateVerificationRequest *vrq);
	void (* destroy_request)(PurpleCertificateVerificationRequest *vrq);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleCertificateVerificationRequest:
 * @verifier:     Reference to the verification logic used.
 * @scheme:       Reference to the scheme used. This is looked up from the
 *                Verifier when the Request is generated.
 * @subject_name: Name to check that the certificate is issued to. For X.509
 *                certificates, this is the Common Name.
 * @cert_chain:   List of certificates in the chain to be verified (such as that
 *                returned by purple_ssl_get_peer_certificates()). This is most
 *                relevant for X.509 certificates used in SSL sessions. The list
 *                order should be: certificate, issuer, issuer's issuer, etc.
 * @data:         Internal data used by the Verifier code.
 * @cb:           Function to call with the verification result.
 * @cb_data:      Data to pass to the post-verification callback.
 *
 * Structure for a single certificate request
 *
 * Useful for keeping track of the state of a verification that involves
 * several steps
 */
struct _PurpleCertificateVerificationRequest
{
	PurpleCertificateVerifier *verifier;
	PurpleCertificateScheme *scheme;
	gchar *subject_name;
	GList *cert_chain;
	gpointer data;
	PurpleCertificateVerifiedCallback cb;
	gpointer cb_data;
};

G_BEGIN_DECLS

/*****************************************************************************/
/** @name Certificate Verification Functions                                 */
/*****************************************************************************/
/*@{*/

/**
 * purple_certificate_verify:
 * @verifier:      Verification logic to use.
 *                      See purple_certificate_find_verifier().
 * @subject_name:  Name that should match the first certificate in the
 *                      chain for the certificate to be valid. Will be strdup'd
 *                      into the Request struct
 * @cert_chain:    Certificate chain to check. If there is more than one
 *                      certificate in the chain (X.509), the peer's
 *                      certificate comes first, then the issuer/signer's
 *                      certificate, etc. The whole list is duplicated into the
 *                      Request struct.
 * @cb:            Callback function to be called with whether the
 *                      certificate was approved or not.
 * @cb_data:       User-defined data for the above.
 *
 * Constructs a verification request and passed control to the specified Verifier
 *
 * It is possible that the callback will be called immediately upon calling
 * this function. Plan accordingly.
 */
void
purple_certificate_verify (PurpleCertificateVerifier *verifier,
			   const gchar *subject_name, GList *cert_chain,
			   PurpleCertificateVerifiedCallback cb,
			   gpointer cb_data);

/**
 * purple_certificate_verify_complete:
 * @vrq:           Request to conclude
 * @st:            Success/failure code to pass to the request's
 *                      completion callback.
 *
 * Completes and destroys a VerificationRequest
 */
void
purple_certificate_verify_complete(PurpleCertificateVerificationRequest *vrq,
				   PurpleCertificateVerificationStatus st);

/*@}*/

/*****************************************************************************/
/** @name Certificate Functions                                              */
/*****************************************************************************/
/*@{*/

/**
 * purple_certificate_get_type:
 *
 * Returns: The #GType for the #PurpleCertificate boxed structure.
 */
GType purple_certificate_get_type(void);

/**
 * purple_certificate_copy:
 * @crt:        Instance to duplicate
 *
 * Makes a duplicate of a certificate
 *
 * Returns: Pointer to new instance
 */
PurpleCertificate *
purple_certificate_copy(PurpleCertificate *crt);

/**
 * purple_certificate_copy_list:
 * @crt_list:   List to duplicate
 *
 * Duplicates an entire list of certificates
 *
 * Returns: New list copy
 */
GList *
purple_certificate_copy_list(GList *crt_list);

/**
 * purple_certificate_destroy:
 * @crt:        Instance to destroy. May be NULL.
 *
 * Destroys and free()'s a Certificate
 */
void
purple_certificate_destroy (PurpleCertificate *crt);

/**
 * purple_certificate_destroy_list:
 * @crt_list:   List of certificates to destroy. May be NULL.
 *
 * Destroy an entire list of Certificate instances and the containing list
 */
void
purple_certificate_destroy_list (GList * crt_list);

/**
 * purple_certificate_signed_by:
 * @crt:        Certificate instance to check signature of
 * @issuer:     Certificate thought to have signed 'crt'
 *
 * Check whether 'crt' has a valid signature made by 'issuer'
 *
 * Returns: TRUE if 'crt' has a valid signature made by 'issuer',
 *         otherwise FALSE
 * @todo Find a way to give the reason (bad signature, not the issuer, etc.)
 */
gboolean
purple_certificate_signed_by(PurpleCertificate *crt, PurpleCertificate *issuer);

/**
 * purple_certificate_check_signature_chain:
 * @chain:      List of PurpleCertificate instances comprising the chain,
 *                   in the order certificate, issuer, issuer's issuer, etc.
 * @failing:    A pointer to a PurpleCertificate*. If not NULL, if the
 *                   chain fails to validate, this will be set to the
 *                   certificate whose signature could not be validated.
 *
 * Check that a certificate chain is valid and, if not, the failing certificate.
 *
 * Uses purple_certificate_signed_by() to verify that each PurpleCertificate
 * in the chain carries a valid signature from the next. A single-certificate
 * chain is considered to be valid.
 *
 * Returns: TRUE if the chain is valid. See description.
 */
gboolean
purple_certificate_check_signature_chain(GList *chain,
		PurpleCertificate **failing);

/**
 * purple_certificate_import:
 * @scheme:      Scheme to import under
 * @filename:    File path to import from
 *
 * Imports a PurpleCertificate from a file
 *
 * Returns: Pointer to a new PurpleCertificate, or NULL on failure
 */
PurpleCertificate *
purple_certificate_import(PurpleCertificateScheme *scheme, const gchar *filename);

/**
 * purple_certificates_import:
 * @scheme:      Scheme to import under
 * @filename:    File path to import from
 *
 * Imports a list of PurpleCertificates from a file
 *
 * Returns: Pointer to a GSList of new PurpleCertificates, or NULL on failure
 */
GSList *
purple_certificates_import(PurpleCertificateScheme *scheme, const gchar *filename);

/**
 * purple_certificate_export:
 * @filename:    File to export the certificate to
 * @crt:         Certificate to export
 *
 * Exports a PurpleCertificate to a file
 *
 * Returns: TRUE if the export succeeded, otherwise FALSE
 */
gboolean
purple_certificate_export(const gchar *filename, PurpleCertificate *crt);


/**
 * purple_certificate_get_fingerprint_sha1:
 * @crt:        Certificate instance
 *
 * Retrieves the certificate public key fingerprint using SHA1.
 * See purple_base16_encode_chunked().
 *
 * Returns: Binary representation of the hash. You are responsible for free()ing
 *         this.
 */
GByteArray *
purple_certificate_get_fingerprint_sha1(PurpleCertificate *crt);

/**
 * purple_certificate_get_unique_id:
 * @crt:        Certificate instance
 *
 * Get a unique identifier for the certificate
 *
 * Returns: String representing the certificate uniquely. Must be g_free()'ed
 */
gchar *
purple_certificate_get_unique_id(PurpleCertificate *crt);

/**
 * purple_certificate_get_issuer_unique_id:
 * @crt:        Certificate instance
 *
 * Get a unique identifier for the certificate's issuer
 *
 * Returns: String representing the certificate's issuer uniquely. Must be
 *         g_free()'ed
 */
gchar *
purple_certificate_get_issuer_unique_id(PurpleCertificate *crt);

/**
 * purple_certificate_get_subject_name:
 * @crt:   Certificate instance
 *
 * Gets the certificate subject's name
 *
 * For X.509, this is the "Common Name" field, as we're only using it
 * for hostname verification at the moment
 *
 * Returns: Newly allocated string with the certificate subject.
 */
gchar *
purple_certificate_get_subject_name(PurpleCertificate *crt);

/**
 * purple_certificate_check_subject_name:
 * @crt:   Certificate instance
 * @name:  Name to check.
 *
 * Check the subject name against that on the certificate
 *
 * Returns: TRUE if it is a match, else FALSE
 */
gboolean
purple_certificate_check_subject_name(PurpleCertificate *crt, const gchar *name);

/**
 * purple_certificate_get_times:
 * @crt:          Certificate instance
 * @activation:   Reference to store the activation time at. May be NULL
 *                     if you don't actually want it.
 * @expiration:   Reference to store the expiration time at. May be NULL
 *                     if you don't actually want it.
 *
 * Get the expiration/activation times.
 *
 * Returns: TRUE if the requested values were obtained, otherwise FALSE.
 */
gboolean
purple_certificate_get_times(PurpleCertificate *crt, gint64 *activation, gint64 *expiration);

/**
 * purple_certificate_get_der_data:
 * @crt: Certificate instance
 *
 * Retrieves the certificate data in DER form.
 *
 * Returns: Binary DER representation of the certificate - must be freed using
 *         g_byte_array_free().
 */
GByteArray *
purple_certificate_get_der_data(PurpleCertificate *crt);

/**
 * purple_certificate_get_display_string:
 * @crt: Certificate instance
 *
 * Retrieves a string suitable for displaying a certificate to the user.
 *
 * Returns: String representing the certificate that may be displayed to the user
 *         - must be freed using g_free().
 */
char *
purple_certificate_get_display_string(PurpleCertificate *crt);

/*@}*/

/*****************************************************************************/
/** @name Certificate Pool Functions                                         */
/*****************************************************************************/
/*@{*/

/**
 * purple_certificate_pool_get_type:
 *
 * Returns: The #GType for the #PurpleCertificatePool boxed structure.
 */
/* TODO: Boxing of PurpleCertificatePool is a temporary solution to having a
 *       GType for certificate pools. This should rather be a GObject instead of
 *       a GBoxed. */
GType purple_certificate_pool_get_type(void);

/**
 * purple_certificate_pool_mkpath:
 * @pool:   CertificatePool to build a path for
 * @id:     Key to look up a Certificate by. May be NULL.
 *
 * Helper function for generating file paths in ~/.purple/certificates for
 * CertificatePools that use them.
 *
 * All components will be escaped for filesystem friendliness.
 *
 * Returns: A newly allocated path of the form
 *         ~/.purple/certificates/scheme_name/pool_name/unique_id
 */
gchar *
purple_certificate_pool_mkpath(PurpleCertificatePool *pool, const gchar *id);

/**
 * purple_certificate_pool_usable:
 * @pool:   Pool to check
 *
 * Determines whether a pool can be used.
 *
 * Checks whether the associated CertificateScheme is loaded.
 *
 * Returns: TRUE if the pool can be used, otherwise FALSE
 */
gboolean
purple_certificate_pool_usable(PurpleCertificatePool *pool);

/**
 * purple_certificate_pool_get_scheme:
 * @pool:   Pool to get the scheme of
 *
 * Looks up the scheme the pool operates under.
 * See purple_certificate_pool_usable()
 *
 * Returns: Pointer to the pool's scheme, or NULL if it isn't loaded.
 */
PurpleCertificateScheme *
purple_certificate_pool_get_scheme(PurpleCertificatePool *pool);

/**
 * purple_certificate_pool_contains:
 * @pool:   Pool to look in
 * @id:     ID to look for
 *
 * Check for presence of an ID in a pool.
 *
 * Returns: TRUE if the ID is in the pool, else FALSE
 */
gboolean
purple_certificate_pool_contains(PurpleCertificatePool *pool, const gchar *id);

/**
 * purple_certificate_pool_retrieve:
 * @pool:   Pool to fish in
 * @id:     ID to look up
 *
 * Retrieve a certificate from a pool.
 *
 * Returns: Retrieved certificate, or NULL if it wasn't there
 */
PurpleCertificate *
purple_certificate_pool_retrieve(PurpleCertificatePool *pool, const gchar *id);

/**
 * purple_certificate_pool_store:
 * @pool:   Pool to add to
 * @id:     ID to store the certificate with
 * @crt:    Certificate to store
 *
 * Add a certificate to a pool
 *
 * Any pre-existing certificate of the same ID will be overwritten.
 *
 * Returns: TRUE if the operation succeeded, otherwise FALSE
 */
gboolean
purple_certificate_pool_store(PurpleCertificatePool *pool, const gchar *id, PurpleCertificate *crt);

/**
 * purple_certificate_pool_delete:
 * @pool:   Pool to remove from
 * @id:     ID to remove
 *
 * Remove a certificate from a pool
 *
 * Returns: TRUE if the operation succeeded, otherwise FALSE
 */
gboolean
purple_certificate_pool_delete(PurpleCertificatePool *pool, const gchar *id);

/**
 * purple_certificate_pool_get_idlist:
 * @pool:   Pool to enumerate
 *
 * Get the list of IDs currently in the pool.
 *
 * Returns: GList pointing to newly-allocated id strings. Free using
 *         purple_certificate_pool_destroy_idlist()
 */
GList *
purple_certificate_pool_get_idlist(PurpleCertificatePool *pool);

/**
 * purple_certificate_pool_destroy_idlist:
 * @idlist: ID List to destroy
 *
 * Destroys the result given by purple_certificate_pool_get_idlist()
 */
void
purple_certificate_pool_destroy_idlist(GList *idlist);

/*@}*/

/*****************************************************************************/
/** @name Certificate Subsystem API                                          */
/*****************************************************************************/
/*@{*/

/**
 * purple_certificate_init:
 *
 * Initialize the certificate system
 */
void
purple_certificate_init(void);

/**
 * purple_certificate_uninit:
 *
 * Un-initialize the certificate system
 */
void
purple_certificate_uninit(void);

/**
 * purple_certificate_get_handle:
 *
 * Get the Certificate subsystem handle for signalling purposes
 */
gpointer
purple_certificate_get_handle(void);

/**
 * purple_certificate_find_scheme:
 * @name:   The scheme name. Case insensitive.
 *
 * Look up a registered CertificateScheme by name
 *
 * Returns: Pointer to the located Scheme, or NULL if it isn't found.
 */
PurpleCertificateScheme *
purple_certificate_find_scheme(const gchar *name);

/**
 * purple_certificate_get_schemes:
 *
 * Get all registered CertificateSchemes
 *
 * Returns: GList pointing to all registered CertificateSchemes . This value
 *         is owned by libpurple
 */
GList *
purple_certificate_get_schemes(void);

/**
 * purple_certificate_register_scheme:
 * @scheme:  Pointer to the scheme to register.
 *
 * Register a CertificateScheme with libpurple
 *
 * No two schemes can be registered with the same name; this function enforces
 * that.
 *
 * Returns: TRUE if the scheme was successfully added, otherwise FALSE
 */
gboolean
purple_certificate_register_scheme(PurpleCertificateScheme *scheme);

/**
 * purple_certificate_unregister_scheme:
 * @scheme:    Scheme to unregister.
 *                  If the scheme is not registered, this is a no-op.
 *
 * Unregister a CertificateScheme from libpurple
 *
 * Returns: TRUE if the unregister completed successfully
 */
gboolean
purple_certificate_unregister_scheme(PurpleCertificateScheme *scheme);

/**
 * purple_certificate_find_verifier:
 * @scheme_name:  Scheme name. Case insensitive.
 * @ver_name:     The verifier name. Case insensitive.
 *
 * Look up a registered PurpleCertificateVerifier by scheme and name
 *
 * Returns: Pointer to the located Verifier, or NULL if it isn't found.
 */
PurpleCertificateVerifier *
purple_certificate_find_verifier(const gchar *scheme_name, const gchar *ver_name);

/**
 * purple_certificate_get_verifiers:
 *
 * Get the list of registered CertificateVerifiers
 *
 * Returns: GList of all registered PurpleCertificateVerifier. This value
 *         is owned by libpurple
 */
GList *
purple_certificate_get_verifiers(void);

/**
 * purple_certificate_register_verifier:
 * @vr:     Verifier to register.
 *
 * Register a CertificateVerifier with libpurple
 *
 * Returns: TRUE if register succeeded, otherwise FALSE
 */
gboolean
purple_certificate_register_verifier(PurpleCertificateVerifier *vr);

/**
 * purple_certificate_unregister_verifier:
 * @vr:     Verifier to unregister.
 *
 * Unregister a CertificateVerifier with libpurple
 *
 * Returns: TRUE if unregister succeeded, otherwise FALSE
 */
gboolean
purple_certificate_unregister_verifier(PurpleCertificateVerifier *vr);

/**
 * purple_certificate_find_pool:
 * @scheme_name:  Scheme name. Case insensitive.
 * @pool_name:    Pool name. Case insensitive.
 *
 * Look up a registered PurpleCertificatePool by scheme and name
 *
 * Returns: Pointer to the located Pool, or NULL if it isn't found.
 */
PurpleCertificatePool *
purple_certificate_find_pool(const gchar *scheme_name, const gchar *pool_name);

/**
 * purple_certificate_get_pools:
 *
 * Get the list of registered Pools
 *
 * Returns: GList of all registered PurpleCertificatePool s. This value
 *         is owned by libpurple
 */
GList *
purple_certificate_get_pools(void);

/**
 * purple_certificate_register_pool:
 * @pool:   Pool to register.
 *
 * Register a CertificatePool with libpurple and call its init function
 *
 * Returns: TRUE if the register succeeded, otherwise FALSE
 */
gboolean
purple_certificate_register_pool(PurpleCertificatePool *pool);

/**
 * purple_certificate_unregister_pool:
 * @pool:   Pool to unregister.
 *
 * Unregister a CertificatePool with libpurple and call its uninit function
 *
 * Returns: TRUE if the unregister succeeded, otherwise FALSE
 */
gboolean
purple_certificate_unregister_pool(PurpleCertificatePool *pool);

/**
 * purple_certificate_add_ca_search_path:
 * @path:   Path to search for certificates.
 *
 * Add a search path for certificates.
 */
void purple_certificate_add_ca_search_path(const char *path);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_CERTIFICATE_H */
