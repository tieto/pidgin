#include "tcpsocket.h"

#include "gg.h"

#include "debug.h"
#include "purple-socket.h"

#if GGP_ENABLE_GG11

static void
ggp_tcpsocket_connected(PurpleSocket *ps, const gchar *error, gpointer priv_gg)
{
	PurpleConnection *gc = purple_socket_get_connection(ps);
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	int fd = -1;

	if (error == NULL)
		fd = purple_socket_get_fd(ps);

	if (!gg_socket_manager_connected(ps, priv_gg, fd)) {
		purple_debug_error("gg", "socket not handled");
		purple_socket_destroy(ps);
	}

	if (info->inpa > 0)
		purple_input_remove(info->inpa);
	if (info->session->fd < 0)
		return;
	info->inpa = purple_input_add(info->session->fd,
		ggp_tcpsocket_inputcond_gg_to_purple(info->session->check),
		ggp_async_login_handler, gc);
}

static void*
ggp_tcpsocket_connect(void *_gc, const char *host, int port, int is_tls,
	int is_async, void *priv)
{
	PurpleConnection *gc = _gc;
	PurpleSocket *ps;

	g_return_val_if_fail(host != NULL, NULL);
	g_return_val_if_fail(is_async, NULL);

	purple_debug_misc("gg", "ggp_tcpsocket_connect(%p, %s:%d, %s, %p)",
		gc, host, port, is_tls ? "tls" : "tcp", priv);

	ps = purple_socket_new(gc);
	purple_socket_set_tls(ps, is_tls);
	purple_socket_set_host(ps, host);
	purple_socket_set_port(ps, port);
	if (!purple_socket_connect(ps, ggp_tcpsocket_connected, priv)) {
		purple_socket_destroy(ps);
		return NULL;
	}

	return ps;
}

static void
ggp_tcpsocket_close(void *_gc, void *_ps)
{
	PurpleSocket *ps = _ps;

	purple_socket_destroy(ps);
}

static ssize_t
ggp_tcpsocket_read(void *_gc, void *_ps, unsigned char *buffer, size_t bufsize)
{
	PurpleSocket *ps = _ps;

	return purple_socket_read(ps, buffer, bufsize);
}

static ssize_t
ggp_tcpsocket_write(void *_gc, void *_ps, const unsigned char *data, size_t len)
{
	PurpleSocket *ps = _ps;

	return purple_socket_write(ps, data, len);
}

void
ggp_tcpsocket_setup(PurpleConnection *gc, struct gg_login_params *glp)
{
	glp->socket_manager_type = purple_ssl_is_supported() ?
		GG_SOCKET_MANAGER_TYPE_TLS : GG_SOCKET_MANAGER_TYPE_TCP;
	glp->socket_manager.cb_data = gc;
	glp->socket_manager.connect = ggp_tcpsocket_connect;
	glp->socket_manager.close = ggp_tcpsocket_close;
	glp->socket_manager.read = ggp_tcpsocket_read;
	glp->socket_manager.write = ggp_tcpsocket_write;
}

#else

void
ggp_tcpsocket_setup(PurpleConnection *gc, struct gg_login_params *glp)
{
}

#endif

PurpleInputCondition
ggp_tcpsocket_inputcond_gg_to_purple(enum gg_check_t check)
{
	PurpleInputCondition cond = 0;

	if (check & GG_CHECK_READ)
		cond |= PURPLE_INPUT_READ;
	if (check & GG_CHECK_WRITE)
		cond |= PURPLE_INPUT_WRITE;

	return cond;
}
