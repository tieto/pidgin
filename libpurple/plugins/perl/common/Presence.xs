#include "module.h"

MODULE = Purple::Presence  PACKAGE = Purple::Presence  PREFIX = purple_presence_
PROTOTYPES: ENABLE

Purple::Status
purple_presence_get_active_status(presence)
	Purple::Presence presence

time_t
purple_presence_get_idle_time(presence)
	Purple::Presence presence

time_t
purple_presence_get_login_time(presence)
	Purple::Presence presence

Purple::Status
purple_presence_get_status(presence, status_id)
	Purple::Presence presence
	const char *status_id

void
purple_presence_get_statuses(presence)
	Purple::Presence presence
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_presence_get_statuses(presence); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Status")));
	}

gboolean
purple_presence_is_available(presence)
	Purple::Presence presence

gboolean
purple_presence_is_idle(presence)
	Purple::Presence presence

gboolean
purple_presence_is_online(presence)
	Purple::Presence presence

gboolean
purple_presence_is_status_active(presence, status_id)
	Purple::Presence presence
	const char *status_id

gboolean
purple_presence_is_status_primitive_active(presence, primitive)
	Purple::Presence presence
	Purple::StatusPrimitive primitive

void
purple_presence_set_idle(presence, idle, idle_time)
	Purple::Presence presence
	gboolean idle
	time_t idle_time

void
purple_presence_set_login_time(presence, login_time)
	Purple::Presence presence
	time_t login_time

void
purple_presence_set_status_active(presence, status_id, active)
	Purple::Presence presence
	const char *status_id
	gboolean active

void
purple_presence_switch_status(presence, status_id)
	Purple::Presence presence
	const char *status_id

MODULE = Purple::Presence  PACKAGE = Purple::AccountPresence  PREFIX = purple_account_presence_
PROTOTYPES: ENABLE

Purple::Account
purple_account_presence_get_account(presence)
	Purple::AccountPresence presence

Purple::AccountPresence
purple_account_presence_new(account)
	Purple::Account account

MODULE = Purple::Presence  PACKAGE = Purple::BuddyPresence  PREFIX = purple_buddy_presence_
PROTOTYPES: ENABLE

gint
purple_buddy_presence_compare(presence1, presence2)
	Purple::BuddyPresence presence1
	Purple::BuddyPresence presence2

Purple::BuddyList::Buddy
purple_buddy_presence_get_buddy(presence)
	Purple::BuddyPresence presence

Purple::BuddyPresence
purple_buddy_presence_new(buddy)
	Purple::BuddyList::Buddy buddy
