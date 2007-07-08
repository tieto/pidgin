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

#include <glib.h>

#include "certificate.h"
#include "debug.h"
#include "internal.h"
#include "request.h"
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
	vrq->cert_chain = cert_chain;
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

gchar *
purple_certificate_pool_mkpath(PurpleCertificatePool *pool, const gchar *id)
{
	gchar *path;
	
	g_return_val_if_fail(pool, NULL);
	g_return_val_if_fail(pool->scheme_name, NULL);
	g_return_val_if_fail(pool->name, NULL);

	path = g_build_filename(purple_user_dir(),
				"certificates", /* TODO: constantize this? */
				pool->scheme_name,
				pool->name,
				id,
				NULL);
	return path;
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
	g_return_val_if_fail(pool, FALSE);
	g_return_val_if_fail(id, FALSE);
	g_return_val_if_fail(pool->put_cert, FALSE);

	/* TODO: Should this just be someone else's problem? */
	/* Whether crt->scheme matches find_scheme(pool->scheme_name) is not
	   relevant... I think... */
	g_return_val_if_fail(
		g_ascii_strcasecmp(pool->scheme_name, crt->scheme->name) == 0,
		FALSE);

	return (pool->put_cert)(id, crt);
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

static PurpleCertificatePool x509_tls_peers = {
	"x509",                       /* Scheme name */
	"tls_peers",                  /* Pool name */
	N_("SSL Peers Cache"),        /* User-friendly name */
	NULL,                         /* Internal data */
	x509_tls_peers_init,          /* init */
	NULL,                         /* uninit not required */
	x509_tls_peers_cert_in_pool,  /* Certificate exists? */
	x509_tls_peers_get_cert,      /* Cert retriever */
	x509_tls_peers_put_cert       /* Cert writer */
};



static PurpleCertificateVerifier x509_tls_cached;

static void
x509_tls_cached_unknown_peer_cb (PurpleCertificateVerificationRequest *vrq, gint id)
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

static void
x509_tls_cached_unknown_peer(PurpleCertificateVerificationRequest *vrq)
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
	primary = g_strdup_printf(_("%s has presented the following certificate:"), vrq->subject_name);
	secondary = g_strdup_printf(_("Common name: %s %s\nFingerprint (SHA1): %s"), cn, cn_match, sha_asc);
	
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
		x509_tls_cached_unknown_peer_cb,
		x509_tls_cached_unknown_peer_cb );
	
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
x509_tls_cached_start_verify(PurpleCertificateVerificationRequest *vrq)
{
	PurpleCertificate *peer_crt = (PurpleCertificate *) vrq->cert_chain->data;
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
		PurpleCertificate *cached_crt;
		GByteArray *peer_fpr, *cached_fpr;

		purple_debug_info("certificate/x509/tls_cached",
				  "...Found cached cert\n");
				
		/* Load up the cached certificate */
		cached_crt = purple_certificate_pool_retrieve(
			tls_peers, vrq->subject_name);

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
	/* Unregister the builtins */
	purple_certificate_unregister_verifier(&x509_singleuse);
	purple_certificate_unregister_pool(&x509_tls_peers);
	purple_certificate_unregister_verifier(&x509_tls_cached);
	
	/* TODO: Unregistering everything would be good... */
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
		
	return TRUE;
}
