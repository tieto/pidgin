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
	Purple::Account account
	const char * username

void
pidgindialogs_info()

void
pidgindialogs_log()

void
pidgindialogs_alias_contact(contact)
	Purple::BuddyList::Contact contact

void
pidgindialogs_alias_buddy(buddy)
	Purple::BuddyList::Buddy buddy

void
pidgindialogs_alias_chat(chat)
	Purple::BuddyList::Chat chat

void
pidgindialogs_remove_buddy(buddy)
	Purple::BuddyList::Buddy buddy

void
pidgindialogs_remove_group(group)
	Purple::BuddyList::Group group

void
pidgindialogs_remove_chat(chat)
	Purple::BuddyList::Chat chat

void
pidgindialogs_remove_contact(contact)
	Purple::BuddyList::Contact contact
