/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 *
 */
#include "internal.h"
#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "multi.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "server.h"
#include "sound.h"
#include "util.h"

/* XXX UI Stuff */
#include "gaim.h"
#include "gtkimhtml.h"
#include "gtkutils.h"
#include "ui.h"

void serv_login(GaimAccount *account)
{
	GaimPlugin *p = gaim_find_prpl(gaim_account_get_protocol(account));
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (account->gc == NULL || p == NULL)
		return;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(p);

	if (prpl_info->login) {
		if (gaim_account_get_password(account) == NULL &&
			!(prpl_info->options & OPT_PROTO_NO_PASSWORD) &&
			!(prpl_info->options & OPT_PROTO_PASSWORD_OPTIONAL)) {
			gaim_notify_error(NULL, NULL,
							  _("Please enter your password"), NULL);
			return;
		}

		gaim_debug(GAIM_DEBUG_INFO, "server",
				   PACKAGE " " VERSION " logging in %s using %s\n",
				   account->username, p->info->name);

		gaim_signal_emit(gaim_accounts_get_handle(),
						 "account-connecting", account);
		prpl_info->login(account);
	}
}

static gboolean send_keepalive(gpointer d)
{
	GaimConnection *gc = d;
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->keepalive)
		prpl_info->keepalive(gc);

	return TRUE;
}

static void update_keepalive(GaimConnection *gc, gboolean on)
{
	if (on && !gc->keep_alive) {
		gaim_debug(GAIM_DEBUG_INFO, "server", "allowing NOP\n");
		gc->keep_alive = g_timeout_add(60000, send_keepalive, gc);
	} else if (!on && gc->keep_alive > 0) {
		gaim_debug(GAIM_DEBUG_INFO, "server", "removing NOP\n");
		g_source_remove(gc->keep_alive);
		gc->keep_alive = 0;
	}
}

void serv_close(GaimConnection *gc)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info = NULL;

	while (gc->buddy_chats) {
		GaimConversation *b = gc->buddy_chats->data;

		gc->buddy_chats = g_slist_remove(gc->buddy_chats, b);
	}

	if (gc->idle_timer > 0)
		g_source_remove(gc->idle_timer);
	gc->idle_timer = 0;

	update_keepalive(gc, FALSE);

	if (gc->prpl != NULL) {
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

		if (prpl_info->close)
			(prpl_info->close)(gc);
	}

	prpl = gc->prpl;
}

void serv_touch_idle(GaimConnection *gc)
{
	/* Are we idle?  If so, not anymore */
	if (gc->is_idle > 0) {
		gc->is_idle = 0;
		serv_set_idle(gc, 0);
	}
	time(&gc->last_sent_time);
	if (gc->is_auto_away)
		check_idle(gc);
}

void serv_finish_login(GaimConnection *gc)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimAccount *account;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	account = gaim_connection_get_account(gc);

	if (gaim_account_get_user_info(account) != NULL) {
		/* buf = strdup_withhtml(gc->user->user_info); */
		serv_set_info(gc, gaim_account_get_user_info(account));
		/* g_free(buf); */
	}

	if (gc->idle_timer > 0)
		g_source_remove(gc->idle_timer);

	gc->idle_timer = g_timeout_add(20000, check_idle, gc);
	serv_touch_idle(gc);

	if (prpl_info->options & OPT_PROTO_CORRECT_TIME)
		serv_add_buddy(gc,
				gaim_account_get_username(gaim_connection_get_account(gc)),
				NULL);

	update_keepalive(gc, TRUE);
}

/* This should return the elapsed time in seconds in which Gaim will not send
 * typing notifications.
 * if it returns zero, it will not send any more typing notifications 
 * typing is a flag - TRUE for typing, FALSE for stopped typing */
int serv_send_typing(GaimConnection *g, const char *name, int typing) {
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (g && prpl_info && prpl_info->send_typing)
		return prpl_info->send_typing(g, name, typing);

	return 0;
}

GSList *last_auto_responses = NULL;
struct last_auto_response {
	GaimConnection *gc;
	char name[80];
	time_t sent;
};

gboolean expire_last_auto_responses(gpointer data)
{
	GSList *tmp, *cur;
	struct last_auto_response *lar;

	tmp = last_auto_responses;

	while (tmp) {
		cur = tmp;
		tmp = tmp->next;
		lar = (struct last_auto_response *)cur->data;

		if ((time(NULL) - lar->sent) >
				gaim_prefs_get_int("/core/away/auto_response/sec_before_resend")) {

			last_auto_responses = g_slist_remove(last_auto_responses, lar);
			g_free(lar);
		}
	}

	return FALSE; /* do not run again */
}

struct last_auto_response *get_last_auto_response(GaimConnection *gc, const char *name)
{
	GSList *tmp;
	struct last_auto_response *lar;

	/* because we're modifying or creating a lar, schedule the
	 * function to expire them as the pref dictates */
	g_timeout_add((gaim_prefs_get_int("/core/away/auto_response/sec_before_resend") + 1) * 1000,
			expire_last_auto_responses, NULL);

	tmp = last_auto_responses;

	while (tmp) {
		lar = (struct last_auto_response *)tmp->data;

		if (gc == lar->gc && !strncmp(name, lar->name, sizeof(lar->name)))
			return lar;

		tmp = tmp->next;
	}

	lar = (struct last_auto_response *)g_new0(struct last_auto_response, 1);
	g_snprintf(lar->name, sizeof(lar->name), "%s", name);
	lar->gc = gc;
	lar->sent = 0;
	last_auto_responses = g_slist_append(last_auto_responses, lar);

	return lar;
}

void flush_last_auto_responses(GaimConnection *gc)
{
	GSList *tmp, *cur;
	struct last_auto_response *lar;

	tmp = last_auto_responses;

	while (tmp) {
		cur = tmp;
		tmp = tmp->next;
		lar = (struct last_auto_response *)cur->data;

		if (lar->gc == gc) {
			last_auto_responses = g_slist_remove(last_auto_responses, lar);
			g_free(lar);
		}
	}
}

int serv_send_im(GaimConnection *gc, const char *name, const char *message,
				 int len, GaimImFlags imflags)
{
	GaimConversation *c;
	int val = -EINVAL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	c = gaim_find_conversation_with_account(name, gc->account);

	if (prpl_info && prpl_info->send_im)
		val = prpl_info->send_im(gc, name, message, len, imflags);

	if (!(imflags & GAIM_IM_AUTO_RESP))
		serv_touch_idle(gc);

	if (gc->away &&
		(gc->flags & GAIM_CONNECTION_AUTO_RESP) &&
		gaim_prefs_get_bool("/core/away/auto_response/enabled") &&
		!gaim_prefs_get_bool("/core/away/auto_response/in_active_conv")) {

		struct last_auto_response *lar;
		lar = get_last_auto_response(gc, name);
		lar->sent = time(NULL);
	}

	if (c && gaim_im_get_type_again_timeout(GAIM_IM(c)))
		gaim_im_stop_type_again_timeout(GAIM_IM(c));

	return val;
}

void serv_get_info(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (g && prpl_info && prpl_info->get_info)
		prpl_info->get_info(g, name);
}

void serv_get_away(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (g && prpl_info && prpl_info->get_away)
		prpl_info->get_away(g, name);
}

void serv_get_dir(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->get_dir)
		prpl_info->get_dir(g, name);
}

void serv_set_dir(GaimConnection *g, const char *first,
				  const char *middle, const char *last, const char *maiden,
				  const char *city, const char *state, const char *country,
				  int web)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->set_dir)
		prpl_info->set_dir(g, first, middle, last, maiden, city, state,
						 country, web);
}

void serv_dir_search(GaimConnection *g, const char *first,
					 const char *middle, const char *last, const char *maiden,
		     const char *city, const char *state, const char *country,
			 const char *email)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->dir_search)
		prpl_info->dir_search(g, first, middle, last, maiden, city, state,
							country, email);
}


void serv_set_away(GaimConnection *gc, const char *state, const char *message)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimAccount *account;

	if (gc->away_state == NULL && state == NULL &&
		gc->away == NULL && message == NULL) {

		return;
	}

	if ((gc->away_state != NULL && state != NULL &&
		 !strcmp(gc->away_state, state) &&
		 !strcmp(gc->away_state, GAIM_AWAY_CUSTOM)) &&
		(gc->away != NULL && message != NULL && !strcmp(gc->away, message))) {

		return;
	}

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	account = gaim_connection_get_account(gc);

	if (prpl_info && prpl_info->set_away) {
		if (gc->away_state) {
			g_free(gc->away_state);
			gc->away_state = NULL;
		}

		prpl_info->set_away(gc, state, message);

		if (gc->away && state) {
			gc->away_state = g_strdup(state);
		}

		gaim_signal_emit(gaim_accounts_get_handle(), "account-away",
						 account, state, message);
	}

	/* New away message... Clear out the record of sent autoresponses */
	flush_last_auto_responses(gc);

	system_log(log_away, gc, NULL, OPT_LOG_BUDDY_AWAY | OPT_LOG_MY_SIGNON);
}

void serv_set_away_all(const char *message)
{
	GList *c;
	GaimConnection *g;

	for (c = gaim_connections_get_all(); c != NULL; c = c->next) {
		g = (GaimConnection *)c->data;

		serv_set_away(g, GAIM_AWAY_CUSTOM, message);
	}
}

void serv_set_info(GaimConnection *g, const char *info)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimAccount *account;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) &&
		prpl_info->set_info) {

		account = gaim_connection_get_account(g);

		if (gaim_signal_emit_return_1(gaim_accounts_get_handle(),
									  "account-setting-info", account, info))
			return;

		prpl_info->set_info(g, info);

		gaim_signal_emit(gaim_accounts_get_handle(),
						 "account-set-info", account, info);
	}
}

void serv_change_passwd(GaimConnection *g, const char *orig, const char *new)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->change_passwd)
		prpl_info->change_passwd(g, orig, new);
}

void serv_add_buddy(GaimConnection *g, const char *name, GaimGroup *group)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->add_buddy)
		prpl_info->add_buddy(g, name, group);
}

void serv_add_buddies(GaimConnection *g, GList *buddies)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g)) {
		if (prpl_info->add_buddies)
			prpl_info->add_buddies(g, buddies);
		else if (prpl_info->add_buddy) {
			while (buddies) {
				prpl_info->add_buddy(g, buddies->data, NULL);
				buddies = buddies->next;
			}
		}
	}
}


void serv_remove_buddy(GaimConnection *g, const char *name, const char *group)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->remove_buddy)
		prpl_info->remove_buddy(g, name, group);
}

void serv_remove_buddies(GaimConnection *gc, GList *g, const char *group)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (!g_list_find(gaim_connections_get_all(), gc))
		return;

	if (!gc->prpl)
		return;		/* how the hell did that happen? */

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info->remove_buddies)
		prpl_info->remove_buddies(gc, g, group);
	else {
		while (g) {
			serv_remove_buddy(gc, g->data, group);
			g = g->next;
		}
	}
}

/*
 * Set buddy's alias on server roster/list
 */
void serv_alias_buddy(GaimBuddy *b)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (b != NULL && b->account->gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(b->account->gc->prpl);

	if (b && prpl_info && prpl_info->alias_buddy) {
		prpl_info->alias_buddy(b->account->gc, b->name, b->alias);
	}
}

void serv_got_alias(GaimConnection *gc, const char *who, const char *alias) {
	GaimBuddy *b = gaim_find_buddy(gc->account, who);

	if(!b)
		return;

	gaim_blist_server_alias_buddy(b, alias);
}

/*
 * Move a buddy from one group to another on server.
 *
 * Note: For now we'll not deal with changing gc's at the same time, but
 * it should be possible.  Probably needs to be done, someday.
 */
void serv_move_buddy(GaimBuddy *b, GaimGroup *og, GaimGroup *ng)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (b->account->gc != NULL && b->account->gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(b->account->gc->prpl);

	if (b && b->account->gc && og && ng) {
		if (prpl_info && prpl_info->group_buddy) {
			prpl_info->group_buddy(b->account->gc, b->name, og->name, ng->name);
		}
	}
}

/*
 * Rename a group on server roster/list.
 */
void serv_rename_group(GaimConnection *g, GaimGroup *old_group,
					   const char *new_name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && old_group && new_name) {
		GList *tobemoved = NULL;
		GaimBlistNode *cnode, *bnode;

		for(cnode = ((GaimBlistNode*)old_group)->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *b;
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				b = (GaimBuddy*)bnode;

				if(b->account == g->account)
					tobemoved = g_list_append(tobemoved, b->name);

			}

		}

		if (prpl_info->rename_group) {
			/* prpl's might need to check if the group already 
			 * exists or not, and handle that differently */
			prpl_info->rename_group(g, old_group->name, new_name, tobemoved);
		} else {
			serv_remove_buddies(g, tobemoved, old_group->name);
			serv_add_buddies(g, tobemoved);
		}

		g_list_free(tobemoved);
	}
}

void serv_add_permit(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->add_permit)
		prpl_info->add_permit(g, name);
}

void serv_add_deny(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->add_deny)
		prpl_info->add_deny(g, name);
}

void serv_rem_permit(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->rem_permit)
		prpl_info->rem_permit(g, name);
}

void serv_rem_deny(GaimConnection *g, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->rem_deny)
		prpl_info->rem_deny(g, name);
}

void serv_set_permit_deny(GaimConnection *g)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	/*
	 * this is called when either you import a buddy list, and make lots
	 * of changes that way, or when the user toggles the permit/deny mode
	 * in the prefs. In either case you should probably be resetting and
	 * resending the permit/deny info when you get this.
	 */
	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->set_permit_deny)
		prpl_info->set_permit_deny(g);
}


void serv_set_idle(GaimConnection *g, int time)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->set_idle)
		prpl_info->set_idle(g, time);
}

void serv_warn(GaimConnection *g, const char *name, int anon)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->warn)
		prpl_info->warn(g, name, anon);
}

void serv_join_chat(GaimConnection *g, GHashTable *data)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->join_chat)
		prpl_info->join_chat(g, data);
}

void serv_chat_invite(GaimConnection *g, int id, const char *message, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimConversation *conv;
	char *buffy = message && *message ? g_strdup(message) : NULL;

	conv = gaim_find_chat(g, id);

	if (conv == NULL)
		return;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-inviting-user",
					 conv, name, &buffy);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->chat_invite)
		prpl_info->chat_invite(g, id, buffy, name);

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-invited-user",
					 conv, name, buffy);

	if (buffy)
		g_free(buffy);
}

void serv_chat_leave(GaimConnection *g, int id)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (!g_list_find(gaim_connections_get_all(), g))
		return;

	if (g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->chat_leave)
		prpl_info->chat_leave(g, id);
}

void serv_chat_whisper(GaimConnection *g, int id, const char *who, const char *message)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->chat_whisper)
		prpl_info->chat_whisper(g, id, who, message);
}

int serv_chat_send(GaimConnection *g, int id, const char *message)
{
	int val = -EINVAL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->chat_send)
		val = prpl_info->chat_send(g, id, message);

	serv_touch_idle(g);

	return val;
}

void serv_set_buddyicon(GaimConnection *gc, const char *filename)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	
	if (gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->set_buddy_icon)
		prpl_info->set_buddy_icon(gc, filename);
	
}	

int find_queue_row_by_name(char *name)
{
	gchar *temp;
	gint i = 0;
	gboolean valid;
	GtkTreeIter iter;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(awayqueuestore), &iter);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(awayqueuestore), &iter, 0, &temp, -1);
		if(!strcmp(name, temp))
			return i;
		g_free(temp);
		
		i++;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(awayqueuestore), &iter);
	}

	return -1;
}

int find_queue_total_by_name(char *name)
{
	GSList *templist;
	int i = 0;

	templist = message_queue;

	while (templist) {
		struct queued_message *qm = (struct queued_message *)templist->data;
		if ((qm->flags & GAIM_MESSAGE_RECV) && !strcmp(name, qm->name))
			i++;

		templist = templist->next;
	}

	return i;
}

/*
 * woo. i'm actually going to comment this function. isn't that fun. make
 * sure to follow along, kids
 */
void serv_got_im(GaimConnection *gc, const char *who, const char *msg,
				 GaimImFlags imflags, time_t mtime, gint len)
{
	GaimConversation *cnv;
	GaimMessageFlags auto_resp;
	char *message, *name;
	char *angel, *buffy;
	int plugin_return;

	/*
	 * We should update the conversation window buttons and menu,
	 * if it exists.
	 */
	cnv = gaim_find_conversation_with_account(who, gc->account);

	/*
	 * Plugin stuff. we pass a char ** but we don't want to pass what's
	 * been given us by the prpls. So we create temp holders and pass
	 * those instead. It's basically just to avoid segfaults. Of course,
	 * if the data is binary, plugins don't see it. Bitch all you want;
	 * I really don't want you to be dealing with it.
	 */
	if (len < 0) {
		buffy = g_malloc(MAX(strlen(msg) + 1, BUF_LONG));
		strcpy(buffy, msg);
		angel = g_strdup(who);

		plugin_return = GPOINTER_TO_INT(
			gaim_signal_emit_return_1(gaim_conversations_get_handle(),
									  "received-im-msg", gc->account,
									  &angel, &buffy, &imflags));

		if (!buffy || !angel || plugin_return) {
			if (buffy)
				g_free(buffy);
			if (angel)
				g_free(angel);
			return;
		}
		name = angel;
		message = buffy;
	} else {
		name = g_strdup(who);
		message = g_memdup(msg, len);
	}

	/*
	 * If you can't figure this out, stop reading right now.
	 * "We're not worthy! We're not worthy!"
	 */
	if (len < 0 &&
		gaim_prefs_get_bool("/gaim/gtk/conversations/show_urls_as_links")) {

		buffy = linkify_text(message);
		g_free(message);
		message = buffy;
	}

	/*
	 * Um. When we call gaim_conversation_write with the message we received,
	 * it's nice to pass whether or not it was an auto-response. So if it
	 * was an auto-response, we set the appropriate flag. This is just so
	 * prpls don't have to know about GAIM_MESSAGE_* (though some do anyway)
	 */
	if (imflags & GAIM_IM_AUTO_RESP)
		auto_resp = GAIM_MESSAGE_AUTO_RESP;
	else
		auto_resp = 0;

	/*
	 * Alright. Two cases for how to handle this. Either we're away or
	 * we're not. If we're not, then it's easy. If we are, then there
	 * are three or four different ways of handling it and different
	 * things we have to do for each.
	 */
	if (gc->away) {
		time_t t = time(NULL);
		char *tmpmsg;
		GaimBuddy *b = gaim_find_buddy(gc->account, name);
		const char *alias = b ? gaim_get_buddy_alias(b) : name;
		int row;
		struct last_auto_response *lar;

		/*
		 * Either we're going to queue it or not. Because of the way
		 * awayness currently works, this is fucked up. It's possible
		 * for an account to be away without the imaway dialog being
		 * shown. In fact, it's possible for *all* the accounts to be
		 * away without the imaway dialog being shown. So in order for
		 * this to be queued properly, we have to make sure that the
		 * imaway dialog actually exists, first.
		 */
		if (!cnv && awayqueue &&
			gaim_prefs_get_bool("/gaim/gtk/away/queue_messages")) {
			/* 
			 * Alright, so we're going to queue it. Neat, eh? :)
			 * So first we create something to store the message, and add
			 * it to our queue. Then we update the away dialog to indicate
			 * that we've queued something.
			 */
			struct queued_message *qm;
			GtkTreeIter iter;
			gchar path[10];

			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_memdup(message, len == -1 ? strlen(message) + 1 : len);
			qm->account = gc->account;
			qm->tm = mtime;
			qm->flags = GAIM_MESSAGE_RECV | auto_resp;
			qm->len = len;
			message_queue = g_slist_append(message_queue, qm);

			row = find_queue_row_by_name(qm->name);
			if (row >= 0) {
				char number[32];
				int qtotal;

				qtotal = find_queue_total_by_name(qm->name);
				g_snprintf(number, 32, ngettext("(%d message)",
						   "(%d messages)", qtotal), qtotal);
				g_snprintf(path, 10, "%d", row);
				gtk_tree_model_get_iter_from_string(
								GTK_TREE_MODEL(awayqueuestore), &iter, path);
				gtk_list_store_set(awayqueuestore, &iter,
								1, number, -1);
			} else {
				gtk_tree_model_get_iter_first(GTK_TREE_MODEL(awayqueuestore), 
								&iter);
				gtk_list_store_append(awayqueuestore, &iter);
				gtk_list_store_set(awayqueuestore, &iter,
								0, qm->name,
								1, _("(1 message)"),
								-1);
			}
		} else {
			/*
			 * Make sure the conversation
			 * exists and is updated (partly handled above already), play
			 * the receive sound (sound.c will take care of not playing
			 * while away), and then write it to the convo window.
			 */
			if (cnv == NULL)
				cnv = gaim_conversation_new(GAIM_CONV_IM, gc->account, name);

			gaim_im_write(GAIM_IM(cnv), NULL, message, len,
						  GAIM_MESSAGE_RECV | auto_resp, mtime);
		}

		/*
		 * Regardless of whether we queue it or not, we should send an
		 * auto-response. That is, of course, unless the horse.... no wait.
		 * Don't autorespond if:
		 *
		 *  - it's not supported on this connection
		 *  - or it's disabled
		 *  - or the away message is empty
		 *  - or we're not idle and the 'only auto respond if idle' pref
		 *    is set
		 */
		if (!(gc->flags & GAIM_CONNECTION_AUTO_RESP) ||
			!gaim_prefs_get_bool("/core/away/auto_response/enabled") ||
			*gc->away == '\0' ||
			(!gc->is_idle &&
			 gaim_prefs_get_bool("/core/away/auto_response/idle_only"))) {

			g_free(name);
			g_free(message);
			return;
		}

		/*
		 * This used to be based on the conversation window. But um, if
		 * you went away, and someone sent you a message and got your
		 * auto-response, and then you closed the window, and then the
		 * sent you another one, they'd get the auto-response back too
		 * soon. Besides that, we need to keep track of this even if we've
		 * got a queue. So the rest of this block is just the auto-response,
		 * if necessary.
		 */
		lar = get_last_auto_response(gc, name);
		if ((t - lar->sent) <
			gaim_prefs_get_int("/core/away/auto_response/sec_before_resend")) {

			g_free(name);
			g_free(message);
			return;
		}
		lar->sent = t;

		/* apply default fonts and colors */
		tmpmsg = stylize(gc->away, MSG_LEN);
		serv_send_im(gc, name, away_subs(tmpmsg, alias), -1, GAIM_IM_AUTO_RESP);
		if (!cnv && awayqueue &&
			gaim_prefs_get_bool("/gaim/gtk/away/queue_messages")) {

			struct queued_message *qm;

			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_strdup(away_subs(tmpmsg, alias));
			qm->account = gc->account;
			qm->tm = mtime;
			qm->flags = GAIM_MESSAGE_SEND | GAIM_MESSAGE_AUTO_RESP;
			qm->len = -1;
			message_queue = g_slist_append(message_queue, qm);
		} else if (cnv != NULL)
			gaim_im_write(GAIM_IM(cnv), NULL, away_subs(tmpmsg, alias),
						  len, GAIM_MESSAGE_SEND | GAIM_MESSAGE_AUTO_RESP, mtime);

		g_free(tmpmsg);
	} else {
		/*
		 * We're not away. This is easy. If the convo window doesn't
		 * exist, create and update it (if it does exist it was updated
		 * earlier), then play a sound indicating we've received it and
		 * then display it. Easy.
		 */

		/* XXX UGLY HACK OF THE YEAR
		 * Robot101 will fix this after his exams. honest.
		 * I guess he didn't specify WHICH exams, exactly...
		 */
		if (docklet_count &&
		    gaim_prefs_get_bool("/plugins/gtk/docklet/queue_messages") &&
		    !gaim_find_conversation_with_account(name, gc->account)) {
			/*
			 * We're gonna queue it up and wait for the user to ask for
			 * it... probably by clicking the docklet or windows tray icon.
			 */
			struct queued_message *qm;
			qm = g_new0(struct queued_message, 1);
			g_snprintf(qm->name, sizeof(qm->name), "%s", name);
			qm->message = g_strdup(message);
			qm->account = gc->account;
			qm->tm = mtime;
			qm->flags = GAIM_MESSAGE_RECV | auto_resp;
			qm->len = len;
			unread_message_queue = g_slist_append(unread_message_queue, qm);
		}
		else {
			if (cnv == NULL)
				cnv = gaim_conversation_new(GAIM_CONV_IM, gc->account, name);

			gaim_im_write(GAIM_IM(cnv), NULL, message, len,
						  GAIM_MESSAGE_RECV | auto_resp, mtime);
			gaim_window_flash(gaim_conversation_get_window(cnv));
		}
	}

	g_free(name);
	g_free(message);
}



void serv_got_update(GaimConnection *gc, const char *name, int loggedin,
					 int evil, time_t signon, time_t idle, int type)
{
	GaimAccount *account;
	GaimConversation *c;
	GaimBuddy *b;
	GSList *buddies;

	account = gaim_connection_get_account(gc);
	b = gaim_find_buddy(account, name);

	if (signon && (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->options &
				   OPT_PROTO_CORRECT_TIME)) {

		char *tmp = g_strdup(normalize(name));
		if (!gaim_utf8_strcasecmp(tmp,
				normalize(gaim_account_get_username(account)))) {

			gc->evil = evil;
			gc->login_time_official = signon;
			/*update_idle_times();*/
		}
		g_free(tmp);
	}

	if (!b) {
		gaim_debug(GAIM_DEBUG_ERROR, "server", "No such buddy: %s\n", name);
		return;
	}

	c = gaim_find_conversation_with_account(b->name, account);

	/* This code will 'align' the name from the TOC */
	/* server with what's in our record.  We want to */
	/* store things how THEY want it... */
	if (strcmp(name, b->name)) {
		gaim_blist_rename_buddy(b, name);
		gaim_blist_save();
	}

	if (!b->idle && idle) {
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-idle", b);
		system_log(log_idle, gc, b, OPT_LOG_BUDDY_IDLE);
	} else if (b->idle && !idle) {
		gaim_signal_emit(gaim_blist_get_handle(), "buddy-unidle", b);
		system_log(log_unidle, gc, b, OPT_LOG_BUDDY_IDLE);
	}

	gaim_blist_update_buddy_idle(b, idle);
	gaim_blist_update_buddy_evil(b, evil);

	if ((b->uc & UC_UNAVAILABLE) && !(type & UC_UNAVAILABLE))
		system_log(log_back, gc, b, OPT_LOG_BUDDY_AWAY);
	else if (!(b->uc & UC_UNAVAILABLE) && (type & UC_UNAVAILABLE))
		system_log(log_away, gc, b, OPT_LOG_BUDDY_AWAY);

	gaim_blist_update_buddy_status(b, type);

	if (loggedin) {
		if (!GAIM_BUDDY_IS_ONLINE(b)) {
			if (gaim_prefs_get_bool("/core/conversations/im/show_login")) {
				if (c != NULL) {

					char *tmp = g_strdup_printf(_("%s logged in."),
												gaim_get_buddy_alias(b));

					gaim_conversation_write(c, NULL, tmp, -1, GAIM_MESSAGE_SYSTEM,
											time(NULL));
					g_free(tmp);
				}
				else if (awayqueue && find_queue_total_by_name(b->name)) {
					struct queued_message *qm = g_new0(struct queued_message, 1);
					g_snprintf(qm->name, sizeof(qm->name), "%s", b->name);
					qm->message = g_strdup_printf(_("%s logged in."),
												  gaim_get_buddy_alias(b));
					qm->account = gc->account;
					qm->tm = time(NULL);
					qm->flags = GAIM_MESSAGE_SYSTEM;
					qm->len = -1;
					message_queue = g_slist_append(message_queue, qm);
				}
			}
			gaim_sound_play_event(GAIM_SOUND_BUDDY_ARRIVE);
			system_log(log_signon, gc, b, OPT_LOG_BUDDY_SIGNON);
		}
	} else {
		if (GAIM_BUDDY_IS_ONLINE(b)) {

			if (gaim_prefs_get_bool("/core/conversations/im/show_login")) {
				if (c != NULL) {

					char *tmp = g_strdup_printf(_("%s logged out."),
												gaim_get_buddy_alias(b));
					gaim_conversation_write(c, NULL, tmp, -1,
											GAIM_MESSAGE_SYSTEM, time(NULL));
					g_free(tmp);
				} else if (awayqueue && find_queue_total_by_name(b->name)) {
					struct queued_message *qm = g_new0(struct queued_message, 1);
					g_snprintf(qm->name, sizeof(qm->name), "%s", b->name);
					qm->message = g_strdup_printf(_("%s logged out."),
												  gaim_get_buddy_alias(b));
					qm->account = gc->account;
					qm->tm = time(NULL);
					qm->flags = GAIM_MESSAGE_SYSTEM;
					qm->len = -1;
					message_queue = g_slist_append(message_queue, qm);
				}
			}
			serv_got_typing_stopped(gc, name); /* obviously not typing */
			gaim_sound_play_event(GAIM_SOUND_BUDDY_LEAVE);
			system_log(log_signoff, gc, b, OPT_LOG_BUDDY_SIGNON);
		}
	}

	if (c != NULL)
		gaim_conversation_update(c, GAIM_CONV_UPDATE_AWAY);

	gaim_blist_update_buddy_presence(b, loggedin);

	for (buddies = gaim_find_buddies(account, name); buddies; buddies = g_slist_remove(buddies, buddies->data)) {
		b = buddies->data;
		gaim_blist_update_buddy_presence(b, loggedin);
		gaim_blist_update_buddy_idle(b, idle);
		gaim_blist_update_buddy_evil(b, evil);
		gaim_blist_update_buddy_status(b, type);
	}
}


void serv_got_eviled(GaimConnection *gc, const char *name, int lev)
{
	char buf2[1024];
	GaimAccount *account;

	account = gaim_connection_get_account(gc);

	gaim_signal_emit(gaim_accounts_get_handle(), "account-warned",
					 account, name, lev);

	if (gc->evil >= lev) {
		gc->evil = lev;
		return;
	}

	gc->evil = lev;

	g_snprintf(buf2, sizeof(buf2),
			   _("%s has just been warned by %s.\n"
				 "Your new warning level is %d%%"),
			   gaim_account_get_username(gaim_connection_get_account(gc)),
			   ((name == NULL) ? _("an anonymous person") : name), lev);

	gaim_notify_info(NULL, NULL, buf2, NULL);
}

void serv_got_typing(GaimConnection *gc, const char *name, int timeout,
					 GaimTypingState state) {

	GaimBuddy *b;
	GaimConversation *cnv = gaim_find_conversation_with_account(name, gc->account);
	GaimIm *im;

	if (!cnv)
		return;

	im = GAIM_IM(cnv);

	gaim_conversation_set_account(cnv, gc->account);
	gaim_im_set_typing_state(im, state);
	gaim_im_update_typing(im);

	b = gaim_find_buddy(gc->account, name);

	if (b != NULL)
	{
		if (state == GAIM_TYPING)
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typing", cnv);
		}
		else
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typing-stopped", cnv);
		}
	}

	if (timeout > 0)
		gaim_im_start_typing_timeout(im, timeout);
}

void serv_got_typing_stopped(GaimConnection *gc, const char *name) {

	GaimConversation *c = gaim_find_conversation_with_account(name, gc->account);
	GaimIm *im;
	GaimBuddy *b;

	if (!c)
		return;

	im = GAIM_IM(c);

	if (im->typing_state == GAIM_NOT_TYPING)
		return;

	gaim_im_stop_typing_timeout(im);
	gaim_im_set_typing_state(im, GAIM_NOT_TYPING);
	gaim_im_update_typing(im);

	b = gaim_find_buddy(gc->account, name);

	if (b != NULL)
	{
		gaim_signal_emit(gaim_conversations_get_handle(),
						 "buddy-typing-stopped", c);
	}
}

struct chat_invite_data {
	GaimConnection *gc;
	GHashTable *components;
};

static void chat_invite_data_free(struct chat_invite_data *cid)
{
	if (cid->components)
		g_hash_table_destroy(cid->components);
	g_free(cid);
}

static void chat_invite_accept(struct chat_invite_data *cid)
{
	serv_join_chat(cid->gc, cid->components);

	chat_invite_data_free(cid);
}



void serv_got_chat_invite(GaimConnection *gc, const char *name,
						  const char *who, const char *message, GHashTable *data)
{
	GaimAccount *account;
	char buf2[BUF_LONG];
	struct chat_invite_data *cid = g_new0(struct chat_invite_data, 1);

	account = gaim_connection_get_account(gc);

	gaim_signal_emit(gaim_conversations_get_handle(),
					 "chat-invited", account, who, name, message);

	if (message)
		g_snprintf(buf2, sizeof(buf2),
				   _("User '%s' invites %s to buddy chat room: '%s'\n%s"),
				   who, gaim_account_get_username(account), name, message);
	else
		g_snprintf(buf2, sizeof(buf2),
				   _("User '%s' invites %s to buddy chat room: '%s'\n"),
				   who, gaim_account_get_username(account), name);

	cid->gc = gc;
	cid->components = data;

	gaim_request_accept_cancel(gc, NULL, _("Accept chat invitation?"),
							   buf2, 0, cid,
							   G_CALLBACK(chat_invite_accept),
							   G_CALLBACK(chat_invite_data_free));
}

GaimConversation *serv_got_joined_chat(GaimConnection *gc,
											   int id, const char *name)
{
	GaimConversation *conv;
	GaimChat *chat;
	GaimAccount *account;

	account = gaim_connection_get_account(gc);

	conv = gaim_conversation_new(GAIM_CONV_CHAT, account, name);
	chat = GAIM_CHAT(conv);

	gc->buddy_chats = g_slist_append(gc->buddy_chats, conv);

	gaim_chat_set_id(chat, id);

	/* TODO Move this to UI logging code! */
	if (gaim_prefs_get_bool("/gaim/gtk/logging/log_chats") ||
		find_log_info(gaim_conversation_get_name(conv))) {

		FILE *fd;
		char *filename;

		filename = (char *)malloc(100);
		g_snprintf(filename, 100, "%s.chat", gaim_conversation_get_name(conv));

		fd = open_log_file(filename, TRUE);

		if (fd) {
			if (!gaim_prefs_get_bool("/gaim/gtk/logging/strip_html"))
				fprintf(fd,
					_("<HR><BR><H3 Align=Center> ---- New Conversation @ %s ----</H3><BR>\n"),
					full_date());
			else
				fprintf(fd, _("---- New Conversation @ %s ----\n"), full_date());

			fclose(fd);
		}
		free(filename);
	}

	gaim_window_show(gaim_conversation_get_window(conv));
	gaim_window_switch_conversation(gaim_conversation_get_window(conv),
									gaim_conversation_get_index(conv));

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-joined", conv);

	return conv;
}

void serv_got_chat_left(GaimConnection *g, int id)
{
	GSList *bcs;
	GaimConversation *conv = NULL;
	GaimChat *chat = NULL;
	GaimAccount *account;

	account = gaim_connection_get_account(g);

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (GaimConversation *)bcs->data;

		chat = GAIM_CHAT(conv);

		if (gaim_chat_get_id(chat) == id)
			break;

		conv = NULL;
	}

	if (!conv)
		return;

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-left", conv);

	gaim_debug(GAIM_DEBUG_INFO, "server", "Leaving room: %s\n",
			   gaim_conversation_get_name(conv));

	g->buddy_chats = g_slist_remove(g->buddy_chats, conv);

	gaim_conversation_destroy(conv);
}

void serv_got_chat_in(GaimConnection *g, int id, const char *who,
					  int whisper, const char *message, time_t mtime)
{
	GaimMessageFlags w;
	GSList *bcs;
	GaimConversation *conv = NULL;
	GaimChat *chat = NULL;
	char *buf;
	char *buffy, *angel;
	int plugin_return;

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (GaimConversation *)bcs->data;

		chat = GAIM_CHAT(conv);

		if (gaim_chat_get_id(chat) == id)
			break;

		conv = NULL;
	}

	if (!conv)
		return;

	/*
	 * Plugin stuff. We pass a char ** but we don't want to pass what's
	 * been given us by the prpls. so we create temp holders and pass those
	 * instead. It's basically just to avoid segfaults. Of course, if the
	 * data is binary, plugins don't see it. Bitch all you want; i really
	 * don't want you to be dealing with it.
	 */

	buffy = g_malloc(MAX(strlen(message) + 1, BUF_LONG));
	strcpy(buffy, message);
	angel = g_strdup(who);

	plugin_return = GPOINTER_TO_INT(
		gaim_signal_emit_return_1(gaim_conversations_get_handle(),
								  "received-chat-msg", g->account,
								  &angel, &buffy,
								  gaim_chat_get_id(GAIM_CHAT(conv))));

	if (!buffy || !angel || plugin_return) {
		if (buffy)
			g_free(buffy);
		if (angel)
			g_free(angel);
		return;
	}
	who = angel;
	message = buffy;



	if (gaim_prefs_get_bool("/gaim/gtk/conversations/show_urls_as_links"))
		buf = linkify_text(message);
	else
		buf = g_strdup(message);

	if (whisper)
		w = GAIM_MESSAGE_WHISPER;
	else
		w = 0;

	gaim_chat_write(chat, who, buf, w, mtime);

	g_free(angel);
	g_free(buf);
	g_free(buffy);
}

static void des_popup(GtkWidget *w, GtkWidget *window)
{
	if (w == window) {
		char *u = g_object_get_data(G_OBJECT(window), "url");
		g_free(u);
	}
	gtk_widget_destroy(window);
}

static void
url_clicked_cb(GtkWidget *w, const char *uri)
{
	gaim_notify_uri(NULL, uri);
}

void serv_got_popup(const char *msg, const char *u, int wid, int hei)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *sw;
	GtkWidget *text;
	GtkWidget *hbox;
	GtkWidget *button;
	char *url = g_strdup(u);

	GAIM_DIALOG(window);
	gtk_window_set_role(GTK_WINDOW(window), "popup");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_title(GTK_WINDOW(window), _("Gaim - Popup"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	g_signal_connect(G_OBJECT(window), "destroy",
					 G_CALLBACK(des_popup), window);
	g_object_set_data(G_OBJECT(window), "url", url);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(sw, wid, hei);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 5);

	text = gtk_imhtml_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), text);
	gaim_setup_imhtml(text);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	button = gaim_pixbuf_button_from_stock(_("Close"), GTK_STOCK_CLOSE, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(des_popup), window);

	button = gaim_pixbuf_button_from_stock(_("More Info"), GTK_STOCK_FIND, GAIM_BUTTON_HORIZONTAL);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(button), "clicked",
					 G_CALLBACK(url_clicked_cb), url);

	gtk_widget_show_all(window);

	gtk_imhtml_append_text(GTK_IMHTML(text), msg, -1, GTK_IMHTML_NO_NEWLINE);
}

