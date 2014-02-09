#ifndef _PURPLE_DBUS_USEFUL_H_
#define _PURPLE_DBUS_USEFUL_H_
/**
 * SECTION:dbus-useful
 * @section_id: libpurple-dbus-useful
 * @short_description: <filename>dbus-useful.h</filename>
 * @title: Misc functions for DBUS server
 */

#include "conversation.h"

G_BEGIN_DECLS

PurpleAccount *purple_accounts_find_ext(const char *name, const char *protocol_id,
				    gboolean (*account_test)(const PurpleAccount *account));

PurpleAccount *purple_accounts_find_any(const char *name, const char *protocol);

PurpleAccount *purple_accounts_find_connected(const char *name, const char *protocol);

G_END_DECLS

#endif
