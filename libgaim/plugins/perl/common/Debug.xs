#include "module.h"

MODULE = Gaim::Debug  PACKAGE = Gaim::Debug  PREFIX = gaim_debug_
PROTOTYPES: ENABLE

void
gaim_debug(level, category, string)
	Gaim::DebugLevel level
	const char *category
	const char *string
CODE:
	gaim_debug(level, category, "%s", string);

void
gaim_debug_misc(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug_misc(category, "%s", string);

void
gaim_debug_info(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug_info(category, "%s", string);

void
gaim_debug_warning(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug_warning(category, "%s", string);

void
gaim_debug_error(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug_error(category, "%s", string);

void
gaim_debug_fatal(category, string)
	const char *category
	const char *string
CODE:
	gaim_debug_fatal(category, "%s", string);

void
gaim_debug_set_enabled(enabled)
	gboolean enabled

gboolean
gaim_debug_is_enabled()
