#include "module.h"
#include "../perl-handlers.h"

MODULE = Gaim::BuddyList  PACKAGE = Gaim  PREFIX = gaim_
PROTOTYPES: ENABLE

Gaim::BuddyList
gaim_get_blist()

void
gaim_set_blist(blist)
	Gaim::BuddyList blist

MODULE = Gaim::BuddyList  PACKAGE = Gaim::Find  PREFIX = gaim_find_
PROTOTYPES: ENABLE

Gaim::BuddyList::Buddy
gaim_find_buddy(account, name)
	Gaim::Account account
	const char * name

void
gaim_find_buddies(account, name)
	Gaim::Account account
	const char * name
PREINIT:
	GSList *l;
PPCODE:
	for (l = gaim_find_buddies(account, name); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::BuddyList::Buddy")));
	}

gboolean
gaim_group_on_account(group, account)
	Gaim::BuddyList::Group  group
	Gaim::Account account

Gaim::BuddyList::Group
gaim_find_group(name)
	const char *name

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList::Contact  PREFIX = gaim_contact_
PROTOTYPES: ENABLE

Gaim::BuddyList::Contact
gaim_contact_new();

Gaim::BuddyList::Buddy
gaim_contact_get_priority_buddy(contact)
	Gaim::BuddyList::Contact contact

void
gaim_contact_set_alias(contact, alias)
	Gaim::BuddyList::Contact contact
	const char * alias

const char *
gaim_contact_get_alias(contact)
	Gaim::BuddyList::Contact contact

gboolean
gaim_contact_on_account(contact, account)
	Gaim::BuddyList::Contact contact
	Gaim::Account account

void
gaim_contact_invalidate_priority_buddy(contact)
	Gaim::BuddyList::Contact contact

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList::Group  PREFIX = gaim_group_
PROTOTYPES: ENABLE

Gaim::BuddyList::Group
gaim_group_new(name)
	const char *name

void
gaim_group_get_accounts(group)
	Gaim::BuddyList::Group  group
PREINIT:
	GSList *l;
PPCODE:
	for (l = gaim_group_get_accounts(group); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Account")));
	}

gboolean
gaim_group_on_account(group, account)
	Gaim::BuddyList::Group  group
	Gaim::Account account

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList  PREFIX = gaim_blist_
PROTOTYPES: ENABLE

void
gaim_blist_add_contact(contact, group, node)
	Gaim::BuddyList::Contact contact
	Gaim::BuddyList::Group  group
	Gaim::BuddyList::Node node

void
gaim_blist_merge_contact(source, node)
	Gaim::BuddyList::Contact source
	Gaim::BuddyList::Node node

void
gaim_blist_add_group(group, node)
	Gaim::BuddyList::Group  group
	Gaim::BuddyList::Node node

void
gaim_blist_add_buddy(buddy, contact, group, node)
	Gaim::BuddyList::Buddy buddy
	Gaim::BuddyList::Contact contact
	Gaim::BuddyList::Group  group
	Gaim::BuddyList::Node node

void
gaim_blist_remove_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

void
gaim_blist_remove_contact(contact)
	Gaim::BuddyList::Contact contact

void
gaim_blist_remove_chat(chat)
	Gaim::BuddyList::Chat chat

void
gaim_blist_remove_group(group)
	Gaim::BuddyList::Group  group

Gaim::BuddyList::Chat
gaim_blist_find_chat(account, name)
	Gaim::Account account
	const char *name

void
gaim_blist_add_chat(chat, group, node)
	Gaim::BuddyList::Chat chat
	Gaim::BuddyList::Group  group
	Gaim::BuddyList::Node node

Gaim::BuddyList
gaim_blist_new()

void
gaim_blist_show()

void
gaim_blist_destroy();

void
gaim_blist_set_visible(show)
	gboolean show

void
gaim_blist_update_buddy_status(buddy, old_status)
	Gaim::BuddyList::Buddy buddy
	Gaim::Status old_status

void
gaim_blist_update_buddy_icon(buddy)
	Gaim::BuddyList::Buddy buddy

void
gaim_blist_rename_buddy(buddy, name)
	Gaim::BuddyList::Buddy buddy
	const char * name

void
gaim_blist_alias_buddy(buddy, alias)
	Gaim::BuddyList::Buddy buddy
	const char * alias

void
gaim_blist_server_alias_buddy(buddy, alias)
	Gaim::BuddyList::Buddy buddy
	const char * alias

void
gaim_blist_alias_chat(chat, alias)
	Gaim::BuddyList::Chat chat
	const char * alias

void
gaim_blist_rename_group(group, name)
	Gaim::BuddyList::Group  group
	const char * name

void
gaim_blist_add_account(account)
	Gaim::Account account

void
gaim_blist_remove_account(account)
	Gaim::Account account

int
gaim_blist_get_group_size(group, offline)
	Gaim::BuddyList::Group  group
	gboolean offline

int
gaim_blist_get_group_online_count(group)
	Gaim::BuddyList::Group  group

void
gaim_blist_load()

void
gaim_blist_schedule_save()

void
gaim_blist_request_add_group()

void
gaim_blist_set_ui_ops(ops)
	Gaim::BuddyList::UiOps ops

Gaim::BuddyList::UiOps
gaim_blist_get_ui_ops()

Gaim::Handle
gaim_blist_get_handle()

void
gaim_blist_init()

void
gaim_blist_uninit()

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList::Node  PREFIX = gaim_blist_node_
PROTOTYPES: ENABLE

void
gaim_blist_node_get_extended_menu(node)
	Gaim::BuddyList::Node node
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_blist_node_get_extended_menu(node); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Menu::Action")));
	}

void
gaim_blist_node_set_bool(node, key, value)
	Gaim::BuddyList::Node node
	const char * key
	gboolean value

gboolean
gaim_blist_node_get_bool(node, key)
	Gaim::BuddyList::Node node
	const char * key

void
gaim_blist_node_set_int(node, key, value)
	Gaim::BuddyList::Node node
	const char * key
	int value

int
gaim_blist_node_get_int(node, key)
	Gaim::BuddyList::Node node
	const char * key

const char *
gaim_blist_node_get_string(node, key)
	Gaim::BuddyList::Node node
	const char * key

void
gaim_blist_node_remove_setting(node, key)
	Gaim::BuddyList::Node node
	const char * key

void
gaim_blist_node_set_flags(node, flags)
	Gaim::BuddyList::Node node
	Gaim::BuddyList::NodeFlags flags

Gaim::BuddyList::NodeFlags
gaim_blist_node_get_flags(node)
	Gaim::BuddyList::Node node

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList::Chat  PREFIX = gaim_chat_
PROTOTYPES: ENABLE

Gaim::BuddyList::Group
gaim_chat_get_group(chat)
	Gaim::BuddyList::Chat chat

const char *
gaim_chat_get_name(chat)
	Gaim::BuddyList::Chat chat

Gaim::BuddyList::Chat
gaim_chat_new(account, alias, components)
	Gaim::Account account
	const char * alias
	SV * components
INIT:
	HV * t_HV;
	HE * t_HE;
	SV * t_SV;
	GHashTable * t_GHash;
	I32 len;
	char *t_key, *t_value;
CODE:
	t_HV =  (HV *)SvRV(components);
	t_GHash = g_hash_table_new(NULL, NULL);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
		t_value = SvPV(t_SV, PL_na);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}

	RETVAL = gaim_chat_new(account, alias, t_GHash);
OUTPUT:
	RETVAL

MODULE = Gaim::BuddyList  PACKAGE = Gaim::BuddyList::Buddy  PREFIX = gaim_buddy_
PROTOTYPES: ENABLE

Gaim::BuddyList::Buddy
gaim_buddy_new(account, screenname, alias)
	Gaim::Account account
	const char *screenname
	const char *alias

const char *
gaim_buddy_get_server_alias(buddy)
    Gaim::BuddyList::Buddy buddy

void
gaim_buddy_set_icon(buddy, icon)
	Gaim::BuddyList::Buddy buddy
	Gaim::Buddy::Icon icon

Gaim::Account
gaim_buddy_get_account(buddy)
	Gaim::BuddyList::Buddy buddy

Gaim::BuddyList::Group
gaim_buddy_get_group(buddy)
	Gaim::BuddyList::Buddy buddy

const char *
gaim_buddy_get_name(buddy)
	Gaim::BuddyList::Buddy buddy

Gaim::Buddy::Icon
gaim_buddy_get_icon(buddy)
	Gaim::BuddyList::Buddy buddy

Gaim::BuddyList::Contact
gaim_buddy_get_contact(buddy)
	Gaim::BuddyList::Buddy buddy

Gaim::Presence
gaim_buddy_get_presence(buddy)
	Gaim::BuddyList::Buddy buddy

const char *
gaim_buddy_get_alias_only(buddy)
	Gaim::BuddyList::Buddy buddy

const char *
gaim_buddy_get_contact_alias(buddy)
	Gaim::BuddyList::Buddy buddy

const char *
gaim_buddy_get_local_alias(buddy)
	Gaim::BuddyList::Buddy buddy

const char *
gaim_buddy_get_alias(buddy)
	Gaim::BuddyList::Buddy buddy
