/**
 * @file sslconn.c SSL API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "internal.h"

#include "debug.h"
#include "sslconn.h"

#ifdef HAVE_NSS
# include <nspr.h>
# include <nss.h>
# include <pk11func.h>
# include <prio.h>
# include <secerr.h>
# include <secmod.h>
# include <ssl.h>
# include <sslerr.h>
# include <sslproto.h>

typedef struct
{
	char *host;
	int port;
	void *user_data;
	GaimSslInputFunction input_func;

	int fd;
	int inpa;

	PRFileDesc *nss_fd;
	PRFileDesc *nss_in;

} GaimSslData;

static gboolean _nss_initialized = FALSE;
static const PRIOMethods *_nss_methods = NULL;
static PRDescIdentity _identity;

static void
destroy_ssl_data(GaimSslData *data)
{
	if (data->inpa)   gaim_input_remove(data->inpa);
	if (data->nss_in) PR_Close(data->nss_in);
	if (data->nss_fd) PR_Close(data->nss_fd);
	if (data->fd)     close(data->fd);

	if (data->host != NULL)
		g_free(data->host);

	g_free(data);
}

static void
init_nss(void)
{
	if (_nss_initialized)
		return;

	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
	NSS_NoDB_Init(NULL);

	/* TODO: Fix this so autoconf does the work trying to find this lib. */
	SECMOD_AddNewModule("Builtins", LIBDIR "/libnssckbi.so", 0, 0);
	NSS_SetDomesticPolicy();

	_identity = PR_GetUniqueIdentity("Gaim");
	_nss_methods = PR_GetDefaultIOMethods();

	_nss_initialized = TRUE;
}

static SECStatus
ssl_auth_cert(void *arg, PRFileDesc *socket, PRBool checksig, PRBool is_server)
{
	return SECSuccess;

#if 0
	CERTCertificate *cert;
	void *pinArg;
	SECStatus status;

	cert = SSL_PeerCertificate(socket);
	pinArg = SSL_RevealPinArg(socket);

	status = CERT_VerifyCertNow((CERTCertDBHandle *)arg, cert, checksig,
								certUsageSSLClient, pinArg);

	if (status != SECSuccess) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn", "CERT_VerifyCertNow failed\n");
		CERT_DestroyCertificate(cert);
		return status;
	}

	CERT_DestroyCertificate(cert);
	return SECSuccess;
#endif
}

SECStatus
ssl_bad_cert(void *arg, PRFileDesc *socket)
{
	SECStatus status = SECFailure;
	PRErrorCode err;

	if (arg == NULL)
		return status;

	*(PRErrorCode *)arg = err = PORT_GetError();

	switch (err)
	{
		case SEC_ERROR_INVALID_AVA:
		case SEC_ERROR_INVALID_TIME:
		case SEC_ERROR_BAD_SIGNATURE:
		case SEC_ERROR_EXPIRED_CERTIFICATE:
		case SEC_ERROR_UNKNOWN_ISSUER:
		case SEC_ERROR_UNTRUSTED_CERT:
		case SEC_ERROR_CERT_VALID:
		case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
		case SEC_ERROR_CRL_EXPIRED:
		case SEC_ERROR_CRL_BAD_SIGNATURE:
		case SEC_ERROR_EXTENSION_VALUE_INVALID:
		case SEC_ERROR_CA_CERT_INVALID:
		case SEC_ERROR_CERT_USAGES_INVALID:
		case SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION:
			status = SECSuccess;
			break;

		default:
			status = SECFailure;
			break;
	}

	gaim_debug(GAIM_DEBUG_ERROR, "msn",
			   "Bad certificate: %d\n");

	return status;
}

static void
input_func(gpointer data, gint source, GaimInputCondition cond)
{
	GaimSslData *ssl_data = (GaimSslData *)data;
	char *cp, *ip, *sp;
	int op, kp0, kp1;
	int result;

	result = SSL_SecurityStatus(ssl_data->nss_in, &op, &cp, &kp0,
								&kp1, &ip, &sp);

	gaim_debug(GAIM_DEBUG_MISC, "msn",
		"bulk cipher %s, %d secret key bits, %d key bits, status: %d\n"
		"subject DN: %s\n"
		"issuer  DN: %s\n",
		cp, kp1, kp0, op, sp, ip);

	PR_Free(cp);
	PR_Free(ip);
	PR_Free(sp);

	ssl_data->input_func(ssl_data->user_data, (GaimSslConnection *)ssl_data,
						 cond);
}

static void
ssl_connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	PRSocketOptionData socket_opt;
	GaimSslData *ssl_data = (GaimSslData *)data;

	if (!_nss_initialized)
		init_nss();

	ssl_data->fd = source;

	ssl_data->nss_fd = PR_ImportTCPSocket(ssl_data->fd);

	if (ssl_data->nss_fd == NULL)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "ssl", "nss_fd == NULL!\n");

		destroy_ssl_data(ssl_data);

		return;
	}

	socket_opt.option = PR_SockOpt_Nonblocking;
	socket_opt.value.non_blocking = PR_FALSE;

	PR_SetSocketOption(ssl_data->nss_fd, &socket_opt);

	ssl_data->nss_in = SSL_ImportFD(NULL, ssl_data->nss_fd);

	if (ssl_data->nss_in == NULL)
	{
		gaim_debug(GAIM_DEBUG_ERROR, "ssl", "nss_in == NUL!\n");

		destroy_ssl_data(ssl_data);

		return;
	}

	SSL_OptionSet(ssl_data->nss_in, SSL_SECURITY,            PR_TRUE);
	SSL_OptionSet(ssl_data->nss_in, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);

	SSL_AuthCertificateHook(ssl_data->nss_in,
							(SSLAuthCertificate)ssl_auth_cert,
							(void *)CERT_GetDefaultCertDB());
	SSL_BadCertHook(ssl_data->nss_in, (SSLBadCertHandler)ssl_bad_cert, NULL);

	SSL_SetURL(ssl_data->nss_in, ssl_data->host);

	SSL_ResetHandshake(ssl_data->nss_in, PR_FALSE);

	if (SSL_ForceHandshake(ssl_data->nss_in))
	{
		gaim_debug(GAIM_DEBUG_ERROR, "ssl", "Handshake failed\n");

		destroy_ssl_data(ssl_data);

		return;
	}

#if 0
	ssl_data->input_func(ssl_data->user_data, (GaimSslConnection *)ssl_data,
						 cond);
#endif

	input_func(ssl_data, source, cond);
}
#endif /* HAVE_NSS */

gboolean
gaim_ssl_is_supported(void)
{
#ifdef HAVE_NSS
	return TRUE;
#else
	return FALSE;
#endif
}

GaimSslConnection *
gaim_ssl_connect(GaimAccount *account, const char *host, int port,
				 GaimSslInputFunction func, void *data)
{
#ifdef HAVE_NSS
	int i;
	GaimSslData *ssl_data;

	g_return_val_if_fail(host != NULL,            NULL);
	g_return_val_if_fail(port != 0 && port != -1, NULL);
	g_return_val_if_fail(func != NULL,            NULL);
	g_return_val_if_fail(gaim_ssl_is_supported(), NULL);

	ssl_data = g_new0(GaimSslData, 1);

	ssl_data->host       = g_strdup(host);
	ssl_data->port       = port;
	ssl_data->user_data  = data;
	ssl_data->input_func = func;

	i = gaim_proxy_connect(account, host, port, ssl_connect_cb, ssl_data);

	if (i < 0)
	{
		g_free(ssl_data->host);
		g_free(ssl_data);

		return NULL;
	}

	return (GaimSslConnection)ssl_data;
#else
	return GINT_TO_POINTER(-1);
#endif
}

void
gaim_ssl_close(GaimSslConnection *gsc)
{
	g_return_if_fail(gsc != NULL);

#ifdef HAVE_NSS
	destroy_ssl_data((GaimSslData *)gsc);
#endif
}

size_t
gaim_ssl_read(GaimSslConnection *gsc, void *data, size_t len)
{
#ifdef HAVE_NSS
	GaimSslData *ssl_data = (GaimSslData *)gsc;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);

	return PR_Read(ssl_data->nss_in, data, len);
#else
	return 0;
#endif
}

size_t
gaim_ssl_write(GaimSslConnection *gsc, const void *data, size_t len)
{
#ifdef HAVE_NSS
	GaimSslData *ssl_data = (GaimSslData *)gsc;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);

	return PR_Write(ssl_data->nss_in, data, len);
#else
	return 0;
#endif
}

void
gaim_ssl_init(void)
{
}

void
gaim_ssl_uninit(void)
{
#ifdef HAVE_NSS
	if (!_nss_initialized)
		return;

	PR_Cleanup();

	_nss_initialized = FALSE;
	_nss_methods     = NULL;
#endif
}
