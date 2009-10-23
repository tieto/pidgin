/**
 * @file backend-fs2.c Farsight 2 backend for media API
 * @ingroup core
 */

/* purple
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

#include "backend-fs2.h"

#include "internal.h"

#include "backend-iface.h"
#include "debug.h"
#include "media-gst.h"

#include <gst/farsight/fs-conference-iface.h>
#include <gst/farsight/fs-element-added-notifier.h>

/** @copydoc _PurpleMediaBackendFs2Class */
typedef struct _PurpleMediaBackendFs2Class PurpleMediaBackendFs2Class;
/** @copydoc _PurpleMediaBackendFs2Private */
typedef struct _PurpleMediaBackendFs2Private PurpleMediaBackendFs2Private;
/** @copydoc _PurpleMediaBackendFs2Session */
typedef struct _PurpleMediaBackendFs2Session PurpleMediaBackendFs2Session;

#define PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj) \
		(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
		PURPLE_TYPE_MEDIA_BACKEND_FS2, PurpleMediaBackendFs2Private))

static void purple_media_backend_iface_init(PurpleMediaBackendIface *iface);

static gboolean
_gst_bus_cb(GstBus *bus, GstMessage *msg, PurpleMediaBackendFs2 *self);
static void
_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, PurpleMediaBackendFs2 *self);
static void
_stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local,
		PurpleMediaBackendFs2 *self);

static gboolean purple_media_backend_fs2_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params);
static void purple_media_backend_fs2_add_remote_candidates(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates);
static GList *purple_media_backend_fs2_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id);
static GList *purple_media_backend_fs2_get_local_candidates(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant);
static void purple_media_backend_fs2_set_remote_codecs(
		PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs);
static void purple_media_backend_fs2_set_send_codec(PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec);

struct _PurpleMediaBackendFs2Class
{
	GObjectClass parent_class;
};

struct _PurpleMediaBackendFs2
{
	GObject parent;
};

G_DEFINE_TYPE_WITH_CODE(PurpleMediaBackendFs2, purple_media_backend_fs2,
		G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(
		PURPLE_TYPE_MEDIA_BACKEND, purple_media_backend_iface_init));

struct _PurpleMediaBackendFs2Session
{
	PurpleMediaBackendFs2 *backend;
	gchar *id;
	gboolean initiator;
	FsSession *session;
	PurpleMediaSessionType type;
};

struct _PurpleMediaBackendFs2Private
{
	PurpleMedia *media;
	GstElement *confbin;
	FsConference *conference;
	gchar *conference_type;

	GHashTable *sessions;
};

enum {
	PROP_0,
	PROP_CONFERENCE_TYPE,
	PROP_MEDIA,
};

static void
purple_media_backend_fs2_init(PurpleMediaBackendFs2 *self)
{
}

static void
purple_media_backend_fs2_dispose(GObject *obj)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj);

	purple_debug_info("backend-fs2", "purple_media_backend_fs2_dispose\n");

	if (priv->confbin) {
		GstElement *pipeline;

		pipeline = purple_media_manager_get_pipeline(
				purple_media_get_manager(priv->media));

		gst_element_set_locked_state(priv->confbin, TRUE);
		gst_element_set_state(GST_ELEMENT(priv->confbin),
				GST_STATE_NULL);

		if (pipeline) {
			GstBus *bus;
			gst_bin_remove(GST_BIN(pipeline), priv->confbin);
			bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
			g_signal_handlers_disconnect_matched(G_OBJECT(bus),
					G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA,
					0, 0, 0, _gst_bus_cb, obj);
			gst_object_unref(bus);
		} else {
			purple_debug_warning("backend-fs2", "Unable to "
					"properly dispose the conference. "
					"Couldn't get the pipeline.\n");
		}

		priv->confbin = NULL;
		priv->conference = NULL;

	}

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session =
					sessions->data;

			if (session->session) {
				g_object_unref(session->session);
				session->session = NULL;
			}
		}
	}

	if (priv->media) {
		g_object_remove_weak_pointer(G_OBJECT(priv->media),
				(gpointer*)&priv->media);
		priv->media = NULL;
	}

	G_OBJECT_CLASS(purple_media_backend_fs2_parent_class)->dispose(obj);
}

static void
purple_media_backend_fs2_finalize(GObject *obj)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj);

	purple_debug_info("backend-fs2", "purple_media_backend_fs2_finalize\n");

	g_free(priv->conference_type);

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session =
					sessions->data;
			g_free(session->id);
			g_free(session);
		}

		g_hash_table_destroy(priv->sessions);
	}

	G_OBJECT_CLASS(purple_media_backend_fs2_parent_class)->finalize(obj);
}

static void
purple_media_backend_fs2_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	PurpleMediaBackendFs2Private *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(object));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_CONFERENCE_TYPE:
			priv->conference_type = g_value_dup_string(value);
			break;
		case PROP_MEDIA:
			priv->media = g_value_get_object(value);

			if (priv->media == NULL)
				break;

			g_object_add_weak_pointer(G_OBJECT(priv->media),
					(gpointer*)&priv->media);

			g_signal_connect(G_OBJECT(priv->media),
					"state-changed",
					G_CALLBACK(_state_changed_cb),
					PURPLE_MEDIA_BACKEND_FS2(object));
			g_signal_connect(G_OBJECT(priv->media), "stream-info",
					G_CALLBACK(_stream_info_cb),
					PURPLE_MEDIA_BACKEND_FS2(object));
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_backend_fs2_get_property(GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	PurpleMediaBackendFs2Private *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(object));
	
	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_CONFERENCE_TYPE:
			g_value_set_string(value, priv->conference_type);
			break;
		case PROP_MEDIA:
			g_value_set_object(value, priv->media);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_backend_fs2_class_init(PurpleMediaBackendFs2Class *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;

	gobject_class->dispose = purple_media_backend_fs2_dispose;
	gobject_class->finalize = purple_media_backend_fs2_finalize;
	gobject_class->set_property = purple_media_backend_fs2_set_property;
	gobject_class->get_property = purple_media_backend_fs2_get_property;

	g_object_class_override_property(gobject_class, PROP_CONFERENCE_TYPE,
			"conference-type");
	g_object_class_override_property(gobject_class, PROP_MEDIA, "media");

	g_type_class_add_private(klass, sizeof(PurpleMediaBackendFs2Private));
}

static void
purple_media_backend_iface_init(PurpleMediaBackendIface *iface)
{
	iface->add_stream = purple_media_backend_fs2_add_stream;
	iface->add_remote_candidates =
			purple_media_backend_fs2_add_remote_candidates;
	iface->get_codecs = purple_media_backend_fs2_get_codecs;
	iface->get_local_candidates =
			purple_media_backend_fs2_get_local_candidates;
	iface->set_remote_codecs = purple_media_backend_fs2_set_remote_codecs;
	iface->set_send_codec = purple_media_backend_fs2_set_send_codec;
}

static FsMediaType
_session_type_to_fs_media_type(PurpleMediaSessionType type)
{
	if (type & PURPLE_MEDIA_AUDIO)
		return FS_MEDIA_TYPE_AUDIO;
	else if (type & PURPLE_MEDIA_VIDEO)
		return FS_MEDIA_TYPE_VIDEO;
	else
		return 0;
}

#if 0
static FsStreamDirection
_session_type_to_fs_stream_direction(PurpleMediaSessionType type)
{
	if ((type & PURPLE_MEDIA_AUDIO) == PURPLE_MEDIA_AUDIO ||
			(type & PURPLE_MEDIA_VIDEO) == PURPLE_MEDIA_VIDEO)
		return FS_DIRECTION_BOTH;
	else if ((type & PURPLE_MEDIA_SEND_AUDIO) ||
			(type & PURPLE_MEDIA_SEND_VIDEO))
		return FS_DIRECTION_SEND;
	else if ((type & PURPLE_MEDIA_RECV_AUDIO) ||
			(type & PURPLE_MEDIA_RECV_VIDEO))
		return FS_DIRECTION_RECV;
	else
		return FS_DIRECTION_NONE;
}

static PurpleMediaSessionType
_session_type_from_fs(FsMediaType type, FsStreamDirection direction)
{
	PurpleMediaSessionType result = PURPLE_MEDIA_NONE;
	if (type == FS_MEDIA_TYPE_AUDIO) {
		if (direction & FS_DIRECTION_SEND)
			result |= PURPLE_MEDIA_SEND_AUDIO;
		if (direction & FS_DIRECTION_RECV)
			result |= PURPLE_MEDIA_RECV_AUDIO;
	} else if (type == FS_MEDIA_TYPE_VIDEO) {
		if (direction & FS_DIRECTION_SEND)
			result |= PURPLE_MEDIA_SEND_VIDEO;
		if (direction & FS_DIRECTION_RECV)
			result |= PURPLE_MEDIA_RECV_VIDEO;
	}
	return result;
}

static FsCandidate *
purple_media_candidate_to_fs(PurpleMediaCandidate *candidate)
{
	FsCandidate *fscandidate;
	gchar *foundation;
	guint component_id;
	gchar *ip;
	guint port;
	gchar *base_ip;
	guint base_port;
	PurpleMediaNetworkProtocol proto;
	guint32 priority;
	PurpleMediaCandidateType type;
	gchar *username;
	gchar *password;
	guint ttl;

	if (candidate == NULL)
		return NULL;

	g_object_get(G_OBJECT(candidate),
			"foundation", &foundation,
			"component-id", &component_id,
			"ip", &ip,
			"port", &port,
			"base-ip", &base_ip,
			"base-port", &base_port,
			"protocol", &proto,
			"priority", &priority,
			"type", &type,
			"username", &username,
			"password", &password,
			"ttl", &ttl,
			NULL);

	fscandidate = fs_candidate_new(foundation,
			component_id, type,
			proto, ip, port);

	fscandidate->base_ip = base_ip;
	fscandidate->base_port = base_port;
	fscandidate->priority = priority;
	fscandidate->username = username;
	fscandidate->password = password;
	fscandidate->ttl = ttl;

	g_free(foundation);
	g_free(ip);
	return fscandidate;
}
#endif

static PurpleMediaCandidate *
purple_media_candidate_from_fs(FsCandidate *fscandidate)
{
	PurpleMediaCandidate *candidate;

	if (fscandidate == NULL)
		return NULL;

	candidate = purple_media_candidate_new(fscandidate->foundation,
		fscandidate->component_id, fscandidate->type,
		fscandidate->proto, fscandidate->ip, fscandidate->port);
	g_object_set(candidate,
			"base-ip", fscandidate->base_ip,
			"base-port", fscandidate->base_port,
			"priority", fscandidate->priority,
			"username", fscandidate->username,
			"password", fscandidate->password,
			"ttl", fscandidate->ttl, NULL);
	return candidate;
}

static PurpleMediaBackendFs2Session *
_get_session(PurpleMediaBackendFs2 *self, const gchar *sess_id)
{
	PurpleMediaBackendFs2Private *priv;
	PurpleMediaBackendFs2Session *session = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (priv->sessions != NULL)
		session = g_hash_table_lookup(priv->sessions, sess_id);

	return session;
}

static PurpleMediaBackendFs2Session *
_get_session_from_fs_stream(PurpleMediaBackendFs2 *self, FsStream *stream)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	FsSession *fssession;
	GList *values;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);
	g_return_val_if_fail(FS_IS_STREAM(stream), NULL);

	g_object_get(stream, "session", &fssession, NULL);

	values = g_hash_table_get_values(priv->sessions);

	for (; values; values = g_list_delete_link(values, values)) {
		PurpleMediaBackendFs2Session *session = values->data;

		if (session->session == fssession) {
			g_list_free(values);
			g_object_unref(fssession);
			return session;
		}
	}

	g_object_unref(fssession);
	return NULL;
}

static void
_gst_handle_message_element(GstBus *bus, GstMessage *msg,
		PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));

	if (!FS_IS_CONFERENCE(src) || !PURPLE_IS_MEDIA_BACKEND(self) ||
			priv->conference != FS_CONFERENCE(src))
		return;

	if (gst_structure_has_name(msg->structure, "farsight-error")) {
		FsError error_no;
		gst_structure_get_enum(msg->structure, "error-no",
				FS_TYPE_ERROR, (gint*)&error_no);
		switch (error_no) {
			case FS_ERROR_NO_CODECS:
				purple_media_error(priv->media, _("No codecs"
						" found. Install some"
						" GStreamer codecs found"
						" in GStreamer plugins"
						" packages."));
				purple_media_end(priv->media, NULL, NULL);
				break;
			case FS_ERROR_NO_CODECS_LEFT:
				purple_media_error(priv->media, _("No codecs"
						" left. Your codec"
						" preferences in"
						" fs-codecs.conf are too"
						" strict."));
				purple_media_end(priv->media, NULL, NULL);
				break;
			case FS_ERROR_UNKNOWN_CNAME:
			/*
			 * Unknown CName is only a problem for the
			 * multicast transmitter which isn't used.
			 * It is also deprecated.
			 */
				break;
			default:
				purple_debug_error("backend-fs2",
						"farsight-error: %i: %s\n",
						error_no,
					  	gst_structure_get_string(
						msg->structure, "error-msg"));
				break;
		}

		if (FS_ERROR_IS_FATAL(error_no)) {
			purple_media_error(priv->media, _("A non-recoverable "
					"Farsight2 error has occurred."));
			purple_media_end(priv->media, NULL, NULL);
		}
	} else if (gst_structure_has_name(msg->structure,
			"farsight-new-local-candidate")) {
		const GValue *value;
		FsStream *stream;
		FsCandidate *local_candidate;
		PurpleMediaCandidate *candidate;
		FsParticipant *participant;
		PurpleMediaBackendFs2Session *session;
		gchar *name;

		value = gst_structure_get_value(msg->structure, "stream");
		stream = g_value_get_object(value);
		value = gst_structure_get_value(msg->structure, "candidate");
		local_candidate = g_value_get_boxed(value);

		session = _get_session_from_fs_stream(self, stream);

		purple_debug_info("backend-fs2",
				"got new local candidate: %s\n",
				local_candidate->foundation);

		g_object_get(stream, "participant", &participant, NULL);
		g_object_get(participant, "cname", &name, NULL);
		g_object_unref(participant);

#if 0
		purple_media_insert_local_candidate(session, name,
				fs_candidate_copy(local_candidate));
#endif

		candidate = purple_media_candidate_from_fs(local_candidate);
		g_signal_emit_by_name(self, "new-candidate",
				session->id, name, candidate);
		g_object_unref(candidate);
	} else if (gst_structure_has_name(msg->structure,
			"farsight-local-candidates-prepared")) {
		const GValue *value;
		FsStream *stream;
#if 0
		PurpleMediaSession *session;
#endif

		value = gst_structure_get_value(msg->structure, "stream");
		stream = g_value_get_object(value);
#if 0
		session = purple_media_session_from_fs_stream(media, stream);
		_candidates_prepared_cb(stream, session);
#endif
	} else if (gst_structure_has_name(msg->structure,
			"farsight-new-active-candidate-pair")) {
		const GValue *value;
		FsStream *stream;
		FsCandidate *local_candidate;
		FsCandidate *remote_candidate;
#if 0
		PurpleMediaSession *session;
#endif

		value = gst_structure_get_value(msg->structure, "stream");
		stream = g_value_get_object(value);
		value = gst_structure_get_value(msg->structure,
				"local-candidate");
		local_candidate = g_value_get_boxed(value);
		value = gst_structure_get_value(msg->structure,
				"remote-candidate");
		remote_candidate = g_value_get_boxed(value);
#if 0
		session = purple_media_session_from_fs_stream(media, stream);
		_candidate_pair_established_cb(stream, local_candidate,
				remote_candidate, session);
#endif
	} else if (gst_structure_has_name(msg->structure,
			"farsight-recv-codecs-changed")) {
		const GValue *value;
		GList *codecs;
		FsCodec *codec;

		value = gst_structure_get_value(msg->structure, "codecs");
		codecs = g_value_get_boxed(value);
		codec = codecs->data;

		purple_debug_info("backend-fs2",
				"farsight-recv-codecs-changed: %s\n",
				codec->encoding_name);
	} else if (gst_structure_has_name(msg->structure,
			"farsight-component-state-changed")) {
		const GValue *value;
		FsStreamState fsstate;
		guint component;
		const gchar *state;

		value = gst_structure_get_value(msg->structure, "state");
		fsstate = g_value_get_enum(value);
		value = gst_structure_get_value(msg->structure, "component");
		component = g_value_get_uint(value);

		switch (fsstate) {
			case FS_STREAM_STATE_FAILED:
				state = "FAILED";
				break;
			case FS_STREAM_STATE_DISCONNECTED:
				state = "DISCONNECTED";
				break;
			case FS_STREAM_STATE_GATHERING:
				state = "GATHERING";
				break;
			case FS_STREAM_STATE_CONNECTING:
				state = "CONNECTING";
				break;
			case FS_STREAM_STATE_CONNECTED:
				state = "CONNECTED";
				break;
			case FS_STREAM_STATE_READY:
				state = "READY";
				break;
			default:
				state = "UNKNOWN";
				break;
		}

		purple_debug_info("backend-fs2",
				"farsight-component-state-changed: "
				"component: %u state: %s\n",
				component, state);
	} else if (gst_structure_has_name(msg->structure,
			"farsight-send-codec-changed")) {
		const GValue *value;
		FsCodec *codec;
		gchar *codec_str;

		value = gst_structure_get_value(msg->structure, "codec");
		codec = g_value_get_boxed(value);
		codec_str = fs_codec_to_string(codec);

		purple_debug_info("backend-fs2",
				"farsight-send-codec-changed: codec: %s\n",
				codec_str);

		g_free(codec_str);
	} else if (gst_structure_has_name(msg->structure,
			"farsight-codecs-changed")) {
		const GValue *value;
		FsSession *fssession;
#if 0
		GList *sessions;
#endif

		value = gst_structure_get_value(msg->structure, "session");
		fssession = g_value_get_object(value);
#if 0
		sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session = sessions->data;
			gchar *session_id;

			if (session->session != fssession)
				continue;

			session_id = g_strdup(session->id);
			g_signal_emit(media,
					purple_media_backend_fs2_signals[
					CODECS_CHANGED], 0, session_id);
			g_free(session_id);
			g_list_free(sessions);
			break;
		}
#endif
	}
}

static void
_gst_handle_message_error(GstBus *bus, GstMessage *msg,
		PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GstElement *element = GST_ELEMENT(GST_MESSAGE_SRC(msg));
	GstElement *lastElement = NULL;
	GList *sessions;

	while (!GST_IS_PIPELINE(element)) {
		if (element == priv->confbin)
			break;

		lastElement = element;
		element = GST_ELEMENT_PARENT(element);
	}

	if (!GST_IS_PIPELINE(element))
		return;

	sessions = purple_media_get_session_ids(priv->media);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		if (purple_media_get_src(priv->media, sessions->data)
				!= lastElement)
			continue;

		if (purple_media_get_session_type(priv->media, sessions->data)
				& PURPLE_MEDIA_AUDIO)
			purple_media_error(priv->media,
					_("Error with your microphone"));
		else
			purple_media_error(priv->media,
					_("Error with your webcam"));

		break;
	}

	g_list_free(sessions);

	purple_media_error(priv->media, _("Conference error"));
	purple_media_end(priv->media, NULL, NULL);
}

static gboolean
_gst_bus_cb(GstBus *bus, GstMessage *msg, PurpleMediaBackendFs2 *self)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ELEMENT:
			_gst_handle_message_element(bus, msg, self);
			break;
		case GST_MESSAGE_ERROR:
			_gst_handle_message_error(bus, msg, self);
			break;
		default:
			break;
	}

	return TRUE;
}

static void
_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, PurpleMediaBackendFs2 *self)
{
}

static void
_stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local,
		PurpleMediaBackendFs2 *self)
{
}

static gboolean
_init_conference(PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GstElement *pipeline;
	GstBus *bus;
	gchar *name;

	priv->conference = FS_CONFERENCE(
			gst_element_factory_make(priv->conference_type, NULL));

	if (priv->conference == NULL) {
		purple_debug_error("backend-fs2", "Conference == NULL\n");
		return FALSE;
	}

	pipeline = purple_media_manager_get_pipeline(
			purple_media_get_manager(priv->media));

	if (pipeline == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't retrieve pipeline.\n");
		return FALSE;
	}

	name = g_strdup_printf("conf_%p", priv->conference);
	priv->confbin = gst_bin_new(name);
	if (priv->confbin == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't create confbin.\n");
		return FALSE;
	}

	g_free(name);

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	if (bus == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't get the pipeline's bus.\n");
		return FALSE;
	}

	g_signal_connect(G_OBJECT(bus), "message",
			G_CALLBACK(_gst_bus_cb), self);
	gst_object_unref(bus);

	if (!gst_bin_add(GST_BIN(pipeline),
			GST_ELEMENT(priv->confbin))) {
		purple_debug_error("backend-fs2", "Couldn't add confbin "
				"element to the pipeline\n");
		return FALSE;
	}

	if (!gst_bin_add(GST_BIN(priv->confbin),
			GST_ELEMENT(priv->conference))) {
		purple_debug_error("backend-fs2", "Couldn't add conference "
				"element to the confbin\n");
		return FALSE;
	}

	if (gst_element_set_state(GST_ELEMENT(priv->confbin),
			GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		purple_debug_error("backend-fs2",
				"Failed to start conference.\n");
		return FALSE;
	}

	return TRUE;
}

static void
_gst_element_added_cb(FsElementAddedNotifier *self,
		GstBin *bin, GstElement *element, gpointer user_data)
{
	/*
	 * Hack to make H264 work with Gmail video.
	 */
	if (!strncmp(GST_ELEMENT_NAME(element), "x264", 4)) {
		g_object_set(GST_OBJECT(element), "cabac", FALSE, NULL);
	}
}

static gboolean
_create_session(PurpleMediaBackendFs2 *self, const gchar *sess_id,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	PurpleMediaBackendFs2Session *session;
	GError *err = NULL;
	GList *codec_conf = NULL, *iter = NULL;
	gchar *filename = NULL;
	gboolean is_nice = !strcmp(transmitter, "nice");

	session = g_new0(PurpleMediaBackendFs2Session, 1);

	session->session = fs_conference_new_session(priv->conference,
			_session_type_to_fs_media_type(type), &err);

	if (err != NULL) {
		purple_media_error(priv->media,
				_("Error creating session: %s"),
				err->message);
		g_error_free(err);
		g_free(session);
		return FALSE;
	}

	filename = g_build_filename(purple_user_dir(), "fs-codec.conf", NULL);
	codec_conf = fs_codec_list_from_keyfile(filename, &err);
	g_free(filename);

	if (err != NULL) {
		if (err->code == 4)
			purple_debug_info("media", "Couldn't read "
					"fs-codec.conf: %s\n",
					err->message);
		else
			purple_debug_error("media", "Error reading "
					"fs-codec.conf: %s\n",
					err->message);
		g_error_free(err);
	}

	/*
	 * Add SPEEX if the configuration file doesn't exist or
	 * there isn't a speex entry.
	 */
	for (iter = codec_conf; iter; iter = g_list_next(iter)) {
		FsCodec *codec = iter->data;
		if (!g_ascii_strcasecmp(codec->encoding_name, "speex"))
			break;
	}

	if (iter == NULL) {
		codec_conf = g_list_prepend(codec_conf,
				fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 8000));
		codec_conf = g_list_prepend(codec_conf,
				fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 16000));
	}

	fs_session_set_codec_preferences(session->session, codec_conf, NULL);
	fs_codec_list_destroy(codec_conf);

	/*
	 * Removes a 5-7 second delay before
	 * receiving the src-pad-added signal.
	 * Only works for non-multicast FsRtpSessions.
	 */
	if (is_nice || !strcmp(transmitter, "rawudp"))
		g_object_set(G_OBJECT(session->session),
				"no-rtcp-timeout", 0, NULL);

	/*
	 * Hack to make x264 work with Gmail video.
	 */
	if (is_nice && !strcmp(sess_id, "google-video")) {
		FsElementAddedNotifier *notifier =
				fs_element_added_notifier_new();
		g_signal_connect(G_OBJECT(notifier), "element-added",
				G_CALLBACK(_gst_element_added_cb), NULL);
		fs_element_added_notifier_add(notifier,
				GST_BIN(priv->conference));
	}

	session->id = g_strdup(sess_id);
	session->backend = self;
	session->type = type;
	session->initiator = initiator;

	if (!priv->sessions) {
		purple_debug_info("backend-fs2",
				"Creating hash table for sessions\n");
		priv->sessions = g_hash_table_new(g_str_hash, g_str_equal);
	}

	g_hash_table_insert(priv->sessions, g_strdup(session->id), session);

	return TRUE;
}

static gboolean
purple_media_backend_fs2_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params)
{
	PurpleMediaBackendFs2 *backend = PURPLE_MEDIA_BACKEND_FS2(self);
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(backend);

	if (priv->conference == NULL && !_init_conference(backend)) {
		purple_debug_error("backend-fs2",
				"Error initializing the conference.\n");
		return FALSE;
	}

	if (_get_session(backend, sess_id) == NULL &&
			!_create_session(backend, sess_id, type,
			initiator, transmitter)) {
		purple_debug_error("backend-fs2",
				"Error creating the session.\n");
		return FALSE;
	}

	return TRUE;
}

static void
purple_media_backend_fs2_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates)
{
}

static GList *
purple_media_backend_fs2_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	return NULL;
}

static GList *
purple_media_backend_fs2_get_local_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant)
{
	return NULL;
}

static void
purple_media_backend_fs2_set_remote_codecs(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs)
{
}

static void
purple_media_backend_fs2_set_send_codec(PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec)
{
}

FsConference *
purple_media_backend_fs2_get_conference(PurpleMediaBackendFs2 *self)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	return priv->conference;
}

FsSession *
purple_media_backend_fs2_get_session(PurpleMediaBackendFs2 *self,
		const gchar *sess_id)
{
	PurpleMediaBackendFs2Session *session = _get_session(self, sess_id);
	return session != NULL? session->session : NULL;
}
