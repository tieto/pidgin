#include "internal.h"
#include "plugins.h"

#include "account.h"
#include "connection.h"
#include "conversation.h"
#include "version.h"

/* include UI for pidgin_dialogs_about() */
#include "gtkplugin.h"
#include "gtkdialogs.h"

#define PURPLEINC_PLUGIN_ID "core-purpleinc"

static void
echo_hi(PurpleConnection *gc)
{
	/* this doesn't do much, just lets you know who we are :) */
	pidgin_dialogs_about();
}

static gboolean
reverse(PurpleAccount *account, char **who, char **message,
		PurpleConversation *conv, int *flags)
{
	/* this will drive you insane. whenever you receive a message,
	 * the text of the message (HTML and all) will be reversed. */
	int i, l;
	char tmp;

	/* this check is necessary in case bad plugins do bad things */
	if (message == NULL || *message == NULL)
		return FALSE;

	l = strlen(*message);

	if (!strcmp(*who, purple_account_get_username(account)))
		return FALSE;

	for (i = 0; i < l/2; i++) {
		tmp = (*message)[i];
		(*message)[i] = (*message)[l - i - 1];
		(*message)[l - i - 1] = tmp;
	}
	return FALSE;
}

static void
bud(PurpleBuddy *who)
{
	PurpleAccount *acct = purple_buddy_get_account(who);
	PurpleIMConversation *im = purple_im_conversation_new(acct,
			purple_buddy_get_name(who));

	purple_conversation_send(PURPLE_CONVERSATION(im), "Hello!");
}

/*
 *  EXPORTED FUNCTIONS
 */

static PidginPluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Eric Warmenhoven <eric@warmenhoven.org>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           PURPLEINC_PLUGIN_ID,
		"name",         N_("Pidgin Demonstration Plugin"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Example"),
		"summary",      N_("An example plugin that does stuff - see the description."),
		"description",  N_("This is a really cool plugin that does a lot of stuff:\n"
		                   "- It tells you who wrote the program when you log in\n"
		                   "- It reverses all incoming text\n"
		                   "- It sends a message to people on your list immediately"
		                   " when they sign on"),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	/* this is for doing something fun when we sign on */
	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						plugin, PURPLE_CALLBACK(echo_hi), NULL);

	/* this is for doing something fun when we get a message */
	purple_signal_connect(purple_conversations_get_handle(), "receiving-im-msg",
						plugin, PURPLE_CALLBACK(reverse), NULL);

	/* this is for doing something fun when a buddy comes online */
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on",
						plugin, PURPLE_CALLBACK(bud), NULL);

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	return TRUE;
}

PURPLE_PLUGIN_INIT(purpleinc, plugin_query, plugin_load, plugin_unload);
