#include "internal.h"
#include "plugin.h"

#include "account.h"
#include "connection.h"
#include "conversation.h"
#include "version.h"

/* include UI for gaim_gtkdialogs_about() */
#include "gtkplugin.h"
#include "gtkdialogs.h"

#define GAIMINC_PLUGIN_ID "core-gaiminc"

static void
echo_hi(GaimConnection *gc)
{
	/* this doesn't do much, just lets you know who we are :) */
	gaim_gtkdialogs_about();
}

static gboolean
reverse(GaimAccount *account, char **who, char **message,
		GaimConversation *conv, int *flags)
{
	/* this will drive you insane. whenever you receive a message,
	 * the text of the message (HTML and all) will be reversed. */
	int i, l;
	char tmp;

	/* this check is necessary in case bad plugins do bad things */
	if (message == NULL || *message == NULL)
		return FALSE;

	l = strlen(*message);

	if (!strcmp(*who, gaim_account_get_username(account)))
		return FALSE;

	for (i = 0; i < l/2; i++) {
		tmp = (*message)[i];
		(*message)[i] = (*message)[l - i - 1];
		(*message)[l - i - 1] = tmp;
	}
	return FALSE;
}

static void
bud(GaimBuddy *who)
{
	GaimAccount *acct = who->account;
	GaimConversation *conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, acct, who->name);

	gaim_conv_im_send(GAIM_CONV_IM(conv), "Hello!");
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	/* this is for doing something fun when we sign on */
	gaim_signal_connect(gaim_connections_get_handle(), "signed-on",
						plugin, GAIM_CALLBACK(echo_hi), NULL);

	/* this is for doing something fun when we get a message */
	gaim_signal_connect(gaim_conversations_get_handle(), "receiving-im-msg",
						plugin, GAIM_CALLBACK(reverse), NULL);

	/* this is for doing something fun when a buddy comes online */
	gaim_signal_connect(gaim_blist_get_handle(), "buddy-signed-on",
						plugin, GAIM_CALLBACK(bud), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	GAIMINC_PLUGIN_ID,                                /**< id             */
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
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	NULL                                              /**< actions        */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(gaiminc, init_plugin, info)
