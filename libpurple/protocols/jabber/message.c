/*
 * purple - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"

#include "debug.h"
#include "notify.h"
#include "server.h"
#include "util.h"

#include "buddy.h"
#include "chat.h"
#include "google.h"
#include "message.h"
#include "xmlnode.h"
#include "pep.h"

void jabber_message_free(JabberMessage *jm)
{
	g_free(jm->from);
	g_free(jm->to);
	g_free(jm->id);
	g_free(jm->subject);
	g_free(jm->body);
	g_free(jm->xhtml);
	g_free(jm->password);
	g_free(jm->error);
	g_free(jm->thread_id);
	g_list_free(jm->etc);
	g_list_free(jm->eventitems);

	g_free(jm);
}

static void handle_chat(JabberMessage *jm)
{
	JabberID *jid = jabber_id_new(jm->from);
	char *from;

	JabberBuddy *jb;
	JabberBuddyResource *jbr;

	if(!jid)
		return;

	jb = jabber_buddy_find(jm->js, jm->from, TRUE);
	jbr = jabber_buddy_find_resource(jb, jid->resource);

	if(jabber_find_unnormalized_conv(jm->from, jm->js->gc->account)) {
		from = g_strdup(jm->from);
	} else  if(jid->node) {
		if(jid->resource) {
			PurpleConversation *conv;

			from = g_strdup_printf("%s@%s", jid->node, jid->domain);
			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, from, jm->js->gc->account);
			if(conv) {
				purple_conversation_set_name(conv, jm->from);
				}
			g_free(from);
		}
		from = g_strdup(jm->from);
	} else {
		from = g_strdup(jid->domain);
	}

	if(!jm->xhtml && !jm->body) {
		if(JM_STATE_COMPOSING == jm->chat_state) {
			serv_got_typing(jm->js->gc, from, 0, PURPLE_TYPING);
		} else if(JM_STATE_PAUSED == jm->chat_state) {
			serv_got_typing(jm->js->gc, from, 0, PURPLE_TYPED);
		} else if(JM_STATE_GONE == jm->chat_state) {
			PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
					from, jm->js->gc->account);
			if (conv && jid->node && jid->domain) {
				char buf[256];
				PurpleBuddy *buddy;

				g_snprintf(buf, sizeof(buf), "%s@%s", jid->node, jid->domain);

				if ((buddy = purple_find_buddy(jm->js->gc->account, buf))) {
					const char *who;
					char *escaped;

					who = purple_buddy_get_alias(buddy);
					escaped = g_markup_escape_text(who, -1);

					g_snprintf(buf, sizeof(buf),
					           _("%s has left the conversation."), escaped);
					g_free(escaped);

					/* At some point when we restructure PurpleConversation,
					 * this should be able to be implemented by removing the
					 * user from the conversation like we do with chats now. */
					purple_conversation_write(conv, "", buf,
					                        PURPLE_MESSAGE_SYSTEM, time(NULL));
				}
			}
			serv_got_typing_stopped(jm->js->gc, from);
			
		} else {
			serv_got_typing_stopped(jm->js->gc, from);
		}
	} else {
		if(jbr) {
			if(JM_TS_JEP_0085 == (jm->typing_style & JM_TS_JEP_0085)) {
				jbr->chat_states = JABBER_CHAT_STATES_SUPPORTED;
			} else {
				jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED;
			}

			if(JM_TS_JEP_0022 == (jm->typing_style & JM_TS_JEP_0022)) {
				jbr->capabilities |= JABBER_CAP_COMPOSING;
			}

			if(jbr->thread_id)
				g_free(jbr->thread_id);
			jbr->thread_id = g_strdup(jbr->thread_id);
		}
		
		if (jm->js->googletalk && jm->xhtml == NULL) {
			char *tmp = jm->body;
			jm->body = jabber_google_format_to_html(jm->body);
			g_free(tmp);
		}
		serv_got_im(jm->js->gc, from, jm->xhtml ? jm->xhtml : jm->body, 0,
				jm->sent);
	}


	g_free(from);
	jabber_id_free(jid);
}

static void handle_headline(JabberMessage *jm)
{
	char *title;
	GString *body;
	GList *etc;

	if(!jm->xhtml && !jm->body)
		return; /* ignore headlines without any content */

	body = g_string_new("");
	title = g_strdup_printf(_("Message from %s"), jm->from);

	if(jm->xhtml)
		g_string_append(body, jm->xhtml);
	else if(jm->body)
		g_string_append(body, jm->body);

	for(etc = jm->etc; etc; etc = etc->next) {
		xmlnode *x = etc->data;
		const char *xmlns = xmlnode_get_namespace(x);
		if(xmlns && !strcmp(xmlns, "jabber:x:oob")) {
			xmlnode *url, *desc;
			char *urltxt, *desctxt;

			url = xmlnode_get_child(x, "url");
			desc = xmlnode_get_child(x, "desc");

			if(!url || !desc)
				continue;

			urltxt = xmlnode_get_data(url);
			desctxt = xmlnode_get_data(desc);

			/* I'm all about ugly hacks */
			if(body->len && jm->body && !strcmp(body->str, jm->body))
				g_string_printf(body, "<a href='%s'>%s</a>",
						urltxt, desctxt);
			else
				g_string_append_printf(body, "<br/><a href='%s'>%s</a>",
						urltxt, desctxt);

			g_free(urltxt);
			g_free(desctxt);
		}
	}

	purple_notify_formatted(jm->js->gc, title, jm->subject ? jm->subject : title,
			NULL, body->str, NULL, NULL);

	g_free(title);
	g_string_free(body, TRUE);
}

static void handle_groupchat(JabberMessage *jm)
{
	JabberID *jid = jabber_id_new(jm->from);
	JabberChat *chat;

	if(!jid)
		return;

	chat = jabber_chat_find(jm->js, jid->node, jid->domain);

	if(!chat)
		return;

	if(jm->subject) {
		purple_conv_chat_set_topic(PURPLE_CONV_CHAT(chat->conv), jid->resource,
				jm->subject);
		if(!jm->xhtml && !jm->body) {
			char *msg, *tmp, *tmp2;
			tmp = g_markup_escape_text(jm->subject, -1);
			tmp2 = purple_markup_linkify(tmp);
			if(jid->resource)
				msg = g_strdup_printf(_("%s has set the topic to: %s"), jid->resource, tmp2);
			else
				msg = g_strdup_printf(_("The topic is: %s"), tmp2);
			purple_conv_chat_write(PURPLE_CONV_CHAT(chat->conv), "", msg, PURPLE_MESSAGE_SYSTEM, jm->sent);
			g_free(tmp);
			g_free(tmp2);
			g_free(msg);
		}
	}

	if(jm->xhtml || jm->body) {
		if(jid->resource)
			serv_got_chat_in(jm->js->gc, chat->id, jid->resource,
							jm->delayed ? PURPLE_MESSAGE_DELAYED : 0,
							jm->xhtml ? jm->xhtml : jm->body, jm->sent);
		else if(chat->muc)
			purple_conv_chat_write(PURPLE_CONV_CHAT(chat->conv), "",
							jm->xhtml ? jm->xhtml : jm->body,
							PURPLE_MESSAGE_SYSTEM, jm->sent);
	}

	jabber_id_free(jid);
}

static void handle_groupchat_invite(JabberMessage *jm)
{
	GHashTable *components;
	JabberID *jid = jabber_id_new(jm->to);

	if(!jid)
		return;

	components = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	g_hash_table_replace(components, "room", g_strdup(jid->node));
	g_hash_table_replace(components, "server", g_strdup(jid->domain));
	g_hash_table_replace(components, "handle", g_strdup(jm->js->user->node));
	g_hash_table_replace(components, "password", g_strdup(jm->password));

	jabber_id_free(jid);
	serv_got_chat_invite(jm->js->gc, jm->to, jm->from, jm->body, components);
}

static void handle_error(JabberMessage *jm)
{
	char *buf;

	if(!jm->body)
		return;

	buf = g_strdup_printf(_("Message delivery to %s failed: %s"),
			jm->from, jm->error ? jm->error : "");

	purple_notify_formatted(jm->js->gc, _("XMPP Message Error"), _("XMPP Message Error"), buf,
			jm->xhtml ? jm->xhtml : jm->body, NULL, NULL);

	g_free(buf);
}

static void handle_buzz(JabberMessage *jm) {
	PurpleBuddy *buddy;
	PurpleAccount *account;
	PurpleConversation *c;
	char *username, *str;
	
	/* Delayed buzz MUST NOT be accepted */
	if(jm->delayed)
		return;
	
	/* Reject buzz when it's not enabled */
	if(!jm->js->allowBuzz)
		return;
	
	account = purple_connection_get_account(jm->js->gc);
	
	if ((buddy = purple_find_buddy(account, jm->from)) != NULL)
		username = g_markup_escape_text(purple_buddy_get_alias(buddy), -1);
	else
		return; /* Do not accept buzzes from unknown people */

	c = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, jm->from);

	str = g_strdup_printf(_("%s has buzzed you!"), username);
	
	purple_conversation_write(c, NULL, str, PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NOTIFY, time(NULL));
	g_free(username);
	g_free(str);
}

void jabber_message_parse(JabberStream *js, xmlnode *packet)
{
	JabberMessage *jm;
	const char *type;
	xmlnode *child;

	jm = g_new0(JabberMessage, 1);
	jm->js = js;
	jm->sent = time(NULL);
	jm->delayed = FALSE;

	type = xmlnode_get_attrib(packet, "type");

	if(type) {
		if(!strcmp(type, "normal"))
			jm->type = JABBER_MESSAGE_NORMAL;
		else if(!strcmp(type, "chat"))
			jm->type = JABBER_MESSAGE_CHAT;
		else if(!strcmp(type, "groupchat"))
			jm->type = JABBER_MESSAGE_GROUPCHAT;
		else if(!strcmp(type, "headline"))
			jm->type = JABBER_MESSAGE_HEADLINE;
		else if(!strcmp(type, "error"))
			jm->type = JABBER_MESSAGE_ERROR;
		else
			jm->type = JABBER_MESSAGE_OTHER;
	} else {
		jm->type = JABBER_MESSAGE_NORMAL;
	}

	jm->from = g_strdup(xmlnode_get_attrib(packet, "from"));
	jm->to = g_strdup(xmlnode_get_attrib(packet, "to"));
	jm->id = g_strdup(xmlnode_get_attrib(packet, "id"));

	for(child = packet->child; child; child = child->next) {
		const char *xmlns = xmlnode_get_namespace(child);
		if(!xmlns)
			xmlns = "";
		if(child->type != XMLNODE_TYPE_TAG)
			continue;

		if(!strcmp(child->name, "subject") && !strcmp(xmlns,"jabber:client")) {
			if(!jm->subject)
				jm->subject = xmlnode_get_data(child);
		} else if(!strcmp(child->name, "thread") && !strcmp(xmlns,"jabber:client")) {
			if(!jm->thread_id)
				jm->thread_id = xmlnode_get_data(child);
		} else if(!strcmp(child->name, "body") && !strcmp(xmlns,"jabber:client")) {
			if(!jm->body) {
				char *msg = xmlnode_to_str(child, NULL);
				jm->body = purple_strdup_withhtml(msg);
				g_free(msg);
			}
		} else if(!strcmp(child->name, "html") && !strcmp(xmlns,"http://jabber.org/protocol/xhtml-im")) {
			if(!jm->xhtml && xmlnode_get_child(child, "body")) {
				char *c;
				jm->xhtml = xmlnode_to_str(child, NULL);
			        /* Convert all newlines to whitespace. Technically, even regular, non-XML HTML is supposed to ignore newlines, but Pidgin has, as convention
			 	 * treated \n as a newline for compatibility with other protocols
				 */
				for (c = jm->xhtml; *c != '\0'; c++) {
					if (*c == '\n') 
						*c = ' ';
				}
			}
		} else if(!strcmp(child->name, "active") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_ACTIVE;
			jm->typing_style |= JM_TS_JEP_0085;
		} else if(!strcmp(child->name, "composing") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_COMPOSING;
			jm->typing_style |= JM_TS_JEP_0085;
		} else if(!strcmp(child->name, "paused") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_PAUSED;
			jm->typing_style |= JM_TS_JEP_0085;
		} else if(!strcmp(child->name, "inactive") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_INACTIVE;
			jm->typing_style |= JM_TS_JEP_0085;
		} else if(!strcmp(child->name, "gone") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_GONE;
			jm->typing_style |= JM_TS_JEP_0085;
		} else if(!strcmp(child->name, "event") && !strcmp(xmlns,"http://jabber.org/protocol/pubsub#event")) {
			xmlnode *items;
			jm->type = JABBER_MESSAGE_EVENT;
			for(items = xmlnode_get_child(child,"items"); items; items = items->next)
				jm->eventitems = g_list_append(jm->eventitems, items);
		} else if(!strcmp(child->name, "attention") && !strcmp(xmlns,"http://www.xmpp.org/extensions/xep-0224.html#ns")) {
			jm->hasBuzz = TRUE;
		} else if(!strcmp(child->name, "error")) {
			const char *code = xmlnode_get_attrib(child, "code");
			char *code_txt = NULL;
			char *text = xmlnode_get_data(child);
			if (!text) {
				xmlnode *enclosed_text_node;
				
				if ((enclosed_text_node = xmlnode_get_child(child, "text")))
					text = xmlnode_get_data(enclosed_text_node);
			}

			if(code)
				code_txt = g_strdup_printf(_("(Code %s)"), code);

			if(!jm->error)
				jm->error = g_strdup_printf("%s%s%s",
						text ? text : "",
						text && code_txt ? " " : "",
						code_txt ? code_txt : "");

			g_free(code_txt);
			g_free(text);
		} else if(!strcmp(child->name, "delay") && xmlns && !strcmp(xmlns,"urn:xmpp:delay")) {
			const char *timestamp = xmlnode_get_attrib(child, "stamp");
			jm->delayed = TRUE;
			if(timestamp)
				jm->sent = purple_str_to_time(timestamp, TRUE, NULL, NULL, NULL);
		} else if(!strcmp(child->name, "x")) {
			if(xmlns && !strcmp(xmlns, "jabber:x:event")) {
				if(xmlnode_get_child(child, "composing")) {
					if(jm->chat_state == JM_STATE_ACTIVE)
						jm->chat_state = JM_STATE_COMPOSING;
					jm->typing_style |= JM_TS_JEP_0022;
				}
			} else if(xmlns && !strcmp(xmlns, "jabber:x:delay")) {
				const char *timestamp = xmlnode_get_attrib(child, "stamp");
				jm->delayed = TRUE;
				if(timestamp)
					jm->sent = purple_str_to_time(timestamp, TRUE, NULL, NULL, NULL);
			} else if(xmlns && !strcmp(xmlns, "jabber:x:conference") &&
					jm->type != JABBER_MESSAGE_GROUPCHAT_INVITE &&
					jm->type != JABBER_MESSAGE_ERROR) {
				const char *jid = xmlnode_get_attrib(child, "jid");
				if(jid) {
					jm->type = JABBER_MESSAGE_GROUPCHAT_INVITE;
					g_free(jm->to);
					jm->to = g_strdup(jid);
				}
			} else if(xmlns && !strcmp(xmlns,
						"http://jabber.org/protocol/muc#user") &&
					jm->type != JABBER_MESSAGE_ERROR) {
				xmlnode *invite = xmlnode_get_child(child, "invite");
				if(invite) {
					xmlnode *reason, *password;
					const char *jid = xmlnode_get_attrib(invite, "from");
					g_free(jm->to);
					jm->to = jm->from;
					jm->from = g_strdup(jid);
					if((reason = xmlnode_get_child(invite, "reason"))) {
						g_free(jm->body);
						jm->body = xmlnode_get_data(reason);
					}
					if((password = xmlnode_get_child(child, "password")))
						jm->password = xmlnode_get_data(password);

					jm->type = JABBER_MESSAGE_GROUPCHAT_INVITE;
				}
			} else {
				jm->etc = g_list_append(jm->etc, child);
			}
		}
	}

	if(jm->hasBuzz)
		handle_buzz(jm);

	switch(jm->type) {
		case JABBER_MESSAGE_NORMAL:
		case JABBER_MESSAGE_CHAT:
			handle_chat(jm);
			break;
		case JABBER_MESSAGE_HEADLINE:
			handle_headline(jm);
			break;
		case JABBER_MESSAGE_GROUPCHAT:
			handle_groupchat(jm);
			break;
		case JABBER_MESSAGE_GROUPCHAT_INVITE:
			handle_groupchat_invite(jm);
			break;
		case JABBER_MESSAGE_EVENT:
			jabber_handle_event(jm);
			break;
		case JABBER_MESSAGE_ERROR:
			handle_error(jm);
			break;
		case JABBER_MESSAGE_OTHER:
			purple_debug(PURPLE_DEBUG_INFO, "jabber",
					"Received message of unknown type: %s\n", type);
			break;
	}
	jabber_message_free(jm);
}

void jabber_message_send(JabberMessage *jm)
{
	xmlnode *message, *child;
	const char *type = NULL;

	message = xmlnode_new("message");

	switch(jm->type) {
		case JABBER_MESSAGE_NORMAL:
			type = "normal";
			break;
		case JABBER_MESSAGE_CHAT:
		case JABBER_MESSAGE_GROUPCHAT_INVITE:
			type = "chat";
			break;
		case JABBER_MESSAGE_HEADLINE:
			type = "headline";
			break;
		case JABBER_MESSAGE_GROUPCHAT:
			type = "groupchat";
			break;
		case JABBER_MESSAGE_ERROR:
			type = "error";
			break;
		case JABBER_MESSAGE_OTHER:
		default:
			type = NULL;
			break;
	}

	if(type)
		xmlnode_set_attrib(message, "type", type);
 
	if (jm->id)
		xmlnode_set_attrib(message, "id", jm->id);

	xmlnode_set_attrib(message, "to", jm->to);

	if(jm->thread_id) {
		child = xmlnode_new_child(message, "thread");
		xmlnode_insert_data(child, jm->thread_id, -1);
	}

	if(JM_TS_JEP_0022 == (jm->typing_style & JM_TS_JEP_0022)) {
		child = xmlnode_new_child(message, "x");
		xmlnode_set_namespace(child, "jabber:x:event");
		if(jm->chat_state == JM_STATE_COMPOSING || jm->body)
			xmlnode_new_child(child, "composing");
	}

	if(JM_TS_JEP_0085 == (jm->typing_style & JM_TS_JEP_0085)) {
		child = NULL;
		switch(jm->chat_state)
		{
			case JM_STATE_ACTIVE:
				child = xmlnode_new_child(message, "active");
				break;
			case JM_STATE_COMPOSING:
				child = xmlnode_new_child(message, "composing");
				break;
			case JM_STATE_PAUSED:
				child = xmlnode_new_child(message, "paused");
				break;
			case JM_STATE_INACTIVE:
				child = xmlnode_new_child(message, "inactive");
				break;
			case JM_STATE_GONE:
				child = xmlnode_new_child(message, "gone");
				break;
		}
		if(child)
			xmlnode_set_namespace(child, "http://jabber.org/protocol/chatstates");
	}

	if(jm->subject) {
		child = xmlnode_new_child(message, "subject");
		xmlnode_insert_data(child, jm->subject, -1);
	}

	if(jm->body) {
		child = xmlnode_new_child(message, "body");
		xmlnode_insert_data(child, jm->body, -1);
	}

	if(jm->xhtml) {
		child = xmlnode_from_str(jm->xhtml, -1);
		if(child) {
			xmlnode_insert_child(message, child);
		} else {
			purple_debug(PURPLE_DEBUG_ERROR, "jabber",
					"XHTML translation/validation failed, returning: %s\n",
					jm->xhtml);
		}
	}

	jabber_send(jm->js, message);

	xmlnode_free(message);
}

int jabber_message_send_im(PurpleConnection *gc, const char *who, const char *msg,
		PurpleMessageFlags flags)
{
	JabberMessage *jm;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *buf;
	char *xhtml;
	char *resource;

	if(!who || !msg)
		return 0;

	resource = jabber_get_resource(who);

	jb = jabber_buddy_find(gc->proto_data, who, TRUE);
	jbr = jabber_buddy_find_resource(jb, resource);

	g_free(resource);

	jm = g_new0(JabberMessage, 1);
	jm->js = gc->proto_data;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->chat_state = JM_STATE_ACTIVE;
	jm->to = g_strdup(who);
	jm->id = jabber_get_next_id(jm->js);
	jm->chat_state = JM_STATE_ACTIVE;

	if(jbr) {
		if(jbr->thread_id)
			jm->thread_id = jbr->thread_id;

		if(jbr->chat_states != JABBER_CHAT_STATES_UNSUPPORTED) {
			jm->typing_style |= JM_TS_JEP_0085;
			/* if(JABBER_CHAT_STATES_UNKNOWN == jbr->chat_states)
			   jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED; */
		}

		if(jbr->chat_states != JABBER_CHAT_STATES_SUPPORTED)
			jm->typing_style |= JM_TS_JEP_0022;
	}

	buf = g_strdup_printf("<html xmlns='http://jabber.org/protocol/xhtml-im'><body xmlns='http://www.w3.org/1999/xhtml'>%s</body></html>", msg);
	
	purple_markup_html_to_xhtml(buf, &xhtml, &jm->body);
	g_free(buf);

	if(!jbr || jbr->capabilities & JABBER_CAP_XHTML)
		jm->xhtml = xhtml;
	else
		g_free(xhtml);

	jabber_message_send(jm);
	jabber_message_free(jm);
	return 1;
}

int jabber_message_send_chat(PurpleConnection *gc, int id, const char *msg, PurpleMessageFlags flags)
{
	JabberChat *chat;
	JabberMessage *jm;
	JabberStream *js;
	char *buf;

	if(!msg || !gc)
		return 0;

	js = gc->proto_data;
	chat = jabber_chat_find_by_id(js, id);

	if(!chat)
		return 0;

	jm = g_new0(JabberMessage, 1);
	jm->js = gc->proto_data;
	jm->type = JABBER_MESSAGE_GROUPCHAT;
	jm->to = g_strdup_printf("%s@%s", chat->room, chat->server);
	jm->id = jabber_get_next_id(jm->js);

	buf = g_strdup_printf("<html xmlns='http://jabber.org/protocol/xhtml-im'><body xmlns='http://www.w3.org/1999/xhtml'>%s</body></html>", msg);
	purple_markup_html_to_xhtml(buf, &jm->xhtml, &jm->body);
	g_free(buf);

	if(!chat->xhtml) {
		g_free(jm->xhtml);
		jm->xhtml = NULL;
	}

	jabber_message_send(jm);
	jabber_message_free(jm);

	return 1;
}

unsigned int jabber_send_typing(PurpleConnection *gc, const char *who, PurpleTypingState state)
{
	JabberMessage *jm;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *resource = jabber_get_resource(who);

	jb = jabber_buddy_find(gc->proto_data, who, TRUE);
	jbr = jabber_buddy_find_resource(jb, resource);

	g_free(resource);

	if(!jbr || !((jbr->capabilities & JABBER_CAP_COMPOSING) || (jbr->chat_states != JABBER_CHAT_STATES_UNSUPPORTED)))
		return 0;

	/* TODO: figure out threading */
	jm = g_new0(JabberMessage, 1);
	jm->js = gc->proto_data;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->to = g_strdup(who);
	jm->id = jabber_get_next_id(jm->js);

	if(PURPLE_TYPING == state)
		jm->chat_state = JM_STATE_COMPOSING;
	else if(PURPLE_TYPED == state)
		jm->chat_state = JM_STATE_PAUSED;
	else
		jm->chat_state = JM_STATE_ACTIVE;

	if(jbr->chat_states != JABBER_CHAT_STATES_UNSUPPORTED) {
		jm->typing_style |= JM_TS_JEP_0085;
		/* if(JABBER_CHAT_STATES_UNKNOWN == jbr->chat_states)
			jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED; */
	}

	if(jbr->chat_states != JABBER_CHAT_STATES_SUPPORTED)
		jm->typing_style |= JM_TS_JEP_0022;

	jabber_message_send(jm);
	jabber_message_free(jm);

	return 0;
}

void jabber_message_conv_closed(JabberStream *js, const char *who)
{
	JabberMessage *jm;
	if (!purple_prefs_get_bool("/purple/conversations/im/send_typing"))
		return;

	jm  = g_new0(JabberMessage, 1);
	jm->js = js;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->to = g_strdup(who);
	jm->id = jabber_get_next_id(jm->js);
	jm->typing_style = JM_TS_JEP_0085;
	jm->chat_state = JM_STATE_GONE;
	jabber_message_send(jm);
	jabber_message_free(jm);
}

gboolean jabber_buzz_isenabled(JabberStream *js, const gchar *shortname, const gchar *namespace) {
	return js->allowBuzz;
}

