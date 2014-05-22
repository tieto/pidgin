/*
 * purple - Jabber Protocol Plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "internal.h"

#include "debug.h"
#include "notify.h"
#include "smiley-custom.h"
#include "smiley-parser.h"
#include "server.h"
#include "util.h"
#include "adhoccommands.h"
#include "buddy.h"
#include "chat.h"
#include "data.h"
#include "google/google.h"
#include "message.h"
#include "xmlnode.h"
#include "pep.h"
#include "smiley.h"
#include "iq.h"

#include <string.h>

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

	PurpleConnection *gc;
	PurpleAccount *account;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;

	if(!jid)
		return;

	gc = jm->js->gc;
	account = purple_connection_get_account(gc);

	jb = jabber_buddy_find(jm->js, jm->from, TRUE);
	jbr = jabber_buddy_find_resource(jb, jid->resource);

	if(!jm->xhtml && !jm->body) {
		if (jbr && jm->chat_state != JM_STATE_NONE)
			jbr->chat_states = JABBER_CHAT_STATES_SUPPORTED;

		if(JM_STATE_COMPOSING == jm->chat_state) {
			purple_serv_got_typing(gc, jm->from, 0, PURPLE_IM_TYPING);
		} else if(JM_STATE_PAUSED == jm->chat_state) {
			purple_serv_got_typing(gc, jm->from, 0, PURPLE_IM_TYPED);
		} else if(JM_STATE_GONE == jm->chat_state) {
			PurpleIMConversation *im = purple_conversations_find_im_with_account(
					jm->from, account);
			if (im && jid->node && jid->domain) {
				char buf[256];
				PurpleBuddy *buddy;

				g_snprintf(buf, sizeof(buf), "%s@%s", jid->node, jid->domain);

				if ((buddy = purple_blist_find_buddy(account, buf))) {
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
					purple_conversation_write_system_message(
						PURPLE_CONVERSATION(im), buf, 0);
				}
			}
			purple_serv_got_typing_stopped(gc, jm->from);

		} else {
			purple_serv_got_typing_stopped(gc, jm->from);
		}
	} else {
		if (jid->resource) {
			/*
			 * We received a message from a specific resource, so
			 * we probably want a reply to go to this specific
			 * resource (i.e. bind/lock the conversation to this
			 * resource).
			 *
			 * This works because purple_im_conversation_send gets the name
			 * from purple_conversation_get_name()
			 */
			PurpleIMConversation *im;

			im = purple_conversations_find_im_with_account(jm->from, account);
			if (im && !g_str_equal(jm->from,
					purple_conversation_get_name(PURPLE_CONVERSATION(im)))) {
				purple_debug_info("jabber", "Binding conversation to %s\n",
				                  jm->from);
				purple_conversation_set_name(PURPLE_CONVERSATION(im), jm->from);
			}
		}

		if(jbr) {
			/* Treat SUPPORTED as a terminal with no escape :) */
			if (jbr->chat_states != JABBER_CHAT_STATES_SUPPORTED) {
				if (jm->chat_state != JM_STATE_NONE)
					jbr->chat_states = JABBER_CHAT_STATES_SUPPORTED;
				else
					jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED;
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
		purple_serv_got_im(gc, jm->from, jm->xhtml ? jm->xhtml : jm->body, 0, jm->sent);
	}

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
		PurpleXmlNode *x = etc->data;
		const char *xmlns = purple_xmlnode_get_namespace(x);
		if(xmlns && !strcmp(xmlns, NS_OOB_X_DATA)) {
			PurpleXmlNode *url, *desc;
			char *urltxt, *desctxt;

			url = purple_xmlnode_get_child(x, "url");
			desc = purple_xmlnode_get_child(x, "desc");

			if(!url || !desc)
				continue;

			urltxt = purple_xmlnode_get_data(url);
			desctxt = purple_xmlnode_get_data(desc);

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
	PurpleMessageFlags messageFlags = 0;

	if(!jid)
		return;

	chat = jabber_chat_find(jm->js, jid->node, jid->domain);

	if(!chat)
		return;

	if(jm->subject) {
		purple_chat_conversation_set_topic(chat->conv, jid->resource,
				jm->subject);
		messageFlags |= PURPLE_MESSAGE_NO_LOG;
		if(!jm->xhtml && !jm->body) {
			char *msg, *tmp, *tmp2;
			tmp = g_markup_escape_text(jm->subject, -1);
			tmp2 = purple_markup_linkify(tmp);
			if(jid->resource)
				msg = g_strdup_printf(_("%s has set the topic to: %s"), jid->resource, tmp2);
			else
				msg = g_strdup_printf(_("The topic is: %s"), tmp2);
			purple_conversation_write_system_message(PURPLE_CONVERSATION(chat->conv),
				msg, messageFlags);
			g_free(tmp);
			g_free(tmp2);
			g_free(msg);
		}
	}

	if(jm->xhtml || jm->body) {
		if(jid->resource)
			purple_serv_got_chat_in(jm->js->gc, chat->id, jid->resource,
							messageFlags | (jm->delayed ? PURPLE_MESSAGE_DELAYED : 0),
							jm->xhtml ? jm->xhtml : jm->body, jm->sent);
		else if(chat->muc)
			purple_conversation_write_system_message(
				PURPLE_CONVERSATION(chat->conv),
				jm->xhtml ? jm->xhtml : jm->body, messageFlags);
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
	purple_serv_got_chat_invite(jm->js->gc, jm->to, jm->from, jm->body, components);
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
	PurpleAccount *account;

	/* Delayed buzz MUST NOT be accepted */
	if(jm->delayed)
		return;

	/* Reject buzz when it's not enabled */
	if(!jm->js->allowBuzz)
		return;

	account = purple_connection_get_account(jm->js->gc);

	if (purple_blist_find_buddy(account, jm->from) == NULL)
		return; /* Do not accept buzzes from unknown people */

	/* xmpp only has 1 attention type, so index is 0 */
	purple_prpl_got_attention(jm->js->gc, jm->from, 0);
}

/* used internally by the functions below */
typedef struct {
	gchar *cid;
	gchar *alt;
} JabberSmileyRef;


static void
jabber_message_get_refs_from_xmlnode_internal(const PurpleXmlNode *message,
	GHashTable *table)
{
	PurpleXmlNode *child;

	for (child = purple_xmlnode_get_child(message, "img") ; child ;
		 child = purple_xmlnode_get_next_twin(child)) {
		const gchar *src = purple_xmlnode_get_attrib(child, "src");

		if (g_str_has_prefix(src, "cid:")) {
			const gchar *cid = src + 4;

			/* if we haven't "fetched" this yet... */
			if (!g_hash_table_lookup(table, cid)) {
				/* take a copy of the cid and let the SmileyRef own it... */
				gchar *temp_cid = g_strdup(cid);
				JabberSmileyRef *ref = g_new0(JabberSmileyRef, 1);
				const gchar *alt = purple_xmlnode_get_attrib(child, "alt");
				ref->cid = temp_cid;
				/* if there is no "alt" string, use the cid...
				 include the entire src, eg. "cid:.." to avoid linkification */
				if (alt && alt[0] != '\0') {
					/* workaround for when "alt" is set to the value of the
					 CID (which Jabbim seems to do), to avoid it showing up
						 as an mailto: link */
					if (purple_email_is_valid(alt)) {
						ref->alt = g_strdup_printf("smiley:%s", alt);
					} else {
						ref->alt = g_strdup(alt);
					}
				} else {
					ref->alt = g_strdup(src);
				}
				g_hash_table_insert(table, temp_cid, ref);
			}
		}
	}

	for (child = message->child ; child ; child = child->next) {
		jabber_message_get_refs_from_xmlnode_internal(child, table);
	}
}

static gboolean
jabber_message_get_refs_steal(gpointer key, gpointer value, gpointer user_data)
{
	GList **refs = (GList **) user_data;
	JabberSmileyRef *ref = (JabberSmileyRef *) value;

	*refs = g_list_append(*refs, ref);

	return TRUE;
}

static GList *
jabber_message_get_refs_from_xmlnode(const PurpleXmlNode *message)
{
	GList *refs = NULL;
	GHashTable *unique_refs = g_hash_table_new(g_str_hash, g_str_equal);

	jabber_message_get_refs_from_xmlnode_internal(message, unique_refs);
	(void) g_hash_table_foreach_steal(unique_refs,
		jabber_message_get_refs_steal, (gpointer) &refs);
	g_hash_table_destroy(unique_refs);
	return refs;
}

static gchar *
jabber_message_xml_to_string_strip_img_smileys(PurpleXmlNode *xhtml)
{
	gchar *markup = purple_xmlnode_to_str(xhtml, NULL);
	int len = strlen(markup);
	int pos = 0;
	GString *out = g_string_new(NULL);

	while (pos < len) {
		/* this is a bit cludgy, maybe there is a better way to do this...
		  we need to find all <img> tags within the XHTML and replace those
			tags with the value of their "alt" attributes */
		if (g_str_has_prefix(&(markup[pos]), "<img")) {
			PurpleXmlNode *img = NULL;
			int pos2 = pos;
			const gchar *src;

			for (; pos2 < len ; pos2++) {
				if (g_str_has_prefix(&(markup[pos2]), "/>")) {
					pos2 += 2;
					break;
				} else if (g_str_has_prefix(&(markup[pos2]), "</img>")) {
					pos2 += 5;
					break;
				}
			}

			/* note, if the above loop didn't find the end of the <img> tag,
			  it the parsed string will be until the end of the input string,
			  in which case purple_xmlnode_from_str will bail out and return NULL,
			  in this case the "if" statement below doesn't trigger and the
			  text is copied unchanged */
			img = purple_xmlnode_from_str(&(markup[pos]), pos2 - pos);
			src = purple_xmlnode_get_attrib(img, "src");

			if (g_str_has_prefix(src, "cid:")) {
				const gchar *alt = purple_xmlnode_get_attrib(img, "alt");
				/* if the "alt" attribute is empty, put the cid as smiley string */
				if (alt && alt[0] != '\0') {
					/* if the "alt" is the same as the CID, as Jabbim does,
					 this prevents linkification... */
					if (purple_email_is_valid(alt)) {
						gchar *safe_alt = g_strdup_printf("smiley:%s", alt);
						out = g_string_append(out, safe_alt);
						g_free(safe_alt);
					} else {
						gchar *alt_escaped = g_markup_escape_text(alt, -1);
						out = g_string_append(out, alt_escaped);
						g_free(alt_escaped);
					}
				} else {
					out = g_string_append(out, src);
				}
				pos += pos2 - pos;
			} else {
				out = g_string_append_c(out, markup[pos]);
				pos++;
			}

			purple_xmlnode_free(img);

		} else {
			out = g_string_append_c(out, markup[pos]);
			pos++;
		}
	}

	g_free(markup);
	return g_string_free(out, FALSE);
}

static void
jabber_message_add_remote_smileys(JabberStream *js, const gchar *who,
    const PurpleXmlNode *message)
{
	PurpleXmlNode *data_tag;
	for (data_tag = purple_xmlnode_get_child_with_namespace(message, "data", NS_BOB) ;
		 data_tag ;
		 data_tag = purple_xmlnode_get_next_twin(data_tag)) {
		const gchar *cid = purple_xmlnode_get_attrib(data_tag, "cid");
		const JabberData *data = jabber_data_find_remote_by_cid(js, who, cid);

		if (!data && cid != NULL) {
			/* we haven't cached this already, let's add it */
			JabberData *new_data = jabber_data_create_from_xml(data_tag);

			if (new_data) {
				jabber_data_associate_remote(js, who, new_data);
			}
		}
	}
}

static void
jabber_message_remote_smiley_got(JabberData *data, gchar *alt, gpointer _smiley)
{
	PurpleSmiley *smiley = _smiley;
	PurpleImage *image = purple_smiley_get_image(smiley);

	g_free(alt); /* we really don't need it */

	if (data) {
		purple_debug_info("jabber",
			"smiley data retrieved successfully");
		purple_image_transfer_write(image, jabber_data_get_data(data),
			jabber_data_get_size(data));
		purple_image_transfer_close(image);
	} else {
		purple_debug_error("jabber", "failed retrieving smiley data");
		purple_image_transfer_failed(image);
	}

	g_object_unref(smiley);
}

static void
jabber_message_remote_smiley_add(JabberStream *js, PurpleConversation *conv,
	const gchar *from, const gchar *shortcut, const gchar *cid)
{
	PurpleSmiley *smiley;
	const JabberData *jdata;

	purple_debug_misc("jabber", "about to add remote smiley %s to the conv",
		shortcut);

	smiley = purple_conversation_add_remote_smiley(conv, shortcut);
	if (!smiley) {
		purple_debug_misc("jabber", "smiley was already present");
		return;
	}

	/* TODO: cache lookup by "cid" */

	jdata = jabber_data_find_remote_by_cid(js, from, cid);
	if (jdata) {
		PurpleImage *image = purple_smiley_get_image(smiley);

		purple_debug_info("jabber", "smiley data is already known");

		purple_image_transfer_write(image, jabber_data_get_data(jdata),
			jabber_data_get_size(jdata));
		purple_image_transfer_close(image);
	} else {
		gchar *alt = g_strdup(shortcut); /* it it really necessary? */

		purple_debug_info("jabber", "smiley data is unknown, "
			"need to request it");

		g_object_ref(smiley);
		jabber_data_request(js, cid, from, alt, FALSE,
			jabber_message_remote_smiley_got, smiley);
	}
}

void jabber_message_parse(JabberStream *js, PurpleXmlNode *packet)
{
	JabberMessage *jm;
	const char *id, *from, *to, *type;
	PurpleXmlNode *child;
	gboolean signal_return;

	from = purple_xmlnode_get_attrib(packet, "from");
	id   = purple_xmlnode_get_attrib(packet, "id");
	to   = purple_xmlnode_get_attrib(packet, "to");
	type = purple_xmlnode_get_attrib(packet, "type");

	signal_return = GPOINTER_TO_INT(purple_signal_emit_return_1(purple_connection_get_prpl(js->gc),
			"jabber-receiving-message", js->gc, type, id, from, to, packet));
	if (signal_return)
		return;

	jm = g_new0(JabberMessage, 1);
	jm->js = js;
	jm->sent = time(NULL);
	jm->delayed = FALSE;
	jm->chat_state = JM_STATE_NONE;

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

	jm->from = g_strdup(from);
	jm->to   = g_strdup(to);
	jm->id   = g_strdup(id);

	for(child = packet->child; child; child = child->next) {
		const char *xmlns = purple_xmlnode_get_namespace(child);
		if(child->type != PURPLE_XMLNODE_TYPE_TAG)
			continue;

		if(!strcmp(child->name, "error")) {
			const char *code = purple_xmlnode_get_attrib(child, "code");
			char *code_txt = NULL;
			char *text = purple_xmlnode_get_data(child);
			if (!text) {
				PurpleXmlNode *enclosed_text_node;

				if ((enclosed_text_node = purple_xmlnode_get_child(child, "text")))
					text = purple_xmlnode_get_data(enclosed_text_node);
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
		} else if (xmlns == NULL) {
			/* QuLogic: Not certain this is correct, but it would have happened
			   with the previous code. */
			if(!strcmp(child->name, "x"))
				jm->etc = g_list_append(jm->etc, child);
			/* The following tests expect xmlns != NULL */
			continue;
		} else if(!strcmp(child->name, "subject") && !strcmp(xmlns, NS_XMPP_CLIENT)) {
			if(!jm->subject) {
				jm->subject = purple_xmlnode_get_data(child);
				if(!jm->subject)
					jm->subject = g_strdup("");
			}
		} else if(!strcmp(child->name, "thread") && !strcmp(xmlns, NS_XMPP_CLIENT)) {
			if(!jm->thread_id)
				jm->thread_id = purple_xmlnode_get_data(child);
		} else if(!strcmp(child->name, "body") && !strcmp(xmlns, NS_XMPP_CLIENT)) {
			if(!jm->body) {
				char *msg = purple_xmlnode_get_data(child);
				char *escaped = purple_markup_escape_text(msg, -1);
				jm->body = purple_strdup_withhtml(escaped);
				g_free(escaped);
				g_free(msg);
			}
		} else if(!strcmp(child->name, "html") && !strcmp(xmlns, NS_XHTML_IM)) {
			if(!jm->xhtml && purple_xmlnode_get_child(child, "body")) {
				char *c;

				const PurpleConnection *gc = js->gc;
				PurpleAccount *account = purple_connection_get_account(gc);
				PurpleConversation *conv = NULL;
				GList *smiley_refs = NULL, *it;
				gchar *reformatted_xhtml;

				if (purple_account_get_bool(account, "custom_smileys", TRUE)) {
					/* find a list of smileys ("cid" and "alt" text pairs)
					  occuring in the message */
					smiley_refs = jabber_message_get_refs_from_xmlnode(child);
					purple_debug_info("jabber", "found %d smileys\n",
						g_list_length(smiley_refs));

					if (smiley_refs) {
						if (jm->type == JABBER_MESSAGE_GROUPCHAT) {
							JabberID *jid = jabber_id_new(jm->from);
							JabberChat *chat = NULL;

							if (jid) {
								chat = jabber_chat_find(js, jid->node, jid->domain);
								if (chat)
									conv = PURPLE_CONVERSATION(chat->conv);
								jabber_id_free(jid);
							}
						} else if (jm->type == JABBER_MESSAGE_NORMAL ||
						           jm->type == JABBER_MESSAGE_CHAT) {
							conv =
								purple_conversations_find_with_account(from, account);
							if (!conv) {
								/* we need to create the conversation here */
								conv = PURPLE_CONVERSATION(
									purple_im_conversation_new(account, from));
							}
						}
					}

					/* process any newly provided smileys */
					jabber_message_add_remote_smileys(js, to, packet);
				}

				purple_xmlnode_strip_prefixes(child);

				/* reformat xhtml so that img tags with a "cid:" src gets
				  translated to the bare text of the emoticon (the "alt" attrib) */
				/* this is done also when custom smiley retrieval is turned off,
				  this way the receiver always sees the shortcut instead */
				reformatted_xhtml =
					jabber_message_xml_to_string_strip_img_smileys(child);

				jm->xhtml = reformatted_xhtml;

				/* add known custom emoticons to the conversation */
				/* note: if there were no smileys in the incoming message, or
				  	if receiving custom smileys is turned off, smiley_refs will
					be NULL */
				for (it = smiley_refs; it; it = g_list_next(it)) {
					JabberSmileyRef *ref = it->data;

					if (conv) {
						jabber_message_remote_smiley_add(js,
							conv, from, ref->alt, ref->cid);
					}

					g_free(ref->cid);
					g_free(ref->alt);
					g_free(ref);
				}
				g_list_free(smiley_refs);

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
		} else if(!strcmp(child->name, "composing") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_COMPOSING;
		} else if(!strcmp(child->name, "paused") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_PAUSED;
		} else if(!strcmp(child->name, "inactive") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_INACTIVE;
		} else if(!strcmp(child->name, "gone") && !strcmp(xmlns,"http://jabber.org/protocol/chatstates")) {
			jm->chat_state = JM_STATE_GONE;
		} else if(!strcmp(child->name, "event") && !strcmp(xmlns,"http://jabber.org/protocol/pubsub#event")) {
			PurpleXmlNode *items;
			jm->type = JABBER_MESSAGE_EVENT;
			for(items = purple_xmlnode_get_child(child,"items"); items; items = items->next)
				jm->eventitems = g_list_append(jm->eventitems, items);
		} else if(!strcmp(child->name, "attention") && !strcmp(xmlns, NS_ATTENTION)) {
			jm->hasBuzz = TRUE;
		} else if(!strcmp(child->name, "delay") && !strcmp(xmlns, NS_DELAYED_DELIVERY)) {
			const char *timestamp = purple_xmlnode_get_attrib(child, "stamp");
			jm->delayed = TRUE;
			if(timestamp)
				jm->sent = purple_str_to_time(timestamp, TRUE, NULL, NULL, NULL);
		} else if(!strcmp(child->name, "x")) {
			if(!strcmp(xmlns, NS_DELAYED_DELIVERY_LEGACY)) {
				const char *timestamp = purple_xmlnode_get_attrib(child, "stamp");
				jm->delayed = TRUE;
				if(timestamp)
					jm->sent = purple_str_to_time(timestamp, TRUE, NULL, NULL, NULL);
			} else if(!strcmp(xmlns, "jabber:x:conference") &&
					jm->type != JABBER_MESSAGE_GROUPCHAT_INVITE &&
					jm->type != JABBER_MESSAGE_ERROR) {
				const char *jid = purple_xmlnode_get_attrib(child, "jid");
				if(jid) {
					const char *reason = purple_xmlnode_get_attrib(child, "reason");
					const char *password = purple_xmlnode_get_attrib(child, "password");

					jm->type = JABBER_MESSAGE_GROUPCHAT_INVITE;
					g_free(jm->to);
					jm->to = g_strdup(jid);

					if (reason) {
						g_free(jm->body);
						jm->body = g_strdup(reason);
					}

					if (password) {
						g_free(jm->password);
						jm->password = g_strdup(password);
					}
				}
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/muc#user") &&
					jm->type != JABBER_MESSAGE_ERROR) {
				PurpleXmlNode *invite = purple_xmlnode_get_child(child, "invite");
				if(invite) {
					PurpleXmlNode *reason, *password;
					const char *jid = purple_xmlnode_get_attrib(invite, "from");
					g_free(jm->to);
					jm->to = jm->from;
					jm->from = g_strdup(jid);
					if((reason = purple_xmlnode_get_child(invite, "reason"))) {
						g_free(jm->body);
						jm->body = purple_xmlnode_get_data(reason);
					}
					if((password = purple_xmlnode_get_child(child, "password"))) {
						g_free(jm->password);
						jm->password = purple_xmlnode_get_data(password);
					}

					jm->type = JABBER_MESSAGE_GROUPCHAT_INVITE;
				}
			} else {
				jm->etc = g_list_append(jm->etc, child);
			}
		} else if (g_str_equal(child->name, "query")) {
			const char *node = purple_xmlnode_get_attrib(child, "node");
			if (purple_strequal(xmlns, NS_DISCO_ITEMS)
					&& purple_strequal(node, "http://jabber.org/protocol/commands")) {
				jabber_adhoc_got_list(js, jm->from, child);
			}
		}
	}

	if(jm->hasBuzz)
		handle_buzz(jm);

	switch(jm->type) {
		case JABBER_MESSAGE_OTHER:
			purple_debug_info("jabber",
					"Received message of unknown type: %s\n", type);
			/* Fall-through is intentional */
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
	}
	jabber_message_free(jm);
}

static gboolean
jabber_conv_support_custom_smileys(JabberStream *js,
								   PurpleConversation *conv,
								   const gchar *who)
{
	JabberBuddy *jb;
	JabberChat *chat;

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		jb = jabber_buddy_find(js, who, FALSE);
		if (jb) {
			return jabber_buddy_has_capability(jb, NS_BOB);
		} else {
			return FALSE;
		}
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		chat = jabber_chat_find_by_conv(PURPLE_CHAT_CONVERSATION(conv));
		if (chat) {
			/* do not attempt to send custom smileys in a MUC with more than
			 10 people, to avoid getting too many BoB requests */
			return jabber_chat_get_num_participants(chat) <= 10 &&
				jabber_chat_all_participants_have_capability(chat,
					NS_BOB);
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

static gboolean
jabber_message_smileyify_cb(GString *out, PurpleSmiley *smiley,
	PurpleConversation *_empty, gpointer _unused)
{
	const gchar *shortcut;
	const JabberData *data;
	PurpleXmlNode *smiley_node;
	gchar *node_xml;

	shortcut = purple_smiley_get_shortcut(smiley);
	data = jabber_data_find_local_by_alt(shortcut);

	if (!data)
		return FALSE;

	smiley_node = jabber_data_get_xhtml_im(data, shortcut);
	node_xml = purple_xmlnode_to_str(smiley_node, NULL);

	g_string_append(out, node_xml);

	purple_xmlnode_free(smiley_node);
	g_free(node_xml);

	return TRUE;
}

static char *
jabber_message_smileyfy_xhtml(JabberMessage *jm, const char *xhtml)
{
	PurpleAccount *account = purple_connection_get_account(jm->js->gc);
	GList *found_smileys, *it, *it_next;
	PurpleConversation *conv;
	gboolean has_too_large_smiley = FALSE;
	gchar *smileyfied_xhtml = NULL;

	conv = purple_conversations_find_with_account(jm->to, account);

	if (!jabber_conv_support_custom_smileys(jm->js, conv, jm->to))
		return NULL;

	found_smileys = purple_smiley_parser_find(
		purple_smiley_custom_get_list(), xhtml, TRUE);
	if (!found_smileys)
		return NULL;

	for (it = found_smileys; it; it = it_next) {
		PurpleSmiley *smiley = it->data;
		PurpleImage *smiley_image;
		gboolean valid = TRUE;

		it_next = g_list_next(it);

		smiley_image = purple_smiley_get_image(smiley);
		if (!smiley_image) {
			valid = FALSE;
			purple_debug_warning("jabber", "broken smiley %s",
				purple_smiley_get_shortcut(smiley));
		} else if (purple_image_get_size(smiley_image) >
			JABBER_DATA_MAX_SIZE)
		{
			has_too_large_smiley = TRUE;
			valid = FALSE;
			purple_debug_warning("jabber", "Refusing to send "
				"smiley %s (too large, max is %d)",
				purple_smiley_get_shortcut(smiley),
				JABBER_DATA_MAX_SIZE);
		}

		if (!valid)
			found_smileys = g_list_delete_link(found_smileys, it);
	}

	if (has_too_large_smiley) {
		purple_conversation_write_system_message(conv,
			_("A custom smiley in the message is too large to send."),
			PURPLE_MESSAGE_ERROR);
	}

	if (!found_smileys)
		return NULL;

	for (it = found_smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;
		PurpleImage *smiley_image;
		const gchar *shortcut = purple_smiley_get_shortcut(smiley);
		const gchar *mimetype;
		JabberData *jdata;

		/* the object has been sent before */
		if (jabber_data_find_local_by_alt(shortcut))
			continue;

		smiley_image = purple_smiley_get_image(smiley);
		g_assert(smiley_image != NULL);

		mimetype = purple_image_get_mimetype(smiley_image);
		if (!mimetype) {
			purple_debug_error("jabber",
				"unknown mime type for image");
			continue;
		}

		jdata = jabber_data_create_from_data(
			purple_image_get_data(smiley_image),
			purple_image_get_size(smiley_image),
			mimetype, FALSE, jm->js);

		purple_debug_info("jabber", "cache local smiley alt=%s, cid=%s",
			shortcut, jabber_data_get_cid(jdata));
		jabber_data_associate_local(jdata, shortcut);
	}

	g_list_free(found_smileys);

	smileyfied_xhtml = purple_smiley_parser_replace(
		purple_smiley_custom_get_list(), xhtml,
		jabber_message_smileyify_cb, NULL);

	return smileyfied_xhtml;
}

void jabber_message_send(JabberMessage *jm)
{
	PurpleXmlNode *message, *child;
	const char *type = NULL;

	message = purple_xmlnode_new("message");

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
		purple_xmlnode_set_attrib(message, "type", type);

	if (jm->id)
		purple_xmlnode_set_attrib(message, "id", jm->id);

	purple_xmlnode_set_attrib(message, "to", jm->to);

	if(jm->thread_id) {
		child = purple_xmlnode_new_child(message, "thread");
		purple_xmlnode_insert_data(child, jm->thread_id, -1);
	}

	child = NULL;
	switch(jm->chat_state)
	{
		case JM_STATE_ACTIVE:
			child = purple_xmlnode_new_child(message, "active");
			break;
		case JM_STATE_COMPOSING:
			child = purple_xmlnode_new_child(message, "composing");
			break;
		case JM_STATE_PAUSED:
			child = purple_xmlnode_new_child(message, "paused");
			break;
		case JM_STATE_INACTIVE:
			child = purple_xmlnode_new_child(message, "inactive");
			break;
		case JM_STATE_GONE:
			child = purple_xmlnode_new_child(message, "gone");
			break;
		case JM_STATE_NONE:
			/* yep, nothing */
			break;
	}
	if(child)
		purple_xmlnode_set_namespace(child, "http://jabber.org/protocol/chatstates");

	if(jm->subject) {
		child = purple_xmlnode_new_child(message, "subject");
		purple_xmlnode_insert_data(child, jm->subject, -1);
	}

	if(jm->body) {
		child = purple_xmlnode_new_child(message, "body");
		purple_xmlnode_insert_data(child, jm->body, -1);
	}

	if(jm->xhtml) {
		if ((child = purple_xmlnode_from_str(jm->xhtml, -1))) {
			purple_xmlnode_insert_child(message, child);
		} else {
			purple_debug_error("jabber",
					"XHTML translation/validation failed, returning: %s\n",
					jm->xhtml);
		}
	}

	jabber_send(jm->js, message);

	purple_xmlnode_free(message);
}

/*
 * Compare the XHTML and plain strings passed in for "equality". Any HTML markup
 * other than <br/> (matches a newline) in the XHTML will cause this to return
 * FALSE.
 */
static gboolean
jabber_xhtml_plain_equal(const char *xhtml_escaped,
                         const char *plain)
{
	int i = 0;
	int j = 0;
	gboolean ret;
	char *xhtml = purple_unescape_html(xhtml_escaped);

	while (xhtml[i] && plain[j]) {
		if (xhtml[i] == plain[j]) {
			i += 1;
			j += 1;
			continue;
		}

		if (plain[j] == '\n' && !strncmp(xhtml+i, "<br/>", 5)) {
			i += 5;
			j += 1;
			continue;
		}

		g_free(xhtml);
		return FALSE;
	}

	/* Are we at the end of both strings? */
	ret = (xhtml[i] == plain[j]) && (xhtml[i] == '\0');
	g_free(xhtml);
	return ret;
}

int jabber_message_send_im(PurpleConnection *gc, PurpleMessage *msg)
{
	JabberMessage *jm;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *xhtml;
	char *tmp;
	char *resource;
	const gchar *who = purple_message_get_who(msg);

	if (!who || purple_message_is_empty(msg))
		return 0;

	resource = jabber_get_resource(who);

	jb = jabber_buddy_find(purple_connection_get_protocol_data(gc), who, TRUE);
	jbr = jabber_buddy_find_resource(jb, resource);

	g_free(resource);

	jm = g_new0(JabberMessage, 1);
	jm->js = purple_connection_get_protocol_data(gc);
	jm->type = JABBER_MESSAGE_CHAT;
	jm->chat_state = JM_STATE_ACTIVE;
	jm->to = g_strdup(who);
	jm->id = jabber_get_next_id(jm->js);

	if(jbr) {
		if(jbr->thread_id)
			jm->thread_id = jbr->thread_id;

		if (jbr->chat_states == JABBER_CHAT_STATES_UNSUPPORTED)
			jm->chat_state = JM_STATE_NONE;
		else {
			/* if(JABBER_CHAT_STATES_UNKNOWN == jbr->chat_states)
			   jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED; */
		}
	}

	tmp = purple_utf8_strip_unprintables(purple_message_get_contents(msg));
	purple_markup_html_to_xhtml(tmp, &xhtml, &jm->body);
	g_free(tmp);

	tmp = jabber_message_smileyfy_xhtml(jm, xhtml);
	if (tmp) {
		g_free(xhtml);
		xhtml = tmp;
	}

	/*
	 * For backward compatibility with user expectations or for those not on
	 * the user's roster, allow sending XHTML-IM markup.
	 */
	if (!jbr || !jbr->caps.info ||
			jabber_resource_has_capability(jbr, NS_XHTML_IM)) {
		if (!jabber_xhtml_plain_equal(xhtml, jm->body))
			/* Wrap the message in <p/> for great interoperability justice. */
			jm->xhtml = g_strdup_printf("<html xmlns='" NS_XHTML_IM "'><body xmlns='" NS_XHTML "'><p>%s</p></body></html>", xhtml);
	}

	g_free(xhtml);

	jabber_message_send(jm);
	jabber_message_free(jm);
	return 1;
}

int jabber_message_send_chat(PurpleConnection *gc, int id, PurpleMessage *msg)
{
	JabberChat *chat;
	JabberMessage *jm;
	JabberStream *js;
	char *xhtml;
	char *tmp;

	if (!gc || purple_message_is_empty(msg))
		return 0;

	js = purple_connection_get_protocol_data(gc);
	chat = jabber_chat_find_by_id(js, id);

	if(!chat)
		return 0;

	jm = g_new0(JabberMessage, 1);
	jm->js = purple_connection_get_protocol_data(gc);
	jm->type = JABBER_MESSAGE_GROUPCHAT;
	jm->to = g_strdup_printf("%s@%s", chat->room, chat->server);
	jm->id = jabber_get_next_id(jm->js);

	tmp = purple_utf8_strip_unprintables(purple_message_get_contents(msg));
	purple_markup_html_to_xhtml(tmp, &xhtml, &jm->body);
	g_free(tmp);
	tmp = jabber_message_smileyfy_xhtml(jm, xhtml);
	if (tmp) {
		g_free(xhtml);
		xhtml = tmp;
	}

	if (chat->xhtml && !jabber_xhtml_plain_equal(xhtml, jm->body))
		/* Wrap the message in <p/> for greater interoperability justice. */
		jm->xhtml = g_strdup_printf("<html xmlns='" NS_XHTML_IM "'><body xmlns='" NS_XHTML "'><p>%s</p></body></html>", xhtml);

	g_free(xhtml);

	jabber_message_send(jm);
	jabber_message_free(jm);

	return 1;
}

unsigned int jabber_send_typing(PurpleConnection *gc, const char *who, PurpleIMTypingState state)
{
	JabberStream *js;
	JabberMessage *jm;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	char *resource;

	js = purple_connection_get_protocol_data(gc);
	jb = jabber_buddy_find(js, who, TRUE);
	if (!jb)
		return 0;

	resource = jabber_get_resource(who);
	jbr = jabber_buddy_find_resource(jb, resource);
	g_free(resource);

	/* We know this entity doesn't support chat states */
	if (jbr && jbr->chat_states == JABBER_CHAT_STATES_UNSUPPORTED)
		return 0;

	/* *If* we don't have presence /and/ the buddy can't see our
	 * presence, don't send typing notifications.
	 */
	if (!jbr && !(jb->subscription & JABBER_SUB_FROM))
		return 0;

	/* TODO: figure out threading */
	jm = g_new0(JabberMessage, 1);
	jm->js = js;
	jm->type = JABBER_MESSAGE_CHAT;
	jm->to = g_strdup(who);
	jm->id = jabber_get_next_id(jm->js);

	if(PURPLE_IM_TYPING == state)
		jm->chat_state = JM_STATE_COMPOSING;
	else if(PURPLE_IM_TYPED == state)
		jm->chat_state = JM_STATE_PAUSED;
	else
		jm->chat_state = JM_STATE_ACTIVE;

	/* if(JABBER_CHAT_STATES_UNKNOWN == jbr->chat_states)
		jbr->chat_states = JABBER_CHAT_STATES_UNSUPPORTED; */

	jabber_message_send(jm);
	jabber_message_free(jm);

	return 0;
}

gboolean jabber_buzz_isenabled(JabberStream *js, const gchar *namespace) {
	return js->allowBuzz;
}

gboolean jabber_custom_smileys_isenabled(JabberStream *js, const gchar *namespace)
{
	const PurpleConnection *gc = js->gc;
	PurpleAccount *account = purple_connection_get_account(gc);

	return purple_account_get_bool(account, "custom_smileys", TRUE);
}
