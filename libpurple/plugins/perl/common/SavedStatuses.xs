#include "module.h"

MODULE = Purple::SavedStatus  PACKAGE = Purple::SavedStatus  PREFIX = purple_savedstatus_
PROTOTYPES: ENABLE

gboolean
purple_savedstatus_delete(title)
	const char *title

Purple::SavedStatus
purple_savedstatus_find(title)
	const char *title

const char *
purple_savedstatus_get_message(saved_status)
	Purple::SavedStatus saved_status

const char *
purple_savedstatus_get_title(saved_status)
	Purple::SavedStatus saved_status

Purple::StatusPrimitive
purple_savedstatus_get_type(saved_status)
	Purple::SavedStatus saved_status

Purple::SavedStatus
purple_savedstatus_new(title, type)
	const char *title
	Purple::StatusPrimitive type

void
purple_savedstatus_set_message(status, message)
	Purple::SavedStatus status
	const char *message

Purple::SavedStatus
purple_savedstatus_get_current()

MODULE = Purple::SavedStatus  PACKAGE = Purple::SavedStatuses  PREFIX = purple_savedstatuses_
PROTOTYPES: ENABLE

void
purple_savedstatuses_get_all()
PREINIT:
	const GList *l;
PPCODE:
	for (l = purple_savedstatuses_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::SavedStatus")));
	}

Purple::Handle
purple_savedstatuses_get_handle()

void
purple_savedstatuses_init()

void
purple_savedstatuses_uninit()
