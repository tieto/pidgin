#include "gtkmodule.h"

MODULE = Pidgin::Dialogs  PACKAGE = Pidgin::Dialogs  PREFIX = pidgindialogs_
PROTOTYPES: ENABLE

void
pidgindialogs_destroy_all()

void
pidgindialogs_about()

void
pidgindialogs_im()

void
pidgindialogs_im_with_user(account, username)
	Gaim::Account account
	const char * username

void
pidgindialogs_info()

void
pidgindialogs_log()

void
pidgindialogs_alias_contact(contact)
	Gaim::BuddyList::Contact contact

void
pidgindialogs_alias_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

void
pidgindialogs_alias_chat(chat)
	Gaim::BuddyList::Chat chat

void
pidgindialogs_remove_buddy(buddy)
	Gaim::BuddyList::Buddy buddy

void
pidgindialogs_remove_group(group)
	Gaim::BuddyList::Group group

void
pidgindialogs_remove_chat(chat)
	Gaim::BuddyList::Chat chat

void
pidgindialogs_remove_contact(contact)
	Gaim::BuddyList::Contact contact
