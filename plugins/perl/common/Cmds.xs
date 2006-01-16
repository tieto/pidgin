#include "module.h"
#include "../perl-handlers.h"

MODULE = Gaim::Cmd  PACKAGE = Gaim::Cmd  PREFIX = gaim_cmd_
PROTOTYPES: ENABLE

void
gaim_cmd_help(conv, command)
	Gaim::Conversation conv
	const gchar *command
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_cmd_help(conv, command); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(newSVpv(l->data, 0)));
	}

void
gaim_cmd_list(conv)
	Gaim::Conversation conv
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_cmd_list(conv); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(newSVpv(l->data, 0)));
	}

Gaim::Cmd::Id
gaim_cmd_register(plugin, command, args, priority, flag, prpl_id, func, helpstr, data = 0)
	Gaim::Plugin plugin
	const gchar *command
	const gchar *args
	Gaim::Cmd::Priority priority
	Gaim::Cmd::Flag flag
	const gchar *prpl_id
	SV *func
	const gchar *helpstr
	SV *data
CODE:
	RETVAL = gaim_perl_cmd_register(plugin, command, args, priority, flag,
	                                prpl_id, func, helpstr, data);
OUTPUT:
	RETVAL

void
gaim_cmd_unregister(id)
	Gaim::Cmd::Id id
CODE:
	gaim_perl_cmd_unregister(id);
