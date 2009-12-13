/**
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
#include "debug.h"
#include "mediamanager.h"
#include "util.h"
#include "privacy.h"
#include "dnsquery.h"
#include "network.h"

#include "buddy.h"
#include "google.h"
#include "jabber.h"
#include "presence.h"
#include "iq.h"
#include "chat.h"

#include "jingle/jingle.h"

#ifdef USE_VV

typedef struct {
	char *id;
	char *initiator;
} GoogleSessionId;

typedef enum {
	UNINIT,
	SENT_INITIATE,
	RECEIVED_INITIATE,
	IN_PRORESS,
	TERMINATED
} GoogleSessionState;

typedef struct {
	GoogleSessionId id;
	GoogleSessionState state;
	PurpleMedia *media;
	JabberStream *js;
	char *remote_jid;
	gboolean video;
} GoogleSession;

static gboolean
google_session_id_equal(gconstpointer a, gconstpointer b)
{
	GoogleSessionId *c = (GoogleSessionId*)a;
	GoogleSessionId *d = (GoogleSessionId*)b;

	return !strcmp(c->id, d->id) && !strcmp(c->initiator, d->initiator);
}

static void
google_session_destroy(GoogleSession *session)
{
	g_free(session->id.id);
	g_free(session->id.initiator);
	g_free(session->remote_jid);
	g_free(session);
}

static xmlnode *
google_session_create_xmlnode(GoogleSession *session, const char *type)
{
	xmlnode *node = xmlnode_new("session");
	xmlnode_set_namespace(node, NS_GOOGLE_SESSION);
	xmlnode_set_attrib(node, "id", session->id.id);
	xmlnode_set_attrib(node, "initiator", session->id.initiator);
	xmlnode_set_attrib(node, "type", type);
	return node;
}

static void
google_session_send_candidates(PurpleMedia *media, gchar *session_id,
		gchar *participant, GoogleSession *session)
{
	GList *candidates = purple_media_get_local_candidates(
			session->media, session_id, session->remote_jid), *iter;
	PurpleMediaCandidate *transport;
	gboolean video = FALSE;

	if (!strcmp(session_id, "google-video"))
		video = TRUE;

	for (iter = candidates; iter; iter = iter->next) {
		JabberIq *iq;
		gchar *ip, *port, *username, *password;
		gchar pref[16];
		PurpleMediaCandidateType type;
		xmlnode *sess;
		xmlnode *candidate;
		guint component_id;
		transport = PURPLE_MEDIA_CANDIDATE(iter->data);
		component_id = purple_media_candidate_get_component_id(
				transport);

		iq = jabber_iq_new(session->js, JABBER_IQ_SET);
		sess = google_session_create_xmlnode(session, "candidates");
		xmlnode_insert_child(iq->node, sess);
		xmlnode_set_attrib(iq->node, "to", session->remote_jid);

		candidate = xmlnode_new("candidate");

		ip = purple_media_candidate_get_ip(transport);
		port = g_strdup_printf("%d",
				purple_media_candidate_get_port(transport));
		g_ascii_dtostr(pref, 16,
			purple_media_candidate_get_priority(transport) / 1000.0);
		username = purple_media_candidate_get_username(transport);
		password = purple_media_candidate_get_password(transport);
		type = purple_media_candidate_get_candidate_type(transport);

		xmlnode_set_attrib(candidate, "address", ip);
		xmlnode_set_attrib(candidate, "port", port);
		xmlnode_set_attrib(candidate, "name",
				component_id == PURPLE_MEDIA_COMPONENT_RTP ?
				video ? "video_rtp" : "rtp" :
				component_id == PURPLE_MEDIA_COMPONENT_RTCP ?
				video ? "video_rtcp" : "rtcp" : "none");
		xmlnode_set_attrib(candidate, "username", username);
		/*
		 * As of this writing, Farsight 2 in Google compatibility
		 * mode doesn't provide a password. The Gmail client
		 * requires this to be set.
		 */
		xmlnode_set_attrib(candidate, "password",
				password != NULL ? password : "");
		xmlnode_set_attrib(candidate, "preference", pref);
		xmlnode_set_attrib(candidate, "protocol",
				purple_media_candidate_get_protocol(transport)
				== PURPLE_MEDIA_NETWORK_PROTOCOL_UDP ?
				"udp" : "tcp");
		xmlnode_set_attrib(candidate, "type", type ==
				PURPLE_MEDIA_CANDIDATE_TYPE_HOST ? "local" :
						      type ==
				PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX ? "stun" :
					       	      type ==
				PURPLE_MEDIA_CANDIDATE_TYPE_RELAY ? "relay" :
				NULL);
		xmlnode_set_attrib(candidate, "generation", "0");
		xmlnode_set_attrib(candidate, "network", "0");
		xmlnode_insert_child(sess, candidate);

		g_free(ip);
		g_free(port);
		g_free(username);
		g_free(password);

		jabber_iq_send(iq);
	}
	purple_media_candidate_list_free(candidates);
}

static void
google_session_ready(GoogleSession *session)
{
	PurpleMedia *media = session->media;
	if (purple_media_codecs_ready(media, NULL) &&
			purple_media_candidates_prepared(media, NULL, NULL)) {
		gchar *me = g_strdup_printf("%s@%s/%s",
				session->js->user->node,
				session->js->user->domain,
				session->js->user->resource);
		JabberIq *iq;
		xmlnode *sess, *desc, *payload;
		GList *codecs, *iter;
		gboolean is_initiator = !strcmp(session->id.initiator, me);

		if (!is_initiator &&
				!purple_media_accepted(media, NULL, NULL)) {
			g_free(me);
			return;
		}

		iq = jabber_iq_new(session->js, JABBER_IQ_SET);

		if (is_initiator) {
			xmlnode_set_attrib(iq->node, "to", session->remote_jid);
			xmlnode_set_attrib(iq->node, "from", session->id.initiator);
			sess = google_session_create_xmlnode(session, "initiate");
		} else {
			google_session_send_candidates(session->media,
					"google-voice", session->remote_jid,
					session);
			google_session_send_candidates(session->media,
					"google-video", session->remote_jid,
					session);
			xmlnode_set_attrib(iq->node, "to", session->remote_jid);
			xmlnode_set_attrib(iq->node, "from", me);
			sess = google_session_create_xmlnode(session, "accept");
		}
		xmlnode_insert_child(iq->node, sess);
		desc = xmlnode_new_child(sess, "description");
		if (session->video)
			xmlnode_set_namespace(desc, NS_GOOGLE_SESSION_VIDEO);
		else
			xmlnode_set_namespace(desc, NS_GOOGLE_SESSION_PHONE);

		codecs = purple_media_get_codecs(media, "google-video");

		for (iter = codecs; iter; iter = g_list_next(iter)) {
			PurpleMediaCodec *codec = (PurpleMediaCodec*)iter->data;
			gchar *id = g_strdup_printf("%d",
					purple_media_codec_get_id(codec));
			gchar *encoding_name =
					purple_media_codec_get_encoding_name(codec);
			payload = xmlnode_new_child(desc, "payload-type");
			xmlnode_set_attrib(payload, "id", id);
			xmlnode_set_attrib(payload, "name", encoding_name);
			xmlnode_set_attrib(payload, "width", "320");
			xmlnode_set_attrib(payload, "height", "200");
			xmlnode_set_attrib(payload, "framerate", "30");
			g_free(encoding_name);
			g_free(id);
		}
		purple_media_codec_list_free(codecs);

		codecs = purple_media_get_codecs(media, "google-voice");

		for (iter = codecs; iter; iter = g_list_next(iter)) {
			PurpleMediaCodec *codec = (PurpleMediaCodec*)iter->data;
			gchar *id = g_strdup_printf("%d",
					purple_media_codec_get_id(codec));
			gchar *encoding_name =
					purple_media_codec_get_encoding_name(codec);
			gchar *clock_rate = g_strdup_printf("%d",
					purple_media_codec_get_clock_rate(codec));
			payload = xmlnode_new_child(desc, "payload-type");
			if (session->video)
				xmlnode_set_namespace(payload, NS_GOOGLE_SESSION_PHONE);
			xmlnode_set_attrib(payload, "id", id);
			/*
			 * Hack to make Gmail accept speex as the codec.
			 * It shouldn't have to be case sensitive.
			 */
			if (purple_strequal(encoding_name, "SPEEX"))
				xmlnode_set_attrib(payload, "name", "speex");
			else
				xmlnode_set_attrib(payload, "name", encoding_name);
			xmlnode_set_attrib(payload, "clockrate", clock_rate);
			g_free(clock_rate);
			g_free(encoding_name);
			g_free(id);
		}
		purple_media_codec_list_free(codecs);

		jabber_iq_send(iq);

		if (is_initiator) {
			google_session_send_candidates(session->media,
					"google-voice", session->remote_jid,
					session);
			google_session_send_candidates(session->media,
					"google-video", session->remote_jid,
					session);
		}

		g_signal_handlers_disconnect_by_func(G_OBJECT(session->media),
				G_CALLBACK(google_session_ready), session);
	}
}

static void
google_session_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, GoogleSession *session)
{
	if (sid == NULL && name == NULL) {
		if (state == PURPLE_MEDIA_STATE_END) {
			google_session_destroy(session);
		}
	}
}

static void
google_session_stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local,
		GoogleSession *session)
{
	if (sid != NULL || name != NULL)
		return;

	if (type == PURPLE_MEDIA_INFO_HANGUP) {
		xmlnode *sess;
		JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);

		xmlnode_set_attrib(iq->node, "to", session->remote_jid);
		sess = google_session_create_xmlnode(session, "terminate");
		xmlnode_insert_child(iq->node, sess);

		jabber_iq_send(iq);
	} else if (type == PURPLE_MEDIA_INFO_REJECT) {
		xmlnode *sess;
		JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);

		xmlnode_set_attrib(iq->node, "to", session->remote_jid);
		sess = google_session_create_xmlnode(session, "reject");
		xmlnode_insert_child(iq->node, sess);

		jabber_iq_send(iq);
	} else if (type == PURPLE_MEDIA_INFO_ACCEPT && local == TRUE) {
		google_session_ready(session);
	}
}

static GParameter *
jabber_google_session_get_params(JabberStream *js, guint *num)
{
	guint num_params;
	GParameter *params = jingle_get_params(js, &num_params);
	GParameter *new_params = g_new0(GParameter, num_params + 1);

	memcpy(new_params, params, sizeof(GParameter) * num_params);

	purple_debug_info("jabber", "setting Google jingle compatibility param\n");
	new_params[num_params].name = "compatibility-mode";
	g_value_init(&new_params[num_params].value, G_TYPE_UINT);
	g_value_set_uint(&new_params[num_params].value, 1); /* NICE_COMPATIBILITY_GOOGLE */

	g_free(params);
	*num = num_params + 1;
	return new_params;
}


gboolean
jabber_google_session_initiate(JabberStream *js, const gchar *who, PurpleMediaSessionType type)
{
	GoogleSession *session;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	gchar *jid;
	GParameter *params;
	guint num_params;

	/* construct JID to send to */
	jb = jabber_buddy_find(js, who, FALSE);
	if (!jb) {
		purple_debug_error("jingle-rtp",
				"Could not find Jabber buddy\n");
		return FALSE;
	}
	jbr = jabber_buddy_find_resource(jb, NULL);
	if (!jbr) {
		purple_debug_error("jingle-rtp",
				"Could not find buddy's resource\n");
	}

	if ((strchr(who, '/') == NULL) && jbr && (jbr->name != NULL)) {
		jid = g_strdup_printf("%s/%s", who, jbr->name);
	} else {
		jid = g_strdup(who);
	}

	session = g_new0(GoogleSession, 1);
	session->id.id = jabber_get_next_id(js);
	session->id.initiator = g_strdup_printf("%s@%s/%s", js->user->node,
			js->user->domain, js->user->resource);
	session->state = SENT_INITIATE;
	session->js = js;
	session->remote_jid = jid;

	if (type & PURPLE_MEDIA_VIDEO)
		session->video = TRUE;

	session->media = purple_media_manager_create_media(
			purple_media_manager_get(),
			purple_connection_get_account(js->gc),
			"fsrtpconference", session->remote_jid, TRUE);

	purple_media_set_prpl_data(session->media, session);

	g_signal_connect_swapped(G_OBJECT(session->media),
			"candidates-prepared",
			G_CALLBACK(google_session_ready), session);
	g_signal_connect_swapped(G_OBJECT(session->media), "codecs-changed",
			G_CALLBACK(google_session_ready), session);
	g_signal_connect(G_OBJECT(session->media), "state-changed",
			G_CALLBACK(google_session_state_changed_cb), session);
	g_signal_connect(G_OBJECT(session->media), "stream-info",
			G_CALLBACK(google_session_stream_info_cb), session);

	params = jabber_google_session_get_params(js, &num_params);

	if (purple_media_add_stream(session->media, "google-voice",
			session->remote_jid, PURPLE_MEDIA_AUDIO,
			TRUE, "nice", num_params, params) == FALSE ||
			(session->video && purple_media_add_stream(
			session->media, "google-video",
			session->remote_jid, PURPLE_MEDIA_VIDEO,
			TRUE, "nice", num_params, params) == FALSE)) {
		purple_media_error(session->media, "Error adding stream.");
		purple_media_end(session->media, NULL, NULL);
		g_free(params);
		return FALSE;
	}

	g_free(params);

	return (session->media != NULL) ? TRUE : FALSE;
}

static gboolean
google_session_handle_initiate(JabberStream *js, GoogleSession *session, xmlnode *sess, const char *iq_id)
{
	JabberIq *result;
	GList *codecs = NULL, *video_codecs = NULL;
	xmlnode *desc_element, *codec_element;
	PurpleMediaCodec *codec;
	const char *xmlns;
	GParameter *params;
	guint num_params;

	if (session->state != UNINIT) {
		purple_debug_error("jabber", "Received initiate for active session.\n");
		return FALSE;
	}

	desc_element = xmlnode_get_child(sess, "description");
	xmlns = xmlnode_get_namespace(desc_element);

	if (purple_strequal(xmlns, NS_GOOGLE_SESSION_PHONE))
		session->video = FALSE;
	else if (purple_strequal(xmlns, NS_GOOGLE_SESSION_VIDEO))
		session->video = TRUE;
	else {
		purple_debug_error("jabber", "Received initiate with "
				"invalid namespace %s.\n", xmlns);
		return FALSE;
	}

	session->media = purple_media_manager_create_media(
			purple_media_manager_get(),
			purple_connection_get_account(js->gc),
			"fsrtpconference", session->remote_jid, FALSE);

	purple_media_set_prpl_data(session->media, session);

	g_signal_connect_swapped(G_OBJECT(session->media),
			"candidates-prepared",
			G_CALLBACK(google_session_ready), session);
	g_signal_connect_swapped(G_OBJECT(session->media), "codecs-changed",
			G_CALLBACK(google_session_ready), session);
	g_signal_connect(G_OBJECT(session->media), "state-changed",
			G_CALLBACK(google_session_state_changed_cb), session);
	g_signal_connect(G_OBJECT(session->media), "stream-info",
			G_CALLBACK(google_session_stream_info_cb), session);

	params = jabber_google_session_get_params(js, &num_params);

	if (purple_media_add_stream(session->media, "google-voice",
			session->remote_jid, PURPLE_MEDIA_AUDIO, FALSE,
			"nice", num_params, params) == FALSE ||
			(session->video && purple_media_add_stream(
			session->media, "google-video",
			session->remote_jid, PURPLE_MEDIA_VIDEO,
			FALSE, "nice", num_params, params) == FALSE)) {
		purple_media_error(session->media, "Error adding stream.");
		purple_media_stream_info(session->media,
				PURPLE_MEDIA_INFO_REJECT, NULL, NULL, TRUE);
		g_free(params);
		return FALSE;
	}

	g_free(params);

	for (codec_element = xmlnode_get_child(desc_element, "payload-type");
	     codec_element; codec_element = codec_element->next) {
		const char *id, *encoding_name,  *clock_rate,
				*width, *height, *framerate;
		gboolean video;
		if (codec_element->name &&
				strcmp(codec_element->name, "payload-type"))
			continue;

		xmlns = xmlnode_get_namespace(codec_element);
		encoding_name = xmlnode_get_attrib(codec_element, "name");
		id = xmlnode_get_attrib(codec_element, "id");

		if (!session->video ||
				(xmlns && !strcmp(xmlns, NS_GOOGLE_SESSION_PHONE))) {
			clock_rate = xmlnode_get_attrib(
					codec_element, "clockrate");
			video = FALSE;
		} else {
			width = xmlnode_get_attrib(codec_element, "width");
			height = xmlnode_get_attrib(codec_element, "height");
			framerate = xmlnode_get_attrib(
					codec_element, "framerate");
			clock_rate = "90000";
			video = TRUE;
		}

		if (id) {
			codec = purple_media_codec_new(atoi(id), encoding_name,
					video ?	PURPLE_MEDIA_VIDEO :
					PURPLE_MEDIA_AUDIO,
					clock_rate ? atoi(clock_rate) : 0);
			if (video)
				video_codecs = g_list_append(
						video_codecs, codec);
			else
				codecs = g_list_append(codecs, codec);
		}
	}

	if (codecs)
		purple_media_set_remote_codecs(session->media, "google-voice",
				session->remote_jid, codecs);
	if (video_codecs)
		purple_media_set_remote_codecs(session->media, "google-video",
				session->remote_jid, video_codecs);

	purple_media_codec_list_free(codecs);
	purple_media_codec_list_free(video_codecs);

	result = jabber_iq_new(js, JABBER_IQ_RESULT);
	jabber_iq_set_id(result, iq_id);
	xmlnode_set_attrib(result->node, "to", session->remote_jid);
	jabber_iq_send(result);

	return TRUE;
}

static void
google_session_handle_candidates(JabberStream  *js, GoogleSession *session, xmlnode *sess, const char *iq_id)
{
	JabberIq *result;
	GList *list = NULL, *video_list = NULL;
	xmlnode *cand;
	static int name = 0;
	char n[4];

	for (cand = xmlnode_get_child(sess, "candidate"); cand;
			cand = xmlnode_get_next_twin(cand)) {
		PurpleMediaCandidate *info;
		const gchar *cname = xmlnode_get_attrib(cand, "name");
		const gchar *type = xmlnode_get_attrib(cand, "type");
		const gchar *protocol = xmlnode_get_attrib(cand, "protocol");
		const gchar *address = xmlnode_get_attrib(cand, "address");
		const gchar *port = xmlnode_get_attrib(cand, "port");
		guint component_id;

		if (cname && type && address && port) {
			PurpleMediaCandidateType candidate_type;

			g_snprintf(n, sizeof(n), "S%d", name++);

			if (g_str_equal(type, "local"))
				candidate_type = PURPLE_MEDIA_CANDIDATE_TYPE_HOST;
			else if (g_str_equal(type, "stun"))
				candidate_type = PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX;
			else if (g_str_equal(type, "relay"))
				candidate_type = PURPLE_MEDIA_CANDIDATE_TYPE_RELAY;
			else
				candidate_type = PURPLE_MEDIA_CANDIDATE_TYPE_HOST;

			if (purple_strequal(cname, "rtcp") ||
					purple_strequal(cname, "video_rtcp"))
				component_id = PURPLE_MEDIA_COMPONENT_RTCP;
			else
				component_id = PURPLE_MEDIA_COMPONENT_RTP;

			info = purple_media_candidate_new(n, component_id,
					candidate_type,
					purple_strequal(protocol, "udp") ?
							PURPLE_MEDIA_NETWORK_PROTOCOL_UDP :
							PURPLE_MEDIA_NETWORK_PROTOCOL_TCP,
					address,
					atoi(port));
			g_object_set(info, "username", xmlnode_get_attrib(cand, "username"),
					"password", xmlnode_get_attrib(cand, "password"), NULL);
			if (!strncmp(cname, "video_", 6))
				video_list = g_list_append(video_list, info);
			else
				list = g_list_append(list, info);
		}
	}

	if (list)
		purple_media_add_remote_candidates(
				session->media, "google-voice",
				session->remote_jid, list);
	if (video_list)
		purple_media_add_remote_candidates(
				session->media, "google-video",
				session->remote_jid, video_list);
	purple_media_candidate_list_free(list);
	purple_media_candidate_list_free(video_list);

	result = jabber_iq_new(js, JABBER_IQ_RESULT);
	jabber_iq_set_id(result, iq_id);
	xmlnode_set_attrib(result->node, "to", session->remote_jid);
	jabber_iq_send(result);
}

static void
google_session_handle_accept(JabberStream *js, GoogleSession *session, xmlnode *sess, const char *iq_id)
{
	xmlnode *desc_element = xmlnode_get_child(sess, "description");
	xmlnode *codec_element = xmlnode_get_child(
			desc_element, "payload-type");
	GList *codecs = NULL, *video_codecs = NULL;
	JabberIq *result = NULL;
	const gchar *xmlns = xmlnode_get_namespace(desc_element);
	gboolean video = (xmlns && !strcmp(xmlns, NS_GOOGLE_SESSION_VIDEO));

	for (; codec_element; codec_element = codec_element->next) {
		const gchar *xmlns, *encoding_name, *id,
				*clock_rate, *width, *height, *framerate;
		gboolean video_codec = FALSE;

		if (!purple_strequal(codec_element->name, "payload-type"))
			continue;

		xmlns = xmlnode_get_namespace(codec_element);
		encoding_name =	xmlnode_get_attrib(codec_element, "name");
		id = xmlnode_get_attrib(codec_element, "id");

		if (!video || purple_strequal(xmlns, NS_GOOGLE_SESSION_PHONE))
			clock_rate = xmlnode_get_attrib(
					codec_element, "clockrate");
		else {
			clock_rate = "90000";
			width = xmlnode_get_attrib(codec_element, "width");
			height = xmlnode_get_attrib(codec_element, "height");
			framerate = xmlnode_get_attrib(
					codec_element, "framerate");
			video_codec = TRUE;
		}

		if (id && encoding_name) {
			PurpleMediaCodec *codec = purple_media_codec_new(
					atoi(id), encoding_name,
					video_codec ? PURPLE_MEDIA_VIDEO :
					PURPLE_MEDIA_AUDIO,
					clock_rate ? atoi(clock_rate) : 0);
			if (video_codec)
				video_codecs = g_list_append(
						video_codecs, codec);
			else
				codecs = g_list_append(codecs, codec);
		}
	}

	if (codecs)
		purple_media_set_remote_codecs(session->media, "google-voice",
				session->remote_jid, codecs);
	if (video_codecs)
		purple_media_set_remote_codecs(session->media, "google-video",
				session->remote_jid, video_codecs);

	purple_media_stream_info(session->media, PURPLE_MEDIA_INFO_ACCEPT,
			NULL, NULL, FALSE);

	result = jabber_iq_new(js, JABBER_IQ_RESULT);
	jabber_iq_set_id(result, iq_id);
	xmlnode_set_attrib(result->node, "to", session->remote_jid);
	jabber_iq_send(result);
}

static void
google_session_handle_reject(JabberStream *js, GoogleSession *session, xmlnode *sess)
{
	purple_media_end(session->media, NULL, NULL);
}

static void
google_session_handle_terminate(JabberStream *js, GoogleSession *session, xmlnode *sess)
{
	purple_media_end(session->media, NULL, NULL);
}

static void
google_session_parse_iq(JabberStream *js, GoogleSession *session, xmlnode *sess, const char *iq_id)
{
	const char *type = xmlnode_get_attrib(sess, "type");

	if (!strcmp(type, "initiate")) {
		google_session_handle_initiate(js, session, sess, iq_id);
	} else if (!strcmp(type, "accept")) {
		google_session_handle_accept(js, session, sess, iq_id);
	} else if (!strcmp(type, "reject")) {
		google_session_handle_reject(js, session, sess);
	} else if (!strcmp(type, "terminate")) {
		google_session_handle_terminate(js, session, sess);
	} else if (!strcmp(type, "candidates")) {
		google_session_handle_candidates(js, session, sess, iq_id);
	}
}

void
jabber_google_session_parse(JabberStream *js, const char *from,
                            JabberIqType type, const char *iq_id,
                            xmlnode *session_node)
{
	GoogleSession *session = NULL;
	GoogleSessionId id;

	xmlnode *desc_node;

	GList *iter = NULL;

	if (type != JABBER_IQ_SET)
		return;

	id.id = (gchar*)xmlnode_get_attrib(session_node, "id");
	if (!id.id)
		return;

	id.initiator = (gchar*)xmlnode_get_attrib(session_node, "initiator");
	if (!id.initiator)
		return;

	iter = purple_media_manager_get_media_by_account(
			purple_media_manager_get(),
			purple_connection_get_account(js->gc));
	for (; iter; iter = g_list_delete_link(iter, iter)) {
		GoogleSession *gsession =
				purple_media_get_prpl_data(iter->data);
		if (google_session_id_equal(&(gsession->id), &id)) {
			session = gsession;
			break;
		}
	}
	if (iter != NULL) {
		g_list_free(iter);
	}

	if (session) {
		google_session_parse_iq(js, session, session_node, iq_id);
		return;
	}

	/* If the session doesn't exist, this has to be an initiate message */
	if (strcmp(xmlnode_get_attrib(session_node, "type"), "initiate"))
		return;
	desc_node = xmlnode_get_child(session_node, "description");
	if (!desc_node)
		return;
	session = g_new0(GoogleSession, 1);
	session->id.id = g_strdup(id.id);
	session->id.initiator = g_strdup(id.initiator);
	session->state = UNINIT;
	session->js = js;
	session->remote_jid = g_strdup(session->id.initiator);

	google_session_handle_initiate(js, session, session_node, iq_id);
}
#endif /* USE_VV */

static void
jabber_gmail_parse(JabberStream *js, const char *from,
                   JabberIqType type, const char *id,
                   xmlnode *packet, gpointer nul)
{
	xmlnode *child;
	xmlnode *message;
	const char *to, *url;
	const char *in_str;
	char *to_name;

	int i, count = 1, returned_count;

	const char **tos, **froms, **urls;
	char **subjects;

	if (type == JABBER_IQ_ERROR)
		return;

	child = xmlnode_get_child(packet, "mailbox");
	if (!child)
		return;

	in_str = xmlnode_get_attrib(child, "total-matched");
	if (in_str && *in_str)
		count = atoi(in_str);

	/* If Gmail doesn't tell us who the mail is to, let's use our JID */
	to = xmlnode_get_attrib(packet, "to");

	message = xmlnode_get_child(child, "mail-thread-info");

	if (count == 0 || !message) {
		if (count > 0) {
			char *bare_jid = jabber_get_bare_jid(to);
			const char *default_tos[2] = { bare_jid };

			purple_notify_emails(js->gc, count, FALSE, NULL, NULL, default_tos, NULL, NULL, NULL);
			g_free(bare_jid);
		} else {
			purple_notify_emails(js->gc, count, FALSE, NULL, NULL, NULL, NULL, NULL, NULL);
		}

		return;
	}

	/* Loop once to see how many messages were returned so we can allocate arrays
	 * accordingly */
	for (returned_count = 0; message; returned_count++, message=xmlnode_get_next_twin(message));

	froms    = g_new0(const char* , returned_count + 1);
	tos      = g_new0(const char* , returned_count + 1);
	subjects = g_new0(char* , returned_count + 1);
	urls     = g_new0(const char* , returned_count + 1);

	to = xmlnode_get_attrib(packet, "to");
	to_name = jabber_get_bare_jid(to);
	url = xmlnode_get_attrib(child, "url");
	if (!url || !*url)
		url = "http://www.gmail.com";

	message= xmlnode_get_child(child, "mail-thread-info");
	for (i=0; message; message = xmlnode_get_next_twin(message), i++) {
		xmlnode *sender_node, *subject_node;
		const char *from, *tid;
		char *subject;

		subject_node = xmlnode_get_child(message, "subject");
		sender_node  = xmlnode_get_child(message, "senders");
		sender_node  = xmlnode_get_child(sender_node, "sender");

		while (sender_node && (!xmlnode_get_attrib(sender_node, "unread") ||
		       !strcmp(xmlnode_get_attrib(sender_node, "unread"),"0")))
			sender_node = xmlnode_get_next_twin(sender_node);

		if (!sender_node) {
			i--;
			continue;
		}

		from = xmlnode_get_attrib(sender_node, "name");
		if (!from || !*from)
			from = xmlnode_get_attrib(sender_node, "address");
		subject = xmlnode_get_data(subject_node);
		/*
		 * url = xmlnode_get_attrib(message, "url");
		 */
		tos[i] = (to_name != NULL ?  to_name : "");
		froms[i] = (from != NULL ?  from : "");
		subjects[i] = (subject != NULL ? subject : g_strdup(""));
		urls[i] = url;

		tid = xmlnode_get_attrib(message, "tid");
		if (tid &&
		    (js->gmail_last_tid == NULL || strcmp(tid, js->gmail_last_tid) > 0)) {
			g_free(js->gmail_last_tid);
			js->gmail_last_tid = g_strdup(tid);
		}
	}

	if (i>0)
		purple_notify_emails(js->gc, count, count == i, (const char**) subjects, froms, tos,
				urls, NULL, NULL);

	g_free(to_name);
	g_free(tos);
	g_free(froms);
	for (i = 0; i < returned_count; i++)
		g_free(subjects[i]);
	g_free(subjects);
	g_free(urls);

	in_str = xmlnode_get_attrib(child, "result-time");
	if (in_str && *in_str) {
		g_free(js->gmail_last_time);
		js->gmail_last_time = g_strdup(in_str);
	}
}

void
jabber_gmail_poke(JabberStream *js, const char *from, JabberIqType type,
                  const char *id, xmlnode *new_mail)
{
	xmlnode *query;
	JabberIq *iq;

	/* bail if the user isn't interested */
	if (!purple_account_get_check_mail(js->gc->account))
		return;

	/* Is this an initial incoming mail notification? If so, send a request for more info */
	if (type != JABBER_IQ_SET)
		return;

	/* Acknowledge the notification */
	iq = jabber_iq_new(js, JABBER_IQ_RESULT);
	if (from)
		xmlnode_set_attrib(iq->node, "to", from);
	xmlnode_set_attrib(iq->node, "id", id);
	jabber_iq_send(iq);

	purple_debug_misc("jabber",
		   "Got new mail notification. Sending request for more info\n");

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_GOOGLE_MAIL_NOTIFY);
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	query = xmlnode_get_child(iq->node, "query");

	if (js->gmail_last_time)
		xmlnode_set_attrib(query, "newer-than-time", js->gmail_last_time);
	if (js->gmail_last_tid)
		xmlnode_set_attrib(query, "newer-than-tid", js->gmail_last_tid);

	jabber_iq_send(iq);
	return;
}

void jabber_gmail_init(JabberStream *js) {
	JabberIq *iq;
	xmlnode *usersetting, *mailnotifications;

	if (!purple_account_get_check_mail(purple_connection_get_account(js->gc)))
		return;

	/*
	 * Quoting http://code.google.com/apis/talk/jep_extensions/usersettings.html:
	 * To ensure better compatibility with other clients, rather than
	 * setting this value to "false" to turn off notifications, it is
	 * recommended that a client set this to "true" and filter incoming
	 * email notifications itself.
	 */
	iq = jabber_iq_new(js, JABBER_IQ_SET);
	usersetting = xmlnode_new_child(iq->node, "usersetting");
	xmlnode_set_namespace(usersetting, "google:setting");
	mailnotifications = xmlnode_new_child(usersetting, "mailnotifications");
	xmlnode_set_attrib(mailnotifications, "value", "true");
	jabber_iq_send(iq);

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, NS_GOOGLE_MAIL_NOTIFY);
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	jabber_iq_send(iq);
}

void jabber_google_roster_init(JabberStream *js)
{
	JabberIq *iq;
	xmlnode *query;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:roster");
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);
}

void jabber_google_roster_outgoing(JabberStream *js, xmlnode *query, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	char *jid_norm = (char *)jabber_normalize(account, jid);

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(query, "gr:ext", "2");
			xmlnode_set_attrib(item, "gr:t", "B");
			return;
		}
		list = list->next;
	}
}

gboolean jabber_google_roster_incoming(JabberStream *js, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	const char *jid = xmlnode_get_attrib(item, "jid");
	gboolean on_block_list = FALSE;

	char *jid_norm;

	const char *grt = xmlnode_get_attrib_with_namespace(item, "t", "google:roster");
	const char *subscription = xmlnode_get_attrib(item, "subscription");
	const char *ask = xmlnode_get_attrib(item, "ask");

	if ((!subscription || !strcmp(subscription, "none")) && !ask) {
		/* The Google Talk servers will automatically add people from your Gmail address book
		 * with subscription=none. If we see someone with subscription=none, ignore them.
		 */
		return FALSE;
	}

 	jid_norm = g_strdup(jabber_normalize(account, jid));

	on_block_list = NULL != g_slist_find_custom(account->deny, jid_norm,
	                                            (GCompareFunc)strcmp);

	if (grt && (*grt == 'H' || *grt == 'h')) {
		/* Hidden; don't show this buddy. */
		GSList *buddies = purple_find_buddies(account, jid_norm);
		if (buddies)
			purple_debug_info("jabber", "Removing %s from local buddy list\n",
			                  jid_norm);

		for ( ; buddies; buddies = g_slist_delete_link(buddies, buddies)) {
			purple_blist_remove_buddy(buddies->data);
		}

		g_free(jid_norm);
		return FALSE;
	}

	if (!on_block_list && (grt && (*grt == 'B' || *grt == 'b'))) {
		purple_debug_info("jabber", "Blocking %s\n", jid_norm);
		purple_privacy_deny_add(account, jid_norm, TRUE);
	} else if (on_block_list && (!grt || (*grt != 'B' && *grt != 'b' ))){
		purple_debug_info("jabber", "Unblocking %s\n", jid_norm);
		purple_privacy_deny_remove(account, jid_norm, TRUE);
	}

	g_free(jid_norm);
	return TRUE;
}

void jabber_google_roster_add_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	PurpleBuddy *b;
	JabberBuddy *jb;
	const char *balias;

	js = (JabberStream*)(gc->proto_data);

	if (!js || !(js->server_caps & JABBER_CAP_GOOGLE_ROSTER))
		return;

	jb = jabber_buddy_find(js, who, TRUE);

	buddies = purple_find_buddies(js->gc->account, who);
	if(!buddies)
		return;

	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, purple_group_get_name(g), -1);

		buddies = buddies->next;
	}

	balias = purple_buddy_get_local_buddy_alias(b);
	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", balias ? balias : "");
	xmlnode_set_attrib(item, "gr:t", "B");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* Synthesize a sign-off */
	if (jb) {
		JabberBuddyResource *jbr;
		GList *l = jb->resources;
		while (l) {
			jbr = l->data;
			if (jbr && jbr->name)
			{
				purple_debug_misc("jabber", "Removing resource %s\n", jbr->name);
				jabber_buddy_remove_resource(jb, jbr->name);
			}
			l = l->next;
		}
	}

	purple_prpl_got_user_status(purple_connection_get_account(gc), who, "offline", NULL);
}

void jabber_google_roster_rem_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	PurpleBuddy *b;
	const char *balias;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	js = (JabberStream*)(gc->proto_data);

	if (!js || !(js->server_caps & JABBER_CAP_GOOGLE_ROSTER))
		return;

	buddies = purple_find_buddies(purple_connection_get_account(js->gc), who);
	if(!buddies)
		return;

	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, purple_group_get_name(g), -1);

		buddies = buddies->next;
	}

	balias = purple_buddy_get_local_buddy_alias(b);
	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", balias ? balias : "");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* See if he's online */
	jabber_presence_subscription_set(js, who, "probe");
}

/* This does two passes on the string. The first pass goes through
 * and determine if all the structured text is properly balanced, and
 * how many instances of each there is. The second pass goes and converts
 * everything to HTML, depending on what's figured out by the first pass.
 * It will short circuit once it knows it has no more replacements to make
 */
char *jabber_google_format_to_html(const char *text)
{
	const char *p;

	/* The start of the screen may be consdiered a space for this purpose */
	gboolean preceding_space = TRUE;

	gboolean in_bold = FALSE, in_italic = FALSE;
	gboolean in_tag = FALSE;

	gint bold_count = 0, italic_count = 0;

	GString *str;

	for (p = text; *p != '\0'; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		if (c == '*' && !in_tag) {
			if (in_bold && (g_unichar_isspace(*(p+1)) ||
					*(p+1) == '\0' ||
					*(p+1) == '<')) {
				bold_count++;
				in_bold = FALSE;
			} else if (preceding_space && !in_bold && !g_unichar_isspace(*(p+1))) {
				bold_count++;
				in_bold = TRUE;
			}
			preceding_space = TRUE;
		} else if (c == '_' && !in_tag) {
			if (in_italic && (g_unichar_isspace(*(p+1)) ||
					*(p+1) == '\0' ||
					*(p+1) == '<')) {
				italic_count++;
				in_italic = FALSE;
			} else if (preceding_space && !in_italic && !g_unichar_isspace(*(p+1))) {
				italic_count++;
				in_italic = TRUE;
			}
			preceding_space = TRUE;
		} else if (c == '<' && !in_tag) {
			in_tag = TRUE;
		} else if (c == '>' && in_tag) {
			in_tag = FALSE;
		} else if (!in_tag) {
			if (g_unichar_isspace(c))
				preceding_space = TRUE;
			else
				preceding_space = FALSE;
		}
	}

	str  = g_string_new(NULL);
	in_bold = in_italic = in_tag = FALSE;
	preceding_space = TRUE;

	for (p = text; *p != '\0'; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);

		if (bold_count < 2 && italic_count < 2 && !in_bold && !in_italic) {
			g_string_append(str, p);
			return g_string_free(str, FALSE);
		}


		if (c == '*' && !in_tag) {
			if (in_bold &&
			    (g_unichar_isspace(*(p+1))||*(p+1)=='<')) { /* This is safe in UTF-8 */
				str = g_string_append(str, "</b>");
				in_bold = FALSE;
				bold_count--;
			} else if (preceding_space && bold_count > 1 && !g_unichar_isspace(*(p+1))) {
				str = g_string_append(str, "<b>");
				bold_count--;
				in_bold = TRUE;
			} else {
				str = g_string_append_unichar(str, c);
			}
			preceding_space = TRUE;
		} else if (c == '_' && !in_tag) {
			if (in_italic &&
			    (g_unichar_isspace(*(p+1))||*(p+1)=='<')) {
				str = g_string_append(str, "</i>");
				italic_count--;
				in_italic = FALSE;
			} else if (preceding_space && italic_count > 1 && !g_unichar_isspace(*(p+1))) {
				str = g_string_append(str, "<i>");
				italic_count--;
				in_italic = TRUE;
			} else {
				str = g_string_append_unichar(str, c);
			}
			preceding_space = TRUE;
		} else if (c == '<' && !in_tag) {
			str = g_string_append_unichar(str, c);
			in_tag = TRUE;
		} else if (c == '>' && in_tag) {
			str = g_string_append_unichar(str, c);
			in_tag = FALSE;
		} else if (!in_tag) {
			str = g_string_append_unichar(str, c);
			if (g_unichar_isspace(c))
				preceding_space = TRUE;
			else
				preceding_space = FALSE;
		} else {
			str = g_string_append_unichar(str, c);
		}
	}
	return g_string_free(str, FALSE);
}

void jabber_google_presence_incoming(JabberStream *js, const char *user, JabberBuddyResource *jbr)
{
	if (!js->googletalk)
		return;
	if (jbr->status && purple_str_has_prefix(jbr->status, "♫ ")) {
		purple_prpl_got_user_status(js->gc->account, user, "tune",
					    PURPLE_TUNE_TITLE, jbr->status + strlen("♫ "), NULL);
		g_free(jbr->status);
		jbr->status = NULL;
	} else {
		purple_prpl_got_user_status_deactive(js->gc->account, user, "tune");
	}
}

char *jabber_google_presence_outgoing(PurpleStatus *tune)
{
	const char *attr = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
	return attr ? g_strdup_printf("♫ %s", attr) : g_strdup("");
}

static void
jabber_google_stun_lookup_cb(GSList *hosts, gpointer data,
	const char *error_message)
{
	JabberStream *js = (JabberStream *) data;

	if (error_message) {
		purple_debug_error("jabber", "Google STUN lookup failed: %s\n",
			error_message);
		g_slist_free(hosts);
		js->stun_query = NULL;
		return;
	}

	if (hosts && g_slist_next(hosts)) {
		struct sockaddr *addr = g_slist_next(hosts)->data;
		char dst[INET6_ADDRSTRLEN];
		int port;

		if (addr->sa_family == AF_INET6) {
			inet_ntop(addr->sa_family, &((struct sockaddr_in6 *) addr)->sin6_addr,
				dst, sizeof(dst));
			port = ntohs(((struct sockaddr_in6 *) addr)->sin6_port);
		} else {
			inet_ntop(addr->sa_family, &((struct sockaddr_in *) addr)->sin_addr,
				dst, sizeof(dst));
			port = ntohs(((struct sockaddr_in *) addr)->sin_port);
		}

		if (js->stun_ip)
			g_free(js->stun_ip);
		js->stun_ip = g_strdup(dst);
		js->stun_port = port;

		purple_debug_info("jabber", "set Google STUN IP/port address: "
		                  "%s:%d\n", dst, port);

		/* unmark ongoing query */
		js->stun_query = NULL;
	}

	while (hosts != NULL) {
		hosts = g_slist_delete_link(hosts, hosts);
		/* Free the address */
		g_free(hosts->data);
		hosts = g_slist_delete_link(hosts, hosts);
	}
}

static void
jabber_google_jingle_info_common(JabberStream *js, const char *from,
                                 JabberIqType type, xmlnode *query)
{
	const xmlnode *stun = xmlnode_get_child(query, "stun");
	gchar *my_bare_jid;

	/*
	 * Make sure that random people aren't sending us STUN servers. Per
	 * http://code.google.com/apis/talk/jep_extensions/jingleinfo.html, these
	 * stanzas are stamped from our bare JID.
	 */
	if (from) {
		my_bare_jid = g_strdup_printf("%s@%s", js->user->node, js->user->domain);
		if (!purple_strequal(from, my_bare_jid)) {
			purple_debug_warning("jabber", "got google:jingleinfo with invalid from (%s)\n",
			                  from);
			g_free(my_bare_jid);
			return;
		}

		g_free(my_bare_jid);
	}

	if (type == JABBER_IQ_ERROR || type == JABBER_IQ_GET)
		return;

	purple_debug_info("jabber", "got google:jingleinfo\n");

	if (stun) {
		xmlnode *server = xmlnode_get_child(stun, "server");

		if (server) {
			const gchar *host = xmlnode_get_attrib(server, "host");
			const gchar *udp = xmlnode_get_attrib(server, "udp");

			if (host && udp) {
				int port = atoi(udp);
				/* if there, would already be an ongoing query,
				 cancel it */
				if (js->stun_query)
					purple_dnsquery_destroy(js->stun_query);

				js->stun_query = purple_dnsquery_a(host, port,
					jabber_google_stun_lookup_cb, js);
			}
		}
	}
	/* should perhaps handle relays later on, or maybe wait until
	 Google supports a common standard... */
}

static void
jabber_google_jingle_info_cb(JabberStream *js, const char *from,
                             JabberIqType type, const char *id,
                             xmlnode *packet, gpointer data)
{
	xmlnode *query = xmlnode_get_child_with_namespace(packet, "query",
			NS_GOOGLE_JINGLE_INFO);

	if (query)
		jabber_google_jingle_info_common(js, from, type, query);
	else
		purple_debug_warning("jabber", "Got invalid google:jingleinfo\n");
}

void
jabber_google_handle_jingle_info(JabberStream *js, const char *from,
                                 JabberIqType type, const char *id,
                                 xmlnode *child)
{
	jabber_google_jingle_info_common(js, from, type, child);
}

void
jabber_google_send_jingle_info(JabberStream *js)
{
	JabberIq *jingle_info =
		jabber_iq_new_query(js, JABBER_IQ_GET, NS_GOOGLE_JINGLE_INFO);

	jabber_iq_set_callback(jingle_info, jabber_google_jingle_info_cb,
		NULL);
	purple_debug_info("jabber", "sending google:jingleinfo query\n");
	jabber_iq_send(jingle_info);
}

void google_buddy_node_chat(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	JabberStream *js;
	JabberChat *chat;
	gchar *room;
	guint32 tmp, a, b;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = PURPLE_BUDDY(node);
	gc = purple_account_get_connection(purple_buddy_get_account(buddy));
	g_return_if_fail(gc != NULL);
	js = purple_connection_get_protocol_data(gc);

	/* Generate a version 4 UUID */
	tmp = g_random_int();
	a = 0x4000 | (tmp & 0xFFF); /* 0x4000 to 0x4FFF */
	tmp >>= 12;
	b = ((1 << 3) << 12) | (tmp & 0x3FFF); /* 0x8000 to 0xBFFF */

	tmp = g_random_int();
	room = g_strdup_printf("private-chat-%08x-%04x-%04x-%04x-%04x%08x",
			g_random_int(),
			tmp & 0xFFFF,
			a,
			b,
			(tmp >> 16) & 0xFFFF, g_random_int());

	chat = jabber_join_chat(js, room, GOOGLE_GROUPCHAT_SERVER, js->user->node,
	                        NULL, NULL);
	if (chat) {
		chat->muc = TRUE;
		jabber_chat_invite(gc, chat->id, "", buddy->name);
	}

	g_free(room);
}
