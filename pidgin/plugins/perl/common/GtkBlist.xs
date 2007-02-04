#include "gtkmodule.h"

MODULE = Pidgin::BuddyList  PACKAGE = Pidgin::BuddyList  PREFIX = pidgin_blist_
PROTOTYPES: ENABLE

Gaim::Handle
pidgin_blist_get_handle()

Pidgin::BuddyList
pidgin_blist_get_default_gtk_blist()

void
pidgin_blist_refresh(list)
	Gaim::BuddyList list

void
pidgin_blist_update_refresh_timeout()

gboolean
pidgin_blist_node_is_contact_expanded(node)
	Gaim::BuddyList::Node node

void
pidgin_blist_toggle_visibility()

void
pidgin_blist_visibility_manager_add()

void
pidgin_blist_visibility_manager_remove()

void
pidgin_blist_get_sort_methods()
PREINIT:
	GList *l;
PPCODE:
	for (l = pidgin_blist_get_sort_methods(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Pidgin::BuddyList::SortMethod")));
	}

void
pidgin_blist_sort_method_reg(id, name, func)
	const char * id
	const char * name
	Pidgin::BuddyList::SortFunction func

void
pidgin_blist_sort_method_unreg(id)
	const char * id

void
pidgin_blist_sort_method_set(id)
	const char * id

void
pidgin_blist_setup_sort_methods()

void
pidgin_blist_update_accounts_menu()

void
pidgin_blist_update_plugin_actions()

void
pidgin_blist_update_sort_methods()

gboolean
pidgin_blist_joinchat_is_showable()

void
pidgin_blist_joinchat_show()

void
pidgin_blist_update_account_error_state(account, message)
	Gaim::Account account
	const char * message
