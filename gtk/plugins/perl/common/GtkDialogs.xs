#include "gtkmodule.h"

MODULE = Gaim::Gtk::Dialogs  PACKAGE = Gaim::Gtk::Dialogs  PREFIX = gaim_gtkdialogs_
PROTOTYPES: ENABLE

void
gaim_gtkdialogs_destroy_all()

void
gaim_gtkdialogs_about()

void
gaim_gtkdialogs_im()

void
gaim_gtkdialogs_im_with_user(account, username)
	Gaim::Account account
	const char * username

void
gaim_gtkdialogs_info()

void
gaim_gtkdialogs_log()

void
gaim_gtkdialogs_alias_contact(contact)
	Gaim::BuddyList::Contact contact

void
gaim_gtkdialogs_alias_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

void
gaim_gtkdialogs_alias_chat(chat)
	Gaim::BuddyList::Chat chat

void
gaim_gtkdialogs_remove_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

void
gaim_gtkdialogs_remove_group(group)
	Gaim::BuddyList::Group group

void
gaim_gtkdialogs_remove_chat(chat)
	Gaim::BuddyList::Chat chat

void
gaim_gtkdialogs_remove_contact(contact)
	Gaim::BuddyList::Contact contact
