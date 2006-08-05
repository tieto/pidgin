#include <string.h>
#include <glib.h>

#include "dbus-useful.h"
#include "conversation.h"
#include "util.h"


GaimAccount *
gaim_accounts_find_ext(const char *name, const char *protocol_id,
		       gboolean (*account_test)(const GaimAccount *account))
{
	GaimAccount *result = NULL;
	GList *l;
	char *who;

	if (name)
		who = g_strdup(gaim_normalize(NULL, name));
	else
		who = NULL;

	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		GaimAccount *account = (GaimAccount *)l->data;

		if (who && strcmp(gaim_normalize(NULL, gaim_account_get_username(account)), who))
			continue;

		if (protocol_id && strcmp(account->protocol_id, protocol_id))
			continue;

		if (account_test && !account_test(account))
			continue;

		result = account;
		break;
	}

	g_free(who);

	return result;
}

GaimAccount *gaim_accounts_find_any(const char *name, const char *protocol)
{
	return gaim_accounts_find_ext(name, protocol, NULL);
}

GaimAccount *gaim_accounts_find_connected(const char *name, const char *protocol)
{
	return gaim_accounts_find_ext(name, protocol, gaim_account_is_connected);
}


