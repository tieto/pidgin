#include "module.h"

MODULE = Gaim::BuddyList::Chat  PACKAGE = Gaim::BuddyList::Chat  PREFIX = gaim_chat_
PROTOTYPES: ENABLE

void
set_alias(chat, alias)
	Gaim::BuddyList::Chat chat
	const char *alias
CODE:
	gaim_blist_alias_chat(chat, alias);

const char *
gaim_chat_get_display_name(chat)
	Gaim::BuddyList::Chat chat
CODE:
	RETVAL = gaim_blist_chat_get_display_name(chat);
OUTPUT:
	RETVAL

Gaim::BuddyList::Group
gaim_blist_chat_get_group(chat)
	Gaim::BuddyList::Chat chat
CODE:
	RETVAL = gaim_blist_chat_get_group(chat);
OUTPUT:
	RETVAL

void
gaim_blist_chat_set_setting(chat, key, value)
	Gaim::BuddyList::Chat chat
	const char *key
	const char *value

const char *
gaim_chat_get_setting(chat, key)
	Gaim::BuddyList::Chat chat
	const char *key
CODE:
	RETVAL = gaim_blist_chat_get_setting(chat, key);
OUTPUT:
	RETVAL

Gaim::Account
get_account(chat)
	Gaim::BuddyList::Chat chat
CODE:
	RETVAL = chat->account;
OUTPUT:
	RETVAL
