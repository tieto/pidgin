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

/** List holding pointers to all registered certificate schemes */
static GList *cert_schemes = NULL;
/** List of registered Verifiers */
static GList *cert_verifiers = NULL;

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
	g_return_if_fail(scheme !=
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

	/* TODO: Should I make it easier on the Verifier programmer and
	   clean up some of vrq's internals here? */
	
	/* Fetch the Verifier responsible... */
	vr = vrq->verifier;
	/* ...and order it to KILL */
	(vr->destroy_request)(vrq);
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
/****************************************************************************/
/* Subsystem                                                                */
/****************************************************************************/
void
purple_certificate_register_builtins(void)
{

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
	cert_schemes = g_list_append(cert_schemes, scheme);

	/* TODO: Signalling and such? */
	return TRUE;
}

gboolean
purple_certificate_unregister_scheme(PurpleCertificateScheme *scheme)
{
	if (NULL == scheme) {
		purple_debug_warning("certificate",
				     "Attempting to unregister NULL scheme");
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
	cert_verifiers = g_list_append(cert_verifiers, vr);

	/* TODO: Signalling and such? */
	return TRUE;
}

gboolean
purple_certificate_unregister_verifier(PurpleCertificateVerifier *vr)
{
	if (NULL == vr) {
		purple_debug_warning("certificate",
				     "Attempting to unregister NULL verifier");
	}

	/* TODO: signalling? */

	cert_verifiers = g_list_remove(cert_verifiers, vr);

	return TRUE;
}
