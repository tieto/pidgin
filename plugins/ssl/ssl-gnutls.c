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
#include "plugin.h"

#define SSL_GNUTLS_PLUGIN_ID "ssl-gnutls"

#ifdef HAVE_GNUTLS

#include "debug.h"
#include "sslconn.h"

#include <gnutls/gnutls.h>

typedef struct
{
	gnutls_session session;

} GaimSslGnutlsData;

#define GAIM_SSL_GNUTLS_DATA(gsc) ((GaimSslGnutlsData *)gsc->private_data)

static gnutls_certificate_client_credentials xcred;

static gboolean
ssl_gnutls_init(void)
{
	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&xcred);
	gnutls_certificate_set_x509_trust_file(xcred, "ca.pem",
										   GNUTLS_X509_FMT_PEM);

	return TRUE;
}

static void
ssl_gnutls_uninit(void)
{
	gnutls_global_deinit();

	gnutls_certificate_free_credentials(xcred);
}

static void
ssl_gnutls_connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimSslConnection *gsc = (GaimSslConnection *)data;
	GaimSslGnutlsData *gnutls_data;
	static const int cert_type_priority[2] = { GNUTLS_CRT_X509, 0 };
	int ret;

	if (source < 0)
		return;

	gsc->fd = source;

	gnutls_data = g_new0(GaimSslGnutlsData, 1);
	gsc->private_data = gnutls_data;

	gnutls_init(&gnutls_data->session, GNUTLS_CLIENT);
	gnutls_set_default_priority(gnutls_data->session);

	gnutls_certificate_type_set_priority(gnutls_data->session,
										 cert_type_priority);

	gnutls_credentials_set(gnutls_data->session, GNUTLS_CRD_CERTIFICATE,
						   xcred);

	gnutls_transport_set_ptr(gnutls_data->session, GINT_TO_POINTER(source));

	gaim_debug_info("gnutls", "Handshaking\n");
	ret = gnutls_handshake(gnutls_data->session);

	if (ret < 0)
	{
		gaim_debug_error("gnutls", "Handshake failed\n");

		/* XXX: notify the guy expecting the callback somehow? */
		gaim_ssl_close(gsc);
	}
	else
	{
		gsc->connect_cb(gsc->connect_cb_data, gsc, cond);
	}
}

static void
ssl_gnutls_close(GaimSslConnection *gsc)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);

	gnutls_bye(gnutls_data->session, GNUTLS_SHUT_RDWR);

	gnutls_deinit(gnutls_data->session);

	g_free(gnutls_data);
}

static size_t
ssl_gnutls_read(GaimSslConnection *gsc, void *data, size_t len)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);
	int s;

	s = gnutls_record_recv(gnutls_data->session, data, len);

	if (s < 0)
		s = 0;

	return s;
}

static size_t
ssl_gnutls_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);
	size_t s;

	s = gnutls_record_send(gnutls_data->session, data, len);

	if (s < 0)
		s = 0;

	return s;
}

static GaimSslOps ssl_ops =
{
	ssl_gnutls_init,
	ssl_gnutls_uninit,
	ssl_gnutls_connect_cb,
	ssl_gnutls_close,
	ssl_gnutls_read,
	ssl_gnutls_write
};

#endif /* HAVE_GNUTLS */

static gboolean
plugin_load(GaimPlugin *plugin)
{
#ifdef HAVE_GNUTLS
	gaim_ssl_set_ops(&ssl_ops);

	return TRUE;
#else
	return FALSE;
#endif
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	return TRUE;
}

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
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
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(ssl_gnutls, init_plugin, info)
