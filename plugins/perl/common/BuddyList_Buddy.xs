#include "module.h"

MODULE = Gaim::BuddyList::Buddy  PACKAGE = Gaim::BuddyList::Buddy  PREFIX = gaim_buddy_
PROTOTYPES: ENABLE

Gaim::BuddyList::Buddy
new(account, name, alias)
	Gaim::Account account
	const char *name
	const char *alias
CODE:
	RETVAL = gaim_buddy_new(account, name, alias);
OUTPUT:
	RETVAL

void
rename(buddy, new_name)
	Gaim::BuddyList::Buddy buddy
	const char *new_name
CODE:
	gaim_blist_rename_buddy(buddy, new_name);

void
set_alias(buddy, alias)
	Gaim::BuddyList::Buddy buddy
	const char *alias
CODE:
	gaim_blist_alias_buddy(buddy, alias);

void
set_server_alias(buddy, alias)
	Gaim::BuddyList::Buddy buddy
	const char *alias
CODE:
	gaim_blist_server_alias_buddy(buddy, alias);

const char *
get_name(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = buddy->name;
OUTPUT:
	RETVAL

Gaim::Account
get_account(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = buddy->account;
OUTPUT:
	RETVAL

const char *
get_alias_only(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = gaim_buddy_get_alias_only(buddy);
OUTPUT:
	RETVAL

const char *
get_alias(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = gaim_buddy_get_alias(buddy);
OUTPUT:
	RETVAL

Gaim::BuddyList::Group
get_group(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = gaim_find_buddys_group(buddy);
OUTPUT:
	RETVAL
