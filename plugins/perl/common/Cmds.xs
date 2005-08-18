#include "module.h"

MODULE = Gaim::Cmds  PACKAGE = Gaim::Cmds  PREFIX = gaim_cmd_
PROTOTYPES: ENABLE

void
gaim_cmd_help(conv, cmd)
	Gaim::Conversation conv
	const gchar *cmd
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_cmd_help(conv, cmd); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
        }

void
gaim_cmd_list(conv)
	Gaim::Conversation conv
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_cmd_list(conv); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListEntry")));
        }

void 
gaim_cmd_unregister(id)
	Gaim::CmdId id

