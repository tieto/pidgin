#include "module.h"

MODULE = Gaim::BuddyList::Group  PACKAGE = Gaim::BuddyList::Group  PREFIX = gaim_group_
PROTOTYPES: ENABLE

Gaim::BuddyList::Group
new(name)
	const char *name
CODE:
	RETVAL = gaim_group_new(name);
OUTPUT:
	RETVAL

void
rename(group, new_name)
	Gaim::BuddyList::Group group
	const char *new_name
CODE:
	gaim_blist_rename_group(group, new_name);

void
get_accounts(group)
	Gaim::BuddyList::Group group
PREINIT:
	GSList *l;
PPCODE:
	for (l = gaim_group_get_accounts(group); l != NULL; l = l->next)
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Account")));

int
get_size(group, offline)
	Gaim::BuddyList::Group group
	gboolean offline
CODE:
	RETVAL = gaim_blist_get_group_size(group, offline);
OUTPUT:
	RETVAL

const char *
get_name(group)
	Gaim::BuddyList::Group group
CODE:
	RETVAL = group->name;
OUTPUT:
	RETVAL

int
get_online_count(group)
	Gaim::BuddyList::Group group
CODE:
	RETVAL = gaim_blist_get_group_online_count(group);
OUTPUT:
	RETVAL

void
gaim_group_set_setting(group, key, value)
	Gaim::BuddyList::Group group
	const char *key
	const char *value

const char *
gaim_group_get_setting(group, key)
	Gaim::BuddyList::Group group
	const char *key

void
buddies(group)
	Gaim::BuddyList::Group group
PREINIT:
	GaimBlistNode *node;
	GaimBlistNode *_group = (GaimBlistNode *)group;
PPCODE:
	for (node = _group->child; node != NULL; node = node->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(node,
			"Gaim::BuddyList::Buddy")));
	}
