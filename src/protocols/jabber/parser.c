/*
 * gaim - Jabber XML parser stuff
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"

#include "connection.h"

#include "jabber.h"
#include "parser.h"
#include "xmlnode.h"

static void
jabber_parser_element_start(GMarkupParseContext *context,
		const char *element_name, const char **attrib_names,
		const char **attrib_values, gpointer user_data, GError **error)
{
	JabberStream *js = user_data;
	xmlnode *node;
	int i;

	if(!element_name) {
		return;
	} else if(!strcmp(element_name, "stream:stream")) {
		js->protocol_version = JABBER_PROTO_0_9;
		for(i=0; attrib_names[i]; i++) {
			if(!strcmp(attrib_names[i], "version")
					&& !strcmp(attrib_values[i], "1.0")) {
				js->protocol_version = JABBER_PROTO_1_0;
			} else if(!strcmp(attrib_names[i], "id")) {
				if(js->stream_id)
					g_free(js->stream_id);
				js->stream_id = g_strdup(attrib_values[i]);
			}
		}
		if(js->protocol_version == JABBER_PROTO_0_9)
			js->auth_type = JABBER_AUTH_IQ_AUTH;

		if(js->state == JABBER_STREAM_INITIALIZING)
			jabber_stream_set_state(js, JABBER_STREAM_AUTHENTICATING);
	} else {

		if(js->current)
			node = xmlnode_new_child(js->current, element_name);
		else
			node = xmlnode_new(element_name);

		for(i=0; attrib_names[i]; i++) {
			xmlnode_set_attrib(node, attrib_names[i], attrib_values[i]);
		}

		js->current = node;
	}
}

static void
jabber_parser_element_end(GMarkupParseContext *context,
		const char *element_name, gpointer user_data, GError **error)
{
	JabberStream *js = user_data;

	if(!js->current)
		return;

	if(js->current->parent) {
		if(!strcmp(js->current->name, element_name))
			js->current = js->current->parent;
	} else {
		xmlnode *packet = js->current;
		js->current = NULL;
		jabber_process_packet(js, packet);
		xmlnode_free(packet);
	}
}

static void
jabber_parser_element_text(GMarkupParseContext *context, const char *text,
		gsize text_len, gpointer user_data, GError **error)
{
	JabberStream *js = user_data;

	if(!js->current)
		return;

	if(!text || !text_len)
		return;

	xmlnode_insert_data(js->current, text, text_len);
}

static GMarkupParser jabber_parser = {
	jabber_parser_element_start,
	jabber_parser_element_end,
	jabber_parser_element_text,
	NULL,
	NULL
};

void
jabber_parser_setup(JabberStream *js)
{
	if(!js->context)
		js->context = g_markup_parse_context_new(&jabber_parser, 0, js, NULL);
}


void jabber_parser_process(JabberStream *js, const char *buf, int len)
{

	/* May need to check for other encodings and do the conversion here */

	if(!g_markup_parse_context_parse(js->context, buf, len, NULL)) {
		g_markup_parse_context_free(js->context);
		js->context = NULL;
		gaim_connection_error(js->gc, _("XML Parse error"));
	}
}

