/**
 * @file connection.c Connection API
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "connection.h"
#include "debug.h"
#include "gaim.h"

static GList *connections = NULL;
static GList *connections_connecting = NULL;
static GaimConnectionUiOps *connection_ui_ops = NULL;

GaimConnection *
gaim_connection_new(GaimAccount *account)
{
	GaimConnection *gc;

	g_return_val_if_fail(account != NULL, NULL);

	gc = g_new0(GaimConnection, 1);

	gc->prpl = gaim_find_prpl(gaim_account_get_protocol(account));

	gaim_connection_set_account(gc, account);
	gaim_account_set_connection(account, gc);

	return gc;
}

void
gaim_connection_destroy(GaimConnection *gc)
{
	GaimAccount *account;

	g_return_if_fail(gc != NULL);

	if (gaim_connection_get_state(gc) != GAIM_DISCONNECTED) {
		gaim_connection_disconnect(gc);

		return;
	}

	account = gaim_connection_get_account(gc);
	gaim_account_set_connection(account, NULL);

	if (gc->display_name != NULL)
		g_free(gc->display_name);

	if (gc->away != NULL)
		g_free(gc->away);

	if (gc->away_state != NULL)
		g_free(gc->away_state);

	g_free(gc);
}

void
gaim_connection_connect(GaimConnection *gc)
{
	GaimAccount *account;
	GaimConnectionUiOps *ops;
	GaimPluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(gc != NULL);

	ops = gaim_get_connection_ui_ops();

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	account = gaim_connection_get_account(gc);

	if ((prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
		!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL) &&
		gaim_account_get_password(account) == NULL) {

		gaim_debug(GAIM_DEBUG_INFO, "connection", "Requestin password\n");

		if (ops != NULL && ops->request_pass != NULL)
			ops->request_pass(gc);

		return;
	}

	gaim_connection_set_state(gc, GAIM_CONNECTING);

	gaim_debug(GAIM_DEBUG_INFO, "connection", "Calling serv_login\n");

	connections = g_list_append(connections, gc);

	serv_login(account);
}

void
gaim_connection_disconnect(GaimConnection *gc)
{
	GList *wins;

	g_return_if_fail(gc != NULL);

	if (gaim_connection_get_state(gc) != GAIM_DISCONNECTED) {
		if (gaim_connection_get_state(gc) != GAIM_CONNECTING)
			gaim_blist_remove_account(gaim_connection_get_account(gc));

		serv_close(gc);

		gaim_connection_set_state(gc, GAIM_DISCONNECTED);

		connections = g_list_remove(connections, gc);

		gaim_event_broadcast(event_signoff, gc);
		system_log(log_signoff, gc, NULL,
				   OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);

		/*
		 * XXX This is a hack! Remove this and replace it with a better event
		 *     notification system.
		 */
		for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
			GaimWindow *win = (GaimWindow *)wins->data;
			gaim_conversation_update(gaim_window_get_conversation_at(win, 0),
									 GAIM_CONV_ACCOUNT_OFFLINE);
		}

		gaim_request_close_with_handle(gc);
		gaim_notify_close_with_handle(gc);

		update_privacy_connections();
	}

	gaim_connection_destroy(gc);

	/* XXX More UI stuff! */
	if (connections != NULL)
		return;

	destroy_all_dialogs();
	gaim_blist_destroy();

	show_login();
}

/*
 * d:)->-< 
 *
 * d:O-\-<
 * 
 * d:D-/-<
 *
 * d8D->-< DANCE!
 */

void
gaim_connection_set_state(GaimConnection *gc, GaimConnectionState state)
{
	g_return_if_fail(gc != NULL);

	if (gc->state == state)
		return;

	gc->state = state;

	if (gc->state == GAIM_CONNECTED) {
		GaimConnectionUiOps *ops = gaim_get_connection_ui_ops();
		GaimBlistNode *gnode,*bnode;
		GList *wins;
		GList *add_buds=NULL;

		/* Set the time the account came online */
		time(&gc->login_time);

		connections_connecting = g_list_append(connections_connecting, gc);

		if (ops != NULL && ops->connected != NULL)
			ops->connected(gc);

		gaim_blist_show();
		gaim_blist_add_account(gc->account);

		/* I hate this. -- ChipX86 */
		gaim_setup(gc);

		/*
		 * XXX This is a hack! Remove this and replace it with a better event
		 *     notification system.
		 */
		for (wins = gaim_get_windows(); wins != NULL; wins = wins->next) {
			GaimWindow *win = (GaimWindow *)wins->data;
			gaim_conversation_update(gaim_window_get_conversation_at(win, 0),
									 GAIM_CONV_ACCOUNT_ONLINE);
		}

		gaim_event_broadcast(event_signon, gc);
		system_log(log_signon, gc, NULL,
				   OPT_LOG_BUDDY_SIGNON | OPT_LOG_MY_SIGNON);

#if 0
		/* away option given? */
		if (opt_away) {
			away_on_login(opt_away_arg);
			/* don't do it again */
			opt_away = 0;
		} else if (awaymessage) {
			serv_set_away(gc, GAIM_AWAY_CUSTOM, awaymessage->message);
		}
		if (opt_away_arg != NULL) {
			g_free(opt_away_arg);
			opt_away_arg = NULL;
		}
#endif

		/* let the prpl know what buddies we pulled out of the local list */
		for (gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
			if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for(bnode = gnode->child; bnode; bnode = bnode->next) {
				if(GAIM_BLIST_NODE_IS_BUDDY(bnode)) {
					struct buddy *b = (struct buddy *)bnode;
					if(b->account == gc->account) {
						add_buds = g_list_append(add_buds, b->name);
					}
				}
			}
		}

		if(add_buds) {
			serv_add_buddies(gc, add_buds);
			g_list_free(add_buds);
		}

		serv_set_permit_deny(gc);
	}
	else {
		connections_connecting = g_list_remove(connections_connecting, gc);
	}
}

void
gaim_connection_set_account(GaimConnection *gc, GaimAccount *account)
{
	g_return_if_fail(gc != NULL);
	g_return_if_fail(account != NULL);

	gc->account = account;
}

void
gaim_connection_set_display_name(GaimConnection *gc, const char *name)
{
	g_return_if_fail(gc != NULL);

	if (gc->display_name != NULL)
		g_free(gc->display_name);

	gc->display_name = (name == NULL ? NULL : g_strdup(name));
}

GaimConnectionState
gaim_connection_get_state(const GaimConnection *gc)
{
	g_return_val_if_fail(gc != NULL, GAIM_DISCONNECTED);

	return gc->state;
}

GaimAccount *
gaim_connection_get_account(const GaimConnection *gc)
{
	g_return_val_if_fail(gc != NULL, NULL);

	return gc->account;
}

const char *
gaim_connection_get_display_name(const GaimConnection *gc)
{
	g_return_val_if_fail(gc != NULL, NULL);

	return gc->display_name;
}

void
gaim_connection_update_progress(GaimConnection *gc, const char *text,
								size_t step, size_t count)
{
	GaimConnectionUiOps *ops;

	g_return_if_fail(gc   != NULL);
	g_return_if_fail(text != NULL);
	g_return_if_fail(step < count);
	g_return_if_fail(count > 1);

	ops = gaim_get_connection_ui_ops();

	if (ops != NULL && ops->connect_progress != NULL)
		ops->connect_progress(gc, text, step, count);
}

void
gaim_connection_notice(GaimConnection *gc, const char *text)
{
	GaimConnectionUiOps *ops;

	g_return_if_fail(gc   != NULL);
	g_return_if_fail(text != NULL);

	ops = gaim_get_connection_ui_ops();

	if (ops != NULL && ops->notice != NULL)
		ops->notice(gc, text);
}

void
gaim_connection_error(GaimConnection *gc, const char *text)
{
	GaimConnectionUiOps *ops;

	g_return_if_fail(gc   != NULL);
	g_return_if_fail(text != NULL);

	ops = gaim_get_connection_ui_ops();

	if (ops != NULL && ops->disconnected != NULL)
		ops->disconnected(gc, text);

	gaim_connection_disconnect(gc);
}

void
gaim_connections_disconnect_all(void)
{
	GList *l;

	while ((l = gaim_connections_get_all()) != NULL)
		gaim_connection_destroy(l->data);
}

GList *
gaim_connections_get_all(void)
{
	return connections;
}

GList *
gaim_connections_get_connecting(void)
{
	return connections_connecting;
}

void
gaim_set_connection_ui_ops(GaimConnectionUiOps *ops)
{
	connection_ui_ops = ops;
}

GaimConnectionUiOps *
gaim_get_connection_ui_ops(void)
{
	return connection_ui_ops;
}

