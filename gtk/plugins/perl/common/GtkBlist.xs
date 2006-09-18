#include "gtkmodule.h"

MODULE = Gaim::GtkUI::BuddyList  PACKAGE = Gaim::GtkUI::BuddyList  PREFIX = gaim_gtk_blist_
PROTOTYPES: ENABLE

void *
gaim_gtk_blist_get_handle()

Gaim::GtkUI::BuddyList
gaim_gtk_blist_get_default_gtk_blist()

void
gaim_gtk_blist_refresh(list)
	Gaim::BuddyList list

void
gaim_gtk_blist_update_refresh_timeout()

gboolean
gaim_gtk_blist_node_is_contact_expanded(node)
	Gaim::BuddyList::Node node

void
gaim_gtk_blist_toggle_visibility()

void
gaim_gtk_blist_visibility_manager_add()

void
gaim_gtk_blist_visibility_manager_remove()

void
gaim_gtk_blist_get_sort_methods()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_gtk_blist_get_sort_methods(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::GtkUI::BuddyList::SortMethod")));
	}

void
gaim_gtk_blist_sort_method_reg(id, name, func)
	const char * id
	const char * name
	Gaim::GtkUI::BuddyList::SortFunction func

void
gaim_gtk_blist_sort_method_unreg(id)
	const char * id

void
gaim_gtk_blist_sort_method_set(id)
	const char * id

void
gaim_gtk_blist_setup_sort_methods()

void
gaim_gtk_blist_update_accounts_menu()

void
gaim_gtk_blist_update_plugin_actions()

void
gaim_gtk_blist_update_sort_methods()

gboolean
gaim_gtk_blist_joinchat_is_showable()

void
gaim_gtk_blist_joinchat_show()

void
gaim_gtk_blist_update_account_error_state(account, message)
	Gaim::Account account
	const char * message
