#include "module.h"

MODULE = Gaim::Prpl  PACKAGE = Gaim::Find  PREFIX = gaim_find_
PROTOTYPES: ENABLE

Gaim::Plugin
gaim_find_prpl(id)
	const char *id

MODULE = Gaim::Prpl  PACKAGE = Gaim::Prpl  PREFIX = gaim_prpl_
PROTOTYPES: ENABLE

void
gaim_prpl_change_account_status(account, old_status, new_status)
	Gaim::Account account
	Gaim::Status old_status
	Gaim::Status new_status

void
gaim_prpl_get_statuses(account, presence)
	Gaim::Account account
	Gaim::Presence presence
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_prpl_get_statuses(account,presence); l != NULL; l = l->next) {
		/* XXX Someone please test and make sure this is the right
		 * type for these things. */
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Status")));
	}

void
gaim_prpl_got_account_idle(account, idle, idle_time)
	Gaim::Account account
	gboolean idle
	time_t idle_time

void
gaim_prpl_got_account_login_time(account, login_time)
	Gaim::Account account
	time_t login_time

void
gaim_prpl_got_user_idle(account, name, idle, idle_time)
	Gaim::Account account
	const char *name
	gboolean idle
	time_t idle_time

void
gaim_prpl_got_user_login_time(account, name, login_time)
	Gaim::Account account
	const char *name
	time_t login_time
