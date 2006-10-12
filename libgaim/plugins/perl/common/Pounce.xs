#include "module.h"

MODULE = Gaim::Pounce  PACKAGE = Gaim::Pounce  PREFIX = gaim_pounce_
PROTOTYPES: ENABLE

void
gaim_pounce_action_register(pounce, name)
	Gaim::Pounce pounce
	const char *name

void
gaim_pounce_destroy(pounce)
	Gaim::Pounce pounce

void
gaim_pounce_destroy_all_by_account(account)
	Gaim::Account account

void *
gaim_pounce_get_data(pounce)
	Gaim::Pounce pounce

Gaim::PounceEvent
gaim_pounce_get_events(pounce)
	Gaim::Pounce pounce

const char *
gaim_pounce_get_pouncee(pounce)
	Gaim::Pounce pounce

Gaim::Account
gaim_pounce_get_pouncer(pounce)
	Gaim::Pounce pounce

gboolean
gaim_pounce_get_save(pounce)
	Gaim::Pounce pounce

void
gaim_pounce_set_data(pounce, data)
	Gaim::Pounce pounce
	void * data

void
gaim_pounce_set_events(pounce, events)
	Gaim::Pounce pounce
	Gaim::PounceEvent events

void
gaim_pounce_set_pouncee(pounce, pouncee)
	Gaim::Pounce pounce
	const char *pouncee

void
gaim_pounce_set_pouncer(pounce, pouncer)
	Gaim::Pounce pounce
	Gaim::Account pouncer

void
gaim_pounce_set_save(pounce, save)
	Gaim::Pounce pounce
	gboolean save

MODULE = Gaim::Pounce  PACKAGE = Gaim::Pounces  PREFIX = gaim_pounces_
PROTOTYPES: ENABLE

void
gaim_pounces_get_all()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_pounces_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Pounce")));
	}

Gaim::Handle
gaim_pounces_get_handle()

void
gaim_pounces_init()

gboolean
gaim_pounces_load()

void
gaim_pounces_uninit()

void
gaim_pounces_unregister_handler(ui)
	const char *ui
