/**
 * @file rtp.c
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

#include "config.h"

#ifdef USE_VV

#include "jabber.h"
#include "jingle.h"
#include "media.h"
#include "mediamanager.h"
#include "iceudp.h"
#include "rawudp.h"
#include "rtp.h"
#include "session.h"
#include "debug.h"

#include <string.h>

struct _JingleRtpPrivate
{
	gchar *media_type;
};

#define JINGLE_RTP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), JINGLE_TYPE_RTP, JingleRtpPrivate))

static void jingle_rtp_class_init (JingleRtpClass *klass);
static void jingle_rtp_init (JingleRtp *rtp);
static void jingle_rtp_finalize (GObject *object);
static void jingle_rtp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void jingle_rtp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static JingleContent *jingle_rtp_parse_internal(xmlnode *rtp);
static xmlnode *jingle_rtp_to_xml_internal(JingleContent *rtp, xmlnode *content, JingleActionType action);
static void jingle_rtp_handle_action_internal(JingleContent *content, xmlnode *jingle, JingleActionType action);

static PurpleMedia *jingle_rtp_get_media(JingleSession *session);

static JingleContentClass *parent_class = NULL;
#if 0
enum {
	LAST_SIGNAL
};
static guint jingle_rtp_signals[LAST_SIGNAL] = {0};
#endif

enum {
	PROP_0,
	PROP_MEDIA_TYPE,
};

GType
jingle_rtp_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(JingleRtpClass),
			NULL,
			NULL,
			(GClassInitFunc) jingle_rtp_class_init,
			NULL,
			NULL,
			sizeof(JingleRtp),
			0,
			(GInstanceInitFunc) jingle_rtp_init,
			NULL
		};
		type = g_type_register_static(JINGLE_TYPE_CONTENT, "JingleRtp", &info, 0);
	}
	return type;
}

static void
jingle_rtp_class_init (JingleRtpClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = jingle_rtp_finalize;
	gobject_class->set_property = jingle_rtp_set_property;
	gobject_class->get_property = jingle_rtp_get_property;
	klass->parent_class.to_xml = jingle_rtp_to_xml_internal;
	klass->parent_class.parse = jingle_rtp_parse_internal;
	klass->parent_class.description_type = JINGLE_APP_RTP;
	klass->parent_class.handle_action = jingle_rtp_handle_action_internal;

	g_object_class_install_property(gobject_class, PROP_MEDIA_TYPE,
			g_param_spec_string("media-type",
			"Media Type",
			"The media type (\"audio\" or \"video\") for this rtp session.",
			NULL,
			G_PARAM_READWRITE));
	g_type_class_add_private(klass, sizeof(JingleRtpPrivate));
}

static void
jingle_rtp_init (JingleRtp *rtp)
{
	rtp->priv = JINGLE_RTP_GET_PRIVATE(rtp);
	memset(rtp->priv, 0, sizeof(rtp->priv));
}

static void
jingle_rtp_finalize (GObject *rtp)
{
	JingleRtpPrivate *priv = JINGLE_RTP_GET_PRIVATE(rtp);
	purple_debug_info("jingle-rtp","jingle_rtp_finalize\n");

	g_free(priv->media_type);
}

static void
jingle_rtp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	JingleRtp *rtp;
	g_return_if_fail(JINGLE_IS_RTP(object));

	rtp = JINGLE_RTP(object);

	switch (prop_id) {
		case PROP_MEDIA_TYPE:
			g_free(rtp->priv->media_type);
			rtp->priv->media_type = g_value_dup_string(value);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_rtp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	JingleRtp *rtp;
	g_return_if_fail(JINGLE_IS_RTP(object));
	
	rtp = JINGLE_RTP(object);

	switch (prop_id) {
		case PROP_MEDIA_TYPE:
			g_value_set_string(value, rtp->priv->media_type);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);	
			break;
	}
}

gchar *
jingle_rtp_get_media_type(JingleContent *content)
{
	gchar *media_type;
	g_object_get(content, "media-type", &media_type, NULL);
	return media_type;
}

static PurpleMedia *
jingle_rtp_get_media(JingleSession *session)
{
	JabberStream *js = jingle_session_get_js(session);
	PurpleMedia *media = NULL;
	GList *iter = purple_media_manager_get_media_by_connection(
			purple_media_manager_get(), js->gc);

	for (; iter; iter = g_list_delete_link(iter, iter)) {
		JingleSession *media_session =
				purple_media_get_prpl_data(iter->data);
		if (media_session == session) {
			media = iter->data;
			break;
		}
	}
	if (iter != NULL)
		g_list_free(iter);

	return media;
}

static JingleTransport *
jingle_rtp_candidates_to_transport(JingleSession *session, GType type, guint generation, GList *candidates)
{
	if (type == JINGLE_TYPE_RAWUDP) {
		JingleTransport *transport = jingle_transport_create(JINGLE_TRANSPORT_RAWUDP);
		JingleRawUdpCandidate *rawudp_candidate;
		for (; candidates; candidates = g_list_next(candidates)) {
			PurpleMediaCandidate *candidate = candidates->data;
			gchar *id = jabber_get_next_id(
					jingle_session_get_js(session));
			rawudp_candidate = jingle_rawudp_candidate_new(id,
					generation, candidate->component_id,
					candidate->ip, candidate->port);
			jingle_rawudp_add_local_candidate(JINGLE_RAWUDP(transport), rawudp_candidate);
			g_free(id);
		}
		return transport;
	} else if (type == JINGLE_TYPE_ICEUDP) {
		JingleTransport *transport = jingle_transport_create(JINGLE_TRANSPORT_ICEUDP);
		JingleIceUdpCandidate *iceudp_candidate;
		for (; candidates; candidates = g_list_next(candidates)) {
			PurpleMediaCandidate *candidate = candidates->data;
			gchar *id = jabber_get_next_id(
					jingle_session_get_js(session));
			iceudp_candidate = jingle_iceudp_candidate_new(candidate->component_id,
					candidate->foundation, generation, id, candidate->ip,
					0, candidate->port, candidate->priority, "udp",
					candidate->type == PURPLE_MEDIA_CANDIDATE_TYPE_HOST ? "host" :
					candidate->type == PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX ? "srflx" :
					candidate->type == PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX ? "prflx" :
					candidate->type == PURPLE_MEDIA_CANDIDATE_TYPE_RELAY ? "relay" : "",
					candidate->username, candidate->password);
			iceudp_candidate->reladdr = g_strdup(candidate->base_ip);
			iceudp_candidate->relport = candidate->base_port;
			jingle_iceudp_add_local_candidate(JINGLE_ICEUDP(transport), iceudp_candidate);
			g_free(id);
		}
		return transport;
	} else {
		return NULL;
	}
}

static GList *
jingle_rtp_transport_to_candidates(JingleTransport *transport)
{
	const gchar *type = jingle_transport_get_transport_type(transport);
	GList *ret = NULL;
	if (!strcmp(type, JINGLE_TRANSPORT_RAWUDP)) {
		GList *candidates = jingle_rawudp_get_remote_candidates(JINGLE_RAWUDP(transport));

		for (; candidates; candidates = g_list_delete_link(candidates, candidates)) {
			JingleRawUdpCandidate *candidate = candidates->data;
			ret = g_list_append(ret, purple_media_candidate_new(
					"", candidate->component,
					PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX,
					PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
					candidate->ip, candidate->port));
		}

		return ret;
	} else if (!strcmp(type, JINGLE_TRANSPORT_ICEUDP)) {
		GList *candidates = jingle_iceudp_get_remote_candidates(JINGLE_ICEUDP(transport));

		for (; candidates; candidates = g_list_delete_link(candidates, candidates)) {
			JingleIceUdpCandidate *candidate = candidates->data;
			PurpleMediaCandidate *new_candidate = purple_media_candidate_new(
					candidate->foundation, candidate->component,
					!strcmp(candidate->type, "host") ?
					PURPLE_MEDIA_CANDIDATE_TYPE_HOST :
					!strcmp(candidate->type, "srflx") ?
					PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX :
					!strcmp(candidate->type, "prflx") ?
					PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX :
					!strcmp(candidate->type, "relay") ?
					PURPLE_MEDIA_CANDIDATE_TYPE_RELAY : 0,
					PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
					candidate->ip, candidate->port);
			new_candidate->base_ip = g_strdup(candidate->reladdr);
			new_candidate->base_port = candidate->relport;
			new_candidate->username = g_strdup(candidate->username);
			new_candidate->password = g_strdup(candidate->password);
			new_candidate->priority = candidate->priority;
			ret = g_list_append(ret, new_candidate);
		}

		return ret;
	} else {
		return NULL;
	}
}

static void
jingle_rtp_accepted_cb(PurpleMedia *media, gchar *sid, gchar *name,
		JingleSession *session)
{
	purple_debug_info("jingle-rtp", "jingle_rtp_accepted_cb\n");
}

static void
jingle_rtp_codecs_changed_cb(PurpleMedia *media, gchar *sid,
		JingleSession *session)
{
	purple_debug_info("jingle-rtp", "jingle_rtp_codecs_changed_cb: "
			"session_id: %s jingle_session: %p\n", sid, session);
}

static void
jingle_rtp_new_candidate_cb(PurpleMedia *media, gchar *sid, gchar *name, PurpleMediaCandidate *candidate, JingleSession *session)
{
	purple_debug_info("jingle-rtp", "jingle_rtp_new_candidate_cb\n");
}

static void
jingle_rtp_initiate_ack_cb(JabberStream *js, xmlnode *packet, gpointer data)
{
	JingleSession *session = data;

	if (!strcmp(xmlnode_get_attrib(packet, "type"), "error") ||
			xmlnode_get_child(packet, "error")) {
		purple_media_end(jingle_rtp_get_media(session), NULL, NULL);
		g_object_unref(session);
		return;
	}

	jabber_iq_send(jingle_session_to_packet(session,
			JINGLE_TRANSPORT_INFO));
}

static void
jingle_rtp_ready_cb(PurpleMedia *media, gchar *sid, gchar *name, JingleSession *session)
{
	purple_debug_info("rtp", "ready-new: session: %s name: %s\n", sid, name);

	if (sid == NULL && name == NULL) {
		if (jingle_session_is_initiator(session) == TRUE) {
			GList *contents = jingle_session_get_contents(session);
			JabberIq *iq = jingle_session_to_packet(
					session, JINGLE_SESSION_INITIATE);

			if (contents->data) {
				JingleTransport *transport =
						jingle_content_get_transport(contents->data);
				if (JINGLE_IS_ICEUDP(transport))
					jabber_iq_set_callback(iq,
							jingle_rtp_initiate_ack_cb, session);
			}

			jabber_iq_send(iq);
		} else {
			jabber_iq_send(jingle_session_to_packet(session, JINGLE_TRANSPORT_INFO));
			jabber_iq_send(jingle_session_to_packet(session, JINGLE_SESSION_ACCEPT));
		}
	} else if (sid != NULL && name != NULL) {
		JingleContent *content = jingle_session_find_content(session, sid, "initiator");
		JingleTransport *oldtransport = jingle_content_get_transport(content);
		GList *candidates = purple_media_get_local_candidates(media, sid, name);
		JingleTransport *transport =
				JINGLE_TRANSPORT(jingle_rtp_candidates_to_transport(
				session, JINGLE_IS_RAWUDP(oldtransport) ?
					JINGLE_TYPE_RAWUDP : JINGLE_TYPE_ICEUDP,
				0, candidates));
		g_list_free(candidates);

		jingle_content_set_pending_transport(content, transport);
		jingle_content_accept_transport(content);
	}
}

static void
jingle_rtp_state_changed_cb(PurpleMedia *media, PurpleMediaStateChangedType type,
		gchar *sid, gchar *name, JingleSession *session)
{
	purple_debug_info("jingle-rtp", "state-changed: type %d id: %s name: %s\n", type, sid, name);

	if ((type == PURPLE_MEDIA_STATE_CHANGED_REJECTED ||
			type == PURPLE_MEDIA_STATE_CHANGED_HANGUP) &&
			sid == NULL && name == NULL) {
		jabber_iq_send(jingle_session_terminate_packet(
				session, "success"));
		g_object_unref(session);
	}
}

static PurpleMedia *
jingle_rtp_create_media(JingleContent *content)
{
	JingleSession *session = jingle_content_get_session(content);
	JabberStream *js = jingle_session_get_js(session);
	gchar *remote_jid = jingle_session_get_remote_jid(session);

	PurpleMedia *media = purple_media_manager_create_media(purple_media_manager_get(), 
						  js->gc, "fsrtpconference", remote_jid,
						  jingle_session_is_initiator(session));
	g_free(remote_jid);

	if (!media) {
		purple_debug_error("jingle-rtp", "Couldn't create media session\n");
		return NULL;
	}

	purple_media_set_prpl_data(media, session);

	/* connect callbacks */
	g_signal_connect(G_OBJECT(media), "accepted",
				 G_CALLBACK(jingle_rtp_accepted_cb), session);
	g_signal_connect(G_OBJECT(media), "codecs-changed",
				 G_CALLBACK(jingle_rtp_codecs_changed_cb), session);
	g_signal_connect(G_OBJECT(media), "new-candidate",
				 G_CALLBACK(jingle_rtp_new_candidate_cb), session);
	g_signal_connect(G_OBJECT(media), "ready-new",
				 G_CALLBACK(jingle_rtp_ready_cb), session);
	g_signal_connect(G_OBJECT(media), "state-changed",
				 G_CALLBACK(jingle_rtp_state_changed_cb), session);

	g_object_unref(session);
	return media;
}

static gboolean
jingle_rtp_init_media(JingleContent *content)
{
	JingleSession *session = jingle_content_get_session(content);
	PurpleMedia *media = jingle_rtp_get_media(session);
	gchar *media_type;
	gchar *remote_jid;
	gchar *senders;
	gchar *name;
	const gchar *transmitter;
	gboolean is_audio;
	PurpleMediaSessionType type;
	JingleTransport *transport;
	GParameter *params = NULL;
	guint num_params;

	/* maybe this create ought to just be in initiate and handle initiate */
	if (media == NULL)
		media = jingle_rtp_create_media(content);

	if (media == NULL)
		return FALSE;

	name = jingle_content_get_name(content);
	media_type = jingle_rtp_get_media_type(content);
	remote_jid = jingle_session_get_remote_jid(session);
	senders = jingle_content_get_senders(content);
	transport = jingle_content_get_transport(content);

	if (JINGLE_IS_RAWUDP(transport))
		transmitter = "rawudp";
	else if (JINGLE_IS_ICEUDP(transport))
		transmitter = "nice";
	else
		transmitter = "notransmitter";

	is_audio = !strcmp(media_type, "audio");

	if (!strcmp(senders, "both"))
		type = is_audio == TRUE ? PURPLE_MEDIA_AUDIO
				: PURPLE_MEDIA_VIDEO;
	else if (!strcmp(senders, "initiator")
			&& jingle_session_is_initiator(session))
		type = is_audio == TRUE ? PURPLE_MEDIA_SEND_AUDIO
				: PURPLE_MEDIA_SEND_VIDEO;
	else
		type = is_audio == TRUE ? PURPLE_MEDIA_RECV_AUDIO
				: PURPLE_MEDIA_RECV_VIDEO;

	params = 
		jingle_get_params(jingle_session_get_js(session), &num_params);
	purple_media_add_stream(media, name, remote_jid,
			type, transmitter, num_params, params);

	g_free(name);
	g_free(media_type);
	g_free(remote_jid);
	g_free(senders);
	g_free(params);
	g_object_unref(session);

	return TRUE;
}

static GList *
jingle_rtp_parse_codecs(xmlnode *description)
{
	GList *codecs = NULL;
	xmlnode *codec_element = NULL;
	const char *encoding_name,*id, *clock_rate;
	PurpleMediaCodec *codec;
	const gchar *media = xmlnode_get_attrib(description, "media");
	PurpleMediaSessionType type =
			!strcmp(media, "video") ? PURPLE_MEDIA_VIDEO :
			!strcmp(media, "audio") ? PURPLE_MEDIA_AUDIO : 0;

	for (codec_element = xmlnode_get_child(description, "payload-type") ;
		 codec_element ;
		 codec_element = xmlnode_get_next_twin(codec_element)) {
		xmlnode *param;
		gchar *codec_str;
		encoding_name = xmlnode_get_attrib(codec_element, "name");

		id = xmlnode_get_attrib(codec_element, "id");
		clock_rate = xmlnode_get_attrib(codec_element, "clockrate");

		codec = purple_media_codec_new(atoi(id), encoding_name, 
				     type, 
				     clock_rate ? atoi(clock_rate) : 0);

		for (param = xmlnode_get_child(codec_element, "parameter");
				param; param = xmlnode_get_next_twin(param)) {
			purple_media_codec_add_optional_parameter(codec,
					xmlnode_get_attrib(param, "name"),
					xmlnode_get_attrib(param, "value"));
		}

		codec_str = purple_media_codec_to_string(codec);
		purple_debug_info("jingle-rtp", "received codec: %s\n", codec_str);
		g_free(codec_str);

		codecs = g_list_append(codecs, codec);
	}
	return codecs;
}

static JingleContent *
jingle_rtp_parse_internal(xmlnode *rtp)
{
	JingleContent *content = parent_class->parse(rtp);
	xmlnode *description = xmlnode_get_child(rtp, "description");
	const gchar *media_type = xmlnode_get_attrib(description, "media");
	purple_debug_info("jingle-rtp", "rtp parse\n");
	g_object_set(content, "media-type", media_type, NULL);
	return content;
}

static void
jingle_rtp_add_payloads(xmlnode *description, GList *codecs)
{
	for (; codecs ; codecs = codecs->next) {
		PurpleMediaCodec *codec = (PurpleMediaCodec*)codecs->data;
		GList *iter = codec->optional_params;
		char id[8], clockrate[10], channels[10];
		gchar *codec_str;
		xmlnode *payload = xmlnode_new_child(description, "payload-type");
		
		g_snprintf(id, sizeof(id), "%d", codec->id);
		g_snprintf(clockrate, sizeof(clockrate), "%d", codec->clock_rate);
		g_snprintf(channels, sizeof(channels), "%d", codec->channels);
		
		xmlnode_set_attrib(payload, "name", codec->encoding_name);
		xmlnode_set_attrib(payload, "id", id);
		xmlnode_set_attrib(payload, "clockrate", clockrate);
		xmlnode_set_attrib(payload, "channels", channels);

		for (; iter; iter = g_list_next(iter)) {
			PurpleMediaCodecParameter *mparam = iter->data;
			xmlnode *param = xmlnode_new_child(payload, "parameter");
			xmlnode_set_attrib(param, "name", mparam->name);
			xmlnode_set_attrib(param, "value", mparam->value);
		}

		codec_str = purple_media_codec_to_string(codec);
		purple_debug_info("jingle", "adding codec: %s\n", codec_str);
		g_free(codec_str);
	}
}

static xmlnode *
jingle_rtp_to_xml_internal(JingleContent *rtp, xmlnode *content, JingleActionType action)
{
	xmlnode *node = parent_class->to_xml(rtp, content, action);
	xmlnode *description = xmlnode_get_child(node, "description");
	if (description != NULL) {
		JingleSession *session = jingle_content_get_session(rtp);
		PurpleMedia *media = jingle_rtp_get_media(session);
		gchar *media_type = jingle_rtp_get_media_type(rtp);
		gchar *name = jingle_content_get_name(rtp);
		GList *codecs = purple_media_get_codecs(media, name);

		xmlnode_set_attrib(description, "media", media_type);

		g_free(media_type);
		g_free(name);
		g_object_unref(session);

		jingle_rtp_add_payloads(description, codecs);
	}
	return node;
}

static void
jingle_rtp_handle_action_internal(JingleContent *content, xmlnode *xmlcontent, JingleActionType action)
{
	switch (action) {
		case JINGLE_SESSION_ACCEPT: {
			JingleSession *session = jingle_content_get_session(content);
			xmlnode *description = xmlnode_get_child(xmlcontent, "description");
			GList *codecs = jingle_rtp_parse_codecs(description);

			purple_media_set_remote_codecs(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session), codecs);

			/* This needs to be for the entire session, not a single content */
			/* very hacky */
			if (xmlnode_get_next_twin(xmlcontent) == NULL)
				purple_media_accept(jingle_rtp_get_media(session));

			g_object_unref(session);
			break;
		}
		case JINGLE_SESSION_INITIATE: {
			JingleSession *session = jingle_content_get_session(content);
			JingleTransport *transport = jingle_transport_parse(
					xmlnode_get_child(xmlcontent, "transport"));
			xmlnode *description = xmlnode_get_child(xmlcontent, "description");
			GList *candidates = jingle_rtp_transport_to_candidates(transport);
			GList *codecs = jingle_rtp_parse_codecs(description);

			if (jingle_rtp_init_media(content) == FALSE) {
				/* XXX: send error */
				jabber_iq_send(jingle_session_terminate_packet(
						session, "general-error"));
				g_object_unref(session);
				break;
			}

			purple_media_set_remote_codecs(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session), codecs);

			if (JINGLE_IS_RAWUDP(transport)) {
				purple_media_add_remote_candidates(jingle_rtp_get_media(session),
						jingle_content_get_name(content),
						jingle_session_get_remote_jid(session),
						candidates);
			}

			g_object_unref(session);
			break;
		}
		case JINGLE_SESSION_TERMINATE: {
			JingleSession *session = jingle_content_get_session(content);
			PurpleMedia *media = jingle_rtp_get_media(session);

			if (media != NULL) {
				purple_media_end(media, NULL, NULL);
			}

			g_object_unref(session);
			break;
		}
		case JINGLE_TRANSPORT_INFO: {
			JingleSession *session = jingle_content_get_session(content);
			JingleTransport *transport = jingle_transport_parse(
					xmlnode_get_child(xmlcontent, "transport"));
			GList *candidates = jingle_rtp_transport_to_candidates(transport);

			purple_media_add_remote_candidates(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session),
					candidates);
			g_object_unref(session);
			break;
		}
		default:
			break;
	}
}

PurpleMedia *
jingle_rtp_initiate_media(JabberStream *js, const gchar *who, 
		      PurpleMediaSessionType type)
{
	/* create content negotiation */
	JingleSession *session;
	JingleContent *content;
	JingleTransport *transport;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	PurpleMedia *media;
	const gchar *transport_type;
	
	gchar *jid = NULL, *me = NULL, *sid = NULL;

	/* construct JID to send to */
	jb = jabber_buddy_find(js, who, FALSE);
	if (!jb) {
		purple_debug_error("jingle-rtp", "Could not find Jabber buddy\n");
		return NULL;
	}
	jbr = jabber_buddy_find_resource(jb, NULL);
	if (!jbr) {
		purple_debug_error("jingle-rtp", "Could not find buddy's resource\n");
	}

	if (jabber_resource_has_capability(jbr, JINGLE_TRANSPORT_ICEUDP)) {
		transport_type = JINGLE_TRANSPORT_ICEUDP;
	} else if (jabber_resource_has_capability(jbr, JINGLE_TRANSPORT_RAWUDP)) {
		transport_type = JINGLE_TRANSPORT_RAWUDP;
	} else {
		purple_debug_error("jingle-rtp", "Resource doesn't support "
				"the same transport types\n");
		return NULL;
	}

	if ((strchr(who, '/') == NULL) && jbr && (jbr->name != NULL)) {
		jid = g_strdup_printf("%s/%s", who, jbr->name);
	} else {
		jid = g_strdup(who);
	}
	
	/* set ourselves as initiator */
	me = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain, js->user->resource);

	sid = jabber_get_next_id(js);
	session = jingle_session_create(js, sid, me, jid, TRUE);
	g_free(sid);


	if (type & PURPLE_MEDIA_AUDIO) {
		transport = jingle_transport_create(transport_type);
		content = jingle_content_create(JINGLE_APP_RTP, "initiator",
				"session", "audio-session", "both", transport);
		jingle_session_add_content(session, content);
		JINGLE_RTP(content)->priv->media_type = g_strdup("audio");
		jingle_rtp_init_media(content);
	}
	if (type & PURPLE_MEDIA_VIDEO) {
		transport = jingle_transport_create(transport_type);
		content = jingle_content_create(JINGLE_APP_RTP, "initiator",
				"session", "video-session", "both", transport);
		jingle_session_add_content(session, content);
		JINGLE_RTP(content)->priv->media_type = g_strdup("video");
		jingle_rtp_init_media(content);
	}

	if ((media = jingle_rtp_get_media(session)) == NULL) {
		return NULL;
	}

	g_free(jid);
	g_free(me);

	return media;
}

void
jingle_rtp_terminate_session(JabberStream *js, const gchar *who)
{
	JingleSession *session;
/* XXX: This may cause file transfers and xml sessions to stop as well */
	session = jingle_session_find_by_jid(js, who);

	if (session) {
		PurpleMedia *media = jingle_rtp_get_media(session);
		if (media) {
			purple_debug_info("jingle-rtp", "hanging up media\n");
			purple_media_hangup(media);
		}
	}
}

#endif /* USE_VV */

