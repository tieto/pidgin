/**
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
 */

#ifndef PURPLE_JABBER_GOOGLE_H_
#define PURPLE_JABBER_GOOGLE_H_

/* This is a place for Google Talk-specific XMPP extensions to live
 * such that they don't intermingle with code for the XMPP RFCs and XEPs :) */

#include "jabber.h"

#define GOOGLE_GROUPCHAT_SERVER "groupchat.google.com"

void jabber_gmail_init(JabberStream *js);
void jabber_gmail_poke(JabberStream *js, const char *from, JabberIqType type,
                       const char *id, xmlnode *new_mail);

void jabber_google_roster_init(JabberStream *js);
void jabber_google_roster_outgoing(JabberStream *js, xmlnode *query, xmlnode *item);

/* Returns FALSE if this should short-circuit processing of this roster item, or TRUE
 * if this roster item should continue to be processed
 */
gboolean jabber_google_roster_incoming(JabberStream *js, xmlnode *item);

void jabber_google_presence_incoming(JabberStream *js, const char *who, JabberBuddyResource *jbr);
char *jabber_google_presence_outgoing(PurpleStatus *tune);

void jabber_google_roster_add_deny(PurpleConnection *gc, const char *who);
void jabber_google_roster_rem_deny(PurpleConnection *gc, const char *who);

char *jabber_google_format_to_html(const char *text);

gboolean jabber_google_session_initiate(JabberStream *js, const gchar *who, PurpleMediaSessionType type);
void jabber_google_session_parse(JabberStream *js, const char *from, JabberIqType type, const char *iq, xmlnode *session);

void jabber_google_handle_jingle_info(JabberStream *js, const char *from,
                                      JabberIqType type, const char *id,
                                      xmlnode *child);
void jabber_google_send_jingle_info(JabberStream *js);

void google_buddy_node_chat(PurpleBlistNode *node, gpointer data);

#endif   /* PURPLE_JABBER_GOOGLE_H_ */
