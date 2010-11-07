#include "gtkmodule.h"

MODULE = Pidgin::Log  PACKAGE = Pidgin::Log  PREFIX = pidgin_log_
PROTOTYPES: ENABLE

Purple::Handle
pidgin_log_get_handle()

void
pidgin_log_show(type, screenname, account)
	Purple::LogType type
	const char * screenname
	Purple::Account account

void
pidgin_log_show_contact(contact)
	Purple::BuddyList::Contact contact

MODULE = Pidgin::Log  PACKAGE = Pidgin::SysLog  PREFIX = pidgin_syslog_
PROTOTYPES: ENABLE

void
pidgin_syslog_show()
