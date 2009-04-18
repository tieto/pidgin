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

#include "jabber.h"
#include "ping.h"
#include "iq.h"

static void jabber_keepalive_pong_cb(JabberStream *js)
{
	purple_timeout_remove(js->keepalive_timeout);
	js->keepalive_timeout = -1;
}

void
jabber_ping_parse(JabberStream *js, const char *from,
                  JabberIqType type, const char *id, xmlnode *ping)
{
	if (type == JABBER_IQ_GET) {
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_RESULT);

		if (from)
			xmlnode_set_attrib(iq->node, "to", from);
		xmlnode_set_attrib(iq->node, "id", id);

		jabber_iq_send(iq);
	} else if (type == JABBER_IQ_SET) {
		/* XXX: error */
	}
}

static void jabber_ping_result_cb(JabberStream *js, const char *from,
                                  JabberIqType type, const char *id,
                                  xmlnode *packet, gpointer data)
{
	if (purple_strequal(from, js->user->domain))
		/* If the pong is from the server, assume it's a result of the
		 * keepalive functions */
		jabber_keepalive_pong_cb(js);

	if (type == JABBER_IQ_RESULT) {
		purple_debug_info("jabber", "PONG!\n");
	} else {
		purple_debug_info("jabber", "(not supported)\n");
	}
}

gboolean jabber_ping_jid(JabberStream *js, const char *jid)
{
	JabberIq *iq;
	xmlnode *ping;

	iq = jabber_iq_new(js, JABBER_IQ_GET);
	if (jid)
		xmlnode_set_attrib(iq->node, "to", jid);

	ping = xmlnode_new_child(iq->node, "ping");
	xmlnode_set_namespace(ping, "urn:xmpp:ping");

	jabber_iq_set_callback(iq, jabber_ping_result_cb, NULL);
	jabber_iq_send(iq);

	return TRUE;
}
