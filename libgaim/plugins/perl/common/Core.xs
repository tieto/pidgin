#include "module.h"

MODULE = Gaim::Core  PACKAGE = Gaim::Core  PREFIX = gaim_core_
PROTOTYPES: ENABLE

gboolean 
gaim_core_quit_cb()
PPCODE:
	/* The argument to gaim_core_quit_cb is not used,
	 * so there's little point in requiring it on the
	 * Perl side. */
	RETVAL = gaim_core_quit_cb(NULL);
	ST(0) = boolSV(RETVAL);
	sv_2mortal(ST(0));

const char *
gaim_core_get_version()

const char *
gaim_core_get_ui()

void
gaim_core_set_ui_ops(ops)
    Gaim::Core::UiOps ops

Gaim::Core::UiOps
gaim_core_get_ui_ops()

