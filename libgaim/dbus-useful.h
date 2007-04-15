#include "conversation.h"

GaimAccount *gaim_accounts_find_ext(const char *name, const char *protocol_id,
				    gboolean (*account_test)(const GaimAccount *account));

GaimAccount *gaim_accounts_find_any(const char *name, const char *protocol);

GaimAccount *gaim_accounts_find_connected(const char *name, const char *protocol);





