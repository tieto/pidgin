#include "module.h"

MODULE = Gaim::Connection  PACKAGE = Gaim::Connection  PREFIX = gaim_connection_
PROTOTYPES: ENABLE

void
gaim_connection_set_display_name(gc, name)
	Gaim::Connection gc
	const char *name

Gaim::Account
gaim_connection_get_account(gc)
	Gaim::Connection gc

const char *
gaim_connection_get_display_name(gc)
	Gaim::Connection gc


MODULE = Gaim::Connections  PACKAGE = Gaim::Connections  PREFIX = gaim_connections_
PROTOTYPES: ENABLE

void
gaim_connections_disconnect_all()

void *
handle()
CODE:
	RETVAL = gaim_connections_get_handle();
OUTPUT:
	RETVAL


MODULE = Gaim::Connection  PACKAGE = Gaim

void
connections()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_connections_get_all(); l != NULL; l = l->next)
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Connection")));
