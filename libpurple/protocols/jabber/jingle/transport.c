/**
 * @file transport.c
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"

#include "transport.h"
#include "jingle.h"
#include "debug.h"

#include <string.h>

struct _JingleTransportPrivate
{
	void *dummy;
};

#define JINGLE_TRANSPORT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), JINGLE_TYPE_TRANSPORT, JingleTransportPrivate))

static void jingle_transport_class_init (JingleTransportClass *klass);
static void jingle_transport_init (JingleTransport *transport);
static void jingle_transport_finalize (GObject *object);
static void jingle_transport_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void jingle_transport_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
JingleTransport *jingle_transport_parse_internal(PurpleXmlNode *transport);
PurpleXmlNode *jingle_transport_to_xml_internal(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action);
static void jingle_transport_add_local_candidate_internal(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate);
static GList *jingle_transport_get_remote_candidates_internal(JingleTransport *transport);

static GObjectClass *parent_class = NULL;

enum {
	PROP_0,
};

PURPLE_DEFINE_TYPE(JingleTransport, jingle_transport, G_TYPE_OBJECT);

static void
jingle_transport_class_init (JingleTransportClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = jingle_transport_finalize;
	gobject_class->set_property = jingle_transport_set_property;
	gobject_class->get_property = jingle_transport_get_property;
	klass->to_xml = jingle_transport_to_xml_internal;
	klass->parse = jingle_transport_parse_internal;
	klass->add_local_candidate = jingle_transport_add_local_candidate_internal;
	klass->get_remote_candidates = jingle_transport_get_remote_candidates_internal;

	g_type_class_add_private(klass, sizeof(JingleTransportPrivate));
}

static void
jingle_transport_init (JingleTransport *transport)
{
	transport->priv = JINGLE_TRANSPORT_GET_PRIVATE(transport);
	transport->priv->dummy = NULL;
}

static void
jingle_transport_finalize (GObject *transport)
{
	/* JingleTransportPrivate *priv = JINGLE_TRANSPORT_GET_PRIVATE(transport); */
	purple_debug_info("jingle","jingle_transport_finalize\n");

	parent_class->finalize(transport);
}

static void
jingle_transport_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(JINGLE_IS_TRANSPORT(object));

	switch (prop_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_transport_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(JINGLE_IS_TRANSPORT(object));

	switch (prop_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

JingleTransport *
jingle_transport_create(const gchar *type)
{
	return g_object_new(jingle_get_type(type), NULL);
}

const gchar *
jingle_transport_get_transport_type(JingleTransport *transport)
{
	return JINGLE_TRANSPORT_GET_CLASS(transport)->transport_type;
}

void
jingle_transport_add_local_candidate(JingleTransport *transport, const gchar *id,
                                     guint generation, PurpleMediaCandidate *candidate)
{
	JINGLE_TRANSPORT_GET_CLASS(transport)->add_local_candidate(transport, id,
	                                                           generation, candidate);
}

void
jingle_transport_add_local_candidate_internal(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate)
{
	/* Nothing to do */
}

GList *
jingle_transport_get_remote_candidates(JingleTransport *transport)
{
	return JINGLE_TRANSPORT_GET_CLASS(transport)->get_remote_candidates(transport);
}

static GList *
jingle_transport_get_remote_candidates_internal(JingleTransport *transport)
{
	return NULL;
}

JingleTransport *
jingle_transport_parse_internal(PurpleXmlNode *transport)
{
	const gchar *type = purple_xmlnode_get_namespace(transport);
	return jingle_transport_create(type);
}

PurpleXmlNode *
jingle_transport_to_xml_internal(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action)
{
	PurpleXmlNode *node = purple_xmlnode_new_child(content, "transport");
	purple_xmlnode_set_namespace(node, jingle_transport_get_transport_type(transport));
	return node;
}

JingleTransport *
jingle_transport_parse(PurpleXmlNode *transport)
{
	const gchar *type_name = purple_xmlnode_get_namespace(transport);
	GType type = jingle_get_type(type_name);
	if (type == G_TYPE_NONE)
		return NULL;
	
	return JINGLE_TRANSPORT_CLASS(g_type_class_ref(type))->parse(transport);
}

PurpleXmlNode *
jingle_transport_to_xml(JingleTransport *transport, PurpleXmlNode *content, JingleActionType action)
{
	g_return_val_if_fail(transport != NULL, NULL);
	g_return_val_if_fail(JINGLE_IS_TRANSPORT(transport), NULL);
	return JINGLE_TRANSPORT_GET_CLASS(transport)->to_xml(transport, content, action);
}

