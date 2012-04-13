#include "conversation.h"

G_BEGIN_DECLS

PurpleAccount *purple_accounts_find_ext(const char *name, const char *protocol_id,
				    gboolean (*account_test)(const PurpleAccount *account));

PurpleAccount *purple_accounts_find_any(const char *name, const char *protocol);

PurpleAccount *purple_accounts_find_connected(const char *name, const char *protocol);

G_END_DECLS

