/**
 * @file ssl-nss.c Mozilla NSS SSL plugin.
 *
 * purple
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
	guint handshake_handler;

} PurpleSslNssData;

#define PURPLE_SSL_NSS_DATA(gsc) ((PurpleSslNssData *)gsc->private_data)

static const PRIOMethods *_nss_methods = NULL;
static PRDescIdentity _identity;

/* Thank you, Evolution */
static void
set_errno(int code)
{
	/* FIXME: this should handle more. */
	switch (code) {
	case PR_INVALID_ARGUMENT_ERROR:
		errno = EINVAL;
		break;
	case PR_PENDING_INTERRUPT_ERROR:
		errno = EINTR;
		break;
	case PR_IO_PENDING_ERROR:
		errno = EAGAIN;
		break;
	case PR_WOULD_BLOCK_ERROR:
		errno = EAGAIN;
		/*errno = EWOULDBLOCK; */
		break;
	case PR_IN_PROGRESS_ERROR:
		errno = EINPROGRESS;
		break;
	case PR_ALREADY_INITIATED_ERROR:
		errno = EALREADY;
		break;
	case PR_NETWORK_UNREACHABLE_ERROR:
		errno = EHOSTUNREACH;
		break;
	case PR_CONNECT_REFUSED_ERROR:
		errno = ECONNREFUSED;
		break;
	case PR_CONNECT_TIMEOUT_ERROR:
	case PR_IO_TIMEOUT_ERROR:
		errno = ETIMEDOUT;
		break;
	case PR_NOT_CONNECTED_ERROR:
		errno = ENOTCONN;
		break;
	case PR_CONNECT_RESET_ERROR:
		errno = ECONNRESET;
		break;
	case PR_IO_ERROR:
	default:
		errno = EIO;
		break;
	}
}

static void
ssl_nss_init_nss(void)
{
	char *lib;
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
	NSS_NoDB_Init(NULL);

	/* TODO: Fix this so autoconf does the work trying to find this lib. */
#ifndef _WIN32
	lib = g_strdup(LIBDIR "/libnssckbi.so");
#else
	lib = g_strdup("nssckbi.dll");
#endif
	SECMOD_AddNewModule("Builtins", lib, 0, 0);
	g_free(lib);
	NSS_SetDomesticPolicy();

	_identity = PR_GetUniqueIdentity("Purple");
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
		purple_debug_error("nss", "CERT_VerifyCertNow failed\n");
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

	purple_debug_error("nss", "Bad certificate: %d\n", err);

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
ssl_nss_handshake_cb(gpointer data, int fd, PurpleInputCondition cond)
{
	PurpleSslConnection *gsc = (PurpleSslConnection *)data;
	PurpleSslNssData *nss_data = gsc->private_data;

	/* I don't think this the best way to do this...
	 * It seems to work because it'll eventually use the cached value
	 */
	if(SSL_ForceHandshake(nss_data->in) != SECSuccess) {
		set_errno(PR_GetError());
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;

		purple_debug_error("nss", "Handshake failed %d\n", PR_GetError());

		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, PURPLE_SSL_HANDSHAKE_FAILED, gsc->connect_cb_data);

		purple_ssl_close(gsc);

		return;
	}

	purple_input_remove(nss_data->handshake_handler);
	nss_data->handshake_handler = 0;

	gsc->connect_cb(gsc->connect_cb_data, gsc, cond);
}

static void
ssl_nss_connect(PurpleSslConnection *gsc)
{
	PurpleSslNssData *nss_data = g_new0(PurpleSslNssData, 1);
	PRSocketOptionData socket_opt;

	gsc->private_data = nss_data;

	nss_data->fd = PR_ImportTCPSocket(gsc->fd);

	if (nss_data->fd == NULL)
	{
		purple_debug_error("nss", "nss_data->fd == NULL!\n");

		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, PURPLE_SSL_CONNECT_FAILED, gsc->connect_cb_data);

		purple_ssl_close((PurpleSslConnection *)gsc);

		return;
	}

	socket_opt.option = PR_SockOpt_Nonblocking;
	socket_opt.value.non_blocking = PR_TRUE;

	if (PR_SetSocketOption(nss_data->fd, &socket_opt) != PR_SUCCESS)
		purple_debug_warning("nss", "unable to set socket into non-blocking mode: %d\n", PR_GetError());

	nss_data->in = SSL_ImportFD(NULL, nss_data->fd);

	if (nss_data->in == NULL)
	{
		purple_debug_error("nss", "nss_data->in == NUL!\n");

		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, PURPLE_SSL_CONNECT_FAILED, gsc->connect_cb_data);

		purple_ssl_close((PurpleSslConnection *)gsc);

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

#if 0
	/* This seems like it'd the be the correct way to implement the
	nonblocking stuff, but it doesn't seem to work */
	SSL_HandshakeCallback(nss_data->in,
		(SSLHandshakeCallback) ssl_nss_handshake_cb, gsc);
#endif
	SSL_ResetHandshake(nss_data->in, PR_FALSE);

	nss_data->handshake_handler = purple_input_add(gsc->fd,
		PURPLE_INPUT_READ, ssl_nss_handshake_cb, gsc);

	ssl_nss_handshake_cb(gsc, gsc->fd, PURPLE_INPUT_READ);
}

static void
ssl_nss_close(PurpleSslConnection *gsc)
{
	PurpleSslNssData *nss_data = PURPLE_SSL_NSS_DATA(gsc);

	if(!nss_data)
		return;

	if (nss_data->in) PR_Close(nss_data->in);
	/* if (nss_data->fd) PR_Close(nss_data->fd); */

	if (nss_data->handshake_handler)
		purple_input_remove(nss_data->handshake_handler);

	g_free(nss_data);
	gsc->private_data = NULL;
}

static size_t
ssl_nss_read(PurpleSslConnection *gsc, void *data, size_t len)
{
	ssize_t ret;
	PurpleSslNssData *nss_data = PURPLE_SSL_NSS_DATA(gsc);

	ret = PR_Read(nss_data->in, data, len);

	if (ret == -1)
		set_errno(PR_GetError());

	return ret;
}

static size_t
ssl_nss_write(PurpleSslConnection *gsc, const void *data, size_t len)
{
	ssize_t ret;
	PurpleSslNssData *nss_data = PURPLE_SSL_NSS_DATA(gsc);

	if(!nss_data)
		return 0;

	ret = PR_Write(nss_data->in, data, len);

	if (ret == -1)
		set_errno(PR_GetError());

	return ret;
}

static PurpleSslOps ssl_ops =
{
	ssl_nss_init,
	ssl_nss_uninit,
	ssl_nss_connect,
	ssl_nss_close,
	ssl_nss_read,
	ssl_nss_write
};

#endif /* HAVE_NSS */


static gboolean
plugin_load(PurplePlugin *plugin)
{
#ifdef HAVE_NSS
	if (!purple_ssl_get_ops()) {
		purple_ssl_set_ops(&ssl_ops);
	}

	/* Init NSS now, so others can use it even if sslconn never does */
	ssl_nss_init_nss();

	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
#ifdef HAVE_NSS
	if (purple_ssl_get_ops() == &ssl_ops) {
		purple_ssl_set_ops(NULL);
	}
#endif

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	PURPLE_PLUGIN_FLAG_INVISIBLE,                       /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	SSL_NSS_PLUGIN_ID,                             /**< id             */
	N_("NSS"),                                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides SSL support through Mozilla NSS."),
	                                                  /**  description    */
	N_("Provides SSL support through Mozilla NSS."),
	"Christian Hammond <chipx86@gnupdate.org>",
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL                                              /**< actions        */
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(ssl_nss, init_plugin, info)
