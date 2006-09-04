#include "gtkmodule.h"

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.

void
gaim_gtk_blist_make_buddy_menu(menu, buddy, sub)
	Gtk::Widget menu
	Gaim::Buddy buddy
	gboolean sub
*/

/* This can't work at the moment since I don't have a typemap for Gdk::Pixbuf.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.

GdkPixbuf
gaim_gtk_blist_get_status_icon(node, size)
	Gaim::BuddyList::Node node
	Gaim::Status::IconSize size
*/

/* This can't work at the moment since I don't have a typemap for Gtk::Widget.
 * I thought about using the one from libgtk2-perl but wasn't sure how to go
 * about doing that.

void
gaim_gtk_append_blist_node_proto_menu(menu, gc, node)
	Gtk::Widget menu
	Gaim::Connection gc
	Gaim::BuddyList::Node node

void
gaim_gtk_append_blist_node_extended_menu(menu, node)
	Gtk::Widget menu
	Gaim::Connection gc
	Gaim::BuddyList::Node node
*/

MODULE = Gaim::Gtk::BuddyList  PACKAGE = Gaim::Gtk::BuddyList  PREFIX = gaim_gtk_blist_
PROTOTYPES: ENABLE

void *
gaim_gtk_blist_get_handle()

Gaim::Gtk::BuddyList
gaim_gtk_blist_get_default_gtk_blist()

void
gaim_gtk_blist_refresh(list)
	Gaim::BuddyList list

#if 0
void
gaim_gtk_blist_update_toolbar()

#endif

void
gaim_gtk_blist_update_columns()

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
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Gtk::BuddyList::SortMethod")));
	}

void
gaim_gtk_blist_sort_method_reg(id, name, func)
	const char * id
	const char * name
	Gaim::Gtk::BuddyList::SortFunction func

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
