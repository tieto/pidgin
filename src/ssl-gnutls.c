/**
 * @file ssl-gnutls.c SSL Operations for GNUTLS
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
#include "debug.h"
#include "sslconn.h"

#include <gnutls/gnutls.h>

typedef struct
{
	gnutls_session session;
	gnutls_certificate_client_credentials xcred;

} GaimSslGnutlsData;

#define GAIM_SSL_GNUTLS_DATA(gsc) ((GaimSslGnutlsData *)gsc->private_data)

static gnutls_certificate_client_credentials xcred;

static gboolean
ssl_gnutls_init(void)
{
	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&xcred);
	gnutls_certificate_set_x509_trust_file(xcred, "ca.pem", GNUTLS_X509_FMT_PEM);

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
	int ret;
	const int cert_type_priority[2] = { GNUTLS_CRT_X509, 0 };

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
	}
	else
	{
	gaim_debug_info("gnutls", "Calling input function\n");
	gsc->input_func(gsc->user_data, (GaimSslConnection *)gsc, cond);
	}
}

static void
ssl_gnutls_close(GaimSslConnection *gsc)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);

	gnutls_bye(gnutls_data->session, GNUTLS_SHUT_RDWR);

	gnutls_deinit(gnutls_data->session);
//	gnutls_certificate_free_credentials(gnutls_data->xcred);

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

	gaim_debug_misc("gnutls", "s = %d\n", s);

	return s;
}

static size_t
ssl_gnutls_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslGnutlsData *gnutls_data = GAIM_SSL_GNUTLS_DATA(gsc);
	size_t s;

	gaim_debug_misc("gnutls", "Writing: {%s}\n", data);

	s = gnutls_record_send(gnutls_data->session, data, len);
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

GaimSslOps *
gaim_ssl_gnutls_get_ops()
{
	return &ssl_ops;
}
