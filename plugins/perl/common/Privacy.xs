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
