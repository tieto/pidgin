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

/* Pre-installed SSL op functions. */
#ifdef HAVE_NSS
GaimSslOps *gaim_ssl_nss_get_ops();
#endif

#ifdef HAVE_GNUTLS
GaimSslOps *gaim_ssl_gnutls_get_ops();
#endif


static gboolean _ssl_initialized = FALSE;
static GaimSslOps *_ssl_ops = NULL;

static gboolean
ssl_init(void)
{
	GaimSslOps *ops = gaim_ssl_get_ops();
	gboolean success = FALSE;

	if (_ssl_initialized)
		return FALSE;

	if (ops != NULL && ops->init != NULL)
		success = ops->init();

	_ssl_initialized = success;

	return success;
}

gboolean
gaim_ssl_is_supported(void)
{
#ifdef HAVE_SSL
	return (gaim_ssl_get_ops() != NULL);
#else
	return FALSE;
#endif
}

GaimSslConnection *
gaim_ssl_connect(GaimAccount *account, const char *host, int port,
				 GaimSslInputFunction func, void *data)
{
	GaimSslConnection *gsc;
	GaimSslOps *ops;
	int i;

	g_return_val_if_fail(host != NULL,            NULL);
	g_return_val_if_fail(port != 0 && port != -1, NULL);
	g_return_val_if_fail(func != NULL,            NULL);
	g_return_val_if_fail(gaim_ssl_is_supported(), NULL);

	ops = gaim_ssl_get_ops();

	g_return_val_if_fail(ops != NULL, NULL);
	g_return_val_if_fail(ops->connect_cb != NULL, NULL);

	if (!_ssl_initialized)
	{
		if (!ssl_init())
			return NULL;
	}

	gsc = g_new0(GaimSslConnection, 1);

	gsc->host       = g_strdup(host);
	gsc->port       = port;
	gsc->user_data  = data;
	gsc->input_func = func;

	i = gaim_proxy_connect(account, host, port, ops->connect_cb, gsc);

	if (i < 0)
	{
		g_free(gsc->host);
		g_free(gsc);

		return NULL;
	}

	return (GaimSslConnection *)gsc;
}

GaimSslConnection *
gaim_ssl_connect_fd(GaimAccount *account, int fd,
					GaimSslInputFunction func, void *data)
{
	GaimSslConnection *gsc;
	GaimSslOps *ops;

	g_return_val_if_fail(fd > 0,                  NULL);
	g_return_val_if_fail(func != NULL,            NULL);
	g_return_val_if_fail(gaim_ssl_is_supported(), NULL);

	ops = gaim_ssl_get_ops();

	g_return_val_if_fail(ops != NULL, NULL);
	g_return_val_if_fail(ops->connect_cb != NULL, NULL);

	if (!_ssl_initialized)
	{
		if (!ssl_init())
			return NULL;
	}

	gsc = g_new0(GaimSslConnection, 1);

	gsc->user_data  = data;
	gsc->input_func = func;

	ops->connect_cb(gsc, fd, GAIM_INPUT_READ);

	return (GaimSslConnection *)gsc;
}

void
gaim_ssl_close(GaimSslConnection *gsc)
{
	GaimSslOps *ops;

	g_return_if_fail(gsc != NULL);

	ops = gaim_ssl_get_ops();

	if (gsc->inpa)
		gaim_input_remove(gsc->inpa);

	if (ops != NULL && ops->close != NULL)
		(ops->close)(gsc);

	if (gsc->fd)
		close(gsc->fd);

	if (gsc->host != NULL)
		g_free(gsc->host);

	g_free(gsc);
}

size_t
gaim_ssl_read(GaimSslConnection *gsc, void *data, size_t len)
{
	GaimSslOps *ops;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);

	ops = gaim_ssl_get_ops();

	if (ops != NULL && (ops->read) != NULL)
		return (ops->read)(gsc, data, len);

	return 0;
}

size_t
gaim_ssl_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslOps *ops;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);

	ops = gaim_ssl_get_ops();

	if (ops != NULL && (ops->write) != NULL)
		return (ops->write)(gsc, data, len);

	return 0;
}

void
gaim_ssl_set_ops(GaimSslOps *ops)
{
	_ssl_ops = ops;
}

GaimSslOps *
gaim_ssl_get_ops(void)
{
	return _ssl_ops;
}

void
gaim_ssl_init(void)
{
#if defined(HAVE_NSS)
	gaim_ssl_set_ops(gaim_ssl_nss_get_ops());
#elif defined(HAVE_GNUTLS)
	gaim_ssl_set_ops(gaim_ssl_gnutls_get_ops());
#endif
}

void
gaim_ssl_uninit(void)
{
	GaimSslOps *ops;

	if (!_ssl_initialized)
		return;

	ops = gaim_ssl_get_ops();

	if (ops != NULL && ops->uninit != NULL)
		ops->uninit();

	_ssl_initialized = FALSE;
}
