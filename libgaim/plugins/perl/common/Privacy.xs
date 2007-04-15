#include "module.h"

MODULE = Gaim::Privacy  PACKAGE = Gaim::Privacy  PREFIX = gaim_privacy_
PROTOTYPES: ENABLE

Gaim::Privacy::UiOps
gaim_privacy_get_ui_ops()

void
gaim_privacy_init()

void
gaim_privacy_set_ui_ops(ops)
	Gaim::Privacy::UiOps ops

gboolean
gaim_privacy_permit_add(account, name, local_only)
	Gaim::Account account
	const char * name
	gboolean local_only

gboolean
gaim_privacy_permit_remove(account, name, local_only)
	Gaim::Account account
	const char * name
	gboolean local_only

gboolean
gaim_privacy_deny_add(account, name, local_only)
	Gaim::Account account
	const char * name
	gboolean local_only

gboolean
gaim_privacy_deny_remove(account, name, local_only)
	Gaim::Account account
	const char * name
	gboolean local_only

gboolean
gaim_privacy_check(account, who)
	Gaim::Account account
	const char * who
