#include "module.h"

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList  PREFIX = gaim_blist_
PROTOTYPES: ENABLE

void
gaim_blist_set_visible(show)
	gboolean show

void
add_buddy(buddy, group)
	Gaim::BuddyList::Buddy buddy
	Gaim::BuddyList::Group group
CODE:
	gaim_blist_add_buddy(buddy, group, NULL);

void
add_group(group)
	Gaim::BuddyList::Group group
CODE:
	gaim_blist_add_group(group, NULL);

void
add_chat(chat, group)
	Gaim::BuddyList::Chat chat
	Gaim::BuddyList::Group group
CODE:
	gaim_blist_add_chat(chat, group, NULL);

void
gaim_blist_remove_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

void
gaim_blist_remove_group(group)
	Gaim::BuddyList::Group group

void
gaim_blist_remove_chat(chat)
	Gaim::BuddyList::Chat chat

Gaim::BuddyList::Buddy
find_buddy(account, name)
	Gaim::Account account
	const char *name
CODE:
	RETVAL = gaim_find_buddy(account, name);
OUTPUT:
	RETVAL

void
find_buddies(account, name)
	Gaim::Account account
	const char *name
PREINIT:
	GSList *l;
PPCODE:
	for (l = gaim_find_buddies(account, name); l != NULL; l = l->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data,
			"Gaim::BuddyList::Buddy")));
	}

	g_slist_free(l);

Gaim::BuddyList::Group
find_group(name)
	const char *name
CODE:
	RETVAL = gaim_find_group(name);
OUTPUT:
	RETVAL

Gaim::BuddyList::Chat
gaim_blist_find_chat(account, name)
	Gaim::Account account
	const char *name

void
groups()
PREINIT:
	GaimBlistNode *node;
CODE:
	for (node = gaim_get_blist()->root; node != NULL; node = node->next)
	{
		XPUSHs(sv_2mortal(gaim_perl_bless_object(node,
			"Gaim::BuddyList::Group")));
	}


###########################################################################
MODULE = Gaim::BuddyList::Group  PACKAGE = Gaim::BuddyList::Group  PREFIX = gaim_group_
PROTOTYPES: ENABLE
###########################################################################

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


###########################################################################
MODULE = Gaim::BuddyList::Buddy  PACKAGE = Gaim::BuddyList::Buddy  PREFIX = gaim_buddy_
PROTOTYPES: ENABLE
###########################################################################

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
update_presence(buddy, presence)
	Gaim::BuddyList::Buddy buddy
	int presence
CODE:
	gaim_blist_update_buddy_presence(buddy, presence);

void
set_idle_time(buddy, idle)
	Gaim::BuddyList::Buddy buddy
	int idle
CODE:
	gaim_blist_update_buddy_idle(buddy, idle);

void
set_warning_percent(buddy, warning)
	Gaim::BuddyList::Buddy buddy
	int warning
CODE:
	gaim_blist_update_buddy_evil(buddy, warning);

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
	RETVAL = gaim_get_buddy_alias_only(buddy);
OUTPUT:
	RETVAL

const char *
get_alias(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = gaim_get_buddy_alias(buddy);
OUTPUT:
	RETVAL

Gaim::BuddyList::Group
get_group(buddy)
	Gaim::BuddyList::Buddy buddy
CODE:
	RETVAL = gaim_find_buddys_group(buddy);
OUTPUT:
	RETVAL

void
gaim_buddy_set_setting(buddy, key, value)
	Gaim::BuddyList::Buddy buddy
	const char *key
	const char *value

const char *
gaim_buddy_get_setting(buddy, key)
	Gaim::BuddyList::Buddy buddy
	const char *key


###########################################################################
MODULE = Gaim::BuddyList::Chat  PACKAGE = Gaim::BuddyList::Chat  PREFIX = gaim_chat_
PROTOTYPES: ENABLE
###########################################################################

void
set_alias(chat, alias)
	Gaim::BuddyList::Chat chat
	const char *alias
CODE:
	gaim_blist_alias_chat(chat, alias);

const char *
gaim_chat_get_display_name(chat)
	Gaim::BuddyList::Chat chat

Gaim::BuddyList::Group
gaim_blist_chat_get_group(chat)
	Gaim::BuddyList::Chat chat
CODE:
	RETVAL = gaim_blist_chat_get_group(chat);
OUTPUT:
	RETVAL

void
gaim_chat_set_setting(chat, key, value)
	Gaim::BuddyList::Chat chat
	const char *key
	const char *value

const char *
gaim_chat_get_setting(chat, key)
	Gaim::BuddyList::Chat chat
	const char *key

Gaim::Account
get_account(chat)
	Gaim::BuddyList::Chat chat
CODE:
	RETVAL = chat->account;
OUTPUT:
	RETVAL
