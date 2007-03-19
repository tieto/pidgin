#include "internal.h"
#include "plugin.h"

#include "account.h"
#include "connection.h"
#include "conversation.h"
#include "version.h"

/* include UI for pidgindialogs_about() */
#include "gtkplugin.h"
#include "gtkdialogs.h"

#define PURPLEINC_PLUGIN_ID "core-purpleinc"

static void
echo_hi(PurpleConnection *gc)
{
	/* this doesn't do much, just lets you know who we are :) */
	pidgindialogs_about();
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
	PurpleAccount *acct = who->account;
	PurpleConversation *conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, acct, who->name);

	purple_conv_im_send(PURPLE_CONV_IM(conv), "Hello!");
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(PurplePlugin *plugin)
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

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	PURPLEINC_PLUGIN_ID,                                /**< id             */
	N_(PIDGIN_NAME " Demonstration Plugin"),                  /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("An example plugin that does stuff - see the description."),
	                                                  /**  description    */
	N_("This is a really cool plugin that does a lot of stuff:\n"
	   "- It tells you who wrote the program when you log in\n"
	   "- It reverses all incoming text\n"
	   "- It sends a message to people on your list immediately"
	   " when they sign on"),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL                                              /**< actions        */
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(purpleinc, init_plugin, info)
