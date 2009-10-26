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
#include "network.h"
#include "media-gst.h"

#include <gst/farsight/fs-conference-iface.h>
#include <gst/farsight/fs-element-added-notifier.h>

/** @copydoc _PurpleMediaBackendFs2Class */
typedef struct _PurpleMediaBackendFs2Class PurpleMediaBackendFs2Class;
/** @copydoc _PurpleMediaBackendFs2Private */
typedef struct _PurpleMediaBackendFs2Private PurpleMediaBackendFs2Private;
/** @copydoc _PurpleMediaBackendFs2Session */
typedef struct _PurpleMediaBackendFs2Session PurpleMediaBackendFs2Session;
/** @copydoc _PurpleMediaBackendFs2Stream */
typedef struct _PurpleMediaBackendFs2Stream PurpleMediaBackendFs2Stream;

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

struct _PurpleMediaBackendFs2Stream
{
	PurpleMediaBackendFs2Session *session;
	gchar *participant;
	FsStream *stream;

	GList *local_candidates;
	GList *remote_candidates;

	GList *active_local_candidates;
	GList *active_remote_candidates;

	guint connected_cb_id;

	gboolean initiator;
	gboolean accepted;
	gboolean candidates_prepared;
};

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
	GHashTable *participants;

	GList *streams;
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

	if (priv->participants) {
		GList *participants =
				g_hash_table_get_values(priv->participants);
		for (; participants; participants = g_list_delete_link(
				participants, participants))
			g_object_unref(participants->data);
		priv->participants = NULL;
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
_candidate_to_fs(PurpleMediaCandidate *candidate)
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

static GList *
_candidate_list_to_fs(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list,
				_candidate_to_fs(candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static PurpleMediaCandidate *
_candidate_from_fs(FsCandidate *fscandidate)
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

#if 0
static FsCodec *
_codec_to_fs(const PurpleMediaCodec *codec)
{
	FsCodec *new_codec;
	gint id;
	char *encoding_name;
	PurpleMediaSessionType media_type;
	guint clock_rate;
	guint channels;
	GList *iter;

	if (codec == NULL)
		return NULL;

	g_object_get(G_OBJECT(codec),
			"id", &id,
			"encoding-name", &encoding_name,
			"media-type", &media_type,
			"clock-rate", &clock_rate,
			"channels", &channels,
			"optional-params", &iter,
			NULL);

	new_codec = fs_codec_new(id, encoding_name,
			_session_type_to_fs_media_type(media_type),
			clock_rate);
	new_codec->channels = channels;

	for (; iter; iter = g_list_next(iter)) {
		PurpleKeyValuePair *param = (PurpleKeyValuePair*)iter->data;
		fs_codec_add_optional_parameter(new_codec,
				param->key, param->value);
	}

	g_free(encoding_name);
	return new_codec;
}
#endif

static PurpleMediaCodec *
_codec_from_fs(const FsCodec *codec)
{
	PurpleMediaCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	new_codec = purple_media_codec_new(codec->id, codec->encoding_name,
			_session_type_from_fs(codec->media_type,
			FS_DIRECTION_BOTH), codec->clock_rate);
	g_object_set(new_codec, "channels", codec->channels, NULL);

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		FsCodecParameter *param = (FsCodecParameter*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
}

static GList *
_codec_list_from_fs(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				_codec_from_fs(codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

#if 0
static GList *
_codec_list_to_fs(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				_codec_to_fs(codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}
#endif

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

static FsParticipant *
_get_participant(PurpleMediaBackendFs2 *self, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv;
	FsParticipant *participant = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (priv->participants != NULL)
		participant = g_hash_table_lookup(priv->participants, name);

	return participant;
}

static PurpleMediaBackendFs2Stream *
_get_stream(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv;
	GList *streams;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	streams = priv->streams;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaBackendFs2Stream *stream = streams->data;
		if (!strcmp(stream->session->id, sess_id) &&
				!strcmp(stream->participant, name))
			return stream;
	}

	return NULL;
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

		candidate = _candidate_from_fs(local_candidate);
		g_signal_emit_by_name(self, "new-candidate",
				session->id, name, candidate);
		g_object_unref(candidate);
	} else if (gst_structure_has_name(msg->structure,
			"farsight-local-candidates-prepared")) {
		const GValue *value;
		FsStream *stream;
		FsParticipant *participant;
		PurpleMediaBackendFs2Session *session;
		gchar *name;

		value = gst_structure_get_value(msg->structure, "stream");
		stream = g_value_get_object(value);
		session = _get_session_from_fs_stream(self, stream);

		g_object_get(stream, "participant", &participant, NULL);
		g_object_get(participant, "cname", &name, NULL);
		g_object_unref(participant);

		g_signal_emit_by_name(self, "candidates-prepared",
				session->id, name);
	} else if (gst_structure_has_name(msg->structure,
			"farsight-new-active-candidate-pair")) {
		const GValue *value;
		FsStream *stream;
		FsCandidate *local_candidate;
		FsCandidate *remote_candidate;
		FsParticipant *participant;
		PurpleMediaBackendFs2Session *session;
		PurpleMediaCandidate *lcandidate, *rcandidate;
		gchar *name;

		value = gst_structure_get_value(msg->structure, "stream");
		stream = g_value_get_object(value);
		value = gst_structure_get_value(msg->structure,
				"local-candidate");
		local_candidate = g_value_get_boxed(value);
		value = gst_structure_get_value(msg->structure,
				"remote-candidate");
		remote_candidate = g_value_get_boxed(value);

		g_object_get(stream, "participant", &participant, NULL);
		g_object_get(participant, "cname", &name, NULL);
		g_object_unref(participant);

		session = _get_session_from_fs_stream(self, stream);

		lcandidate = _candidate_from_fs(local_candidate);
		rcandidate = _candidate_from_fs(remote_candidate);

		g_signal_emit_by_name(self, "active-candidate-pair",
				session->id, name, lcandidate, rcandidate);

		g_object_unref(lcandidate);
		g_object_unref(rcandidate);
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
		GList *sessions;

		value = gst_structure_get_value(msg->structure, "session");
		fssession = g_value_get_object(value);
		sessions = g_hash_table_get_values(priv->sessions);

		for (; sessions; sessions =
				g_list_delete_link(sessions, sessions)) {
			PurpleMediaBackendFs2Session *session = sessions->data;
			gchar *session_id;

			if (session->session != fssession)
				continue;

			session_id = g_strdup(session->id);
			g_signal_emit_by_name(self, "codecs-changed",
					session_id);
			g_free(session_id);
			g_list_free(sessions);
			break;
		}
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
			purple_debug_info("backend-fs2", "Couldn't read "
					"fs-codec.conf: %s\n",
					err->message);
		else
			purple_debug_error("backend-fs2", "Error reading "
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
_create_participant(PurpleMediaBackendFs2 *self, const gchar *name)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	FsParticipant *participant;
	GError *err = NULL;

	participant = fs_conference_new_participant(
			priv->conference, name, &err);

	if (err) {
		purple_debug_error("backend-fs2",
				"Error creating participant: %s\n",
				err->message);
		g_error_free(err);
		return FALSE;
	}

	if (!priv->participants) {
		purple_debug_info("backend-fs2",
				"Creating hash table for participants\n");
		priv->participants = g_hash_table_new_full(g_str_hash,
				g_str_equal, g_free, NULL);
	}

	g_hash_table_insert(priv->participants, g_strdup(name), participant);

	g_signal_emit_by_name(priv->media, "state-changed",
			PURPLE_MEDIA_STATE_NEW, NULL, name);

	return TRUE;
}

static gboolean
_create_stream(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	GError *err = NULL;
	FsStream *fsstream = NULL;
	const gchar *stun_ip = purple_network_get_stun_ip();
	const gchar *turn_ip = purple_network_get_turn_ip();
	guint _num_params = num_params;
	GParameter *_params = g_new0(GParameter, num_params + 2);
	FsStreamDirection type_direction =
			_session_type_to_fs_stream_direction(type);
	PurpleMediaBackendFs2Session *session;
	PurpleMediaBackendFs2Stream *stream;
	FsParticipant *participant;

	memcpy(_params, params, sizeof(GParameter) * num_params);

	if (stun_ip) {
		purple_debug_info("backend-fs2", 
			"Setting stun-ip on new stream: %s\n", stun_ip);

		_params[_num_params].name = "stun-ip";
		g_value_init(&_params[_num_params].value, G_TYPE_STRING);
		g_value_set_string(&_params[_num_params].value, stun_ip);
		++_num_params;
	}

	if (turn_ip && !strcmp("nice", transmitter)) {
		GValueArray *relay_info = g_value_array_new(0);
		GValue value;
		gint turn_port = purple_prefs_get_int(
				"/purple/network/turn_port");
		const gchar *username =	purple_prefs_get_string(
				"/purple/network/turn_username");
		const gchar *password = purple_prefs_get_string(
				"/purple/network/turn_password");
		GstStructure *turn_setup = gst_structure_new("relay-info",
				"ip", G_TYPE_STRING, turn_ip, 
				"port", G_TYPE_UINT, turn_port,
				"username", G_TYPE_STRING, username,
				"password", G_TYPE_STRING, password,
				NULL);

		if (!turn_setup) {
			purple_debug_error("backend-fs2",
					"Error creating relay info structure");
			return FALSE;
		}

		memset(&value, 0, sizeof(GValue));
		g_value_init(&value, GST_TYPE_STRUCTURE);
		gst_value_set_structure(&value, turn_setup);
		relay_info = g_value_array_append(relay_info, &value);
		gst_structure_free(turn_setup);

		purple_debug_info("backend-fs2",
			"Setting relay-info on new stream\n");
		_params[_num_params].name = "relay-info";
		g_value_init(&_params[_num_params].value, 
			G_TYPE_VALUE_ARRAY);
		g_value_set_boxed(&_params[_num_params].value,
			relay_info);
		g_value_array_free(relay_info);
	}

	session = _get_session(self, sess_id);

	if (session == NULL) {
		purple_debug_error("backend-fs2",
				"Couldn't find session to create stream.\n");
		return FALSE;
	}

	participant = _get_participant(self, who);

	if (participant == NULL) {
		purple_debug_error("backend-fs2", "Couldn't find "
				"participant to create stream.\n");
		return FALSE;
	}

	fsstream = fs_session_new_stream(session->session, participant,
			type_direction & FS_DIRECTION_RECV, transmitter,
			_num_params, _params, &err);
	g_free(_params);

	if (fsstream == NULL) {
		if (err) {
			purple_debug_error("backend-fs2",
					"Error creating stream: %s\n",
					err && err->message ?
					err->message : "NULL");
			g_error_free(err);
		} else
			purple_debug_error("backend-fs2",
					"Error creating stream\n");
		return FALSE;
	}

	stream = g_new0(PurpleMediaBackendFs2Stream, 1);
	stream->initiator = initiator;
	stream->participant = g_strdup(who);
	stream->session = session;
	stream->stream = fsstream;

	priv->streams =	g_list_append(priv->streams, stream);

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

	if (_get_participant(backend, who) == NULL &&
			!_create_participant(backend, who)) {
		purple_debug_error("backend-fs2",
				"Error creating the participant.\n");
		return FALSE;
	}

	if (_get_stream(backend, sess_id, who) == NULL &&
			!_create_stream(backend, sess_id, who, type,
			initiator, transmitter, num_params, params)) {
		purple_debug_error("backend-fs2",
				"Error creating the stream.\n");
		return FALSE;
	}

	return TRUE;
}

static void
purple_media_backend_fs2_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates)
{
	PurpleMediaBackendFs2Private *priv;
	PurpleMediaBackendFs2Stream *stream;
	GError *err = NULL;

	g_return_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self));

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);
	stream = _get_stream(PURPLE_MEDIA_BACKEND_FS2(self),
			sess_id, participant);

	if (stream == NULL) {
		purple_debug_error("backend-fs2",
				"purple_media_add_remote_candidates: "
				"couldn't find stream %s %s.\n",
				sess_id ? sess_id : "(null)",
				participant ? participant : "(null)");
		return;
	}

	stream->remote_candidates = g_list_concat(stream->remote_candidates,
			_candidate_list_to_fs(remote_candidates));

	if (purple_media_accepted(priv->media, sess_id, participant)) {
		fs_stream_set_remote_candidates(stream->stream,
				stream->remote_candidates, &err);

		if (err) {
			purple_debug_error("backend-fs2", "Error adding remote"
					" candidates: %s\n", err->message);
			g_error_free(err);
		}
	}
}

static GList *
purple_media_backend_fs2_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	PurpleMediaBackendFs2Private *priv;
	PurpleMediaBackendFs2Session *session;
	GList *fscodecs;
	GList *codecs;

	g_return_val_if_fail(PURPLE_IS_MEDIA_BACKEND_FS2(self), NULL);

	priv = PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	session = _get_session(PURPLE_MEDIA_BACKEND_FS2(self), sess_id);

	if (session == NULL)
		return NULL;

	g_object_get(G_OBJECT(session->session),
		     "codecs", &fscodecs, NULL);
	codecs = _codec_list_from_fs(fscodecs);
	fs_codec_list_destroy(fscodecs);

	return codecs;
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

FsParticipant *
purple_media_backend_fs2_get_participant(PurpleMediaBackendFs2 *self,
		const gchar *name)
{
	return _get_participant(self, name);
}

FsStream *
purple_media_backend_fs2_get_stream(PurpleMediaBackendFs2 *self,
		const gchar *sess_id, const gchar *who)
{
	PurpleMediaBackendFs2Stream *stream =
			_get_stream(self, sess_id, who);
	return stream != NULL? stream->stream : NULL;
}
