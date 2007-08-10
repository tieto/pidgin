/**
 * @file certificate.c Public-Key Certificate API
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

#include <glib.h>

#include "certificate.h"
#include "debug.h"
#include "internal.h"
#include "request.h"
#include "signals.h"
#include "util.h"

/** List holding pointers to all registered certificate schemes */
static GList *cert_schemes = NULL;
/** List of registered Verifiers */
static GList *cert_verifiers = NULL;
/** List of registered Pools */
static GList *cert_pools = NULL;

void
purple_certificate_verify (PurpleCertificateVerifier *verifier,
			   const gchar *subject_name, GList *cert_chain,
			   PurpleCertificateVerifiedCallback cb,
			   gpointer cb_data)
{
	PurpleCertificateVerificationRequest *vrq;
	PurpleCertificateScheme *scheme;
	
	g_return_if_fail(subject_name != NULL);
	/* If you don't have a cert to check, why are you requesting that it
	   be verified? */
	g_return_if_fail(cert_chain != NULL);
	g_return_if_fail(cb != NULL);

	/* Look up the CertificateScheme */
	scheme = purple_certificate_find_scheme(verifier->scheme_name);
	g_return_if_fail(scheme);

	/* Check that at least the first cert in the chain matches the
	   Verifier scheme */
	g_return_if_fail(scheme ==
			 ((PurpleCertificate *) (cert_chain->data))->scheme);

	/* Construct and fill in the request fields */
	vrq = g_new0(PurpleCertificateVerificationRequest, 1);
	vrq->verifier = verifier;
	vrq->scheme = scheme;
	vrq->subject_name = g_strdup(subject_name);
	vrq->cert_chain = purple_certificate_copy_list(cert_chain);
	vrq->cb = cb;
	vrq->cb_data = cb_data;

	/* Initiate verification */
	(verifier->start_verification)(vrq);
}

void
purple_certificate_verify_destroy (PurpleCertificateVerificationRequest *vrq)
{
	PurpleCertificateVerifier *vr;

	if (NULL == vrq) return;

	/* Fetch the Verifier responsible... */
	vr = vrq->verifier;
	/* ...and order it to KILL */
	(vr->destroy_request)(vrq);

	/* Now the internals have been cleaned up, so clean up the libpurple-
	   created elements */
	g_free(vrq->subject_name);
	purple_certificate_destroy_list(vrq->cert_chain);

	g_free(vrq);
}

void
purple_certificate_verify_complete(PurpleCertificateVerificationRequest *vrq,
				   PurpleCertificateVerificationStatus st)
{
	PurpleCertificateVerifier *vr;

	g_return_if_fail(vrq);

	/* Pass the results on to the request's callback */
	(vrq->cb)(st, vrq->cb_data);

	/* And now to eliminate the request */
	/* Fetch the Verifier responsible... */
	vr = vrq->verifier;
	/* ...and order it to KILL */
	(vr->destroy_request)(vrq);

	/* Now the internals have been cleaned up, so clean up the libpurple-
	   created elements */
	g_free(vrq->subject_name);
	purple_certificate_destroy_list(vrq->cert_chain);

	/*  A structure born
	 *          to much ado
	 *                   and with so much within.
	 * It reaches now
	 *             its quiet end. */
	g_free(vrq);
}


PurpleCertificate *
purple_certificate_copy(PurpleCertificate *crt)
{
	g_return_val_if_fail(crt, NULL);
	g_return_val_if_fail(crt->scheme, NULL);
	g_return_val_if_fail(crt->scheme->copy_certificate, NULL);

	return (crt->scheme->copy_certificate)(crt);
}

GList *
purple_certificate_copy_list(GList *crt_list)
{
	GList *new, *l;

	/* First, make a shallow copy of the list */
	new = g_list_copy(crt_list);

	/* Now go through and actually duplicate each certificate */
	for (l = new; l; l = l->next) {
		l->data = purple_certificate_copy(l->data);
	}

	return new;
}

void
purple_certificate_destroy (PurpleCertificate *crt)
{
	PurpleCertificateScheme *scheme;
	
	if (NULL == crt) return;

	scheme = crt->scheme;

	(scheme->destroy_certificate)(crt);
}

void
purple_certificate_destroy_list (GList * crt_list)
{
	PurpleCertificate *crt;
	GList *l;

	for (l=crt_list; l; l = l->next) {
		crt = (PurpleCertificate *) l->data;
		purple_certificate_destroy(crt);
	}

	g_list_free(crt_list);
}

gboolean
purple_certificate_signed_by(PurpleCertificate *crt, PurpleCertificate *issuer)
{
	PurpleCertificateScheme *scheme;

	g_return_val_if_fail(crt, FALSE);
	g_return_val_if_fail(issuer, FALSE);

	scheme = crt->scheme;
	g_return_val_if_fail(scheme, FALSE);
	/* We can't compare two certs of unrelated schemes, obviously */
	g_return_val_if_fail(issuer->scheme == scheme, FALSE);

	return (scheme->signed_by)(crt, issuer);
}

gboolean
purple_certificate_check_signature_chain(GList *chain)
{
	GList *cur;
	PurpleCertificate *crt, *issuer;
	gchar *uid;

	g_return_val_if_fail(chain, FALSE);

	uid = purple_certificate_get_unique_id((PurpleCertificate *) chain->data);
	purple_debug_info("certificate",
			  "Checking signature chain for uid=%s\n",
			  uid);
	g_free(uid);
	
	/* If this is a single-certificate chain, say that it is valid */
	if (chain->next == NULL) {
		purple_debug_info("certificate",
				  "...Singleton. We'll say it's valid.\n");
		return TRUE;
	}

	/* Load crt with the first certificate */
	crt = (PurpleCertificate *)(chain->data);
	/* And start with the second certificate in the chain */
	for ( cur = chain->next; cur; cur = cur->next ) {
		
		issuer = (PurpleCertificate *)(cur->data);
		
		/* Check the signature for this link */
		if (! purple_certificate_signed_by(crt, issuer) ) {
			uid = purple_certificate_get_unique_id(issuer);
			purple_debug_info("certificate",
					  "...Bad or missing signature by %s\nChain is INVALID\n",
					  uid);
			g_free(uid);
		
			return FALSE;
		}

		uid = purple_certificate_get_unique_id(issuer);
		purple_debug_info("certificate",
				  "...Good signature by %s\n",
				  uid);
		g_free(uid);
		
		/* The issuer is now the next crt whose signature is to be
		   checked */
		crt = issuer;
	}

	/* If control reaches this point, the chain is valid */
	purple_debug_info("certificate", "Chain is VALID\n");
	return TRUE;
}

PurpleCertificate *
purple_certificate_import(PurpleCertificateScheme *scheme, const gchar *filename)
{
	g_return_val_if_fail(scheme, NULL);
	g_return_val_if_fail(scheme->import_certificate, NULL);
	g_return_val_if_fail(filename, NULL);

	return (scheme->import_certificate)(filename);
}

gboolean
purple_certificate_export(const gchar *filename, PurpleCertificate *crt)
{
	PurpleCertificateScheme *scheme;

	g_return_val_if_fail(filename, FALSE);
	g_return_val_if_fail(crt, FALSE);
	g_return_val_if_fail(crt->scheme, FALSE);

	scheme = crt->scheme;
	g_return_val_if_fail(scheme->export_certificate, FALSE);

	return (scheme->export_certificate)(filename, crt);
}

GByteArray *
purple_certificate_get_fingerprint_sha1(PurpleCertificate *crt)
{
	PurpleCertificateScheme *scheme;
	GByteArray *fpr;

	g_return_val_if_fail(crt, NULL);
	g_return_val_if_fail(crt->scheme, NULL);

	scheme = crt->scheme;
	
	g_return_val_if_fail(scheme->get_fingerprint_sha1, NULL);

	fpr = (scheme->get_fingerprint_sha1)(crt);

	return fpr;
}

gchar *
purple_certificate_get_unique_id(PurpleCertificate *crt)
{
	g_return_val_if_fail(crt, NULL);
	g_return_val_if_fail(crt->scheme, NULL);
	g_return_val_if_fail(crt->scheme->get_unique_id, NULL);

	return (crt->scheme->get_unique_id)(crt);
}

gchar *
purple_certificate_get_issuer_unique_id(PurpleCertificate *crt)
{
	g_return_val_if_fail(crt, NULL);
	g_return_val_if_fail(crt->scheme, NULL);
	g_return_val_if_fail(crt->scheme->get_issuer_unique_id, NULL);

	return (crt->scheme->get_issuer_unique_id)(crt);
}

gchar *
purple_certificate_get_subject_name(PurpleCertificate *crt)
{
	PurpleCertificateScheme *scheme;
	gchar *subject_name;

	g_return_val_if_fail(crt, NULL);
	g_return_val_if_fail(crt->scheme, NULL);

	scheme = crt->scheme;

	g_return_val_if_fail(scheme->get_subject_name, NULL);

	subject_name = (scheme->get_subject_name)(crt);

	return subject_name;
}

gboolean
purple_certificate_check_subject_name(PurpleCertificate *crt, const gchar *name)
{
	PurpleCertificateScheme *scheme;

	g_return_val_if_fail(crt, FALSE);
	g_return_val_if_fail(crt->scheme, FALSE);
	g_return_val_if_fail(name, FALSE);

	scheme = crt->scheme;

	/* TODO: Instead of failing, maybe use get_subject_name and strcmp? */
	g_return_val_if_fail(scheme->check_subject_name, FALSE);

	return (scheme->check_subject_name)(crt, name);
}

gboolean
purple_certificate_get_times(PurpleCertificate *crt, time_t *activation, time_t *expiration)
{
	PurpleCertificateScheme *scheme;

	g_return_val_if_fail(crt, FALSE);

	scheme = crt->scheme;
	
	g_return_val_if_fail(scheme, FALSE);

	/* If both provided references are NULL, what are you doing calling
	   this? */
	g_return_val_if_fail( (activation != NULL) || (expiration != NULL), FALSE);

	/* Throw the request on down to the certscheme */
	return (scheme->get_times)(crt, activation, expiration);
}


gchar *
purple_certificate_pool_mkpath(PurpleCertificatePool *pool, const gchar *id)
{
	gchar *path;
	gchar *esc_scheme_name, *esc_name, *esc_id;
	
	g_return_val_if_fail(pool, NULL);
	g_return_val_if_fail(pool->scheme_name, NULL);
	g_return_val_if_fail(pool->name, NULL);

	/* Escape all the elements for filesystem-friendliness */
	esc_scheme_name = pool ? g_strdup(purple_escape_filename(pool->scheme_name)) : NULL;
	esc_name = pool ? g_strdup(purple_escape_filename(pool->name)) : NULL;
	esc_id = id ? g_strdup(purple_escape_filename(id)) : NULL;
	
	path = g_build_filename(purple_user_dir(),
				"certificates", /* TODO: constantize this? */
				esc_scheme_name,
				esc_name,
				esc_id,
				NULL);

	g_free(esc_scheme_name);
	g_free(esc_name);
	g_free(esc_id);
	return path;
}

gboolean
purple_certificate_pool_usable(PurpleCertificatePool *pool)
{
	g_return_val_if_fail(pool, FALSE);
	g_return_val_if_fail(pool->scheme_name, FALSE);

	/* Check that the pool's scheme is loaded */
	if (purple_certificate_find_scheme(pool->scheme_name) == NULL) {
		return FALSE;
	}
	
	return TRUE;
}

PurpleCertificateScheme *
purple_certificate_pool_get_scheme(PurpleCertificatePool *pool)
{
	g_return_val_if_fail(pool, NULL);
	g_return_val_if_fail(pool->scheme_name, NULL);

	return purple_certificate_find_scheme(pool->scheme_name);
}

gboolean
purple_certificate_pool_contains(PurpleCertificatePool *pool, const gchar *id)
{
	g_return_val_if_fail(pool, FALSE);
	g_return_val_if_fail(id, FALSE);
	g_return_val_if_fail(pool->cert_in_pool, FALSE);

	return (pool->cert_in_pool)(id);
}

PurpleCertificate *
purple_certificate_pool_retrieve(PurpleCertificatePool *pool, const gchar *id)
{
	g_return_val_if_fail(pool, NULL);
	g_return_val_if_fail(id, NULL);
	g_return_val_if_fail(pool->get_cert, NULL);

	return (pool->get_cert)(id);
}

gboolean
purple_certificate_pool_store(PurpleCertificatePool *pool, const gchar *id, PurpleCertificate *crt)
{
	gboolean ret = FALSE;
	
	g_return_val_if_fail(pool, FALSE);
	g_return_val_if_fail(id, FALSE);
	g_return_val_if_fail(pool->put_cert, FALSE);

	/* TODO: Should this just be someone else's problem? */
	/* Whether crt->scheme matches find_scheme(pool->scheme_name) is not
	   relevant... I think... */
	g_return_val_if_fail(
		g_ascii_strcasecmp(pool->scheme_name, crt->scheme->name) == 0,
		FALSE);

	ret = (pool->put_cert)(id, crt);

	/* Signal that the certificate was stored if success*/
	if (ret) {
		purple_signal_emit(pool, "certificate-stored",
				   pool, id);
	}

	return ret;
}	

gboolean
purple_certificate_pool_delete(PurpleCertificatePool *pool, const gchar *id)
{
	gboolean ret = FALSE;
	
	g_return_val_if_fail(pool, FALSE);
	g_return_val_if_fail(id, FALSE);
	g_return_val_if_fail(pool->delete_cert, FALSE);

	ret = (pool->delete_cert)(id);

	/* Signal that the certificate was deleted if success */
	if (ret) {
		purple_signal_emit(pool, "certificate-deleted",
				   pool, id);
	}

	return ret;
}

GList *
purple_certificate_pool_get_idlist(PurpleCertificatePool *pool)
{
	g_return_val_if_fail(pool, NULL);
	g_return_val_if_fail(pool->get_idlist, NULL);

	return (pool->get_idlist)();
}

void
purple_certificate_pool_destroy_idlist(GList *idlist)
{
	GList *l;
	
	/* Iterate through and free them strings */
	for ( l = idlist; l; l = l->next ) {
		g_free(l->data);
	}

	g_list_free(idlist);
}


/****************************************************************************/
/* Builtin Verifiers, Pools, etc.                                           */
/****************************************************************************/

static void
x509_singleuse_verify_cb (PurpleCertificateVerificationRequest *vrq, gint id)
{
	g_return_if_fail(vrq);

	purple_debug_info("certificate/x509_singleuse",
			  "VRQ on cert from %s gave %d\n",
			  vrq->subject_name, id);

	/* Signal what happened back to the caller */
	if (1 == id) {		
		/* Accepted! */
		(vrq->cb)(PURPLE_CERTIFICATE_VALID, vrq->cb_data);
	} else {
		/* Not accepted */
		(vrq->cb)(PURPLE_CERTIFICATE_INVALID, vrq->cb_data);
	}

	/* Now clean up the request */
	purple_certificate_verify_destroy(vrq);
}

static void
x509_singleuse_start_verify (PurpleCertificateVerificationRequest *vrq)
{
	gchar *sha_asc;
	GByteArray *sha_bin;
	gchar *cn;
	const gchar *cn_match;
	gchar *primary, *secondary;
	PurpleCertificate *crt = (PurpleCertificate *) vrq->cert_chain->data;

	/* Pull out the SHA1 checksum */
	sha_bin = purple_certificate_get_fingerprint_sha1(crt);
	/* Now decode it for display */
	sha_asc = purple_base16_encode_chunked(sha_bin->data,
					       sha_bin->len);

	/* Get the cert Common Name */
	cn = purple_certificate_get_subject_name(crt);

	/* Determine whether the name matches */
	/* TODO: Worry about strcmp safety? */
	if (!strcmp(cn, vrq->subject_name)) {
		cn_match = _("");
	} else {
		cn_match = _("(DOES NOT MATCH)");
	}
	
	/* Make messages */
	primary = g_strdup_printf(_("%s has presented the following certificate for just-this-once use:"), vrq->subject_name);
	secondary = g_strdup_printf(_("Common name: %s %s\nFingerprint (SHA1): %s"), cn, cn_match, sha_asc);
	
	/* Make a semi-pretty display */
	purple_request_accept_cancel(
		vrq->cb_data, /* TODO: Find what the handle ought to be */
		_("Single-use Certificate Verification"),
		primary,
		secondary,
		1,            /* Accept by default */
		NULL,         /* No account */
		NULL,         /* No other user */
		NULL,         /* No associated conversation */
		vrq,
		x509_singleuse_verify_cb,
		x509_singleuse_verify_cb );
	
	/* Cleanup */
	g_free(primary);
	g_free(secondary);
	g_free(sha_asc);
	g_byte_array_free(sha_bin, TRUE);
}

static void
x509_singleuse_destroy_request (PurpleCertificateVerificationRequest *vrq)
{
	/* I don't do anything! */
}

PurpleCertificateVerifier x509_singleuse = {
	"x509",                         /* Scheme name */
	"singleuse",                    /* Verifier name */
	x509_singleuse_start_verify,    /* start_verification function */
	x509_singleuse_destroy_request  /* Request cleanup operation */
};




static PurpleCertificatePool x509_tls_peers;

static gboolean
x509_tls_peers_init(void)
{
	gchar *poolpath;
	int ret;
	
	/* Set up key cache here if it isn't already done */
	poolpath = purple_certificate_pool_mkpath(&x509_tls_peers, NULL);
	ret = purple_build_dir(poolpath, 0700); /* Make it this user only */

	g_free(poolpath);

	g_return_val_if_fail(ret == 0, FALSE);
	return TRUE;
}

static gboolean
x509_tls_peers_cert_in_pool(const gchar *id)
{
	gchar *keypath;
	gboolean ret = FALSE;
	
	g_return_val_if_fail(id, FALSE);

	keypath = purple_certificate_pool_mkpath(&x509_tls_peers, id);

	ret = g_file_test(keypath, G_FILE_TEST_IS_REGULAR);
	
	g_free(keypath);
	return ret;
}

static PurpleCertificate *
x509_tls_peers_get_cert(const gchar *id)
{
	PurpleCertificateScheme *x509;
	PurpleCertificate *crt;
	gchar *keypath;
	
	g_return_val_if_fail(id, NULL);

	/* Is it in the pool? */
	if ( !x509_tls_peers_cert_in_pool(id) ) {
		return NULL;
	}
	
	/* Look up the X.509 scheme */
	x509 = purple_certificate_find_scheme("x509");
	g_return_val_if_fail(x509, NULL);

	/* Okay, now find and load that key */
	keypath = purple_certificate_pool_mkpath(&x509_tls_peers, id);
	crt = purple_certificate_import(x509, keypath);

	g_free(keypath);

	return crt;
}

static gboolean
x509_tls_peers_put_cert(const gchar *id, PurpleCertificate *crt)
{
	gboolean ret = FALSE;
	gchar *keypath;

	g_return_val_if_fail(crt, FALSE);
	g_return_val_if_fail(crt->scheme, FALSE);
	/* Make sure that this is some kind of X.509 certificate */
	/* TODO: Perhaps just check crt->scheme->name instead? */
	g_return_val_if_fail(crt->scheme == purple_certificate_find_scheme(x509_tls_peers.scheme_name), FALSE);

	/* Work out the filename and export */
	keypath = purple_certificate_pool_mkpath(&x509_tls_peers, id);
	ret = purple_certificate_export(keypath, crt);
	
	g_free(keypath);
	return ret;
}

static gboolean
x509_tls_peers_delete_cert(const gchar *id)
{
	gboolean ret = FALSE;
	gchar *keypath;

	g_return_val_if_fail(id, FALSE);

	/* Is the id even in the pool? */
	if (!x509_tls_peers_cert_in_pool(id)) {
		purple_debug_warning("certificate/tls_peers",
				     "Id %s wasn't in the pool\n",
				     id);
		return FALSE;
	}

	/* OK, so work out the keypath and delete the thing */
	keypath = purple_certificate_pool_mkpath(&x509_tls_peers, id);	
	if ( unlink(keypath) != 0 ) {
		purple_debug_error("certificate/tls_peers",
				   "Unlink of %s failed!\n",
				   keypath);
		ret = FALSE;
	} else {
		ret = TRUE;
	}

	g_free(keypath);
	return ret;
}

static GList *
x509_tls_peers_get_idlist(void)
{
	GList *idlist = NULL;
	GDir *dir;
	const gchar *entry;
	gchar *poolpath;

	/* Get a handle on the pool directory */
	poolpath = purple_certificate_pool_mkpath(&x509_tls_peers, NULL);
	dir = g_dir_open(poolpath,
			 0,     /* No flags */
			 NULL); /* Not interested in what the error is */
	g_free(poolpath);

	g_return_val_if_fail(dir, NULL);

	/* Traverse the directory listing and create an idlist */
	while ( (entry = g_dir_read_name(dir)) != NULL ) {
		/* Unescape the filename */
		const char *unescaped = purple_unescape_filename(entry);
		
		/* Copy the entry name into our list (GLib owns the original
		   string) */
		idlist = g_list_prepend(idlist, g_strdup(unescaped));
	}

	/* Release the directory */
	g_dir_close(dir);
	
	return idlist;
}

static PurpleCertificatePool x509_tls_peers = {
	"x509",                       /* Scheme name */
	"tls_peers",                  /* Pool name */
	N_("SSL Peers Cache"),        /* User-friendly name */
	NULL,                         /* Internal data */
	x509_tls_peers_init,          /* init */
	NULL,                         /* uninit not required */
	x509_tls_peers_cert_in_pool,  /* Certificate exists? */
	x509_tls_peers_get_cert,      /* Cert retriever */
	x509_tls_peers_put_cert,      /* Cert writer */
	x509_tls_peers_delete_cert,   /* Cert remover */
	x509_tls_peers_get_idlist     /* idlist retriever */
};



static PurpleCertificateVerifier x509_tls_cached;

static void
x509_tls_cached_user_auth_cb (PurpleCertificateVerificationRequest *vrq, gint id)
{
	PurpleCertificatePool *tls_peers;
	
	g_return_if_fail(vrq);

	tls_peers = purple_certificate_find_pool("x509","tls_peers");

	if (1 == id) {
		gchar *cache_id = vrq->subject_name;
		purple_debug_info("certificate/x509/tls_cached",
				  "User ACCEPTED cert\nCaching first in chain for future use as %s...\n",
				  cache_id);
		
		purple_certificate_pool_store(tls_peers, cache_id,
					      vrq->cert_chain->data);
			
		(vrq->cb)(PURPLE_CERTIFICATE_VALID, vrq->cb_data);
	} else {
		purple_debug_info("certificate/x509/tls_cached",
				  "User REJECTED cert\n");
		(vrq->cb)(PURPLE_CERTIFICATE_INVALID, vrq->cb_data);
	}

	/* Finish off the request */
	purple_certificate_verify_destroy(vrq);
}

/* Validates a certificate by asking the user */
static void
x509_tls_cached_user_auth(PurpleCertificateVerificationRequest *vrq)
{
	gchar *sha_asc;
	GByteArray *sha_bin;
	gchar *cn;
	const gchar *cn_match;
	time_t activation, expiration;
	/* Length of these buffers is dictated by 'man ctime_r' */
	gchar activ_str[26], expir_str[26];
	gchar *primary, *secondary;
	PurpleCertificate *crt = (PurpleCertificate *) vrq->cert_chain->data;

	/* Pull out the SHA1 checksum */
	sha_bin = purple_certificate_get_fingerprint_sha1(crt);
	/* Now decode it for display */
	sha_asc = purple_base16_encode_chunked(sha_bin->data,
					       sha_bin->len);

	/* Get the cert Common Name */
	cn = purple_certificate_get_subject_name(crt);

	/* Determine whether the name matches */
	if (purple_certificate_check_subject_name(crt, vrq->subject_name)) {
		cn_match = _("");
	} else {
		cn_match = _("(DOES NOT MATCH)");
	}

	/* Get the certificate times */
	/* TODO: Check the times against localtime */
	/* TODO: errorcheck? */
	g_assert(purple_certificate_get_times(crt, &activation, &expiration));
	ctime_r(&activation, activ_str);
	ctime_r(&expiration, expir_str);
	
	/* Make messages */
	primary = g_strdup_printf(_("%s has presented the following certificate:"), vrq->subject_name);
	secondary = g_strdup_printf(_("Common name: %s %s\n\nFingerprint (SHA1): %s\n\nActivation date: %s\nExpiration date: %s\n"), cn, cn_match, sha_asc, activ_str, expir_str);
	
	/* Make a semi-pretty display */
	purple_request_accept_cancel(
		vrq->cb_data, /* TODO: Find what the handle ought to be */
		_("SSL Certificate Verification"),
		primary,
		secondary,
		1,            /* Accept by default */
		NULL,         /* No account */
		NULL,         /* No other user */
		NULL,         /* No associated conversation */
		vrq,
		x509_tls_cached_user_auth_cb,
		x509_tls_cached_user_auth_cb );
	
	/* Cleanup */
	g_free(primary);
	g_free(secondary);
	g_free(sha_asc);
	g_byte_array_free(sha_bin, TRUE);
}

static void
x509_tls_cached_peer_cert_changed(PurpleCertificateVerificationRequest *vrq)
{
	/* TODO: Prompt the user, etc. */

	(vrq->cb)(PURPLE_CERTIFICATE_INVALID, vrq->cb_data);
	/* Okay, we're done here */
	purple_certificate_verify_destroy(vrq);
	return;
}

static void
x509_tls_cached_cert_in_cache(PurpleCertificateVerificationRequest *vrq)
{
	/* TODO: Looking this up by name over and over is expensive.
	   Fix, please! */
	PurpleCertificatePool *tls_peers =
		purple_certificate_find_pool(x509_tls_cached.scheme_name,
					     "tls_peers");

	/* The peer's certificate should be the first in the list */
	PurpleCertificate *peer_crt =
		(PurpleCertificate *) vrq->cert_chain->data;
	
	PurpleCertificate *cached_crt;
	GByteArray *peer_fpr, *cached_fpr;

	/* Load up the cached certificate */
	cached_crt = purple_certificate_pool_retrieve(
		tls_peers, vrq->subject_name);
	g_assert(cached_crt);

	/* Now get SHA1 sums for both and compare them */
	/* TODO: This is not an elegant way to compare certs */
	peer_fpr = purple_certificate_get_fingerprint_sha1(peer_crt);
	cached_fpr = purple_certificate_get_fingerprint_sha1(cached_crt);
	if (!memcmp(peer_fpr->data, cached_fpr->data, peer_fpr->len)) {
		purple_debug_info("certificate/x509/tls_cached",
				  "Peer cert matched cached\n");
		(vrq->cb)(PURPLE_CERTIFICATE_VALID, vrq->cb_data);
		
		/* vrq is now finished */
		purple_certificate_verify_destroy(vrq);
	} else {
		purple_debug_info("certificate/x509/tls_cached",
				  "Peer cert did NOT match cached\n");
		/* vrq now becomes the problem of cert_changed */
		x509_tls_cached_peer_cert_changed(vrq);
	}
	
	purple_certificate_destroy(cached_crt);
	g_byte_array_free(peer_fpr, TRUE);
	g_byte_array_free(cached_fpr, TRUE);
}

/* For when we've never communicated with this party before */
static void
x509_tls_cached_unknown_peer(PurpleCertificateVerificationRequest *vrq)
{
	PurpleCertificatePool *ca, *tls_peers;
	PurpleCertificate *end_crt, *ca_crt, *peer_crt;
	GList *chain = vrq->cert_chain;
	GList *last;
	gchar *ca_id;

	peer_crt = (PurpleCertificate *) chain->data;

	/* First, check that the hostname matches */
	if ( ! purple_certificate_check_subject_name(peer_crt,
						     vrq->subject_name) ) {
		gchar *sn = purple_certificate_get_subject_name(peer_crt);
		
		purple_debug_info("certificate/x509/tls_cached",
				  "Name mismatch: Certificate given for %s "
				  "has a name of %s\n",
				  vrq->subject_name, sn);
		g_free(sn);

		/* Prompt the user to authenticate the certificate */
		/* TODO: Provide the user with more guidance about why he is
		   being prompted */
		/* vrq will be completed by user_auth */
		x509_tls_cached_user_auth(vrq);
		return;
	} /* if (name mismatch) */

			
	
	/* Next, check that the certificate chain is valid */
	if ( ! purple_certificate_check_signature_chain(chain) ) {
		/* TODO: Tell the user where the chain broke? */
		/* TODO: This error will hopelessly confuse any
		   non-elite user. */
		gchar *secondary;

		secondary = g_strdup_printf(_("The certificate chain presented"
					      " for %s is not valid."),
					    vrq->subject_name);

		/* TODO: Make this error either block the ensuing SSL
		   connection error until the user dismisses this one, or
		   stifle it. */
		purple_notify_error(NULL, /* TODO: Probably wrong. */
				    _("SSL Certificate Error"),
				    _("Invalid certificate chain"),
				    secondary );
		g_free(secondary);

		/* Okay, we're done here */
		purple_certificate_verify_complete(vrq,
						   PURPLE_CERTIFICATE_INVALID);
	} /* if (signature chain not good) */

	/* Next, attempt to verify the last certificate against a CA */
	ca = purple_certificate_find_pool(x509_tls_cached.scheme_name, "ca");

	/* If, for whatever reason, there is no Certificate Authority pool
	   loaded, we will simply present it to the user for checking. */
	if ( !ca ) {
		purple_debug_error("certificate/x509/tls_cached",
				   "No X.509 Certificate Authority pool "
				   "could be found!\n");
		
		/* vrq will be completed by user_auth */
		x509_tls_cached_user_auth(vrq);
		return;
	}

	/* TODO: I don't have the Glib documentation handy; is this correct? */
	last = g_list_last(chain);
	end_crt = (PurpleCertificate *) last->data;

	/* Attempt to look up the last certificate's issuer */
	ca_id = purple_certificate_get_issuer_unique_id(end_crt);
	if ( !purple_certificate_pool_contains(ca, ca_id) ) {
		purple_debug_info("certificate/x509/tls_cached",
				  "Certificate Authority with DN='%s' not "
				  "found. I'll prompt the user, I guess.\n",
				  ca_id);
		g_free(ca_id);
		/* vrq will be completed by user_auth */
		x509_tls_cached_user_auth(vrq);
		return;
	}

	ca_crt = purple_certificate_pool_retrieve(ca, ca_id);
	g_free(ca_id);
	g_assert(ca_crt);
	
	/* Check the signature */
	if ( !purple_certificate_signed_by(end_crt, ca_crt) ) {
		/* TODO: If signed_by ever returns a reason, maybe mention
		   that, too. */
		/* TODO: Also mention the CA involved. While I could do this
		   now, a full DN is a little much with which to assault the
		   user's poor, leaky eyes. */
		/* TODO: This error message makes my eyes cross, and I wrote it */
		gchar * secondary =
			g_strdup_printf(_("The certificate chain presented by "
					  "%s does not have a valid digital "
					  "signature from the Certificate "
					  "Authority it claims to have one "
					  "from."),
					vrq->subject_name);
		
		purple_notify_error(NULL, /* TODO: Probably wrong */
				    _("SSL Certificate Error"),
				    _("Invalid certificate authority"
				      " signature"),
				    secondary);
		g_free(secondary);

		/* Signal "bad cert" */
		purple_certificate_verify_complete(vrq,
						   PURPLE_CERTIFICATE_INVALID);
		return;
	} /* if (CA signature not good) */

	/* If we reach this point, the certificate is good. */
	/* Look up the local cache and store it there for future use */
	tls_peers = purple_certificate_find_pool(x509_tls_cached.scheme_name,
						 "tls_peers");

	if (tls_peers) {
		g_assert(purple_certificate_pool_store(tls_peers,
						       vrq->subject_name,
						       peer_crt) );
	} else {
		purple_debug_error("certificate/x509/tls_cached",
				   "Unable to locate tls_peers certificate "
				   "cache.\n");
	}
	
	/* Whew! Done! */
	purple_certificate_verify_complete(vrq, PURPLE_CERTIFICATE_VALID);
}

static void
x509_tls_cached_start_verify(PurpleCertificateVerificationRequest *vrq)
{
	const gchar *tls_peers_name = "tls_peers"; /* Name of local cache */
	PurpleCertificatePool *tls_peers;

	g_return_if_fail(vrq);

	purple_debug_info("certificate/x509/tls_cached",
			  "Starting verify for %s\n",
			  vrq->subject_name);
	
	tls_peers = purple_certificate_find_pool(x509_tls_cached.scheme_name,tls_peers_name);

	/* TODO: This should probably just prompt the user instead of throwing
	   an angry fit */
	if (!tls_peers) {
		purple_debug_error("certificate/x509/tls_cached",
				   "Couldn't find local peers cache %s\nReturning INVALID to callback\n",
				   tls_peers_name);
		(vrq->cb)(PURPLE_CERTIFICATE_INVALID, vrq->cb_data);
		purple_certificate_verify_destroy(vrq);
		return;
	}
	
	/* Check if the peer has a certificate cached already */
	purple_debug_info("certificate/x509/tls_cached",
			  "Checking for cached cert...\n");
	if (purple_certificate_pool_contains(tls_peers, vrq->subject_name)) {
		purple_debug_info("certificate/x509/tls_cached",
				  "...Found cached cert\n");
		/* vrq is now the responsibility of cert_in_cache */
		x509_tls_cached_cert_in_cache(vrq);
	} else {
		/* TODO: Prompt the user, etc. */
		purple_debug_info("certificate/x509/tls_cached",
				  "...Not in cache\n");
		/* vrq now becomes the problem of unknown_peer */
		x509_tls_cached_unknown_peer(vrq);
	}
}

static void
x509_tls_cached_destroy_request(PurpleCertificateVerificationRequest *vrq)
{
	g_return_if_fail(vrq);
}

static PurpleCertificateVerifier x509_tls_cached = {
	"x509",                         /* Scheme name */
	"tls_cached",                   /* Verifier name */
	x509_tls_cached_start_verify,   /* Verification begin */
	x509_tls_cached_destroy_request /* Request cleanup */
};

/****************************************************************************/
/* Subsystem                                                                */
/****************************************************************************/
void
purple_certificate_init(void)
{
	/* Register builtins */
	purple_certificate_register_verifier(&x509_singleuse);
	purple_certificate_register_pool(&x509_tls_peers);
	purple_certificate_register_verifier(&x509_tls_cached);
}

void
purple_certificate_uninit(void)
{
	GList *full_list, *l;

	/* Unregister all Schemes */
	full_list = g_list_copy(cert_schemes); /* Make a working copy */
	for (l = full_list; l; l = l->next) {
		purple_certificate_unregister_scheme(
			(PurpleCertificateScheme *) l->data );
	}
	g_list_free(full_list);

	/* Unregister all Verifiers */
	full_list = g_list_copy(cert_verifiers); /* Make a working copy */
	for (l = full_list; l; l = l->next) {
		purple_certificate_unregister_verifier(
			(PurpleCertificateVerifier *) l->data );
	}
	g_list_free(full_list);

	/* Unregister all Pools */
	full_list = g_list_copy(cert_pools); /* Make a working copy */
	for (l = full_list; l; l = l->next) {
		purple_certificate_unregister_pool(
			(PurpleCertificatePool *) l->data );
	}
	g_list_free(full_list);
}

gpointer
purple_certificate_get_handle(void)
{
	static gint handle;
	return &handle;
}

PurpleCertificateScheme *
purple_certificate_find_scheme(const gchar *name)
{
	PurpleCertificateScheme *scheme = NULL;
	GList *l;

	g_return_val_if_fail(name, NULL);

	/* Traverse the list of registered schemes and locate the
	   one whose name matches */
	for(l = cert_schemes; l; l = l->next) {
		scheme = (PurpleCertificateScheme *)(l->data);

		/* Name matches? that's our man */
		if(!g_ascii_strcasecmp(scheme->name, name))
			return scheme;
	}

	purple_debug_warning("certificate",
			     "CertificateScheme %s requested but not found.\n",
			     name);

	/* TODO: Signalling and such? */
	
	return NULL;
}

GList *
purple_certificate_get_schemes(void)
{
	return cert_schemes;
}

gboolean
purple_certificate_register_scheme(PurpleCertificateScheme *scheme)
{
	g_return_val_if_fail(scheme != NULL, FALSE);

	/* Make sure no scheme is registered with the same name */
	if (purple_certificate_find_scheme(scheme->name) != NULL) {
		return FALSE;
	}

	/* Okay, we're golden. Register it. */
	cert_schemes = g_list_prepend(cert_schemes, scheme);

	/* TODO: Signalling and such? */

	purple_debug_info("certificate",
			  "CertificateScheme %s registered\n",
			  scheme->name);
	
	return TRUE;
}

gboolean
purple_certificate_unregister_scheme(PurpleCertificateScheme *scheme)
{
	if (NULL == scheme) {
		purple_debug_warning("certificate",
				     "Attempting to unregister NULL scheme\n");
		return FALSE;
	}

	/* TODO: signalling? */

	/* TODO: unregister all CertificateVerifiers for this scheme?*/
	/* TODO: unregister all CertificatePools for this scheme? */
	/* Neither of the above should be necessary, though */
	cert_schemes = g_list_remove(cert_schemes, scheme);

	purple_debug_info("certificate",
			  "CertificateScheme %s unregistered\n",
			  scheme->name);


	return TRUE;
}

PurpleCertificateVerifier *
purple_certificate_find_verifier(const gchar *scheme_name, const gchar *ver_name)
{
	PurpleCertificateVerifier *vr = NULL;
	GList *l;

	g_return_val_if_fail(scheme_name, NULL);
	g_return_val_if_fail(ver_name, NULL);

	/* Traverse the list of registered verifiers and locate the
	   one whose name matches */
	for(l = cert_verifiers; l; l = l->next) {
		vr = (PurpleCertificateVerifier *)(l->data);

		/* Scheme and name match? */
		if(!g_ascii_strcasecmp(vr->scheme_name, scheme_name) &&
		   !g_ascii_strcasecmp(vr->name, ver_name))
			return vr;
	}

	purple_debug_warning("certificate",
			     "CertificateVerifier %s, %s requested but not found.\n",
			     scheme_name, ver_name);

	/* TODO: Signalling and such? */
	
	return NULL;
}


GList *
purple_certificate_get_verifiers(void)
{
	return cert_verifiers;
}

gboolean
purple_certificate_register_verifier(PurpleCertificateVerifier *vr)
{
	g_return_val_if_fail(vr != NULL, FALSE);

	/* Make sure no verifier is registered with the same scheme/name */
	if (purple_certificate_find_verifier(vr->scheme_name, vr->name) != NULL) {
		return FALSE;
	}

	/* Okay, we're golden. Register it. */
	cert_verifiers = g_list_prepend(cert_verifiers, vr);

	/* TODO: Signalling and such? */

	purple_debug_info("certificate",
			  "CertificateVerifier %s registered\n",
			  vr->name);
	return TRUE;
}

gboolean
purple_certificate_unregister_verifier(PurpleCertificateVerifier *vr)
{
	if (NULL == vr) {
		purple_debug_warning("certificate",
				     "Attempting to unregister NULL verifier\n");
		return FALSE;
	}

	/* TODO: signalling? */

	cert_verifiers = g_list_remove(cert_verifiers, vr);


	purple_debug_info("certificate",
			  "CertificateVerifier %s unregistered\n",
			  vr->name);

	return TRUE;
}

PurpleCertificatePool *
purple_certificate_find_pool(const gchar *scheme_name, const gchar *pool_name)
{
	PurpleCertificatePool *pool = NULL;
	GList *l;

	g_return_val_if_fail(scheme_name, NULL);
	g_return_val_if_fail(pool_name, NULL);

	/* Traverse the list of registered pools and locate the
	   one whose name matches */
	for(l = cert_pools; l; l = l->next) {
		pool = (PurpleCertificatePool *)(l->data);

		/* Scheme and name match? */
		if(!g_ascii_strcasecmp(pool->scheme_name, scheme_name) &&
		   !g_ascii_strcasecmp(pool->name, pool_name))
			return pool;
	}

	purple_debug_warning("certificate",
			     "CertificatePool %s, %s requested but not found.\n",
			     scheme_name, pool_name);

	/* TODO: Signalling and such? */
	
	return NULL;

}

GList *
purple_certificate_get_pools(void)
{
	return cert_pools;
}

gboolean
purple_certificate_register_pool(PurpleCertificatePool *pool)
{
	gboolean success = FALSE;
	g_return_val_if_fail(pool, FALSE);
	g_return_val_if_fail(pool->scheme_name, FALSE);
	g_return_val_if_fail(pool->name, FALSE);
	g_return_val_if_fail(pool->fullname, FALSE);

	/* Make sure no pools are registered under this name */
	if (purple_certificate_find_pool(pool->scheme_name, pool->name)) {
		return FALSE;
	}

	/* Initialize the pool if needed */
	if (pool->init) {
		success = pool->init();
	} else {
		success = TRUE;
	}
	
	if (success) {
		/* Register the Pool */
		cert_pools = g_list_prepend(cert_pools, pool);

		/* TODO: Emit a signal that the pool got registered */

		purple_signal_register(pool, /* Signals emitted from pool */
				       "certificate-stored",
				       purple_marshal_VOID__POINTER_POINTER,
				       NULL, /* No callback return value */
				       2,    /* Two non-data arguments */
				       purple_value_new(PURPLE_TYPE_SUBTYPE,
							PURPLE_SUBTYPE_CERTIFICATEPOOL),
				       purple_value_new(PURPLE_TYPE_STRING));

		purple_signal_register(pool, /* Signals emitted from pool */
				       "certificate-deleted",
				       purple_marshal_VOID__POINTER_POINTER,
				       NULL, /* No callback return value */
				       2,    /* Two non-data arguments */
				       purple_value_new(PURPLE_TYPE_SUBTYPE,
							PURPLE_SUBTYPE_CERTIFICATEPOOL),
				       purple_value_new(PURPLE_TYPE_STRING));


		purple_debug_info("certificate",
			  "CertificatePool %s registered\n",
			  pool->name);
		return TRUE;
	} else {
		return FALSE;
	}
	
	/* Control does not reach this point */
}

gboolean
purple_certificate_unregister_pool(PurpleCertificatePool *pool)
{
	/* TODO: Better error checking? */
	if (NULL == pool) {
		purple_debug_warning("certificate",
				     "Attempting to unregister NULL pool\n");
		return FALSE;
	}

	/* Check that the pool is registered */
	if (!g_list_find(cert_pools, pool)) {
		purple_debug_warning("certificate",
				     "Pool to unregister isn't registered!\n");

		return FALSE;
	}

	/* Uninit the pool if needed */
	if (pool->uninit) {
		pool->uninit();
	}

	cert_pools = g_list_remove(cert_pools, pool);
	
	/* TODO: Signalling? */
	purple_signal_unregister(pool, "certificate-stored");
	purple_signal_unregister(pool, "certificate-deleted");
		
	purple_debug_info("certificate",
			  "CertificatePool %s unregistered\n",
			  pool->name);
	return TRUE;
}
