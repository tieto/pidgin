#include "module.h"
#include "../perl-handlers.h"

MODULE = Purple::Cmd  PACKAGE = Purple::Cmd  PREFIX = purple_cmd_
PROTOTYPES: ENABLE

void
purple_cmd_help(conv, command)
	Purple::Conversation conv
	const gchar *command
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_cmd_help(conv, command); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(newSVpv(l->data, 0)));
	}

void
purple_cmd_list(conv)
	Purple::Conversation conv
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_cmd_list(conv); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(newSVpv(l->data, 0)));
	}

Purple::Cmd::Id
purple_cmd_register(plugin, command, args, priority, flag, prpl_id, func, helpstr, data = 0)
	Purple::Plugin plugin
	const gchar *command
	const gchar *args
	Purple::Cmd::Priority priority
	Purple::Cmd::Flag flag
	const gchar *prpl_id
	SV *func
	const gchar *helpstr
	SV *data
CODE:
	RETVAL = purple_perl_cmd_register(plugin, command, args, priority, flag,
	                                prpl_id, func, helpstr, data);
OUTPUT:
	RETVAL

void
purple_cmd_unregister(id)
	Purple::Cmd::Id id
CODE:
	purple_perl_cmd_unregister(id);
