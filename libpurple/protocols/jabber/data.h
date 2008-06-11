/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef JABBER_DATA_H
#define JABBER_DATA_H

#include "xmlnode.h"
#include "conversation.h"
#include "jabber.h"

#define XEP_0231_NAMESPACE "urn:xmpp:tmp:data-element"
#define XEP_0231_IB_IMAGE_NAMESPACE "urn:xmpp:tmp:data-element:inband-image"

#include <glib.h>

typedef struct {
	char *cid;
	char *alt;
	char *type;
	gsize size;
	gpointer data;
} JabberData;

void jabber_data_init(void);

/* creates a JabberData instance from raw data */
JabberData *jabber_data_create_from_data(gconstpointer data, gsize size,
										 const char *type, const char *alt);

/* create a JabberData instance from an XML "data" element (as defined by
  XEP 0231 */
JabberData *jabber_data_create_from_xml(xmlnode *tag);

void jabber_data_delete(JabberData *data);

const char *jabber_data_get_cid(const JabberData *data);
const char *jabber_data_get_alt(const JabberData *data);
const char *jabber_data_get_type(const JabberData *data);

gsize jabber_data_get_size(const JabberData *data);
gpointer jabber_data_get_data(const JabberData *data);

/* returns the XML definition for the data element */
xmlnode *jabber_data_get_xml_definition(const JabberData *data);

/* returns an XHTML-IM "img" tag given a data instance */
xmlnode *jabber_data_get_xhtml_im(const JabberData *data);

/* returns a data request element (to be included in an iq stanza) for requesting
  data */
xmlnode *jabber_data_get_xml_request(const gchar *cid);

/* lookup functions */
const JabberData *jabber_data_find_local_by_alt(const PurpleConversation *conv,
												const char *alt);
const JabberData *jabber_data_find_local_by_cid(const PurpleConversation *conv,
												const char *cid);
const JabberData *jabber_data_find_remote_by_cid(const PurpleConversation *conv,
												 const char *cid);

/* associate data objects with a conversation */
void jabber_data_associate_local_with_conv(JabberData *data, PurpleConversation *conv);
void jabber_data_associate_remote_with_conv(JabberData *data, PurpleConversation *conv);
void jabber_data_delete_associated_with_conv(PurpleConversation *conv);

/* handles iq requests */
void jabber_data_parse(JabberStream *js, xmlnode *packet);


#endif /* JABBER_DATA_H */
