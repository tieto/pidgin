#include "module.h"
#include "../perl-handlers.h"

MODULE = Purple::BuddyList  PACKAGE = Purple  PREFIX = purple_
PROTOTYPES: ENABLE

Purple::BuddyList
purple_get_blist()

void
purple_set_blist(blist)
	Purple::BuddyList blist

MODULE = Purple::BuddyList  PACKAGE = Purple::Find  PREFIX = purple_find_
PROTOTYPES: ENABLE

Purple::BuddyList::Buddy
purple_find_buddy(account, name)
	Purple::Account account
	const char * name

void
purple_find_buddies(account, name)
	Purple::Account account
	const char * name
PREINIT:
	GSList *l;
PPCODE:
	for (l = purple_find_buddies(account, name); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::BuddyList::Buddy")));
	}

gboolean
purple_group_on_account(group, account)
	Purple::BuddyList::Group  group
	Purple::Account account

Purple::BuddyList::Group
purple_find_group(name)
	const char *name

MODULE = Purple::BuddyList  PACKAGE = Purple::BuddyList::Contact  PREFIX = purple_contact_
PROTOTYPES: ENABLE

Purple::BuddyList::Contact
purple_contact_new();

Purple::BuddyList::Buddy
purple_contact_get_priority_buddy(contact)
	Purple::BuddyList::Contact contact

void
purple_contact_set_alias(contact, alias)
	Purple::BuddyList::Contact contact
	const char * alias

const char *
purple_contact_get_alias(contact)
	Purple::BuddyList::Contact contact

gboolean
purple_contact_on_account(contact, account)
	Purple::BuddyList::Contact contact
	Purple::Account account

void
purple_contact_invalidate_priority_buddy(contact)
	Purple::BuddyList::Contact contact

MODULE = Purple::BuddyList  PACKAGE = Purple::BuddyList::Group  PREFIX = purple_group_
PROTOTYPES: ENABLE

Purple::BuddyList::Group
purple_group_new(name)
	const char *name

void
purple_group_get_accounts(group)
	Purple::BuddyList::Group  group
PREINIT:
	GSList *l;
PPCODE:
	for (l = purple_group_get_accounts(group); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Account")));
	}

gboolean
purple_group_on_account(group, account)
	Purple::BuddyList::Group  group
	Purple::Account account

MODULE = Purple::BuddyList  PACKAGE = Purple::BuddyList  PREFIX = purple_blist_
PROTOTYPES: ENABLE

void
purple_blist_add_contact(contact, group, node)
	Purple::BuddyList::Contact contact
	Purple::BuddyList::Group  group
	Purple::BuddyList::Node node

void
purple_blist_merge_contact(source, node)
	Purple::BuddyList::Contact source
	Purple::BuddyList::Node node

void
purple_blist_add_group(group, node)
	Purple::BuddyList::Group  group
	Purple::BuddyList::Node node

void
purple_blist_add_buddy(buddy, contact, group, node)
	Purple::BuddyList::Buddy buddy
	Purple::BuddyList::Contact contact
	Purple::BuddyList::Group  group
	Purple::BuddyList::Node node

void
purple_blist_remove_buddy(buddy)
	Purple::BuddyList::Buddy buddy

void
purple_blist_remove_contact(contact)
	Purple::BuddyList::Contact contact

void
purple_blist_remove_chat(chat)
	Purple::BuddyList::Chat chat

void
purple_blist_remove_group(group)
	Purple::BuddyList::Group  group

Purple::BuddyList::Chat
purple_blist_find_chat(account, name)
	Purple::Account account
	const char *name

void
purple_blist_add_chat(chat, group, node)
	Purple::BuddyList::Chat chat
	Purple::BuddyList::Group  group
	Purple::BuddyList::Node node

Purple::BuddyList
purple_blist_new()

void
purple_blist_show()

void
purple_blist_destroy();

void
purple_blist_set_visible(show)
	gboolean show

void
purple_blist_update_buddy_status(buddy, old_status)
	Purple::BuddyList::Buddy buddy
	Purple::Status old_status

void
purple_blist_update_buddy_icon(buddy)
	Purple::BuddyList::Buddy buddy

void
purple_blist_rename_buddy(buddy, name)
	Purple::BuddyList::Buddy buddy
	const char * name

void
purple_blist_alias_buddy(buddy, alias)
	Purple::BuddyList::Buddy buddy
	const char * alias

void
purple_blist_server_alias_buddy(buddy, alias)
	Purple::BuddyList::Buddy buddy
	const char * alias

void
purple_blist_alias_chat(chat, alias)
	Purple::BuddyList::Chat chat
	const char * alias

void
purple_blist_rename_group(group, name)
	Purple::BuddyList::Group  group
	const char * name

void
purple_blist_add_account(account)
	Purple::Account account

void
purple_blist_remove_account(account)
	Purple::Account account

int
purple_blist_get_group_size(group, offline)
	Purple::BuddyList::Group  group
	gboolean offline

int
purple_blist_get_group_online_count(group)
	Purple::BuddyList::Group  group

void
purple_blist_load()

void
purple_blist_schedule_save()

void
purple_blist_request_add_group()

void
purple_blist_set_ui_ops(ops)
	Purple::BuddyList::UiOps ops

Purple::BuddyList::UiOps
purple_blist_get_ui_ops()

Purple::Handle
purple_blist_get_handle()

void
purple_blist_init()

void
purple_blist_uninit()

MODULE = Purple::BuddyList  PACKAGE = Purple::BuddyList::Node  PREFIX = purple_blist_node_
PROTOTYPES: ENABLE

void
purple_blist_node_get_extended_menu(node)
	Purple::BuddyList::Node node
PREINIT:
	GList *l;
PPCODE:
	for (l = purple_blist_node_get_extended_menu(node); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(purple_perl_bless_object(l->data, "Purple::Menu::Action")));
	}

void
purple_blist_node_set_bool(node, key, value)
	Purple::BuddyList::Node node
	const char * key
	gboolean value

gboolean
purple_blist_node_get_bool(node, key)
	Purple::BuddyList::Node node
	const char * key

void
purple_blist_node_set_int(node, key, value)
	Purple::BuddyList::Node node
	const char * key
	int value

int
purple_blist_node_get_int(node, key)
	Purple::BuddyList::Node node
	const char * key

const char *
purple_blist_node_get_string(node, key)
	Purple::BuddyList::Node node
	const char * key

void
purple_blist_node_remove_setting(node, key)
	Purple::BuddyList::Node node
	const char * key

void
purple_blist_node_set_flags(node, flags)
	Purple::BuddyList::Node node
	Purple::BuddyList::NodeFlags flags

Purple::BuddyList::NodeFlags
purple_blist_node_get_flags(node)
	Purple::BuddyList::Node node

MODULE = Purple::BuddyList  PACKAGE = Purple::BuddyList::Chat  PREFIX = purple_chat_
PROTOTYPES: ENABLE

Purple::BuddyList::Group
purple_chat_get_group(chat)
	Purple::BuddyList::Chat chat

const char *
purple_chat_get_name(chat)
	Purple::BuddyList::Chat chat

Purple::BuddyList::Chat
purple_chat_new(account, alias, components)
	Purple::Account account
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
	t_GHash = g_hash_table_new(g_str_hash, g_str_equal);

	for (t_HE = hv_iternext(t_HV); t_HE != NULL; t_HE = hv_iternext(t_HV) ) {
		t_key = hv_iterkey(t_HE, &len);
		t_SV = *hv_fetch(t_HV, t_key, len, 0);
		t_value = SvPV(t_SV, PL_na);

		g_hash_table_insert(t_GHash, t_key, t_value);
	}

	RETVAL = purple_chat_new(account, alias, t_GHash);
OUTPUT:
	RETVAL

MODULE = Purple::BuddyList  PACKAGE = Purple::BuddyList::Buddy  PREFIX = purple_buddy_
PROTOTYPES: ENABLE

Purple::BuddyList::Buddy
purple_buddy_new(account, screenname, alias)
	Purple::Account account
	const char *screenname
	const char *alias

const char *
purple_buddy_get_server_alias(buddy)
    Purple::BuddyList::Buddy buddy

void
purple_buddy_set_icon(buddy, icon)
	Purple::BuddyList::Buddy buddy
	Purple::Buddy::Icon icon

Purple::Account
purple_buddy_get_account(buddy)
	Purple::BuddyList::Buddy buddy

Purple::BuddyList::Group
purple_buddy_get_group(buddy)
	Purple::BuddyList::Buddy buddy

const char *
purple_buddy_get_name(buddy)
	Purple::BuddyList::Buddy buddy

Purple::Buddy::Icon
purple_buddy_get_icon(buddy)
	Purple::BuddyList::Buddy buddy

Purple::BuddyList::Contact
purple_buddy_get_contact(buddy)
	Purple::BuddyList::Buddy buddy

Purple::Presence
purple_buddy_get_presence(buddy)
	Purple::BuddyList::Buddy buddy

const char *
purple_buddy_get_alias_only(buddy)
	Purple::BuddyList::Buddy buddy

const char *
purple_buddy_get_contact_alias(buddy)
	Purple::BuddyList::Buddy buddy

const char *
purple_buddy_get_local_alias(buddy)
	Purple::BuddyList::Buddy buddy

const char *
purple_buddy_get_alias(buddy)
	Purple::BuddyList::Buddy buddy
