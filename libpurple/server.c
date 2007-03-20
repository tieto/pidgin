/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
serv_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState state)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->send_typing)
		return prpl_info->send_typing(gc, name, state);

	return 0;
}

static GSList *last_auto_responses = NULL;
struct last_auto_response {
	PurpleConnection *gc;
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
get_last_auto_response(PurpleConnection *gc, const char *name)
{
	GSList *tmp;
	struct last_auto_response *lar;

	/* because we're modifying or creating a lar, schedule the
	 * function to expire them as the pref dictates */
	purple_timeout_add((SECS_BEFORE_RESENDING_AUTORESPONSE + 1) * 1000, expire_last_auto_responses, NULL);

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

int serv_send_im(PurpleConnection *gc, const char *name, const char *message,
				 PurpleMessageFlags flags)
{
	PurpleConversation *conv;
	PurpleAccount *account;
	PurplePresence *presence;
	PurplePluginProtocolInfo *prpl_info;
	int val = -EINVAL;
	const gchar *auto_reply_pref;

	g_return_val_if_fail(gc != NULL, val);
	g_return_val_if_fail(gc->prpl != NULL, val);

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	account  = purple_connection_get_account(gc);
	presence = purple_account_get_presence(account);

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, name, gc->account);

	if (prpl_info && prpl_info->send_im)
		val = prpl_info->send_im(gc, name, message, flags);

	/*
	 * XXX - If "only auto-reply when away & idle" is set, then shouldn't
	 * this only reset lar->sent if we're away AND idle?
	 */
	auto_reply_pref = purple_prefs_get_string("/core/away/auto_reply");
	if ((gc->flags & PURPLE_CONNECTION_AUTO_RESP) &&
			!purple_presence_is_available(presence) &&
			strcmp(auto_reply_pref, "never")) {

		struct last_auto_response *lar;
		lar = get_last_auto_response(gc, name);
		lar->sent = time(NULL);
	}

	if (conv && purple_conv_im_get_send_typed_timeout(PURPLE_CONV_IM(conv)))
		purple_conv_im_stop_send_typed_timeout(PURPLE_CONV_IM(conv));

	return val;
}

void serv_get_info(PurpleConnection *gc, const char *name)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (gc && prpl_info && prpl_info->get_info)
		prpl_info->get_info(gc, name);
}

void serv_set_info(PurpleConnection *gc, const char *info)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleAccount *account;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->set_info) {

		account = purple_connection_get_account(gc);

		if (purple_signal_emit_return_1(purple_accounts_get_handle(),
									  "account-setting-info", account, info))
			return;

		prpl_info->set_info(gc, info);

		purple_signal_emit(purple_accounts_get_handle(),
						 "account-set-info", account, info);
	}
}

/*
 * Set buddy's alias on server roster/list
 */
void serv_alias_buddy(PurpleBuddy *b)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (b != NULL && b->account->gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(b->account->gc->prpl);

	if (b && prpl_info && prpl_info->alias_buddy) {
		prpl_info->alias_buddy(b->account->gc, b->name, b->alias);
	}
}

void
serv_got_alias(PurpleConnection *gc, const char *who, const char *alias)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GSList *buds, *buddies = purple_find_buddies(account, who);
	PurpleBuddy *b;
	PurpleConversation *conv;

	for (buds = buddies; buds; buds = buds->next)
	{
		b = buds->data;
		if ((b->server_alias == NULL && alias == NULL) ||
		    (b->server_alias && alias && !strcmp(b->server_alias, alias)))
		{
			continue;
		}
		purple_blist_server_alias_buddy(b, alias);

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, b->name, account);

		if (conv != NULL && alias != NULL && strcmp(alias, who))
		{
			char *tmp = g_strdup_printf(_("%s is now known as %s.\n"),
										who, alias);

			purple_conversation_write(conv, NULL, tmp, PURPLE_MESSAGE_SYSTEM,
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
 * the UI for that would be difficult, because groups are Purple-wide.
 */
void serv_move_buddy(PurpleBuddy *b, PurpleGroup *og, PurpleGroup *ng)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	g_return_if_fail(b != NULL);
	g_return_if_fail(og != NULL);
	g_return_if_fail(ng != NULL);

	if (b->account->gc != NULL && b->account->gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(b->account->gc->prpl);

	if (b->account->gc && og && ng) {
		if (prpl_info && prpl_info->group_buddy) {
			prpl_info->group_buddy(b->account->gc, b->name, og->name, ng->name);
		}
	}
}

void serv_add_permit(PurpleConnection *g, const char *name)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->add_permit)
		prpl_info->add_permit(g, name);
}

void serv_add_deny(PurpleConnection *g, const char *name)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->add_deny)
		prpl_info->add_deny(g, name);
}

void serv_rem_permit(PurpleConnection *g, const char *name)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->rem_permit)
		prpl_info->rem_permit(g, name);
}

void serv_rem_deny(PurpleConnection *g, const char *name)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->rem_deny)
		prpl_info->rem_deny(g, name);
}

void serv_set_permit_deny(PurpleConnection *g)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	/*
	 * this is called when either you import a buddy list, and make lots
	 * of changes that way, or when the user toggles the permit/deny mode
	 * in the prefs. In either case you should probably be resetting and
	 * resending the permit/deny info when you get this.
	 */
	if (prpl_info && prpl_info->set_permit_deny)
		prpl_info->set_permit_deny(g);
}

void serv_join_chat(PurpleConnection *g, GHashTable *data)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->join_chat)
		prpl_info->join_chat(g, data);
}


void serv_reject_chat(PurpleConnection *g, GHashTable *data)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->reject_chat)
		prpl_info->reject_chat(g, data);
}

void serv_chat_invite(PurpleConnection *g, int id, const char *message, const char *name)
{
	PurplePluginProtocolInfo *prpl_info = NULL;
	PurpleConversation *conv;
	char *buffy = message && *message ? g_strdup(message) : NULL;

	conv = purple_find_chat(g, id);

	if (conv == NULL)
		return;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	purple_signal_emit(purple_conversations_get_handle(), "chat-inviting-user",
					 conv, name, &buffy);

	if (prpl_info && prpl_info->chat_invite)
		prpl_info->chat_invite(g, id, buffy, name);

	purple_signal_emit(purple_conversations_get_handle(), "chat-invited-user",
					 conv, name, buffy);

	g_free(buffy);
}

/* Ya know, nothing uses this except purple_conversation_destroy(),
 * I think I'll just merge it into that later...
 * Then again, something might want to use this, from outside prpl-land
 * to leave a chat without destroying the conversation.
 */

void serv_chat_leave(PurpleConnection *g, int id)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->chat_leave)
		prpl_info->chat_leave(g, id);
}

void serv_chat_whisper(PurpleConnection *g, int id, const char *who, const char *message)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (g != NULL && g->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(g->prpl);

	if (prpl_info && prpl_info->chat_whisper)
		prpl_info->chat_whisper(g, id, who, message);
}

int serv_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	int val = -EINVAL;
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->chat_send)
		val = prpl_info->chat_send(gc, id, message, flags);

	return val;
}

/*
 * woo. i'm actually going to comment this function. isn't that fun. make
 * sure to follow along, kids
 */
void serv_got_im(PurpleConnection *gc, const char *who, const char *msg,
				 PurpleMessageFlags flags, time_t mtime)
{
	PurpleAccount *account;
	PurpleConversation *cnv;
	char *message, *name;
	char *angel, *buffy;
	int plugin_return;

	g_return_if_fail(msg != NULL);

	account  = purple_connection_get_account(gc);

	if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->set_permit_deny == NULL) {
		/* protocol does not support privacy, handle it ourselves */
		if (!purple_privacy_check(account, who))
			return;
	}

	/*
	 * We should update the conversation window buttons and menu,
	 * if it exists.
	 */
	cnv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, gc->account);

	/*
	 * Plugin stuff. we pass a char ** but we don't want to pass what's
	 * been given us by the prpls. So we create temp holders and pass
	 * those instead. It's basically just to avoid segfaults.
	 */
	/* TODO: MAX(message, BUF_LONG) is pretty ugly. */
	buffy = g_malloc(MAX(strlen(msg) + 1, BUF_LONG));
	strcpy(buffy, msg);
	angel = g_strdup(who);

	plugin_return = GPOINTER_TO_INT(
		purple_signal_emit_return_1(purple_conversations_get_handle(),
								  "receiving-im-msg", gc->account,
								  &angel, &buffy, cnv, &flags));

	if (!buffy || !angel || plugin_return) {
		g_free(buffy);
		g_free(angel);
		return;
	}

	name = angel;
	message = buffy;

	purple_signal_emit(purple_conversations_get_handle(), "received-im-msg", gc->account,
					 name, message, cnv, flags);

	/* search for conversation again in case it was created by received-im-msg handler */
	if (cnv == NULL)
		cnv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, name, gc->account);

	/*
	 * XXX: Should we be setting this here, or relying on prpls to set it?
	 */
	flags |= PURPLE_MESSAGE_RECV;

	if (cnv == NULL)
		cnv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, name);

	purple_conv_im_write(PURPLE_CONV_IM(cnv), NULL, message, flags, mtime);
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
	if (gc->flags & PURPLE_CONNECTION_AUTO_RESP)
	{
		PurplePresence *presence;
		PurpleStatus *status;
		PurpleStatusType *status_type;
		PurpleStatusPrimitive primitive;
		const gchar *auto_reply_pref;
		const char *away_msg = NULL;

		auto_reply_pref = purple_prefs_get_string("/core/away/auto_reply");

		presence = purple_account_get_presence(account);
		status = purple_presence_get_active_status(presence);
		status_type = purple_status_get_type(status);
		primitive = purple_status_type_get_primitive(status_type);
		if ((primitive == PURPLE_STATUS_AVAILABLE) ||
			(primitive == PURPLE_STATUS_INVISIBLE) ||
			(primitive == PURPLE_STATUS_MOBILE) ||
		    !strcmp(auto_reply_pref, "never") ||
		    (!purple_presence_is_idle(presence) && !strcmp(auto_reply_pref, "awayidle")))
		{
			g_free(name);
			return;
		}

		away_msg = purple_value_get_string(
			purple_status_get_attr_value(status, "message"));

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
				/*
				 * We don't want to send an autoresponse in response to the other user's
				 * autoresponse.  We do, however, not want to then send one in response to the
				 * _next_ message, so we still set lar->sent to now.
				 */
				lar->sent = now;

				if (!(flags & PURPLE_MESSAGE_AUTO_RESP))
				{
					serv_send_im(gc, name, away_msg, PURPLE_MESSAGE_AUTO_RESP);

					purple_conv_im_write(PURPLE_CONV_IM(cnv), NULL, away_msg,
									   PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_AUTO_RESP,
									   mtime);
				}
			}
		}
	}

	g_free(name);
}

void serv_got_typing(PurpleConnection *gc, const char *name, int timeout,
					 PurpleTypingState state) {
	PurpleConversation *conv;
	PurpleConvIm *im = NULL;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, name, gc->account);
	if (conv != NULL) {
		im = PURPLE_CONV_IM(conv);

		purple_conv_im_set_typing_state(im, state);
		purple_conv_im_update_typing(im);
	} else {
		if (state == PURPLE_TYPING)
		{
			purple_signal_emit(purple_conversations_get_handle(),
							 "buddy-typing", gc->account, name);
		}
		else
		{
			purple_signal_emit(purple_conversations_get_handle(),
							 "buddy-typed", gc->account, name);
		}
	}

	if (conv != NULL && timeout > 0)
		purple_conv_im_start_typing_timeout(im, timeout);
}

void serv_got_typing_stopped(PurpleConnection *gc, const char *name) {

	PurpleConversation *conv;
	PurpleConvIm *im;

	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, name, gc->account);
	if (conv != NULL)
	{
		im = PURPLE_CONV_IM(conv);

		if (im->typing_state == PURPLE_NOT_TYPING)
			return;

		purple_conv_im_stop_typing_timeout(im);
		purple_conv_im_set_typing_state(im, PURPLE_NOT_TYPING);
		purple_conv_im_update_typing(im);
	}
	else
	{
		purple_signal_emit(purple_conversations_get_handle(),
						 "buddy-typing-stopped", gc->account, name);
	}
}

struct chat_invite_data {
	PurpleConnection *gc;
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



void serv_got_chat_invite(PurpleConnection *gc, const char *name,
						  const char *who, const char *message, GHashTable *data)
{
	PurpleAccount *account;
	char buf2[BUF_LONG];
	struct chat_invite_data *cid = g_new0(struct chat_invite_data, 1);
	int plugin_return;

	account = purple_connection_get_account(gc);
	if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->set_permit_deny == NULL) {
		/* protocol does not support privacy, handle it ourselves */
		if (!purple_privacy_check(account, who))
			return;
	}

	plugin_return = GPOINTER_TO_INT(purple_signal_emit_return_1(
					purple_conversations_get_handle(),
					"chat-invited", account, who, name, message, data));

	cid->gc = gc;
	cid->components = data;

	if (plugin_return == 0)
	{
		if (message != NULL)
		{
			g_snprintf(buf2, sizeof(buf2),
				   _("%s has invited %s to the chat room %s:\n%s"),
				   who, purple_account_get_username(account), name, message);
		}
		else
			g_snprintf(buf2, sizeof(buf2),
				   _("%s has invited %s to the chat room %s\n"),
				   who, purple_account_get_username(account), name);


		purple_request_accept_cancel(gc, NULL, _("Accept chat invitation?"), buf2,
							   PURPLE_DEFAULT_ACTION_NONE, cid,
							   G_CALLBACK(chat_invite_accept),
							   G_CALLBACK(chat_invite_reject));
	}
	else if (plugin_return > 0)
		chat_invite_accept(cid);
	else
		chat_invite_reject(cid);
}

PurpleConversation *serv_got_joined_chat(PurpleConnection *gc,
											   int id, const char *name)
{
	PurpleConversation *conv;
	PurpleConvChat *chat;
	PurpleAccount *account;

	account = purple_connection_get_account(gc);

	conv = purple_conversation_new(PURPLE_CONV_TYPE_CHAT, account, name);
	chat = PURPLE_CONV_CHAT(conv);

	if (!g_slist_find(gc->buddy_chats, conv))
		gc->buddy_chats = g_slist_append(gc->buddy_chats, conv);

	purple_conv_chat_set_id(chat, id);

	purple_signal_emit(purple_conversations_get_handle(), "chat-joined", conv);

	return conv;
}

void serv_got_chat_left(PurpleConnection *g, int id)
{
	GSList *bcs;
	PurpleConversation *conv = NULL;
	PurpleConvChat *chat = NULL;

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (PurpleConversation *)bcs->data;

		chat = PURPLE_CONV_CHAT(conv);

		if (purple_conv_chat_get_id(chat) == id)
			break;

		conv = NULL;
	}

	if (!conv)
		return;

	purple_debug(PURPLE_DEBUG_INFO, "server", "Leaving room: %s\n",
			   purple_conversation_get_name(conv));

	g->buddy_chats = g_slist_remove(g->buddy_chats, conv);

	purple_conv_chat_left(PURPLE_CONV_CHAT(conv));

	purple_signal_emit(purple_conversations_get_handle(), "chat-left", conv);
}

void serv_got_chat_in(PurpleConnection *g, int id, const char *who,
					  PurpleMessageFlags flags, const char *message, time_t mtime)
{
	GSList *bcs;
	PurpleConversation *conv = NULL;
	PurpleConvChat *chat = NULL;
	char *buffy, *angel;
	int plugin_return;

	g_return_if_fail(who != NULL);
	g_return_if_fail(message != NULL);

	for (bcs = g->buddy_chats; bcs != NULL; bcs = bcs->next) {
		conv = (PurpleConversation *)bcs->data;

		chat = PURPLE_CONV_CHAT(conv);

		if (purple_conv_chat_get_id(chat) == id)
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
	/* TODO: MAX(message, BUF_LONG) is pretty ugly. */
	buffy = g_malloc(MAX(strlen(message) + 1, BUF_LONG));
	strcpy(buffy, message);
	angel = g_strdup(who);

	plugin_return = GPOINTER_TO_INT(
		purple_signal_emit_return_1(purple_conversations_get_handle(),
								  "receiving-chat-msg", g->account,
								  &angel, &buffy, conv, &flags));

	if (!buffy || !angel || plugin_return) {
		g_free(buffy);
		g_free(angel);
		return;
	}
	who = angel;
	message = buffy;

	purple_signal_emit(purple_conversations_get_handle(), "received-chat-msg", g->account,
					 who, message, conv, flags);

	purple_conv_chat_write(chat, who, message, flags, mtime);

	g_free(angel);
	g_free(buffy);
}

void serv_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	PurplePluginProtocolInfo *prpl_info = NULL;

	if (gc != NULL && gc->prpl != NULL)
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if (prpl_info && prpl_info->send_file) {
		if (!prpl_info->can_receive_file || prpl_info->can_receive_file(gc, who)) {
			prpl_info->send_file(gc, who, file);
		}
	}
}
