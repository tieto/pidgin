#include "module.h"

MODULE = Purple::Prpl  PACKAGE = Purple::Find  PREFIX = purple_find_
PROTOTYPES: ENABLE

Purple::Plugin
purple_find_prpl(id)
	const char *id

MODULE = Purple::Prpl  PACKAGE = Purple::Prpl  PREFIX = purple_prpl_
PROTOTYPES: ENABLE

void
purple_prpl_change_account_status(account, old_status, new_status)
	Purple::Account account
	Purple::Status old_status
	Purple::Status new_status

void
purple_prpl_get_statuses(account, presence)
	Purple::Account account
	Purple::Presence presence
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_prpl_get_statuses(account,presence); l != NULL; l = l->next) {
		/* XXX Someone please test and make sure this is the right
		 * type for these things. */
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Status")));
	}

void
purple_prpl_got_account_idle(account, idle, idle_time)
	Purple::Account account
	gboolean idle
	time_t idle_time

void
purple_prpl_got_account_login_time(account, login_time)
	Purple::Account account
	time_t login_time

void
purple_prpl_got_user_idle(account, name, idle, idle_time)
	Purple::Account account
	const char *name
	gboolean idle
	time_t idle_time

void
purple_prpl_got_user_login_time(account, name, login_time)
	Purple::Account account
	const char *name
	time_t login_time
