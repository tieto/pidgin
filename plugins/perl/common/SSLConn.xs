#include "module.h"

/* TODO


Gaim::Ssl::Connection
gaim_ssl_connect(account, host, port, func, error_func, data)
	Gaim::Account account
	const char *host
	int port
	GaimSslInputFunction func
	GaimSslErrorFunction error_func

void
gaim_ssl_input_add(gsc, func, data)
	Gaim::Ssl::Connection gsc
	Gaim::SslInputFunction func

Gaim::Ssl::Connection
gaim_ssl_connect_fd(account, fd, func, error_func, data)
	Gaim::Account account
	int fd
	GaimSslInputFunction func
	GaimSslErrorFunction error_func

*/

MODULE = Gaim::SSL  PACKAGE = Gaim::SSL   PREFIX = gaim_ssl_
PROTOTYPES: ENABLE

void
gaim_ssl_close(gsc)
	Gaim::Ssl::Connection gsc

Gaim::Ssl::Ops
gaim_ssl_get_ops()

void
gaim_ssl_init()

gboolean
gaim_ssl_is_supported()

size_t
gaim_ssl_read(gsc, buffer, len)
	Gaim::Ssl::Connection gsc
	void * buffer
	size_t len

void
gaim_ssl_set_ops(ops)
	Gaim::Ssl::Ops ops

void
gaim_ssl_uninit()

size_t
gaim_ssl_write(gsc, buffer, len)
	Gaim::Ssl::Connection gsc
	void * buffer
	size_t len
