#include "module.h"

MODULE = Purple::Debug  PACKAGE = Purple::Debug  PREFIX = purple_debug_
PROTOTYPES: ENABLE

void
purple_debug(level, category, string)
	Purple::DebugLevel level
	const char *category
	const char *string
CODE:
	purple_debug(level, category, "%s", string);

void
purple_debug_misc(category, string)
	const char *category
	const char *string
CODE:
	purple_debug_misc(category, "%s", string);

void
purple_debug_info(category, string)
	const char *category
	const char *string
CODE:
	purple_debug_info(category, "%s", string);

void
purple_debug_warning(category, string)
	const char *category
	const char *string
CODE:
	purple_debug_warning(category, "%s", string);

void
purple_debug_error(category, string)
	const char *category
	const char *string
CODE:
	purple_debug_error(category, "%s", string);

void
purple_debug_fatal(category, string)
	const char *category
	const char *string
CODE:
	purple_debug_fatal(category, "%s", string);

void
purple_debug_set_enabled(enabled)
	gboolean enabled

gboolean
purple_debug_is_enabled()
