/**
 * @file ssl-nss.c Mozilla NSS SSL plugin.
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
#include "plugin.h"
#include "sslconn.h"
#include "version.h"

#define SSL_NSS_PLUGIN_ID "ssl-nss"

#ifdef HAVE_NSS

#undef HAVE_LONG_LONG /* Make Mozilla less angry. If angry, Mozilla SMASH! */

#include <nspr.h>
#include <private/pprio.h>
#include <nss.h>
#include <pk11func.h>
#include <prio.h>
#include <secerr.h>
#include <secmod.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

typedef struct
{
	PRFileDesc *fd;
	PRFileDesc *in;

} GaimSslNssData;

#define GAIM_SSL_NSS_DATA(gsc) ((GaimSslNssData *)gsc->private_data)

static const PRIOMethods *_nss_methods = NULL;
static PRDescIdentity _identity;

static void
ssl_nss_init_nss(void)
{
	char *lib;
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
	NSS_NoDB_Init(NULL);

	/* TODO: Fix this so autoconf does the work trying to find this lib. */
#ifndef _WIN32
	lib = g_strdup(BR_LIBDIR("/libnssckbi.so"));
#else
	lib = g_strdup("nssckbi.dll");
#endif
	SECMOD_AddNewModule("Builtins", lib, 0, 0);
	g_free(lib);
	NSS_SetDomesticPolicy();

	_identity = PR_GetUniqueIdentity("Gaim");
	_nss_methods = PR_GetDefaultIOMethods();
}

static SECStatus
ssl_auth_cert(void *arg, PRFileDesc *socket, PRBool checksig,
			  PRBool is_server)
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
		gaim_debug_error("nss", "CERT_VerifyCertNow failed\n");
		CERT_DestroyCertificate(cert);
		return status;
	}

	CERT_DestroyCertificate(cert);
	return SECSuccess;
#endif
}

static SECStatus
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

	gaim_debug_error("nss", "Bad certificate: %d\n");

	return status;
}

static gboolean
ssl_nss_init(void)
{
   return TRUE;
}

static void
ssl_nss_uninit(void)
{
	PR_Cleanup();

	_nss_methods = NULL;
}

static void
ssl_nss_connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimSslConnection *gsc = (GaimSslConnection *)data;
	GaimSslNssData *nss_data = g_new0(GaimSslNssData, 1);
	PRSocketOptionData socket_opt;

	gsc->private_data = nss_data;

	gsc->fd = source;

	nss_data->fd = PR_ImportTCPSocket(gsc->fd);

	if (nss_data->fd == NULL)
	{
		gaim_debug_error("nss", "nss_data->fd == NULL!\n");

		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, GAIM_SSL_CONNECT_FAILED, gsc->connect_cb_data);

		gaim_ssl_close((GaimSslConnection *)gsc);

		return;
	}

	socket_opt.option = PR_SockOpt_Nonblocking;
	socket_opt.value.non_blocking = PR_FALSE;

	PR_SetSocketOption(nss_data->fd, &socket_opt);

	nss_data->in = SSL_ImportFD(NULL, nss_data->fd);

	if (nss_data->in == NULL)
	{
		gaim_debug_error("nss", "nss_data->in == NUL!\n");

		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, GAIM_SSL_CONNECT_FAILED, gsc->connect_cb_data);

		gaim_ssl_close((GaimSslConnection *)gsc);

		return;
	}

	SSL_OptionSet(nss_data->in, SSL_SECURITY,            PR_TRUE);
	SSL_OptionSet(nss_data->in, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);

	SSL_AuthCertificateHook(nss_data->in,
							(SSLAuthCertificate)ssl_auth_cert,
							(void *)CERT_GetDefaultCertDB());
	SSL_BadCertHook(nss_data->in, (SSLBadCertHandler)ssl_bad_cert, NULL);

	if(gsc->host)
		SSL_SetURL(nss_data->in, gsc->host);

	SSL_ResetHandshake(nss_data->in, PR_FALSE);

	if (SSL_ForceHandshake(nss_data->in))
	{
		gaim_debug_error("nss", "Handshake failed\n");

		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, GAIM_SSL_HANDSHAKE_FAILED, gsc->connect_cb_data);

		gaim_ssl_close(gsc);

		return;
	}

	gsc->connect_cb(gsc->connect_cb_data, gsc, cond);
}

static void
ssl_nss_close(GaimSslConnection *gsc)
{
	GaimSslNssData *nss_data = GAIM_SSL_NSS_DATA(gsc);

	if(!nss_data)
		return;

	if (nss_data->in) PR_Close(nss_data->in);
	/* if (nss_data->fd) PR_Close(nss_data->fd); */

	g_free(nss_data);
}

static size_t
ssl_nss_read(GaimSslConnection *gsc, void *data, size_t len)
{
	GaimSslNssData *nss_data = GAIM_SSL_NSS_DATA(gsc);

	return PR_Read(nss_data->in, data, len);
}

static size_t
ssl_nss_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslNssData *nss_data = GAIM_SSL_NSS_DATA(gsc);

	if(!nss_data)
		return 0;

	return PR_Write(nss_data->in, data, len);
}

static GaimSslOps ssl_ops =
{
	ssl_nss_init,
	ssl_nss_uninit,
	ssl_nss_connect_cb,
	ssl_nss_close,
	ssl_nss_read,
	ssl_nss_write
};

#endif /* HAVE_NSS */


static gboolean
plugin_load(GaimPlugin *plugin)
{
#ifdef HAVE_NSS
	if (!gaim_ssl_get_ops()) {
		gaim_ssl_set_ops(&ssl_ops);
	}

	/* Init NSS now, so others can use it even if sslconn never does */
	ssl_nss_init_nss();

	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
#ifdef HAVE_NSS
	if (gaim_ssl_get_ops() == &ssl_ops) {
		gaim_ssl_set_ops(NULL);
	}
#endif

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	GAIM_PLUGIN_FLAG_INVISIBLE,                       /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	SSL_NSS_PLUGIN_ID,                             /**< id             */
	N_("NSS"),                                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides SSL support through Mozilla NSS."),
	                                                  /**  description    */
	N_("Provides SSL support through Mozilla NSS."),
	"Christian Hammond <chipx86@gnupdate.org>",
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(ssl_nss, init_plugin, info)
