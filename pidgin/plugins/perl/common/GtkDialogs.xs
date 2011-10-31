#include "gtkmodule.h"

MODULE = Pidgin::Dialogs  PACKAGE = Pidgin::Dialogs  PREFIX = pidgin_dialogs_
PROTOTYPES: ENABLE

void
pidgin_dialogs_destroy_all()

void
pidgin_dialogs_about()

void
pidgin_dialogs_im()

void
pidgin_dialogs_im_with_user(account, username)
	Purple::Account account
	const char * username

void
pidgin_dialogs_info()

void
pidgin_dialogs_log()

void
pidgin_dialogs_alias_buddy(buddy)
	Purple::BuddyList::Buddy buddy

void
pidgin_dialogs_alias_chat(chat)
	Purple::BuddyList::Chat chat

void
pidgin_dialogs_remove_buddy(buddy)
	Purple::BuddyList::Buddy buddy

void
pidgin_dialogs_remove_group(group)
	Purple::BuddyList::Group group

void
pidgin_dialogs_remove_chat(chat)
	Purple::BuddyList::Chat chat

void
pidgin_dialogs_remove_contact(contact)
	Purple::BuddyList::Contact contact
