/*
 * purple - Bonjour Jabber XML parser stuff
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

#include <libxml/parser.h>

#include "connection.h"
#include "debug.h"
#include "jabber.h"
#include "parser.h"
#include "util.h"
#include "xmlnode.h"

static void
bonjour_parser_element_start_libxml(void *user_data,
				   const xmlChar *element_name, const xmlChar *prefix, const xmlChar *namespace,
				   int nb_namespaces, const xmlChar **namespaces,
				   int nb_attributes, int nb_defaulted, const xmlChar **attributes)
{
	PurpleBuddy *pb = user_data;
	BonjourBuddy *bb = pb->proto_data;
	BonjourJabberConversation *bconv = bb->conversation;

	xmlnode *node;
	int i;

	if(!element_name) {
		return;
	} else if(!xmlStrcmp(element_name, (xmlChar*) "stream")) {
		bconv->recv_stream_start = TRUE;
		bonjour_jabber_stream_started(pb);
	} else {

		if(bconv->current)
			node = xmlnode_new_child(bconv->current, (const char*) element_name);
		else
			node = xmlnode_new((const char*) element_name);
		xmlnode_set_namespace(node, (const char*) namespace);

		for(i=0; i < nb_attributes * 5; i+=5) {
			char *txt;
			int attrib_len = attributes[i+4] - attributes[i+3];
			char *attrib = g_malloc(attrib_len + 1);
			char *attrib_ns = NULL;

			if (attributes[i+2]) {
				attrib_ns = g_strdup((char*)attributes[i+2]);;
			}

			memcpy(attrib, attributes[i+3], attrib_len);
			attrib[attrib_len] = '\0';

			txt = attrib;
			attrib = purple_unescape_html(txt);
			g_free(txt);
			xmlnode_set_attrib_with_namespace(node, (const char*) attributes[i], attrib_ns, attrib);
			g_free(attrib);
			g_free(attrib_ns);
		}

		bconv->current = node;
	}
}

static gboolean _async_bonjour_jabber_stream_ended_cb(gpointer data) {
	bonjour_jabber_stream_ended((PurpleBuddy *) data);
	return FALSE;
}

static void
bonjour_parser_element_end_libxml(void *user_data, const xmlChar *element_name,
				 const xmlChar *prefix, const xmlChar *namespace)
{
	PurpleBuddy *pb = user_data;
	BonjourBuddy *bb = pb->proto_data;
	BonjourJabberConversation *bconv = bb->conversation;

	if(!bconv->current) {
		/* We don't keep a reference to the start stream xmlnode,
		 * so we have to check for it here to close the conversation */
		if(!xmlStrcmp(element_name, (xmlChar*) "stream")) {
			/* Asynchronously close the conversation to prevent bonjour_parser_setup()
			 * being called from within this context */
			g_idle_add(_async_bonjour_jabber_stream_ended_cb, pb);
		}
		return;
	}

	if(bconv->current->parent) {
		if(!xmlStrcmp((xmlChar*) bconv->current->name, element_name))
			bconv->current = bconv->current->parent;
	} else {
		xmlnode *packet = bconv->current;
		bconv->current = NULL;
		bonjour_jabber_process_packet(pb, packet);
		xmlnode_free(packet);
	}
}

static void
bonjour_parser_element_text_libxml(void *user_data, const xmlChar *text, int text_len)
{
	PurpleBuddy *pb = user_data;
	BonjourBuddy *bb = pb->proto_data;
	BonjourJabberConversation *bconv = bb->conversation;

	if(!bconv->current)
		return;

	if(!text || !text_len)
		return;

	xmlnode_insert_data(bconv->current, (const char*) text, text_len);
}

static xmlSAXHandler bonjour_parser_libxml = {
	.internalSubset         = NULL,
	.isStandalone           = NULL,
	.hasInternalSubset      = NULL,
	.hasExternalSubset      = NULL,
	.resolveEntity          = NULL,
	.getEntity              = NULL,
	.entityDecl             = NULL,
	.notationDecl           = NULL,
	.attributeDecl          = NULL,
	.elementDecl            = NULL,
	.unparsedEntityDecl     = NULL,
	.setDocumentLocator     = NULL,
	.startDocument          = NULL,
	.endDocument            = NULL,
	.startElement           = NULL,
	.endElement             = NULL,
	.reference              = NULL,
	.characters             = bonjour_parser_element_text_libxml,
	.ignorableWhitespace    = NULL,
	.processingInstruction  = NULL,
	.comment                = NULL,
	.warning                = NULL,
	.error                  = NULL,
	.fatalError             = NULL,
	.getParameterEntity     = NULL,
	.cdataBlock             = NULL,
	.externalSubset         = NULL,
	.initialized            = XML_SAX2_MAGIC,
	._private               = NULL,
	.startElementNs         = bonjour_parser_element_start_libxml,
	.endElementNs           = bonjour_parser_element_end_libxml,
	.serror                 = NULL
};

void
bonjour_parser_setup(BonjourJabberConversation *bconv)
{

	/* This seems backwards, but it makes sense. The libxml code creates
	 * the parser context when you try to use it (this way, it can figure
	 * out the encoding at creation time. So, setting up the parser is
	 * just a matter of destroying any current parser. */
	if (bconv->context) {
		xmlParseChunk(bconv->context, NULL,0,1);
		xmlFreeParserCtxt(bconv->context);
		bconv->context = NULL;
	}
}


void bonjour_parser_process(PurpleBuddy *pb, const char *buf, int len)
{
	BonjourBuddy *bb = pb->proto_data;

	if (bb->conversation->context ==  NULL) {
		/* libxml inconsistently starts parsing on creating the
		 * parser, so do a ParseChunk right afterwards to force it. */
		bb->conversation->context = xmlCreatePushParserCtxt(&bonjour_parser_libxml, pb, buf, len, NULL);
		xmlParseChunk(bb->conversation->context, "", 0, 0);
	} else if (xmlParseChunk(bb->conversation->context, buf, len, 0) < 0) {
		/* TODO: What should we do here - I assume we should display an error or something (maybe just print something to the conv?) */
		purple_debug_error("bonjour", "Error parsing xml.\n");
	}
}

