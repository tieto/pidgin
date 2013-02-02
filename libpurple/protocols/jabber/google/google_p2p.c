/**
 * @file google_p2p.c
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

#include "google_p2p.h"
#include "jingle/jingle.h"
#include "debug.h"

#include <string.h>

struct _JingleGoogleP2PPrivate
{
	GList *local_candidates;
	GList *remote_candidates;
};

#define JINGLE_GOOGLE_P2P_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), JINGLE_TYPE_GOOGLE_P2P, JingleGoogleP2PPrivate))

static void jingle_google_p2p_class_init (JingleGoogleP2PClass *klass);
static void jingle_google_p2p_init (JingleGoogleP2P *google_p2p);
static void jingle_google_p2p_finalize (GObject *object);
static void jingle_google_p2p_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void jingle_google_p2p_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static JingleTransport *jingle_google_p2p_parse_internal(xmlnode *google_p2p);
static xmlnode *jingle_google_p2p_to_xml_internal(JingleTransport *transport, xmlnode *content, JingleActionType action);
static void jingle_google_p2p_add_local_candidate(JingleTransport *transport, const gchar *id, guint generation, PurpleMediaCandidate *candidate);
static GList *jingle_google_p2p_get_remote_candidates(JingleTransport *transport);

static JingleTransportClass *parent_class = NULL;

enum {
	PROP_0,
	PROP_LOCAL_CANDIDATES,
	PROP_REMOTE_CANDIDATES,
};

static JingleGoogleP2PCandidate *
jingle_google_p2p_candidate_copy(JingleGoogleP2PCandidate *candidate)
{
	JingleGoogleP2PCandidate *new_candidate = g_new0(JingleGoogleP2PCandidate, 1);
	new_candidate->id = g_strdup(candidate->id);
	new_candidate->address = g_strdup(candidate->address);
	new_candidate->port = candidate->port;
	new_candidate->preference = candidate->preference;
	new_candidate->type = g_strdup(candidate->type);
	new_candidate->protocol = g_strdup(candidate->protocol);
	new_candidate->username = g_strdup(candidate->username);
	new_candidate->password = g_strdup(candidate->password);
	new_candidate->generation = candidate->generation;

	new_candidate->rem_known = candidate->rem_known;

	return new_candidate;
}

static void
jingle_google_p2p_candidate_free(JingleGoogleP2PCandidate *candidate)
{
	g_free(candidate->id);
	g_free(candidate->address);
	g_free(candidate->type);
	g_free(candidate->protocol);
	g_free(candidate->username);
	g_free(candidate->password);
}

GType
jingle_google_p2p_candidate_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("JingleGoogleP2PCandidate",
				(GBoxedCopyFunc)jingle_google_p2p_candidate_copy,
				(GBoxedFreeFunc)jingle_google_p2p_candidate_free);
	}
	return type;
}

JingleGoogleP2PCandidate *
jingle_google_p2p_candidate_new(const gchar *id, guint generation,
		const gchar *address, guint port, guint preference,
		const gchar *type, const gchar *protocol,
		const gchar *username, const gchar *password)
{
	JingleGoogleP2PCandidate *candidate = g_new0(JingleGoogleP2PCandidate, 1);
	candidate->id = g_strdup(id);
	candidate->address = g_strdup(address);
	candidate->port = port;
	candidate->preference = preference;
	candidate->type = g_strdup(type);
	candidate->protocol = g_strdup(protocol);
	candidate->username = g_strdup(username);
	candidate->password = g_strdup(password);
	candidate->generation = generation;

	candidate->rem_known = FALSE;
	return candidate;
}

GType
jingle_google_p2p_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(JingleGoogleP2PClass),
			NULL,
			NULL,
			(GClassInitFunc) jingle_google_p2p_class_init,
			NULL,
			NULL,
			sizeof(JingleGoogleP2P),
			0,
			(GInstanceInitFunc) jingle_google_p2p_init,
			NULL
		};
		type = g_type_register_static(JINGLE_TYPE_TRANSPORT, "JingleGoogleP2P", &info, 0);
	}
	return type;
}

static void
jingle_google_p2p_class_init(JingleGoogleP2PClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass *)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = jingle_google_p2p_finalize;
	gobject_class->set_property = jingle_google_p2p_set_property;
	gobject_class->get_property = jingle_google_p2p_get_property;
	klass->parent_class.to_xml = jingle_google_p2p_to_xml_internal;
	klass->parent_class.parse = jingle_google_p2p_parse_internal;
	klass->parent_class.transport_type = NS_GOOGLE_TRANSPORT_P2P;
	klass->parent_class.add_local_candidate = jingle_google_p2p_add_local_candidate;
	klass->parent_class.get_remote_candidates = jingle_google_p2p_get_remote_candidates;

	g_object_class_install_property(gobject_class, PROP_LOCAL_CANDIDATES,
			g_param_spec_pointer("local-candidates",
			"Local candidates",
			"The local candidates for this transport.",
			G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_REMOTE_CANDIDATES,
			g_param_spec_pointer("remote-candidates",
			"Remote candidates",
			"The remote candidates for this transport.",
			G_PARAM_READABLE));

	g_type_class_add_private(klass, sizeof(JingleGoogleP2PPrivate));
}

static void
jingle_google_p2p_init(JingleGoogleP2P *google_p2p)
{
	google_p2p->priv = JINGLE_GOOGLE_P2P_GET_PRIVATE(google_p2p);
	google_p2p->priv->local_candidates = NULL;
	google_p2p->priv->remote_candidates = NULL;
}

static void
jingle_google_p2p_finalize(GObject *google_p2p)
{
/*	JingleGoogleP2PPrivate *priv = JINGLE_GOOGLE_P2P_GET_PRIVATE(google_p2p); */
	purple_debug_info("jingle","jingle_google_p2p_finalize\n");

	G_OBJECT_CLASS(parent_class)->finalize(google_p2p);
}

static void
jingle_google_p2p_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	JingleGoogleP2P *google_p2p;

	g_return_if_fail(object != NULL);
	g_return_if_fail(JINGLE_IS_GOOGLE_P2P(object));

	google_p2p = JINGLE_GOOGLE_P2P(object);

	switch (prop_id) {
		case PROP_LOCAL_CANDIDATES:
			google_p2p->priv->local_candidates = g_value_get_pointer(value);
			break;
		case PROP_REMOTE_CANDIDATES:
			google_p2p->priv->remote_candidates = g_value_get_pointer(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void
jingle_google_p2p_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	JingleGoogleP2P *google_p2p;

	g_return_if_fail(object != NULL);
	g_return_if_fail(JINGLE_IS_GOOGLE_P2P(object));

	google_p2p = JINGLE_GOOGLE_P2P(object);

	switch (prop_id) {
		case PROP_LOCAL_CANDIDATES:
			g_value_set_pointer(value, google_p2p->priv->local_candidates);
			break;
		case PROP_REMOTE_CANDIDATES:
			g_value_set_pointer(value, google_p2p->priv->remote_candidates);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_google_p2p_add_local_candidate(JingleTransport *transport, const gchar *id,
                                      guint generation, PurpleMediaCandidate *candidate)
{
	JingleGoogleP2P *google_p2p = JINGLE_GOOGLE_P2P(transport);
	JingleGoogleP2PCandidate *google_p2p_candidate;
	gchar *ip;
	gchar *username;
	gchar *password;
	PurpleMediaCandidateType type;
	PurpleMediaNetworkProtocol protocol;
	GList *iter;

	ip = purple_media_candidate_get_ip(candidate);
	username = purple_media_candidate_get_username(candidate);
	password = purple_media_candidate_get_password(candidate);
	type = purple_media_candidate_get_candidate_type(candidate);
	protocol = purple_media_candidate_get_protocol(candidate);

	google_p2p_candidate = jingle_google_p2p_candidate_new(id, generation,
			ip, purple_media_candidate_get_port(candidate),
			purple_media_candidate_get_priority(candidate),
			type == PURPLE_MEDIA_CANDIDATE_TYPE_HOST ? "host" :
			type == PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX ? "srflx" :
			type == PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX ? "prflx" :
			type == PURPLE_MEDIA_CANDIDATE_TYPE_RELAY ? "relay" :
			"",
			protocol == PURPLE_MEDIA_NETWORK_PROTOCOL_UDP ? "udp" : "tcp",
			username, password);

	g_free(password);
	g_free(username);
	g_free(ip);

	for (iter = google_p2p->priv->local_candidates; iter; iter = g_list_next(iter)) {
		JingleGoogleP2PCandidate *c = iter->data;
		if (!strcmp(c->id, id)) {
			generation = c->generation + 1;

			g_boxed_free(JINGLE_TYPE_GOOGLE_P2P_CANDIDATE, c);
			google_p2p->priv->local_candidates = g_list_delete_link(
					google_p2p->priv->local_candidates, iter);

			google_p2p_candidate->generation = generation;

			google_p2p->priv->local_candidates = g_list_append(
					google_p2p->priv->local_candidates, candidate);
			return;
		}
	}

	google_p2p->priv->local_candidates = g_list_append(
			google_p2p->priv->local_candidates, google_p2p_candidate);
}

static GList *
jingle_google_p2p_get_remote_candidates(JingleTransport *transport)
{
	JingleGoogleP2P *google_p2p = JINGLE_GOOGLE_P2P(transport);
	GList *candidates = google_p2p->priv->remote_candidates;
	GList *ret = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		JingleGoogleP2PCandidate *candidate = candidates->data;
		PurpleMediaCandidate *new_candidate = purple_media_candidate_new("", 0,
				!strcmp(candidate->type, "host") ?
					PURPLE_MEDIA_CANDIDATE_TYPE_HOST :
					!strcmp(candidate->type, "srflx") ?
						PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX :
						!strcmp(candidate->type, "prflx") ?
							PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX :
							!strcmp(candidate->type, "relay") ?
								PURPLE_MEDIA_CANDIDATE_TYPE_RELAY : 0,
				!strcmp(candidate->protocol, "udp") ?
					PURPLE_MEDIA_NETWORK_PROTOCOL_UDP :
					PURPLE_MEDIA_NETWORK_PROTOCOL_TCP,
				candidate->address, candidate->port);
		g_object_set(new_candidate,
		             "username", candidate->username,
		             "password", candidate->password,
		             "priority", candidate->preference,
		             NULL);
		ret = g_list_append(ret, new_candidate);
	}

	return ret;
}

static JingleGoogleP2PCandidate *
jingle_google_p2p_get_remote_candidate_by_id(JingleGoogleP2P *google_p2p,
		const gchar *id)
{
	GList *iter = google_p2p->priv->remote_candidates;
	for (; iter; iter = g_list_next(iter)) {
		JingleGoogleP2PCandidate *candidate = iter->data;
		if (!strcmp(candidate->id, id)) {
			return candidate;
		}
	}
	return NULL;
}

static void
jingle_google_p2p_add_remote_candidate(JingleGoogleP2P *google_p2p, JingleGoogleP2PCandidate *candidate)
{
	JingleGoogleP2PPrivate *priv = JINGLE_GOOGLE_P2P_GET_PRIVATE(google_p2p);
	JingleGoogleP2PCandidate *google_p2p_candidate =
			jingle_google_p2p_get_remote_candidate_by_id(google_p2p,
					candidate->id);
	if (google_p2p_candidate != NULL) {
		priv->remote_candidates = g_list_remove(priv->remote_candidates,
		                                        google_p2p_candidate);
		g_boxed_free(JINGLE_TYPE_GOOGLE_P2P_CANDIDATE, google_p2p_candidate);
	}
	priv->remote_candidates = g_list_append(priv->remote_candidates, candidate);
}

static JingleTransport *
jingle_google_p2p_parse_internal(xmlnode *google_p2p)
{
	JingleTransport *transport = parent_class->parse(google_p2p);
	xmlnode *candidate = xmlnode_get_child(google_p2p, "candidate");
	JingleGoogleP2PCandidate *google_p2p_candidate = NULL;

	for (; candidate; candidate = xmlnode_get_next_twin(candidate)) {
		const gchar *generation = xmlnode_get_attrib(candidate, "generation");
		const gchar *id = xmlnode_get_attrib(candidate, "name");
		const gchar *address = xmlnode_get_attrib(candidate, "address");
		const gchar *port = xmlnode_get_attrib(candidate, "port");
		const gchar *preference = xmlnode_get_attrib(candidate, "preference");
		const gchar *type = xmlnode_get_attrib(candidate, "type");
		const gchar *protocol = xmlnode_get_attrib(candidate, "protocol");
		const gchar *username = xmlnode_get_attrib(candidate, "username");
		const gchar *password = xmlnode_get_attrib(candidate, "password");

		if (!generation || !id || !address || !port || !preference ||
				!type || !protocol || !username || !password)
			continue;

		google_p2p_candidate = jingle_google_p2p_candidate_new(id,
				atoi(generation),
				address,
				atoi(port),
				atoi(preference),
				type,
				protocol,
				username, password);
		google_p2p_candidate->rem_known = TRUE;
		jingle_google_p2p_add_remote_candidate(JINGLE_GOOGLE_P2P(transport), google_p2p_candidate);
	}

	return transport;
}

static xmlnode *
jingle_google_p2p_to_xml_internal(JingleTransport *transport, xmlnode *content, JingleActionType action)
{
	xmlnode *node = parent_class->to_xml(transport, content, action);

	if (action == JINGLE_SESSION_INITIATE ||
			action == JINGLE_SESSION_ACCEPT ||
			action == JINGLE_TRANSPORT_INFO ||
			action == JINGLE_CONTENT_ADD ||
			action == JINGLE_TRANSPORT_REPLACE) {
		JingleGoogleP2PPrivate *priv = JINGLE_GOOGLE_P2P_GET_PRIVATE(transport);
		GList *iter = priv->local_candidates;

		for (; iter; iter = g_list_next(iter)) {
			JingleGoogleP2PCandidate *candidate = iter->data;
			xmlnode *xmltransport;
			gchar *generation, *network, *port, *preference;

			if (candidate->rem_known == TRUE)
				continue;

			candidate->rem_known = TRUE;

			xmltransport = xmlnode_new_child(node, "candidate");
			generation = g_strdup_printf("%d", candidate->generation);
			network = g_strdup_printf("%d", candidate->network);
			port = g_strdup_printf("%d", candidate->port);
			preference = g_strdup_printf("%d", candidate->preference);

			xmlnode_set_attrib(xmltransport, "generation", generation);
			xmlnode_set_attrib(xmltransport, "name", candidate->id);
			xmlnode_set_attrib(xmltransport, "address", candidate->address);
			xmlnode_set_attrib(xmltransport, "network", network);
			xmlnode_set_attrib(xmltransport, "port", port);
			xmlnode_set_attrib(xmltransport, "preference", preference);
			xmlnode_set_attrib(xmltransport, "protocol", candidate->protocol);
			xmlnode_set_attrib(xmltransport, "type", candidate->type);
			xmlnode_set_attrib(xmltransport, "username", candidate->username);
			xmlnode_set_attrib(xmltransport, "password", candidate->password);

			g_free(generation);
			g_free(network);
			g_free(port);
			g_free(preference);
		}
	}

	return node;
}

