#include "module.h"

MODULE = Gaim::SavedStatus  PACKAGE = Gaim::SavedStatus  PREFIX = gaim_savedstatus_
PROTOTYPES: ENABLE

gboolean
gaim_savedstatus_delete(title)
	const char *title

Gaim::SavedStatus
gaim_savedstatus_find(title)
	const char *title

const char *
gaim_savedstatus_get_message(saved_status)
	Gaim::SavedStatus saved_status

const char *
gaim_savedstatus_get_title(saved_status)
	Gaim::SavedStatus saved_status

Gaim::StatusPrimitive
gaim_savedstatus_get_type(saved_status)
	Gaim::SavedStatus saved_status

Gaim::SavedStatus
gaim_savedstatus_new(title, type)
	const char *title
	Gaim::StatusPrimitive type

void
gaim_savedstatus_set_message(status, message)
	Gaim::SavedStatus status
	const char *message

Gaim::SavedStatus
gaim_savedstatus_get_current()

MODULE = Gaim::SavedStatus  PACKAGE = Gaim::SavedStatuses  PREFIX = gaim_savedstatuses_
PROTOTYPES: ENABLE

void
gaim_savedstatuses_get_all()
PREINIT:
	const GList *l;
PPCODE:
	for (l = gaim_savedstatuses_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::SavedStatus")));
	}

Gaim::Handle
gaim_savedstatuses_get_handle()

void
gaim_savedstatuses_init()

void
gaim_savedstatuses_uninit()
