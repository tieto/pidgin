#include "module.h"

MODULE = Gaim::Proxy  PACKAGE = Gaim::Proxy  PREFIX = gaim_proxy_
PROTOTYPES: ENABLE

Gaim::ProxyInfo
gaim_global_proxy_get_info()

void *
gaim_proxy_get_handle()

void
gaim_proxy_info_destroy(info)
	Gaim::ProxyInfo info

const char *
gaim_proxy_info_get_host(info)
	Gaim::ProxyInfo info

const char *
gaim_proxy_info_get_password(info)
	Gaim::ProxyInfo info

int
gaim_proxy_info_get_port(info)
	Gaim::ProxyInfo info

Gaim::ProxyType
gaim_proxy_info_get_type(info)
	Gaim::ProxyInfo info

const char *
gaim_proxy_info_get_username(info)
	Gaim::ProxyInfo info

Gaim::ProxyInfo
gaim_proxy_info_new()

void
gaim_proxy_info_set_host(info, host)
	Gaim::ProxyInfo info
	const char *host

void
gaim_proxy_info_set_password(info, password)
	Gaim::ProxyInfo info
	const char *password

void
gaim_proxy_info_set_port(info, port)
	Gaim::ProxyInfo info
	int port

void
gaim_proxy_info_set_type(info, type)
	Gaim::ProxyInfo info
	Gaim::ProxyType type

void
gaim_proxy_info_set_username(info, username)
	Gaim::ProxyInfo info
	const char *username

void
gaim_proxy_init()
