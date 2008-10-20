/**
 * @file rawudp.c
 *
 * purple
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

#include "rawudp.h"
#include "jingle.h"
#include "debug.h"

#include <string.h>

struct _JingleRawUdpPrivate
{
	guint generation;
	gchar *id;
	gchar *ip;
	guint port;
};

#define JINGLE_RAWUDP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), JINGLE_TYPE_RAWUDP, JingleRawUdpPrivate))

static void jingle_rawudp_class_init (JingleRawUdpClass *klass);
static void jingle_rawudp_init (JingleRawUdp *rawudp);
static void jingle_rawudp_finalize (GObject *object);
static void jingle_rawudp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void jingle_rawudp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static JingleTransport *jingle_rawudp_parse_internal(xmlnode *rawudp);
static xmlnode *jingle_rawudp_to_xml_internal(JingleTransport *transport, xmlnode *content, JingleActionType action);

static JingleTransportClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_GENERATION,
	PROP_ID,
	PROP_IP,
	PROP_PORT,
};

GType
jingle_rawudp_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(JingleRawUdpClass),
			NULL,
			NULL,
			(GClassInitFunc) jingle_rawudp_class_init,
			NULL,
			NULL,
			sizeof(JingleRawUdp),
			0,
			(GInstanceInitFunc) jingle_rawudp_init,
			NULL
		};
		type = g_type_register_static(JINGLE_TYPE_TRANSPORT, "JingleRawUdp", &info, 0);
	}
	return type;
}

static void
jingle_rawudp_class_init (JingleRawUdpClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = jingle_rawudp_finalize;
	gobject_class->set_property = jingle_rawudp_set_property;
	gobject_class->get_property = jingle_rawudp_get_property;
	klass->parent_class.to_xml = jingle_rawudp_to_xml_internal;
	klass->parent_class.parse = jingle_rawudp_parse_internal;
	klass->parent_class.transport_type = JINGLE_TRANSPORT_RAWUDP;

	g_object_class_install_property(gobject_class, PROP_GENERATION,
			g_param_spec_uint("generation",
			"Generation",
			"The generation for this transport.",
			0,
			G_MAXUINT,
			0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ID,
			g_param_spec_string("id",
			"Id",
			"The id for this transport.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_IP,
			g_param_spec_string("ip",
			"IP Address",
			"The IP address for this transport.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PORT,
			g_param_spec_uint("port",
			"Port",
			"The port for this transport.",
			0,
			65535,
			0,
			G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(JingleRawUdpPrivate));
}

static void
jingle_rawudp_init (JingleRawUdp *rawudp)
{
	rawudp->priv = JINGLE_RAWUDP_GET_PRIVATE(rawudp);
	memset(rawudp->priv, 0, sizeof(rawudp->priv));
}

static void
jingle_rawudp_finalize (GObject *rawudp)
{
	JingleRawUdpPrivate *priv = JINGLE_RAWUDP_GET_PRIVATE(rawudp);
	purple_debug_info("jingle","jingle_rawudp_finalize\n");

	g_free(priv->id);
	g_free(priv->ip);
}

static void
jingle_rawudp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	JingleRawUdp *rawudp;
	g_return_if_fail(JINGLE_IS_RAWUDP(object));

	rawudp = JINGLE_RAWUDP(object);

	switch (prop_id) {
		case PROP_GENERATION:
			rawudp->priv->generation = g_value_get_uint(value);
			break;
		case PROP_ID:
			g_free(rawudp->priv->id);
			rawudp->priv->id = g_value_dup_string(value);
			break;
		case PROP_IP:
			g_free(rawudp->priv->ip);
			rawudp->priv->ip = g_value_dup_string(value);
			break;
		case PROP_PORT:
			rawudp->priv->port = g_value_get_uint(value);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_rawudp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	JingleRawUdp *rawudp;
	g_return_if_fail(JINGLE_IS_RAWUDP(object));
	
	rawudp = JINGLE_RAWUDP(object);

	switch (prop_id) {
		case PROP_GENERATION:
			g_value_set_uint(value, rawudp->priv->generation);
			break;
		case PROP_ID:
			g_value_set_string(value, rawudp->priv->id);
			break;
		case PROP_IP:
			g_value_set_string(value, rawudp->priv->ip);
			break;
		case PROP_PORT:
			g_value_set_uint(value, rawudp->priv->port);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);	
			break;
	}
}

JingleRawUdp *
jingle_rawudp_create(guint generation, const gchar *id, const gchar *ip, guint port)
{
	return g_object_new(jingle_rawudp_get_type(),
			"generation", generation,
			"id", id,
			"ip", ip,
			"port", port, NULL);
}

static JingleTransport *
jingle_rawudp_parse_internal(xmlnode *rawudp)
{
	JingleTransport *transport = parent_class->parse(rawudp);
	
	return transport;
}

static xmlnode *
jingle_rawudp_to_xml_internal(JingleTransport *transport, xmlnode *content, JingleActionType action)
{
	xmlnode *node = parent_class->to_xml(transport, content, action);

	if (action == JINGLE_SESSION_INITIATE || action == JINGLE_TRANSPORT_INFO) {
		xmlnode *xmltransport = xmlnode_new_child(node, "candidate");
		JingleRawUdpPrivate *priv = JINGLE_RAWUDP_GET_PRIVATE(transport);
		gchar *generation = g_strdup_printf("%d", priv->generation);
		gchar *port = g_strdup_printf("%d", priv->port);

		xmlnode_set_attrib(xmltransport, "generation", generation);
		xmlnode_set_attrib(xmltransport, "id", priv->id);
		xmlnode_set_attrib(xmltransport, "ip", priv->ip);
		xmlnode_set_attrib(xmltransport, "port", port);

		g_free(port);
		g_free(generation);
	}

	return node;
}

