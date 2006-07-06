/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "notify.h"
#include "prefs.h"
#include "privacy.h"
#include "prpl.h"
#include "request.h"
#include "signals.h"
#include "server.h"
#include "status.h"
#include "util.h"

#define SECS_BEFORE_RESENDING_AUTORESPONSE 600
#define SEX_BEFORE_RESENDING_AUTORESPONSE "Only after you're married"

unsigned int
serv_send_typing(GaimConnection *gc, const char *name, GaimTypingState state)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->send_typing)
		return prpl_info->send_typing(gc, name, state);

	return 0;
}

static GSList *last_auto_responses = NULL;
struct last_auto_response {
	GaimConnection *gc;
	char name[80];
	time_t sent;
};

static gboolean
expire_last_auto_responses(gpointer data)
{
	GSList *tmp, *cur;
	struct last_auto_response *lar;

	tmp = last_auto_responses;

	while (tmp) {
		cur = tmp;
		tmp = tmp->next;
		lar = (struct last_auto_response *)cur->data;

		if ((time(NULL) - lar->sent) > SECS_BEFORE_RESENDING_AUTORESPONSE) {
			last_auto_responses = g_slist_remove(last_auto_responses, lar);
			g_free(lar);
		}
	}

	return FALSE; /* do not run again */
}

static struct last_auto_response *
get_last_auto_response(GaimConnection *gc, const char *name)
{
	GSList *tmp;
	struct last_auto_response *lar;

	/* because we're modifying or creating a lar, schedule the
	 * function to expire them as the pref dictates */
	gaim_timeout_add((SECS_BEFORE_RESENDING_AUTORESPONSE + 1) * 1000, expire_last_auto_responses, NULL);

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
	last_auto_responses = g_slist_prepend(last_auto_responses, lar);

	return lar;
}

int serv_send_im(GaimConnection *gc, const char *name, const char *message,
				 GaimMessageFlags flags)
{
	GaimConversation *conv;
	GaimAccount *account;
	GaimPresence *presence;
	GaimPluginProtocolInfo *prpl_info;
	int val = -EINVAL;
	const gchar *auto_reply_pref;

	g_return_val_if_fail(gc != NULL, val);
	g_return_val_if_fail(gc->prpl != NULL, val);

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	account  = gaim_connection_get_account(gc);
	presence = gaim_account_get_presence(account);

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, gc->account);

	if (prpl_info && prpl_info->send_im)
		val = prpl_info->send_im(gc, name, message, flags);

	/*
	 * XXX - If "only auto-reply when away & idle" is set, then shouldn't
	 * this only reset lar->sent if we're away AND idle?
	 */
	auto_reply_pref = gaim_prefs_get_string("/core/away/auto_reply");
	if ((gc->flags & GAIM_CONNECTION_AUTO_RESP) &&
			!gaim_presence_is_available(presence) &&
			strcmp(auto_reply_pref, "never")) {

		struct last_auto_response *lar;
		lar = get_last_auto_response(gc, name);
		lar->sent = time(NULL);
	}

	if (conv && gaim_conv_im_get_send_typed_timeout(GAIM_CONV_IM(conv)))
		gaim_conv_im_stop_send_typed_timeout(GAIM_CONV_IM(conv));

	return val;
}

void serv_get_info(GaimConnection *gc, const char *name)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (gc && prpl_info && prpl_info->get_info)
		prpl_info->get_info(gc, name);
}

void serv_set_info(GaimConnection *gc, const char *info)
{
	GaimPluginProtocolInfo *prpl_info = NULL;
	GaimAccount *account;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), gc) &&
		prpl_info->set_info) {

		account = gaim_connection_get_account(gc);

		if (gaim_signal_emit_return_1(gaim_accounts_get_handle(),
									  "account-setting-info", account, info))
			return;

		prpl_info->set_info(gc, info);

		gaim_signal_emit(gaim_accounts_get_handle(),
						 "account-set-info", account, info);
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

void
serv_got_alias(GaimConnection *gc, const char *who, const char *alias)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GSList *buds, *buddies = gaim_find_buddies(account, who);
	GaimBuddy *b;
	GaimConversation *conv;

	for (buds = buddies; buds; buds = buds->next)
	{
		b = buds->data;
		gaim_blist_server_alias_buddy(b, alias);

		conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, b->name, account);

		if (conv != NULL && b->server_alias != NULL &&
			strcmp(b->server_alias, alias))
		{
			char *tmp = g_strdup_printf(_("%s is now known as %s.\n"),
										who, alias);

			gaim_conversation_write(conv, NULL, tmp, GAIM_MESSAGE_SYSTEM,
									time(NULL));

			g_free(tmp);
		}
	}
	g_slist_free(buddies);
}

/*
 * Move a buddy from one group to another on server.
 *
 * Note: For now we'll not deal with changing gc's at the same time, but
 * it should be possible.  Probably needs to be done, someday.  Although,
 * the UI for that would be difficult, because groups are Gaim-wide.
 */
void serv_move_buddy(GaimBuddy *b, GaimGroup *og, GaimGroup *ng)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(b != NULL);
	g_return_if_fail(og != NULL);
	g_return_if_fail(ng != NULL);

	if (b->account->gc != NULL && b->account->gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(b->account->gc->prpl);

	if (b->account->gc && og && ng) {
		if (prpl_info && prpl_info->group_buddy) {
			prpl_info->group_buddy(b->account->gc, b->name, og->name, ng->name);
		}
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

void serv_join_chat(GaimConnection *g, GHashTable *data)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->join_chat)
		prpl_info->join_chat(g, data);
}


void serv_reject_chat(GaimConnection *g, GHashTable *data)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && g_list_find(gaim_connections_get_all(), g) && prpl_info->reject_chat)
		prpl_info->reject_chat(g, data);
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

/* Ya know, nothing uses this except gaim_conversation_destroy(),
 * I think I'll just merge it into that later...
 * Then again, something might want to use this, from outside prpl-land
 * to leave a chat without destroying the conversation.
 */

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

int serv_chat_send(GaimConnection *gc, int id, const char *message, GaimMessageFlags flags)
{
	int val = -EINVAL;
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->chat_send)
		val = prpl_info->chat_send(gc, id, message, flags);

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

/*
 * woo. i'm actually going to comment this function. isn't that fun. make
 * sure to follow along, kids
 */
void serv_got_im(GaimConnection *gc, const char *who, const char *msg,
				 GaimMessageFlags flags, time_t mtime)
{
	GaimAccount *account;
	GaimConversation *cnv;
	char *message, *name;
	char *angel, *buffy;
	int plugin_return;

	g_return_if_fail(msg != NULL);

	account  = gaim_connection_get_account(gc);

	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->set_permit_deny == NULL) {
		/* protocol does not support privacy, handle it ourselves */
		if (!gaim_privacy_check(account, who))
			return;
	}

	/*
	 * We should update the conversation window buttons and menu,
	 * if it exists.
	 */
	cnv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, who, gc->account);

	/*
	 * Plugin stuff. we pass a char ** but we don't want to pass what's
	 * been given us by the prpls. So we create temp holders and pass
	 * those instead. It's basically just to avoid segfaults.
	 */
	buffy = g_malloc(MAX(strlen(msg) + 1, BUF_LONG));
	strcpy(buffy, msg);
	angel = g_strdup(who);

	plugin_return = GPOINTER_TO_INT(
		gaim_signal_emit_return_1(gaim_conversations_get_handle(),
								  "receiving-im-msg", gc->account,
								  &angel, &buffy, cnv, &flags));

	if (!buffy || !angel || plugin_return) {
		if (buffy)
			g_free(buffy);
		if (angel)
			g_free(angel);
		return;
	}

	name = angel;
	message = buffy;

	gaim_signal_emit(gaim_conversations_get_handle(), "received-im-msg", gc->account,
					 name, message, cnv, flags);

	/* search for conversation again in case it was created by received-im-msg handler */
	if (cnv == NULL)
		cnv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, gc->account);

	/* Make sure URLs are clickable */
	buffy = gaim_markup_linkify(message);
	g_free(message);
	message = buffy;

	/*
	 * XXX: Should we be setting this here, or relying on prpls to set it?
	 */
	flags |= GAIM_MESSAGE_RECV;

	if (cnv == NULL)
		cnv = gaim_conversation_new(GAIM_CONV_TYPE_IM, account, name);

	gaim_conv_im_write(GAIM_CONV_IM(cnv), NULL, message, flags, mtime);
	g_free(message);

	/*
	 * Don't autorespond if:
	 *
	 *  - it's not supported on this connection
	 *  - we are available
	 *  - or it's disabled
	 *  - or we're not idle and the 'only auto respond if idle' pref
	 *    is set
	 */
	if (gc->flags & GAIM_CONNECTION_AUTO_RESP)
	{
		GaimPresence *presence;
		GaimStatus *status;
		GaimStatusType *status_type;
		GaimStatusPrimitive primitive;
		const gchar *auto_reply_pref;
		const char *away_msg = NULL;

		auto_reply_pref = gaim_prefs_get_string("/core/away/auto_reply");

		presence = gaim_account_get_presence(account);
		status = gaim_presence_get_active_status(presence);
		status_type = gaim_status_get_type(status);
		primitive = gaim_status_type_get_primitive(status_type);
		if ((primitive == GAIM_STATUS_AVAILABLE) ||
			(primitive == GAIM_STATUS_INVISIBLE) ||
			(primitive == GAIM_STATUS_MOBILE) ||
		    !strcmp(auto_reply_pref, "never") ||
		    (!gaim_presence_is_idle(presence) && !strcmp(auto_reply_pref, "awayidle")))
		{
			g_free(name);
			return;
		}

		away_msg = gaim_value_get_string(
			gaim_status_get_attr_value(status, "message"));

		if ((away_msg != NULL) && (*away_msg != '\0')) {
			struct last_auto_response *lar;
			time_t now = time(NULL);

			/*
			 * This used to be based on the conversation window. But um, if
			 * you went away, and someone sent you a message and got your
			 * auto-response, and then you closed the window, and then they
			 * sent you another one, they'd get the auto-response back too
			 * soon. Besides that, we need to keep track of this even if we've
			 * got a queue. So the rest of this block is just the auto-response,
			 * if necessary.
			 */
			lar = get_last_auto_response(gc, name);
			if ((now - lar->sent) >= SECS_BEFORE_RESENDING_AUTORESPONSE)
			{
				lar->sent = now;

				serv_send_im(gc, name, away_msg, GAIM_MESSAGE_AUTO_RESP);

				gaim_conv_im_write(GAIM_CONV_IM(cnv), NULL, away_msg,
				                   GAIM_MESSAGE_SEND | GAIM_MESSAGE_AUTO_RESP,
				                   mtime);
			}
		}
	}

	g_free(name);
}

void serv_got_typing(GaimConnection *gc, const char *name, int timeout,
					 GaimTypingState state) {
	GaimConversation *conv;
	GaimConvIm *im = NULL;

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, gc->account);
	if (conv != NULL) {
		im = GAIM_CONV_IM(conv);

		gaim_conv_im_set_typing_state(im, state);
		gaim_conv_im_update_typing(im);
	} else {
		if (state == GAIM_TYPING)
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typing", gc->account, name);
		}
		else
		{
			gaim_signal_emit(gaim_conversations_get_handle(),
							 "buddy-typed", gc->account, name);
		}
	}

	if (conv != NULL && timeout > 0)
		gaim_conv_im_start_typing_timeout(im, timeout);
}

void serv_got_typing_stopped(GaimConnection *gc, const char *name) {

	GaimConversation *conv;
	GaimConvIm *im;

	conv = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, name, gc->account);
	if (conv != NULL)
	{
		im = GAIM_CONV_IM(conv);

		if (im->typing_state == GAIM_NOT_TYPING)
			return;

		gaim_conv_im_stop_typing_timeout(im);
		gaim_conv_im_set_typing_state(im, GAIM_NOT_TYPING);
		gaim_conv_im_update_typing(im);
	}
	else
	{
		gaim_signal_emit(gaim_conversations_get_handle(),
						 "buddy-typing-stopped", gc->account, name);
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


static void chat_invite_reject(struct chat_invite_data *cid)
{
	serv_reject_chat(cid->gc, cid->components);
	chat_invite_data_free(cid);
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
	int plugin_return;

	account = gaim_connection_get_account(gc);
	if (GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl)->set_permit_deny == NULL) {
		/* protocol does not support privacy, handle it ourselves */
		if (!gaim_privacy_check(account, who))
			return;
	}

	plugin_return = GPOINTER_TO_INT(gaim_signal_emit_return_1(
					gaim_conversations_get_handle(),
					"chat-invited", account, who, name, message, data));

	cid->gc = gc;
	cid->components = data;

	if (plugin_return == 0)
	{
		if (message != NULL)
		{
			g_snprintf(buf2, sizeof(buf2),
				   _("%s has invited %s to the chat room %s:\n%s"),
				   who, gaim_account_get_username(account), name, message);
		}
		else
			g_snprintf(buf2, sizeof(buf2),
				   _("%s has invited %s to the chat room %s\n"),
				   who, gaim_account_get_username(account), name);


		gaim_request_accept_cancel(gc, NULL, _("Accept chat invitation?"), buf2,
							   GAIM_DEFAULT_ACTION_NONE, cid,
							   G_CALLBACK(chat_invite_accept),
							   G_CALLBACK(chat_invite_reject));
	}
	else if (plugin_return > 0)
		chat_invite_accept(cid);
	else
		chat_invite_reject(cid);
}

GaimConversation *serv_got_joined_chat(GaimConnection *gc,
											   int id, const char *name)
{
	GaimConversation *conv;
	GaimConvChat *chat;
	GaimAccount *account;

	account = gaim_connection_get_account(gc);

	conv = gaim_conversation_new(GAIM_CONV_TYPE_CHAT, account, name);
	chat = GAIM_CONV_CHAT(conv);

	if (!g_slist_find(gc->buddy_chats, conv))
		gc->buddy_chats = g_slist_append(gc->buddy_chats, conv);

	gaim_conv_chat_set_id(chat, id);

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-joined", conv);

	return conv;
}

void serv_got_chat_left(GaimConnection *g, int id)
{
	GSList *bcs;
	GaimConversation *conv = NULL;
	GaimConvChat *chat = NULL;

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (GaimConversation *)bcs->data;

		chat = GAIM_CONV_CHAT(conv);

		if (gaim_conv_chat_get_id(chat) == id)
			break;

		conv = NULL;
	}

	if (!conv)
		return;

	gaim_debug(GAIM_DEBUG_INFO, "server", "Leaving room: %s\n",
			   gaim_conversation_get_name(conv));

	g->buddy_chats = g_slist_remove(g->buddy_chats, conv);

	gaim_conv_chat_left(GAIM_CONV_CHAT(conv));

	gaim_signal_emit(gaim_conversations_get_handle(), "chat-left", conv);
}

void serv_got_chat_in(GaimConnection *g, int id, const char *who,
					  GaimMessageFlags flags, const char *message, time_t mtime)
{
	GSList *bcs;
	GaimConversation *conv = NULL;
	GaimConvChat *chat = NULL;
	char *buf;
	char *buffy, *angel;
	int plugin_return;

	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (GaimConversation *)bcs->data;

		chat = GAIM_CONV_CHAT(conv);

		if (gaim_conv_chat_get_id(chat) == id)
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
								  "receiving-chat-msg", g->account,
								  &angel, &buffy, conv, &flags));

	if (!buffy || !angel || plugin_return) {
		g_free(buffy);
		g_free(angel);
		return;
	}
	who = angel;
	message = buffy;

	gaim_signal_emit(gaim_conversations_get_handle(), "received-chat-msg", g->account,
					 who, message, conv, flags);

	/* Make sure URLs are clickable */
	buf = gaim_markup_linkify(message);

	gaim_conv_chat_write(chat, who, buf, flags, mtime);

	g_free(angel);
	g_free(buf);
	g_free(buffy);
}

void serv_send_file(GaimConnection *gc, const char *who, const char *file)
{
	GaimPluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->send_file) {
		if (!prpl_info->can_receive_file || prpl_info->can_receive_file(gc, who)) {
			prpl_info->send_file(gc, who, file);
		}
	}
}
