/*
 * purple - Jabber XML parser stuff
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

#include <libxml/parser.h>

#include "connection.h"
#include "debug.h"
#include "jabber.h"
#include "parser.h"
#include "util.h"
#include "xmlnode.h"

static void
jabber_parser_element_start_libxml(void *user_data,
				   const xmlChar *element_name, const xmlChar *prefix, const xmlChar *namespace,
				   int nb_namespaces, const xmlChar **namespaces,
				   int nb_attributes, int nb_defaulted, const xmlChar **attributes)
{
	JabberStream *js = user_data;
	xmlnode *node;
	int i;

	if(!element_name) {
		return;
	} else if(!xmlStrcmp(element_name, (xmlChar*) "stream")) {
		js->protocol_version = JABBER_PROTO_0_9;
		for(i=0; i < nb_attributes * 5; i += 5) {
			int attrib_len = attributes[i+4] - attributes[i+3];
			char *attrib = g_malloc(attrib_len + 1);
			memcpy(attrib, attributes[i+3], attrib_len);
			attrib[attrib_len] = '\0';

			if(!xmlStrcmp(attributes[i], (xmlChar*) "version")
					&& !strcmp(attrib, "1.0")) {
				js->protocol_version = JABBER_PROTO_1_0;
				g_free(attrib);
			} else if(!xmlStrcmp(attributes[i], (xmlChar*) "id")) {
				g_free(js->stream_id);
				js->stream_id = attrib;
			} else {
				g_free(attrib);	
			}
		}
		if(js->protocol_version == JABBER_PROTO_0_9)
			js->auth_type = JABBER_AUTH_IQ_AUTH;

		if(js->state == JABBER_STREAM_INITIALIZING || js->state == JABBER_STREAM_INITIALIZING_ENCRYPTION)
			jabber_stream_set_state(js, JABBER_STREAM_AUTHENTICATING);
	} else {

		if(js->current)
			node = xmlnode_new_child(js->current, (const char*) element_name);
		else
			node = xmlnode_new((const char*) element_name);
		xmlnode_set_namespace(node, (const char*) namespace);

		for(i=0; i < nb_attributes * 5; i+=5) {
			char *txt;
			int attrib_len = attributes[i+4] - attributes[i+3];
			char *attrib = g_malloc(attrib_len + 1);
			char *attrib_ns = NULL;

			if (attributes[i+2]) {
				attrib_ns = g_strdup((char*)attributes[i+2]);
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

		js->current = node;
	}
}

static void
jabber_parser_element_end_libxml(void *user_data, const xmlChar *element_name,
				 const xmlChar *prefix, const xmlChar *namespace)
{
	JabberStream *js = user_data;

	if(!js->current)
		return;

	if(js->current->parent) {
		if(!xmlStrcmp((xmlChar*) js->current->name, element_name))
			js->current = js->current->parent;
	} else {
		xmlnode *packet = js->current;
		js->current = NULL;
		jabber_process_packet(js, &packet);
		if (packet != NULL)
			xmlnode_free(packet);
	}
}

static void
jabber_parser_element_text_libxml(void *user_data, const xmlChar *text, int text_len)
{
	JabberStream *js = user_data;

	if(!js->current)
		return;

	if(!text || !text_len)
		return;

	xmlnode_insert_data(js->current, (const char*) text, text_len);
}

static void
jabber_parser_structured_error_handler(void *user_data, xmlErrorPtr error)
{
	JabberStream *js = user_data;

	if (error->level == XML_ERR_WARNING && error->message != NULL
			&& strcmp(error->message, "xmlns: URI vcard-temp is not absolute\n") == 0)
		/*
		 * This message happens when parsing vcards, and is normal, so don't
		 * bother logging it because people scare easily.
		 */
		return;

	purple_debug_error("jabber", "XML parser error for JabberStream %p: "
								 "Domain %i, code %i, level %i: %s",
					   js,
					   error->domain, error->code, error->level,
					   (error->message ? error->message : "(null)\n"));
}

static xmlSAXHandler jabber_parser_libxml = {
	NULL,									/*internalSubset*/
	NULL,									/*isStandalone*/
	NULL,									/*hasInternalSubset*/
	NULL,									/*hasExternalSubset*/
	NULL,									/*resolveEntity*/
	NULL,									/*getEntity*/
	NULL,									/*entityDecl*/
	NULL,									/*notationDecl*/
	NULL,									/*attributeDecl*/
	NULL,									/*elementDecl*/
	NULL,									/*unparsedEntityDecl*/
	NULL,									/*setDocumentLocator*/
	NULL,									/*startDocument*/
	NULL,									/*endDocument*/
	NULL,									/*startElement*/
	NULL,									/*endElement*/
	NULL,									/*reference*/
	jabber_parser_element_text_libxml,		/*characters*/
	NULL,									/*ignorableWhitespace*/
	NULL,									/*processingInstruction*/
	NULL,									/*comment*/
	NULL,									/*warning*/
	NULL,									/*error*/
	NULL,									/*fatalError*/
	NULL,									/*getParameterEntity*/
	NULL,									/*cdataBlock*/
	NULL,									/*externalSubset*/
	XML_SAX2_MAGIC,							/*initialized*/
	NULL,									/*_private*/
	jabber_parser_element_start_libxml,		/*startElementNs*/
	jabber_parser_element_end_libxml,		/*endElementNs*/
	jabber_parser_structured_error_handler	/*serror*/
};

void
jabber_parser_setup(JabberStream *js)
{
	/* This seems backwards, but it makes sense. The libxml code creates
	 * the parser context when you try to use it (this way, it can figure
	 * out the encoding at creation time. So, setting up the parser is
	 * just a matter of destroying any current parser. */
	jabber_parser_free(js);
}

void jabber_parser_free(JabberStream *js) {
	if (js->context) {
		xmlParseChunk(js->context, NULL,0,1);
		xmlFreeParserCtxt(js->context);
		js->context = NULL;
	}
}

void jabber_parser_process(JabberStream *js, const char *buf, int len)
{
	int ret;

	if (js->context == NULL) {
		/* libxml inconsistently starts parsing on creating the
		 * parser, so do a ParseChunk right afterwards to force it. */
		js->context = xmlCreatePushParserCtxt(&jabber_parser_libxml, js, buf, len, NULL);
		xmlParseChunk(js->context, "", 0, 0);
	} else if ((ret = xmlParseChunk(js->context, buf, len, 0)) != XML_ERR_OK) {
		xmlError *err = xmlCtxtGetLastError(js->context);

		purple_debug_error("jabber", "xmlParseChunk returned error %i\n", ret);

		if (err->level == XML_ERR_FATAL) {
			purple_connection_error_reason (js->gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("XML Parse error"));
		}
	}
}

