/**
 * @file sslconn.c SSL API
 * @ingroup core
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#include "internal.h"

#include "debug.h"
#include "sslconn.h"

static gboolean _ssl_initialized = FALSE;
static GaimSslOps *_ssl_ops = NULL;

static gboolean
ssl_init(void)
{
	GaimPlugin *plugin;
	GaimSslOps *ops;

	if (_ssl_initialized)
		return FALSE;

	plugin = gaim_plugins_find_with_id("core-ssl");

	if (plugin != NULL && !gaim_plugin_is_loaded(plugin))
		gaim_plugin_load(plugin);

	ops = gaim_ssl_get_ops();
	if ((ops == NULL) || (ops->init == NULL) || (ops->uninit == NULL) ||
		(ops->connect == NULL) || (ops->close == NULL) ||
		(ops->read == NULL) || (ops->write == NULL))
	{
		return FALSE;
	}

	return ops->init();
}

gboolean
gaim_ssl_is_supported(void)
{
#ifdef HAVE_SSL
	ssl_init();
	return (gaim_ssl_get_ops() != NULL);
#else
	return FALSE;
#endif
}

static void
gaim_ssl_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	GaimSslConnection *gsc;
	GaimSslOps *ops;
	void (*connect_func)(GaimSslConnection *gsc);

	gsc = data;
	gsc->connect_info = NULL;

	if (source < 0)
	{
		if (gsc->error_cb != NULL)
			gsc->error_cb(gsc, GAIM_SSL_CONNECT_FAILED, gsc->connect_cb_data);

		gaim_ssl_close(gsc);
		return;
	}

	gsc->fd = source;

	ops = gaim_ssl_get_ops();
	connect_func = (ops->connect);
	connect_func(gsc);
}

GaimSslConnection *
gaim_ssl_connect(GaimAccount *account, const char *host, int port,
				 GaimSslInputFunction func, GaimSslErrorFunction error_func,
				 void *data)
{
	GaimSslConnection *gsc;

	g_return_val_if_fail(host != NULL,            NULL);
	g_return_val_if_fail(port != 0 && port != -1, NULL);
	g_return_val_if_fail(func != NULL,            NULL);
	g_return_val_if_fail(gaim_ssl_is_supported(), NULL);

	if (!_ssl_initialized)
	{
		if (!ssl_init())
			return NULL;
	}

	gsc = g_new0(GaimSslConnection, 1);

	gsc->fd              = -1;
	gsc->host            = g_strdup(host);
	gsc->port            = port;
	gsc->connect_cb_data = data;
	gsc->connect_cb      = func;
	gsc->error_cb        = error_func;

	gsc->connect_info = gaim_proxy_connect(account, host, port, gaim_ssl_connect_cb, gsc);

	if (gsc->connect_info == NULL)
	{
		g_free(gsc->host);
		g_free(gsc);

		return NULL;
	}

	return (GaimSslConnection *)gsc;
}

static void
recv_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimSslConnection *gsc = data;

	gsc->recv_cb(gsc->recv_cb_data, gsc, cond);
}

void
gaim_ssl_input_add(GaimSslConnection *gsc, GaimSslInputFunction func,
				   void *data)
{
	g_return_if_fail(func != NULL);
	g_return_if_fail(gaim_ssl_is_supported());

	gsc->recv_cb_data = data;
	gsc->recv_cb      = func;

	gsc->inpa = gaim_input_add(gsc->fd, GAIM_INPUT_READ, recv_cb, gsc);
}

GaimSslConnection *
gaim_ssl_connect_fd(GaimAccount *account, int fd,
					GaimSslInputFunction func,
					GaimSslErrorFunction error_func, void *data)
{
	GaimSslConnection *gsc;
	GaimSslOps *ops;
	void (*connect_func)(GaimSslConnection *gsc);

	g_return_val_if_fail(fd != -1,                NULL);
	g_return_val_if_fail(func != NULL,            NULL);
	g_return_val_if_fail(gaim_ssl_is_supported(), NULL);

	if (!_ssl_initialized)
	{
		if (!ssl_init())
			return NULL;
	}

	gsc = g_new0(GaimSslConnection, 1);

	gsc->connect_cb_data = data;
	gsc->connect_cb      = func;
	gsc->error_cb        = error_func;
	gsc->fd              = fd;

	ops = gaim_ssl_get_ops();
	connect_func = ops->connect;
	connect_func(gsc);

	return (GaimSslConnection *)gsc;
}

void
gaim_ssl_close(GaimSslConnection *gsc)
{
	GaimSslOps *ops;

	g_return_if_fail(gsc != NULL);

	ops = gaim_ssl_get_ops();
	(ops->close)(gsc);

	if (gsc->connect_info != NULL)
		gaim_proxy_connect_cancel(gsc->connect_info);

	if (gsc->inpa > 0)
		gaim_input_remove(gsc->inpa);

	if (gsc->fd >= 0)
		close(gsc->fd);

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
	return (ops->read)(gsc, data, len);
}

size_t
gaim_ssl_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslOps *ops;

	g_return_val_if_fail(gsc  != NULL, 0);
	g_return_val_if_fail(data != NULL, 0);
	g_return_val_if_fail(len  >  0,    0);

	ops = gaim_ssl_get_ops();
	return (ops->write)(gsc, data, len);
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
}

void
gaim_ssl_uninit(void)
{
	GaimSslOps *ops;

	if (!_ssl_initialized)
		return;

	ops = gaim_ssl_get_ops();
	ops->uninit();

	_ssl_initialized = FALSE;
}
