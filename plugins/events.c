/* events.c
 *
 * test every callback, print to stdout
 *
 * by EW
 *
 * GPL and all that jazz
 *
 */

#define EVENTTEST_PLUGIN_ID "core-eventtest"

#include <stdio.h>

#include "gtkplugin.h"
#include "connection.h"
#include "conversation.h"
#include "internal.h"

static void evt_signon(GaimConnection *gc, void *data)
{
	printf("event_signon\n");
}

static void evt_signoff(GaimConnection *gc, void *data)
{
	printf("event_signoff\n");
}

static void evt_away(GaimConnection *gc, char *state, char *message, void *data)
{
	printf("event_away: %s %s %s\n", gaim_account_get_username(gaim_connection_get_account(gc)),
		state, message);
}

static void evt_back(void *data)
{
	printf("event_back\n");
}

static void evt_im_recv(GaimConnection *gc, char **who, char **what, guint *flags, void *data)
{
	printf("event_im_recv: %s %s\n", *who, *what);
}

static void evt_im_send(GaimConnection *gc, char *who, char **what, void *data)
{
	printf("event_im_send: %s %s\n", who, *what);
}

static void evt_buddy_signon(GaimConnection *gc, char *who, void *data)
{
	printf("event_buddy_signon: %s\n", who);
}

static void evt_buddy_signoff(GaimConnection *gc, char *who, void *data)
{
	printf("event_buddy_signoff: %s\n", who);
}

static void evt_buddy_away(GaimConnection *gc, char *who, void *data)
{
	printf("event_buddy_away: %s\n", who);
}

static void evt_buddy_back(GaimConnection *gc, char *who, void *data)
{
	printf("event_buddy_back: %s\n", who);
}

static void evt_buddy_idle(GaimConnection *gc, char *who, void *data)
{
	printf("event_buddy_idle: %s\n", who);
}

static void evt_buddy_unidle(GaimConnection *gc, char *who, void *data)
{
	printf("event_buddy_unidle: %s\n", who);
}

static void evt_blist_update(void *data)
{
	printf("event_blist_update\n");
}

static void evt_chat_invited(GaimConnection *gc, char *who, char *room, char *message, void *data)
{
	printf("event_chat_invited: %s %s %s\n", who, room, message);
}

static void evt_chat_join(GaimConnection *gc, int id, void *data)
{
	printf("event_chat_join: %d\n", id);
}

static void evt_chat_leave(GaimConnection *gc, int id, void *data)
{
	printf("event_chat_leave: %d\n", id);
}

static void evt_chat_buddy_join(GaimConnection *gc, int id, char *who, void *data)
{
	printf("event_chat_buddy_join: %d %s\n", id, who);
}

static void evt_chat_buddy_leave(GaimConnection *gc, int id, char *who, void *data)
{
	printf("event_chat_buddy_leave: %d %s\n", id, who);
}

static void evt_chat_recv(GaimConnection *gc, int id, char *who, char *text, void *data)
{
	printf("event_chat_recv: %d %s %s\n", id, who, text);
}

static void evt_chat_send(GaimConnection *gc, int id, char **what, void *data)
{
	printf("event_chat_send: %d %s\n", id, *what);
}

static void evt_warned(GaimConnection *gc, char *who, int level, void *data)
{
	printf("event_warned: %s %d\n", who, level);
}

static void evt_error(int error, void *data)
{
	printf("event_error: %d\n", error);
}

static void evt_quit(void *data)
{
	printf("event_quit\n");
}

static void evt_new_conversation(char *who, void *data)
{
	printf("event_new_conversation: %s\n", who);
}

static void evt_set_info(GaimConnection *gc, char *info, void *data)
{
	printf("event_set_info: %s\n", info);
}

static void evt_draw_menu(GtkWidget *menu, char *name, void *data)
{
	printf("event_draw_menu: %s\n", name);
}

static void evt_im_displayed_sent(GaimConnection *gc, char *who, char **what, void *data)
{
	printf("event_im_displayed_sent: %s %s\n", who, *what);
}

static void evt_im_displayed_rcvd(GaimConnection *gc, char *who, char *what, guint32 flags, time_t time, void *data)
{
	printf("event_im_displayed_rcvd: %s %s %u %u\n", who, what, flags, time);
}

static void evt_chat_send_invite(GaimConnection *gc, int id, char *who, char **msg, void *data)
{
	printf("event_chat_send_invite: %d %s %s\n", id, who, *msg);
}

static void evt_got_typing(GaimConnection *gc, char *who, void *data)
{
	printf("event_got_typing: %s\n", who);
}

static void evt_del_conversation(GaimConversation *c, void *data)
{
	printf("event_del_conversation\n");
}

static void evt_connecting(GaimAccount *u, void *data)
{
	printf("event_connecting\n");
}

static void evt_change(GaimConversation *c)
{
	printf("event_conversation_switch\n");
}

/*
 *  EXPORTED FUNCTIONS
 */

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_signal_connect(plugin, event_signon,		evt_signon, NULL);
	gaim_signal_connect(plugin, event_signoff,		evt_signoff, NULL);
	gaim_signal_connect(plugin, event_away,		evt_away, NULL);
	gaim_signal_connect(plugin, event_back,		evt_back, NULL);
	gaim_signal_connect(plugin, event_im_recv,		evt_im_recv, NULL);
	gaim_signal_connect(plugin, event_im_send,		evt_im_send, NULL);
	gaim_signal_connect(plugin, event_buddy_signon,	evt_buddy_signon, NULL);
	gaim_signal_connect(plugin, event_buddy_signoff,	evt_buddy_signoff, NULL);
	gaim_signal_connect(plugin, event_buddy_away,	evt_buddy_away, NULL);
	gaim_signal_connect(plugin, event_buddy_back,	evt_buddy_back, NULL);
	gaim_signal_connect(plugin, event_chat_invited,	evt_chat_invited, NULL);
	gaim_signal_connect(plugin, event_chat_join,		evt_chat_join, NULL);
	gaim_signal_connect(plugin, event_chat_leave,	evt_chat_leave, NULL);
	gaim_signal_connect(plugin, event_chat_buddy_join,	evt_chat_buddy_join, NULL);
	gaim_signal_connect(plugin, event_chat_buddy_leave,	evt_chat_buddy_leave, NULL);
	gaim_signal_connect(plugin, event_chat_recv,		evt_chat_recv, NULL);
	gaim_signal_connect(plugin, event_chat_send,		evt_chat_send, NULL);
	gaim_signal_connect(plugin, event_warned,		evt_warned, NULL);
	gaim_signal_connect(plugin, event_error,		evt_error, NULL);
	gaim_signal_connect(plugin, event_quit,		evt_quit, NULL);
	gaim_signal_connect(plugin, event_new_conversation,	evt_new_conversation, NULL);
	gaim_signal_connect(plugin, event_set_info,		evt_set_info, NULL);
	gaim_signal_connect(plugin, event_draw_menu,		evt_draw_menu, NULL);
	gaim_signal_connect(plugin, event_im_displayed_sent,	evt_im_displayed_sent, NULL);
	gaim_signal_connect(plugin, event_im_displayed_rcvd, evt_im_displayed_rcvd, NULL);
	gaim_signal_connect(plugin, event_chat_send_invite,	evt_chat_send_invite, NULL);
	gaim_signal_connect(plugin, event_got_typing, 	evt_got_typing, NULL);
	gaim_signal_connect(plugin, event_del_conversation,	evt_del_conversation, NULL);
	gaim_signal_connect(plugin, event_connecting,	evt_connecting, NULL);
	gaim_signal_connect(plugin, event_conversation_switch, evt_change, NULL);
	return TRUE;
}

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	EVENTTEST_PLUGIN_ID,                              /**< id             */
	N_("Event Test"),                                 /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Test to see that all events are working properly."),
	                                                  /**  description    */
	N_("Test to see that all events are working properly."),
	"Eric Warmenhoven <eric@warmenhoven.org>",        /**< author         */
	WEBSITE,                                          /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(eventtester, init_plugin, info);
