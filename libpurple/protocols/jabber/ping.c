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
 *
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "internal.h"

#include "debug.h"
#include "xmlnode.h"

#include "jabber.h"
#include "ping.h"
#include "iq.h"

void
jabber_ping_parse(JabberStream *js, xmlnode *packet)
{
	JabberIq *iq;

	purple_debug_info("jabber", "jabber_ping_parse\n");

	iq = jabber_iq_new(js, JABBER_IQ_RESULT);

	xmlnode_set_attrib(iq->node, "to", xmlnode_get_attrib(packet, "from") );

	jabber_iq_set_id(iq, xmlnode_get_attrib(packet, "id"));

	jabber_iq_send(iq);
}

static void jabber_ping_result_cb(JabberStream *js, xmlnode *packet,
		gpointer data)
{
	const char *type = xmlnode_get_attrib(packet, "type");

	purple_debug_info("jabber", "jabber_ping_result_cb\n");
	if(type && !strcmp(type, "result")) {
		purple_debug_info("jabber", "PONG!\n");
	} else {
		purple_debug_info("jabber", "(not supported)\n");
	}
}

gboolean jabber_ping_jid(PurpleConversation *conv, const char *jid)
{
	JabberIq *iq;
	xmlnode *ping;

	purple_debug_info("jabber", "jabber_ping_jid\n");

	iq = jabber_iq_new(conv->account->gc->proto_data, JABBER_IQ_GET);
	xmlnode_set_attrib(iq->node, "to", jid);

	ping = xmlnode_new_child(iq->node, "ping");
	xmlnode_set_namespace(ping, "urn:xmpp:ping");

	jabber_iq_set_callback(iq, jabber_ping_result_cb, NULL);
	jabber_iq_send(iq);



	return TRUE;
}
