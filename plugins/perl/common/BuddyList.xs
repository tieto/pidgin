#include "module.h"

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList  PREFIX = gaim_blist_
PROTOTYPES: ENABLE

void
gaim_blist_set_visible(show)
	gboolean show

void
add_buddy(account, buddy, group)
	Gaim::Account account
	Gaim::BuddyList::Buddy buddy
	Gaim::BuddyList::Group group
CODE:
	gaim_blist_add_buddy(buddy, NULL, group, NULL);
	serv_add_buddy(gaim_account_get_connection(account), buddy);

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

void
get_buddies(account)
	Gaim::Account account
PREINIT:
	GSList *l;
PPCODE:
	for (l = gaim_get_account_buddies(account); l != NULL; l = l->next)
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
PPCODE:
	if (gaim_get_blist() != NULL)
	{
		for (node = gaim_get_blist()->root; node != NULL; node = node->next)
		{
			XPUSHs(sv_2mortal(gaim_perl_bless_object(node,
				"Gaim::BuddyList::Group")));
		}
	}

void *
handle()
CODE:
	RETVAL = gaim_blist_get_handle();
OUTPUT:
	RETVAL
