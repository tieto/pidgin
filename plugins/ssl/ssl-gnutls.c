/**
 * @file ssl-gnutls.c GNUTLS SSL plugin.
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

#define SSL_GNUTLS_PLUGIN_ID "ssl-gnutls"

#ifdef HAVE_GNUTLS

#include <gnutls/gnutls.h>

typedef struct
{
	gnutls_session session;
	guint handshake_handler;
} GaimSslGnutlsData;

#define GAIM_SSL_GNUTLS_DATA(gsc) ((GaimSslGnutlsData *)gsc->private_data)

static gnutls_certificate_client_credentials xcred;

static void
ssl_gnutls_init_gnutls(void)
{
	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&xcred);
	gnutls_certificate_set_x509_trust_file(xcred, "ca.pem",
		GNUTLS_X509_FMT_PEM);
}

static gboolean
ssl_gnutls_init(void)
{
   return TRUE;
}

static void
ssl_gnutls_uninit(void)
{
	gnutls_global_deinit();

	gnutls_certificate_free_credentials(xcred);
}


static void ssl_gnutls_handshake_cb(gpointer data, gint source,
		GaimInputCondition cond)
{
	GaimSslConnection *gsc = data;
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);
	ssize_t ret;

	gaim_debug_info("gnutls", "Handshaking\n");
	ret = gnutls_handshake(gnutls_data->session);

	if(ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED)
		return;

	gaim_input_remove(gnutls_data->handshake_handler);
	gnutls_data->handshake_handler = 0;

	if(ret != 0) {
		gaim_debug_error("gnutls", "Handshake failed. Error %d\n", ret);

		if(gsc->error_cb != NULL)
			gsc->error_cb(gsc, GAIM_SSL_HANDSHAKE_FAILED,
				gsc->connect_cb_data);

		gaim_ssl_close(gsc);
	} else {
		gaim_debug_info("gnutls", "Handshake complete\n");

		gsc->connect_cb(gsc->connect_cb_data, gsc, cond);
	}

}


static void
ssl_gnutls_connect(GaimSslConnection *gsc)
{
	GaimSslGnutlsData *gnutls_data;
	static const int cert_type_priority[2] = { GNUTLS_CRT_X509, 0 };

	gnutls_data = g_new0(GaimSslGnutlsData, 1);
	gsc->private_data = gnutls_data;

	gnutls_init(&gnutls_data->session, GNUTLS_CLIENT);
	gnutls_set_default_priority(gnutls_data->session);

	gnutls_certificate_type_set_priority(gnutls_data->session,
		cert_type_priority);

	gnutls_credentials_set(gnutls_data->session, GNUTLS_CRD_CERTIFICATE,
		xcred);

	gnutls_transport_set_ptr(gnutls_data->session, GINT_TO_POINTER(gsc->fd));

	gnutls_data->handshake_handler = gaim_input_add(gsc->fd,
		GAIM_INPUT_READ, ssl_gnutls_handshake_cb, gsc);

	ssl_gnutls_handshake_cb(gsc, gsc->fd, GAIM_INPUT_READ);
}

static void
ssl_gnutls_close(GaimSslConnection *gsc)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);

	if(!gnutls_data)
		return;

	if(gnutls_data->handshake_handler)
		gaim_input_remove(gnutls_data->handshake_handler);

	gnutls_bye(gnutls_data->session, GNUTLS_SHUT_RDWR);

	gnutls_deinit(gnutls_data->session);

	g_free(gnutls_data);
	gsc->private_data = NULL;
}

static size_t
ssl_gnutls_read(GaimSslConnection *gsc, void *data, size_t len)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);
	ssize_t s;

	s = gnutls_record_recv(gnutls_data->session, data, len);

	if(s == GNUTLS_E_AGAIN || s == GNUTLS_E_INTERRUPTED) {
		s = -1;
		errno = EAGAIN;
	} else if(s < 0) {
		gaim_debug_error("gnutls", "receive failed: %d\n", s);
		s = 0;
	}

	return s;
}

static size_t
ssl_gnutls_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);
	ssize_t s = 0;

	/* XXX: when will gnutls_data be NULL? */
	if(gnutls_data)
		s = gnutls_record_send(gnutls_data->session, data, len);

	if(s == GNUTLS_E_AGAIN || s == GNUTLS_E_INTERRUPTED) {
		s = -1;
		errno = EAGAIN;
	} else if(s < 0) {
		gaim_debug_error("gnutls", "send failed: %d\n", s);
		s = 0;
	}

	return s;
}

static GaimSslOps ssl_ops =
{
	ssl_gnutls_init,
	ssl_gnutls_uninit,
	ssl_gnutls_connect,
	ssl_gnutls_close,
	ssl_gnutls_read,
	ssl_gnutls_write
};

#endif /* HAVE_GNUTLS */

static gboolean
plugin_load(GaimPlugin *plugin)
{
#ifdef HAVE_GNUTLS
	if(!gaim_ssl_get_ops()) {
		gaim_ssl_set_ops(&ssl_ops);
	}

	/* Init GNUTLS now so others can use it even if sslconn never does */
	ssl_gnutls_init_gnutls();

	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
#ifdef HAVE_GNUTLS
	if(gaim_ssl_get_ops() == &ssl_ops) {
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

	SSL_GNUTLS_PLUGIN_ID,                             /**< id             */
	N_("GNUTLS"),                                     /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Provides SSL support through GNUTLS."),
	                                                  /**  description    */
	N_("Provides SSL support through GNUTLS."),
	"Christian Hammond <chipx86@gnupdate.org>",
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL                                              /**< actions        */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(ssl_gnutls, init_plugin, info)
