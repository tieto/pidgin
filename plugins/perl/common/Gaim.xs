#include "module.h"

MODULE = Gaim  PACKAGE = Gaim
PROTOTYPES: ENABLE

void
debug(string)
	const char *string
CODE:
	gaim_debug(GAIM_DEBUG_INFO, "perl script", string);

BOOT:
	GAIM_PERL_BOOT(Account);
