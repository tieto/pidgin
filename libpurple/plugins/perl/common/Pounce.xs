#include "module.h"

MODULE = Purple::Pounce  PACKAGE = Purple::Pounce  PREFIX = purple_pounce_
PROTOTYPES: ENABLE

void
purple_pounce_action_register(pounce, name)
	Purple::Pounce pounce
	const char *name

void
purple_pounce_destroy(pounce)
	Purple::Pounce pounce

void
purple_pounce_destroy_all_by_account(account)
	Purple::Account account

void *
purple_pounce_get_data(pounce)
	Purple::Pounce pounce

Purple::PounceEvent
purple_pounce_get_events(pounce)
	Purple::Pounce pounce

const char *
purple_pounce_get_pouncee(pounce)
	Purple::Pounce pounce

Purple::Account
purple_pounce_get_pouncer(pounce)
	Purple::Pounce pounce

gboolean
purple_pounce_get_save(pounce)
	Purple::Pounce pounce

void
purple_pounce_set_data(pounce, data)
	Purple::Pounce pounce
	void * data

void
purple_pounce_set_events(pounce, events)
	Purple::Pounce pounce
	Purple::PounceEvent events

void
purple_pounce_set_pouncee(pounce, pouncee)
	Purple::Pounce pounce
	const char *pouncee

void
purple_pounce_set_pouncer(pounce, pouncer)
	Purple::Pounce pounce
	Purple::Account pouncer

void
purple_pounce_set_save(pounce, save)
	Purple::Pounce pounce
	gboolean save

MODULE = Purple::Pounce  PACKAGE = Purple::Pounces  PREFIX = purple_pounces_
PROTOTYPES: ENABLE

void
purple_pounces_get_all()
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_pounces_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Pounce")));
	}

Purple::Handle
purple_pounces_get_handle()

void
purple_pounces_init()

gboolean
purple_pounces_load()

void
purple_pounces_uninit()

void
purple_pounces_unregister_handler(ui)
	const char *ui
