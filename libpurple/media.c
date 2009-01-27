/**
 * @file media.c Media API
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>

#include "internal.h"

#include "connection.h"
#include "media.h"
#include "marshallers.h"
#include "mediamanager.h"
#include "network.h"

#include "debug.h"

#ifdef USE_VV

#include <gst/interfaces/propertyprobe.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/farsight/fs-conference-iface.h>

/** @copydoc _PurpleMediaSession */
typedef struct _PurpleMediaSession PurpleMediaSession;
/** @copydoc _PurpleMediaStream */
typedef struct _PurpleMediaStream PurpleMediaStream;

struct _PurpleMediaSession
{
	gchar *id;
	PurpleMedia *media;
	GstElement *src;
	FsSession *session;

	PurpleMediaSessionType type;

	gboolean codecs_ready;
	gboolean accepted;

	GstElement *sink;
	gulong window_id;
};

struct _PurpleMediaStream
{
	PurpleMediaSession *session;
	gchar *participant;
	FsStream *stream;
	GstElement *sink;

	GList *local_candidates;
	GList *remote_candidates;

	gboolean candidates_prepared;

	FsCandidate *local_candidate;
	FsCandidate *remote_candidate;

	gulong window_id;
};

struct _PurpleMediaPrivate
{
	FsConference *conference;
	gboolean initiator;

	GHashTable *sessions;	/* PurpleMediaSession table */
	GHashTable *participants; /* FsParticipant table */

	GList *streams;		/* PurpleMediaStream table */

	GstElement *pipeline;
	
};

#define PURPLE_MEDIA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA, PurpleMediaPrivate))

static void purple_media_class_init (PurpleMediaClass *klass);
static void purple_media_init (PurpleMedia *media);
static void purple_media_dispose (GObject *object);
static void purple_media_finalize (GObject *object);
static void purple_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void purple_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static void purple_media_new_local_candidate_cb(FsStream *stream,
		FsCandidate *local_candidate, PurpleMediaSession *session);
static void purple_media_candidates_prepared_cb(FsStream *stream,
		PurpleMediaSession *session);
static void purple_media_candidate_pair_established_cb(FsStream *stream,
		FsCandidate *native_candidate, FsCandidate *remote_candidate,
		PurpleMediaSession *session);

static GObjectClass *parent_class = NULL;



enum {
	ERROR,
	ACCEPTED,
	CODECS_CHANGED,
	NEW_CANDIDATE,
	READY_NEW,
	STATE_CHANGED,
	LAST_SIGNAL
};
static guint purple_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_CONFERENCE,
	PROP_INITIATOR,
};

GType
purple_media_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMediaClass),
			NULL,
			NULL,
			(GClassInitFunc) purple_media_class_init,
			NULL,
			NULL,
			sizeof(PurpleMedia),
			0,
			(GInstanceInitFunc) purple_media_init,
			NULL
		};
		type = g_type_register_static(G_TYPE_OBJECT, "PurpleMedia", &info, 0);
	}
	return type;
}

GType
purple_media_state_changed_get_type()
{
	static GType type = 0;
	if (type == 0) {
		static const GEnumValue values[] = {
			{ PURPLE_MEDIA_STATE_CHANGED_NEW, "PURPLE_MEDIA_STATE_CHANGED_NEW", "new" },
			{ PURPLE_MEDIA_STATE_CHANGED_CONNECTED, "PURPLE_MEDIA_STATE_CHANGED_CONNECTED", "connected" },
			{ PURPLE_MEDIA_STATE_CHANGED_END, "PURPLE_MEDIA_STATE_CHANGED_END", "end" },
			{ 0, NULL, NULL }
		};
		type = g_enum_register_static("PurpleMediaStateChangedType", values);
	}
	return type;
}

static void
purple_media_class_init (PurpleMediaClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);
	
	gobject_class->dispose = purple_media_dispose;
	gobject_class->finalize = purple_media_finalize;
	gobject_class->set_property = purple_media_set_property;
	gobject_class->get_property = purple_media_get_property;

	g_object_class_install_property(gobject_class, PROP_CONFERENCE,
			g_param_spec_object("conference",
			"Farsight conference",
			"The FsConference associated with this media.",
			FS_TYPE_CONFERENCE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_INITIATOR,
			g_param_spec_boolean("initiator",
			"initiator",
			"If the local user initiated the conference.",
			FALSE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	purple_media_signals[ERROR] = g_signal_new("error", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_signals[ACCEPTED] = g_signal_new("accepted", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	purple_media_signals[CODECS_CHANGED] = g_signal_new("codecs-changed", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_signals[NEW_CANDIDATE] = g_signal_new("new-candidate", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__POINTER_POINTER_OBJECT,
					 G_TYPE_NONE, 3, G_TYPE_POINTER,
					 G_TYPE_POINTER, PURPLE_TYPE_MEDIA_CANDIDATE);
	purple_media_signals[READY_NEW] = g_signal_new("ready-new", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	purple_media_signals[STATE_CHANGED] = g_signal_new("state-changed", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__ENUM_STRING_STRING,
					 G_TYPE_NONE, 3, PURPLE_MEDIA_TYPE_STATE_CHANGED,
					 G_TYPE_STRING, G_TYPE_STRING);
	g_type_class_add_private(klass, sizeof(PurpleMediaPrivate));
}


static void
purple_media_init (PurpleMedia *media)
{
	media->priv = PURPLE_MEDIA_GET_PRIVATE(media);
	memset(media->priv, 0, sizeof(media->priv));
}

static void
purple_media_stream_free(PurpleMediaStream *stream)
{
	g_free(stream->participant);

	if (stream->local_candidates)
		fs_candidate_list_destroy(stream->local_candidates);
	if (stream->remote_candidates)
		fs_candidate_list_destroy(stream->remote_candidates);

	if (stream->local_candidate)
		fs_candidate_destroy(stream->local_candidate);
	if (stream->remote_candidate)
		fs_candidate_destroy(stream->remote_candidate);

	g_free(stream);
}

static void
purple_media_session_free(PurpleMediaSession *session)
{
	g_free(session->id);
	g_free(session);
}

static void
purple_media_dispose(GObject *media)
{
	PurpleMediaPrivate *priv = PURPLE_MEDIA_GET_PRIVATE(media);
	GList *iter = NULL;

	purple_debug_info("media","purple_media_dispose\n");

	purple_media_manager_remove_media(purple_media_manager_get(),
			PURPLE_MEDIA(media));

	if (priv->pipeline) {
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(priv->pipeline));
		gst_bus_remove_signal_watch(bus);
		gst_object_unref(bus);
		gst_element_set_state(priv->pipeline, GST_STATE_NULL);
		gst_object_unref(priv->pipeline);
		priv->pipeline = NULL;
	}

	for (iter = priv->streams; iter; iter = g_list_next(iter)) {
		PurpleMediaStream *stream = iter->data;
		if (stream->stream) {
			g_object_unref(stream->stream);
			stream->stream = NULL;
		}
	}

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);
		for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
			PurpleMediaSession *session = sessions->data;
			if (session->session) {
				g_object_unref(session->session);
				session->session = NULL;
			}
		}
	}

	if (priv->participants) {
		GList *participants = g_hash_table_get_values(priv->participants);
		for (; participants; participants = g_list_delete_link(participants, participants))
			g_object_unref(participants->data);
	}

	G_OBJECT_CLASS(parent_class)->finalize(media);
}

static void
purple_media_finalize(GObject *media)
{
	PurpleMediaPrivate *priv = PURPLE_MEDIA_GET_PRIVATE(media);
	purple_debug_info("media","purple_media_finalize\n");

	for (; priv->streams; priv->streams = g_list_delete_link(priv->streams, priv->streams))
		purple_media_stream_free(priv->streams->data);

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);
		for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
			purple_media_session_free(sessions->data);
		}
		g_hash_table_destroy(priv->sessions);
	}

	gst_object_unref(priv->conference);

	G_OBJECT_CLASS(parent_class)->finalize(media);
}

static void
purple_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PurpleMedia *media;
	g_return_if_fail(PURPLE_IS_MEDIA(object));

	media = PURPLE_MEDIA(object);

	switch (prop_id) {
		case PROP_CONFERENCE:
			if (media->priv->conference)
				g_object_unref(media->priv->conference);
			media->priv->conference = g_value_get_object(value);
			g_object_ref(media->priv->conference);
			break;
		case PROP_INITIATOR:
			media->priv->initiator = g_value_get_boolean(value);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
purple_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	PurpleMedia *media;
	g_return_if_fail(PURPLE_IS_MEDIA(object));
	
	media = PURPLE_MEDIA(object);

	switch (prop_id) {
		case PROP_CONFERENCE:
			g_value_set_object(value, media->priv->conference);
			break;
		case PROP_INITIATOR:
			g_value_set_boolean(value, media->priv->initiator);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);	
			break;
	}

}

PurpleMediaCandidate *
purple_media_candidate_new(const gchar *foundation, guint component_id,
		PurpleMediaCandidateType type,
		PurpleMediaNetworkProtocol proto,
		const gchar *ip, guint port)
{
	PurpleMediaCandidate *candidate = g_new0(PurpleMediaCandidate, 1);
	candidate->foundation = g_strdup(foundation);
	candidate->component_id = component_id;
	candidate->type = type;
	candidate->proto = proto;
	candidate->ip = g_strdup(ip);
	candidate->port = port;
	return candidate;
}

static PurpleMediaCandidate *
purple_media_candidate_copy(PurpleMediaCandidate *candidate)
{
	PurpleMediaCandidate *new_candidate;

	if (candidate == NULL)
		return NULL;

	new_candidate = g_new0(PurpleMediaCandidate, 1);
	new_candidate->foundation = g_strdup(candidate->foundation);
	new_candidate->component_id = candidate->component_id;
	new_candidate->ip = g_strdup(candidate->ip);
	new_candidate->port = candidate->port;
	new_candidate->base_ip = g_strdup(candidate->base_ip);
	new_candidate->base_port = candidate->base_port;
	new_candidate->proto = candidate->proto;
	new_candidate->priority = candidate->priority;
	new_candidate->type = candidate->type;
	new_candidate->username = g_strdup(candidate->username);
	new_candidate->password = g_strdup(candidate->password);
	new_candidate->ttl = candidate->ttl;
	return new_candidate;
}

static void
purple_media_candidate_free(PurpleMediaCandidate *candidate)
{
	if (candidate == NULL)
		return;

	g_free((gchar*)candidate->foundation);
	g_free((gchar*)candidate->ip);
	g_free((gchar*)candidate->base_ip);
	g_free((gchar*)candidate->username);
	g_free((gchar*)candidate->password);
	g_free(candidate);
}

static FsCandidate *
purple_media_candidate_to_fs(PurpleMediaCandidate *candidate)
{
	FsCandidate *fscandidate;

	if (candidate == NULL)
		return NULL;

	fscandidate = fs_candidate_new(candidate->foundation,
		candidate->component_id, candidate->type,
		candidate->proto, candidate->ip, candidate->port);

	fscandidate->base_ip = g_strdup(candidate->base_ip);
	fscandidate->base_port = candidate->base_port;
	fscandidate->priority = candidate->priority;
	fscandidate->username = g_strdup(candidate->username);
	fscandidate->password = g_strdup(candidate->password);
	fscandidate->ttl = candidate->ttl;
	return fscandidate;
}

static PurpleMediaCandidate *
purple_media_candidate_from_fs(FsCandidate *fscandidate)
{
	PurpleMediaCandidate *candidate;

	if (fscandidate == NULL)
		return NULL;

	candidate = purple_media_candidate_new(fscandidate->foundation,
		fscandidate->component_id, fscandidate->type,
		fscandidate->proto, fscandidate->ip, fscandidate->port);
	candidate->base_ip = g_strdup(fscandidate->base_ip);
	candidate->base_port = fscandidate->base_port;
	candidate->priority = fscandidate->priority;
	candidate->username = g_strdup(fscandidate->username);
	candidate->password = g_strdup(fscandidate->password);
	candidate->ttl = fscandidate->ttl;
	return candidate;
}

static GList *
purple_media_candidate_list_from_fs(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list,
				purple_media_candidate_from_fs(
				candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static GList *
purple_media_candidate_list_to_fs(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list,
				purple_media_candidate_to_fs(
				candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

GList *
purple_media_candidate_list_copy(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list, g_boxed_copy(
				PURPLE_TYPE_MEDIA_CANDIDATE,
				candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

void
purple_media_candidate_list_free(GList *candidates)
{
	for (; candidates; candidates =
			g_list_delete_link(candidates, candidates)) {
		g_boxed_free(PURPLE_TYPE_MEDIA_CANDIDATE,
				candidates->data);
	}
}

GType
purple_media_candidate_get_type()
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleMediaCandidate",
				(GBoxedCopyFunc)purple_media_candidate_copy,
				(GBoxedFreeFunc)purple_media_candidate_free);
	}
	return type;
}

static FsMediaType
purple_media_to_fs_media_type(PurpleMediaSessionType type)
{
	if (type & PURPLE_MEDIA_AUDIO)
		return FS_MEDIA_TYPE_AUDIO;
	else if (type & PURPLE_MEDIA_VIDEO)
		return FS_MEDIA_TYPE_VIDEO;
	else
		return 0;
}

static FsStreamDirection
purple_media_to_fs_stream_direction(PurpleMediaSessionType type)
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
purple_media_from_fs(FsMediaType type, FsStreamDirection direction)
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

void
purple_media_codec_add_optional_parameter(PurpleMediaCodec *codec,
		const gchar *name, const gchar *value)
{
	PurpleMediaCodecParameter *new_param;

	g_return_if_fail(name != NULL && value != NULL);

	new_param = g_new0(PurpleMediaCodecParameter, 1);
	new_param->name = g_strdup(name);
	new_param->value = g_strdup(value);
	codec->optional_params = g_list_append(
			codec->optional_params, new_param);
}

void
purple_media_codec_remove_optional_parameter(PurpleMediaCodec *codec,
		PurpleMediaCodecParameter *param)
{
	g_free(param->name);
	g_free(param->value);
	g_free(param);
	codec->optional_params =
			g_list_remove(codec->optional_params, param);
}

PurpleMediaCodecParameter *
purple_media_codec_get_optional_parameter(PurpleMediaCodec *codec,
		const gchar *name, const gchar *value)
{
	GList *iter;

	g_return_val_if_fail(codec != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		PurpleMediaCodecParameter *param = iter->data;
		if (!g_ascii_strcasecmp(param->name, name) &&
				(value == NULL ||
				!g_ascii_strcasecmp(param->value, value)))
			return param;
	}

	return NULL;
}

PurpleMediaCodec *
purple_media_codec_new(int id, const char *encoding_name,
		PurpleMediaSessionType media_type, guint clock_rate)
{
	PurpleMediaCodec *codec = g_new0(PurpleMediaCodec, 1);

	codec->id = id;
	codec->encoding_name = g_strdup(encoding_name);
	codec->media_type = media_type;
	codec->clock_rate = clock_rate;
	return codec;
}

static PurpleMediaCodec *
purple_media_codec_copy(PurpleMediaCodec *codec)
{
	PurpleMediaCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	new_codec = purple_media_codec_new(codec->id, codec->encoding_name,
			codec->media_type, codec->clock_rate);
	new_codec->channels = codec->channels;

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		PurpleMediaCodecParameter *param =
				(PurpleMediaCodecParameter*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
}

static void
purple_media_codec_free(PurpleMediaCodec *codec)
{
	if (codec == NULL)
		return;

	g_free(codec->encoding_name);

	for (; codec->optional_params; codec->optional_params =
			g_list_delete_link(codec->optional_params,
			codec->optional_params)) {
		purple_media_codec_remove_optional_parameter(codec,
				codec->optional_params->data);
	}

	g_free(codec);
}

static FsCodec *
purple_media_codec_to_fs(const PurpleMediaCodec *codec)
{
	FsCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	new_codec = fs_codec_new(codec->id, codec->encoding_name,
			purple_media_to_fs_media_type(codec->media_type),
			codec->clock_rate);
	new_codec->channels = codec->channels;

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		PurpleMediaCodecParameter *param =
				(PurpleMediaCodecParameter*)iter->data;
		fs_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
}

static PurpleMediaCodec *
purple_media_codec_from_fs(const FsCodec *codec)
{
	PurpleMediaCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	new_codec = purple_media_codec_new(codec->id, codec->encoding_name,
			purple_media_from_fs(codec->media_type,
			FS_DIRECTION_BOTH), codec->clock_rate);
	new_codec->channels = codec->channels;

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		FsCodecParameter *param = (FsCodecParameter*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
}

gchar *
purple_media_codec_to_string(const PurpleMediaCodec *codec)
{
	FsCodec *fscodec = purple_media_codec_to_fs(codec);
	gchar *str = fs_codec_to_string(fscodec);
	fs_codec_destroy(fscodec);
	return str;
}

static GList *
purple_media_codec_list_from_fs(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				purple_media_codec_from_fs(
				codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

static GList *
purple_media_codec_list_to_fs(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				purple_media_codec_to_fs(
				codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

GList *
purple_media_codec_list_copy(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list, g_boxed_copy(
				PURPLE_TYPE_MEDIA_CODEC,
				codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

void
purple_media_codec_list_free(GList *codecs)
{
	for (; codecs; codecs =
			g_list_delete_link(codecs, codecs)) {
		g_boxed_free(PURPLE_TYPE_MEDIA_CODEC,
				codecs->data);
	}
}

GType
purple_media_codec_get_type()
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleMediaCodec",
				(GBoxedCopyFunc)purple_media_codec_copy,
				(GBoxedFreeFunc)purple_media_codec_free);
	}
	return type;
}



PurpleMediaSessionType
purple_media_get_overall_type(PurpleMedia *media)
{
	GList *values = g_hash_table_get_values(media->priv->sessions);
	PurpleMediaSessionType type = PURPLE_MEDIA_NONE;

	for (; values; values = g_list_delete_link(values, values)) {
		PurpleMediaSession *session = values->data;
		type |= session->type;
	}

	return type;
}

static PurpleMediaSession*
purple_media_get_session(PurpleMedia *media, const gchar *sess_id)
{
	return (PurpleMediaSession*) (media->priv->sessions) ?
			g_hash_table_lookup(media->priv->sessions, sess_id) : NULL;
}

static FsParticipant*
purple_media_get_participant(PurpleMedia *media, const gchar *name)
{
	return (FsParticipant*) (media->priv->participants) ?
			g_hash_table_lookup(media->priv->participants, name) : NULL;
}

static PurpleMediaStream*
purple_media_get_stream(PurpleMedia *media, const gchar *session, const gchar *participant)
{
	GList *streams = media->priv->streams;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaStream *stream = streams->data;
		if (!strcmp(stream->session->id, session) &&
				!strcmp(stream->participant, participant))
			return stream;
	}

	return NULL;
}

static GList *
purple_media_get_streams(PurpleMedia *media, const gchar *session,
		const gchar *participant)
{
	GList *streams = media->priv->streams;
	GList *ret = NULL;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaStream *stream = streams->data;
		if ((session == NULL ||
				!strcmp(stream->session->id, session)) &&
				(participant == NULL ||
				!strcmp(stream->participant, participant)))
			ret = g_list_append(ret, stream);
	}

	return ret;
}

static void
purple_media_add_session(PurpleMedia *media, PurpleMediaSession *session)
{
	if (!media->priv->sessions) {
		purple_debug_info("media", "Creating hash table for sessions\n");
		media->priv->sessions = g_hash_table_new(g_str_hash, g_str_equal);
	}
	g_hash_table_insert(media->priv->sessions, g_strdup(session->id), session);
}

static gboolean
purple_media_remove_session(PurpleMedia *media, PurpleMediaSession *session)
{
	return g_hash_table_remove(media->priv->sessions, session->id);
}

static FsParticipant *
purple_media_add_participant(PurpleMedia *media, const gchar *name)
{
	FsParticipant *participant = purple_media_get_participant(media, name);
	GError *err = NULL;

	if (participant)
		return participant;

	participant = fs_conference_new_participant(media->priv->conference,
						    (gchar*)name, &err);

	if (err) {
		purple_debug_error("media", "Error creating participant: %s\n",
				   err->message);
		g_error_free(err);
		return NULL;
	}

	if (!media->priv->participants) {
		purple_debug_info("media", "Creating hash table for participants\n");
		media->priv->participants = g_hash_table_new_full(g_str_hash,
				g_str_equal, g_free, NULL);
	}

	g_hash_table_insert(media->priv->participants, g_strdup(name), participant);

	return participant;
}

static PurpleMediaStream *
purple_media_insert_stream(PurpleMediaSession *session, const gchar *name, FsStream *stream)
{
	PurpleMediaStream *media_stream = g_new0(PurpleMediaStream, 1);
	media_stream->stream = stream;
	media_stream->participant = g_strdup(name);
	media_stream->session = session;

	session->media->priv->streams =
			g_list_append(session->media->priv->streams, media_stream);

	return media_stream;
}

static void
purple_media_insert_local_candidate(PurpleMediaSession *session, const gchar *name,
				     FsCandidate *candidate)
{
	PurpleMediaStream *stream = purple_media_get_stream(session->media, session->id, name);
	stream->local_candidates = g_list_append(stream->local_candidates, candidate);
}

GList *
purple_media_get_session_names(PurpleMedia *media)
{
	return g_hash_table_get_keys(media->priv->sessions);
}

void 
purple_media_get_elements(PurpleMedia *media, GstElement **audio_src, GstElement **audio_sink,
                                                  GstElement **video_src, GstElement **video_sink)
{
	GList *values = g_hash_table_get_values(media->priv->sessions);

	for (; values; values = g_list_delete_link(values, values)) {
		PurpleMediaSession *session = (PurpleMediaSession*)values->data;

		if (session->type & PURPLE_MEDIA_SEND_AUDIO && audio_src)
			*audio_src = session->src;
		if (session->type & PURPLE_MEDIA_SEND_VIDEO && video_src)
			*video_src = session->src;
	}

	values = media->priv->streams;
	for (; values; values = g_list_next(values)) {
		PurpleMediaStream *stream = (PurpleMediaStream*)values->data;

		if (stream->session->type & PURPLE_MEDIA_RECV_AUDIO && audio_sink)
			*audio_sink = stream->sink;
		if (stream->session->type & PURPLE_MEDIA_RECV_VIDEO && video_sink)
			*video_sink = stream->sink;
	}
}

void 
purple_media_set_src(PurpleMedia *media, const gchar *sess_id, GstElement *src)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	GstPad *sinkpad;
	GstPad *srcpad;
	
	if (session->src)
		gst_object_unref(session->src);
	session->src = src;
	gst_bin_add(GST_BIN(purple_media_get_pipeline(media)),
		    session->src);

	g_object_get(session->session, "sink-pad", &sinkpad, NULL);
	srcpad = gst_element_get_static_pad(src, "ghostsrc");
	purple_debug_info("media", "connecting pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK
			  ? "success" : "failure");
}

void 
purple_media_set_sink(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant, GstElement *sink)
{
	PurpleMediaStream *stream =
			purple_media_get_stream(media, sess_id, participant);

	if (stream->sink)
		gst_object_unref(stream->sink);
	stream->sink = sink;
	gst_bin_add(GST_BIN(purple_media_get_pipeline(media)),
		    stream->sink);
}

GstElement *
purple_media_get_src(PurpleMedia *media, const gchar *sess_id)
{
	return purple_media_get_session(media, sess_id)->src;
}

GstElement *
purple_media_get_sink(PurpleMedia *media, const gchar *sess_id, const gchar *participant)
{
	return purple_media_get_stream(media, sess_id, participant)->sink;
}

static PurpleMediaSession *
purple_media_session_from_fs_stream(PurpleMedia *media, FsStream *stream)
{
	FsSession *fssession;
	GList *values;

	g_object_get(stream, "session", &fssession, NULL);

	values = g_hash_table_get_values(media->priv->sessions);

	for (; values; values = g_list_delete_link(values, values)) {
		PurpleMediaSession *session = values->data;

		if (session->session == fssession) {
			g_list_free(values);
			g_object_unref(fssession);
			return session;
		}
	}

	g_object_unref(fssession);
	return NULL;
}

/* This could also emit when participants are ready */
static void
purple_media_emit_ready(PurpleMedia *media, PurpleMediaSession *session, const gchar *participant)
{
	GList *sessions;
	gboolean conf_ready = TRUE;

	if ((session != NULL) && ((media->priv->initiator == FALSE &&
			session->accepted == FALSE) ||
			(purple_media_codecs_ready(media, session->id) == FALSE)))
		return;

	sessions = g_hash_table_get_values(media->priv->sessions);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session_data = sessions->data;
		GList *streams = purple_media_get_streams(media,
				session_data->id, NULL);
		gboolean session_ready = TRUE;

		if ((media->priv->initiator == FALSE &&
				session_data->accepted == FALSE) ||
				(purple_media_codecs_ready(
				media, session_data->id) == FALSE))
			conf_ready = FALSE;

		for (; streams; streams = g_list_delete_link(streams, streams)) {
			PurpleMediaStream *stream = streams->data;
			if (stream->candidates_prepared == FALSE) {
				session_ready = FALSE;
				conf_ready = FALSE;
			} else if (session_data == session)
				g_signal_emit(media, purple_media_signals[READY_NEW],
						0, session_data->id, stream->participant);
		}

		if (session_ready == TRUE &&
				(session == session_data || session == NULL))
			g_signal_emit(media, purple_media_signals[READY_NEW],
					0, session_data->id, NULL);
	}

	if (conf_ready == TRUE) {
		g_signal_emit(media, purple_media_signals[READY_NEW],
				0, NULL, NULL);
	}
}

static gboolean
media_bus_call(GstBus *bus, GstMessage *msg, gpointer media)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_EOS:
			purple_debug_info("media", "End of Stream\n");
			break;
		case GST_MESSAGE_ERROR: {
			gchar *debug = NULL;
			GError *err = NULL;

			gst_message_parse_error(msg, &err, &debug);

			purple_debug_error("media", "gst pipeline error: %s\n", err->message);
			g_error_free(err);

			if (debug) {
				purple_debug_error("media", "Debug details: %s\n", debug);
				g_free (debug);
			}
			break;
		}
		case GST_MESSAGE_ELEMENT: {
			if (gst_structure_has_name(msg->structure, "farsight-error")) {
				FsError error_no;
				gst_structure_get_enum(msg->structure, "error-no",
						FS_TYPE_ERROR, (gint*)&error_no);
				/*
				 * Unknown CName is only a problem for the
				 * multicast transmitter which isn't used.
				 */
				if (error_no != FS_ERROR_UNKNOWN_CNAME)
					purple_debug_error("media", "farsight-error: %i: %s\n", error_no,
						  	gst_structure_get_string(msg->structure, "error-msg"));
			} else if (gst_structure_has_name(msg->structure,
					"farsight-new-local-candidate")) {
				FsStream *stream = g_value_get_object(gst_structure_get_value(msg->structure, "stream"));
				FsCandidate *local_candidate = g_value_get_boxed(gst_structure_get_value(msg->structure, "candidate"));
				PurpleMediaSession *session = purple_media_session_from_fs_stream(media, stream);
				purple_media_new_local_candidate_cb(stream, local_candidate, session);
			} else if (gst_structure_has_name(msg->structure,
					"farsight-local-candidates-prepared")) {
				FsStream *stream = g_value_get_object(gst_structure_get_value(msg->structure, "stream"));
				PurpleMediaSession *session = purple_media_session_from_fs_stream(media, stream);
				purple_media_candidates_prepared_cb(stream, session);
			} else if (gst_structure_has_name(msg->structure,
					"farsight-new-active-candidate-pair")) {
				FsStream *stream = g_value_get_object(gst_structure_get_value(msg->structure, "stream"));
				FsCandidate *local_candidate = g_value_get_boxed(gst_structure_get_value(msg->structure, "local-candidate"));
				FsCandidate *remote_candidate = g_value_get_boxed(gst_structure_get_value(msg->structure, "remote-candidate"));
				PurpleMediaSession *session = purple_media_session_from_fs_stream(media, stream);
				purple_media_candidate_pair_established_cb(stream, local_candidate, remote_candidate, session);
			} else if (gst_structure_has_name(msg->structure,
					"farsight-recv-codecs-changed")) {
				GList *codecs = g_value_get_boxed(gst_structure_get_value(msg->structure, "codecs"));
				FsCodec *codec = codecs->data;
				purple_debug_info("media", "farsight-recv-codecs-changed: %s\n", codec->encoding_name);
				
			} else if (gst_structure_has_name(msg->structure,
					"farsight-component-state-changed")) {
				
			} else if (gst_structure_has_name(msg->structure,
					"farsight-send-codec-changed")) {
				
			} else if (gst_structure_has_name(msg->structure,
					"farsight-codecs-changed")) {
				GList *sessions = g_hash_table_get_values(PURPLE_MEDIA(media)->priv->sessions);
				FsSession *fssession = g_value_get_object(gst_structure_get_value(msg->structure, "session"));
				for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
					PurpleMediaSession *session = sessions->data;
					if (session->session == fssession) {
						gboolean ready;
						gchar *session_id;

						g_object_get(session->session, "codecs-ready", &ready, NULL);
						if (session->codecs_ready == FALSE && ready == TRUE) {
							session->codecs_ready = ready;
							purple_media_emit_ready(media, session, NULL);
						}

						session_id = g_strdup(session->id);
						g_signal_emit(media, purple_media_signals[CODECS_CHANGED], 0, session_id);
						g_free(session_id);
						g_list_free(sessions);
						break;
					}
				}
			}
			break;
		}
		default:
			break;
	}

	return TRUE;
}

GstElement *
purple_media_get_pipeline(PurpleMedia *media)
{
	if (!media->priv->pipeline) {
		GstBus *bus;
		media->priv->pipeline = gst_pipeline_new(NULL);
		bus = gst_pipeline_get_bus(GST_PIPELINE(media->priv->pipeline));
		gst_bus_add_signal_watch(GST_BUS(bus));
		g_signal_connect(G_OBJECT(bus), "message",
				G_CALLBACK(media_bus_call), media);
		gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, NULL);
		gst_object_unref(bus);

		gst_bin_add(GST_BIN(media->priv->pipeline), GST_ELEMENT(media->priv->conference));
	}

	return media->priv->pipeline;
}

void
purple_media_error(PurpleMedia *media, const gchar *error, ...)
{
	va_list args;
	gchar *message;

	va_start(args, error);
	message = g_strdup_vprintf(error, args);
	va_end(args);

	purple_debug_error("media", "%s\n", message);
	g_signal_emit(media, purple_media_signals[ERROR], 0, message);

	g_free(message);
}

void
purple_media_accept(PurpleMedia *media)
{
	GList *sessions;
	GList *streams;

	sessions = g_hash_table_get_values(media->priv->sessions);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session = sessions->data;
		session->accepted = TRUE;

		if (media->priv->initiator == FALSE)
			purple_media_emit_ready(media, session, NULL);
	}

	g_signal_emit(media, purple_media_signals[ACCEPTED],
			0, NULL, NULL);
	streams = media->priv->streams;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaStream *stream = streams->data;
		g_object_set(G_OBJECT(stream->stream), "direction",
				purple_media_to_fs_stream_direction(
				stream->session->type), NULL);
	}
}

void
purple_media_hangup(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CHANGED_HANGUP,
			NULL, NULL);
	purple_media_end(media, NULL, NULL);
}

void
purple_media_reject(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CHANGED_REJECTED,
			NULL, NULL);
	purple_media_end(media, NULL, NULL);
}

void
purple_media_end(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	if (session_id == NULL && participant == NULL)
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_CHANGED_END,
				NULL, NULL);
}

GList*
purple_media_get_devices(const gchar *plugin)
{
	GObjectClass *klass;
	GstPropertyProbe *probe;
	const GParamSpec *pspec;
	GstElement *element = gst_element_factory_make(plugin, NULL);
	GstElementFactory *factory;
	const gchar *longname = NULL;
	GList *ret = NULL;

	if (element == NULL)
		return NULL;

	factory = gst_element_get_factory(element);

	longname = gst_element_factory_get_longname(factory);
	klass = G_OBJECT_GET_CLASS(element);

	if (!g_object_class_find_property (klass, "device") ||
			!GST_IS_PROPERTY_PROBE (element) ||
			!(probe = GST_PROPERTY_PROBE (element)) ||
			!(pspec = gst_property_probe_get_property (probe, "device"))) {
		purple_debug_info("media", "Found source '%s' (%s) - no device\n",
				longname, GST_PLUGIN_FEATURE (factory)->name);
	} else {
		gint n;
		gchar *name;
		GValueArray *array;

		purple_debug_info("media", "Found devices\n");

		/* Set autoprobe[-fps] to FALSE to avoid delays when probing. */
		if (g_object_class_find_property (klass, "autoprobe")) {
			g_object_set (G_OBJECT (element), "autoprobe", FALSE, NULL);
			if (g_object_class_find_property (klass, "autoprobe-fps")) {
				g_object_set (G_OBJECT (element), "autoprobe-fps", FALSE, NULL);
			}
		}

		array = gst_property_probe_probe_and_get_values (probe, pspec);
		if (array != NULL) {
			for (n = 0 ; n < array->n_values ; n++) {
				GValue *device = g_value_array_get_nth (array, n);
				
				ret = g_list_append(ret, g_value_dup_string(device));

				g_object_set(G_OBJECT(element), "device",
						g_value_get_string(device), NULL);
				g_object_get(G_OBJECT(element), "device-name", &name, NULL);
				purple_debug_info("media", "Found source '%s' (%s) - device '%s' (%s)\n",
						  longname, GST_PLUGIN_FEATURE (factory)->name,
						  name, g_value_get_string(device));
				g_free(name);
			}
			g_value_array_free(array);
		}

		/* Restore autoprobe[-fps] to TRUE. */
		if (g_object_class_find_property (klass, "autoprobe")) {
			g_object_set (G_OBJECT (element), "autoprobe", TRUE, NULL);
			if (g_object_class_find_property (klass, "autoprobe-fps")) {
				g_object_set (G_OBJECT (element), "autoprobe-fps", TRUE, NULL);
			}
		}
	}

	gst_object_unref(element);
	return ret;
}

gchar *
purple_media_element_get_device(GstElement *element)
{
	gchar *device;
	g_object_get(G_OBJECT(element), "device", &device, NULL);
	return device;
}

void
purple_media_audio_init_src(GstElement **sendbin, GstElement **sendlevel)
{
	GstElement *src;
	GstElement *volume;
	GstPad *pad;
	GstPad *ghost;
	const gchar *audio_device = purple_prefs_get_string("/purple/media/audio/device");
	double input_volume = purple_prefs_get_int("/purple/media/audio/volume/input")/10.0;

	*sendbin = gst_bin_new("purplesendaudiobin");
	src = gst_element_factory_make("alsasrc", "asrc");
	volume = gst_element_factory_make("volume", "purpleaudioinputvolume");
	g_object_set(volume, "volume", input_volume, NULL);
	*sendlevel = gst_element_factory_make("level", "sendlevel");
	gst_bin_add_many(GST_BIN(*sendbin), src, volume, *sendlevel, NULL);
	gst_element_link(src, volume);
	gst_element_link(volume, *sendlevel);
	pad = gst_element_get_pad(*sendlevel, "src");
	ghost = gst_ghost_pad_new("ghostsrc", pad);
	gst_element_add_pad(*sendbin, ghost);
	g_object_set(G_OBJECT(*sendlevel), "message", TRUE, NULL);

	if (audio_device != NULL && strcmp(audio_device, ""))
		g_object_set(G_OBJECT(src), "device", audio_device, NULL);
}

void
purple_media_video_init_src(GstElement **sendbin)
{
	GstElement *src, *tee, *queue;
	GstPad *pad;
	GstPad *ghost;
	const gchar *video_plugin = purple_prefs_get_string(
			"/purple/media/video/plugin");
	const gchar *video_device = purple_prefs_get_string(
			"/purple/media/video/device");

	*sendbin = gst_bin_new("purplesendvideobin");
	src = gst_element_factory_make(video_plugin, "purplevideosource");
	gst_bin_add(GST_BIN(*sendbin), src);

	tee = gst_element_factory_make("tee", "purplevideosrctee");
	gst_bin_add(GST_BIN(*sendbin), tee);
	gst_element_link(src, tee);

	queue = gst_element_factory_make("queue", NULL);
	gst_bin_add(GST_BIN(*sendbin), queue);
	gst_element_link(tee, queue);

	if (!strcmp(video_plugin, "videotestsrc")) {
		/* unless is-live is set to true it doesn't throttle videotestsrc */
		g_object_set (G_OBJECT(src), "is-live", TRUE, NULL);
	}

	pad = gst_element_get_static_pad(queue, "src");
	ghost = gst_ghost_pad_new("ghostsrc", pad);
	gst_object_unref(pad);
	gst_element_add_pad(*sendbin, ghost);

	if (video_device != NULL && strcmp(video_device, ""))
		g_object_set(G_OBJECT(src), "device", video_device, NULL);
}

void
purple_media_audio_init_recv(GstElement **recvbin, GstElement **recvlevel)
{
	GstElement *sink, *volume;
	GstPad *pad, *ghost;
	double output_volume = purple_prefs_get_int(
			"/purple/media/audio/volume/output")/10.0;

	*recvbin = gst_bin_new("pidginrecvaudiobin");
	sink = gst_element_factory_make("alsasink", "asink");
	g_object_set(G_OBJECT(sink), "sync", FALSE, NULL);
	volume = gst_element_factory_make("volume", "purpleaudiooutputvolume");
	g_object_set(volume, "volume", output_volume, NULL);
	*recvlevel = gst_element_factory_make("level", "recvlevel");
	gst_bin_add_many(GST_BIN(*recvbin), sink, volume, *recvlevel, NULL);
	gst_element_link(*recvlevel, sink);
	gst_element_link(volume, *recvlevel);
	pad = gst_element_get_pad(volume, "sink");
	ghost = gst_ghost_pad_new("ghostsink", pad);
	gst_element_add_pad(*recvbin, ghost);
	g_object_set(G_OBJECT(*recvlevel), "message", TRUE, NULL);
}

void
purple_media_video_init_recv(GstElement **recvbin)
{
	GstElement *sink;
	GstPad *pad, *ghost;

	*recvbin = gst_bin_new("fakebin");
	sink = gst_element_factory_make("fakesink", NULL);
	gst_bin_add(GST_BIN(*recvbin), sink);
	pad = gst_element_get_pad(sink, "sink");
	ghost = gst_ghost_pad_new("ghostsink", pad);
	gst_element_add_pad(*recvbin, ghost);
}

static void
purple_media_new_local_candidate_cb(FsStream *stream,
				    FsCandidate *local_candidate,
				    PurpleMediaSession *session)
{
	gchar *name;
	FsParticipant *participant;
	PurpleMediaCandidate *candidate;
	purple_debug_info("media", "got new local candidate: %s\n", local_candidate->foundation);
	g_object_get(stream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	purple_media_insert_local_candidate(session, name, fs_candidate_copy(local_candidate));

	candidate = purple_media_candidate_from_fs(local_candidate);
	g_signal_emit(session->media, purple_media_signals[NEW_CANDIDATE],
		      0, session->id, name, candidate);
	purple_media_candidate_free(candidate);

	g_free(name);
}

static void
purple_media_candidates_prepared_cb(FsStream *stream, PurpleMediaSession *session)
{
	gchar *name;
	FsParticipant *participant;
	PurpleMediaStream *stream_data;

	g_object_get(stream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	stream_data = purple_media_get_stream(session->media, session->id, name);
	stream_data->candidates_prepared = TRUE;

	purple_media_emit_ready(session->media, session, name);
	g_free(name);
}

/* callback called when a pair of transport candidates (local and remote)
 * has been established */
static void
purple_media_candidate_pair_established_cb(FsStream *fsstream,
					   FsCandidate *native_candidate,
					   FsCandidate *remote_candidate,
					   PurpleMediaSession *session)
{
	gchar *name;
	FsParticipant *participant;
	PurpleMediaStream *stream;

	g_object_get(fsstream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	stream = purple_media_get_stream(session->media, session->id, name);

	stream->local_candidate = fs_candidate_copy(native_candidate);
	stream->remote_candidate = fs_candidate_copy(remote_candidate);

	purple_debug_info("media", "candidate pair established\n");
}

static gboolean
purple_media_connected_cb(PurpleMediaStream *stream)
{
	g_signal_emit(stream->session->media,
			purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CHANGED_CONNECTED,
			stream->session->id, stream->participant);
	return FALSE;
}

static void
purple_media_src_pad_added_cb(FsStream *fsstream, GstPad *srcpad,
			      FsCodec *codec, PurpleMediaStream *stream)
{
	PurpleMediaSessionType type = purple_media_from_fs(codec->media_type, FS_DIRECTION_RECV);
	GstPad *sinkpad = NULL;

	if (stream->sink == NULL)
		stream->sink = purple_media_manager_get_element(
			purple_media_manager_get(), type);

	gst_bin_add(GST_BIN(purple_media_get_pipeline(stream->session->media)),
		    stream->sink);
	sinkpad = gst_element_get_static_pad(stream->sink, "ghostsink");
	gst_pad_link(srcpad, sinkpad);
	gst_element_set_state(stream->sink, GST_STATE_PLAYING);

	g_timeout_add(0, (GSourceFunc)purple_media_connected_cb, stream);
}

static gboolean
purple_media_add_stream_internal(PurpleMedia *media, const gchar *sess_id,
				 const gchar *who, FsMediaType type,
				 FsStreamDirection type_direction,
				 const gchar *transmitter,
				 guint num_params, GParameter *params)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsParticipant *participant = NULL;
	PurpleMediaStream *stream = NULL;
	FsStreamDirection *direction = NULL;

	if (!session) {
		GError *err = NULL;
		GList *codec_conf = NULL;
		gchar *filename = NULL;

		session = g_new0(PurpleMediaSession, 1);

		session->session = fs_conference_new_session(media->priv->conference, type, &err);

		if (err != NULL) {
			purple_media_error(media, "Error creating session: %s\n", err->message);
			g_error_free(err);
			g_free(session);
			return FALSE;
		}

	/* XXX: SPEEX has a latency of 5 or 6 seconds for me */
#if 0
	/* SPEEX is added through the configuration */
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 8000));
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 16000));
#endif

		filename = g_build_filename(purple_user_dir(), "fs-codec.conf", NULL);
		codec_conf = fs_codec_list_from_keyfile(filename, &err);
		g_free(filename);

		if (err != NULL) {
			purple_debug_error("media", "Error reading codec configuration file: %s\n", err->message);
			g_error_free(err);
		}

		fs_session_set_codec_preferences(session->session, codec_conf, NULL);

		/*
		 * Removes a 5-7 second delay before
		 * receiving the src-pad-added signal.
		 * Only works for non-multicast FsRtpSessions.
		 */
		if (!strcmp(transmitter, "nice") || !strcmp(transmitter, "rawudp"))
			g_object_set(G_OBJECT(session->session),
					"no-rtcp-timeout", 0, NULL);

		fs_codec_list_destroy(codec_conf);

		session->id = g_strdup(sess_id);
		session->media = media;
		session->type = purple_media_from_fs(type, type_direction);

		purple_media_add_session(media, session);
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_CHANGED_NEW,
				session->id, NULL);
	}

	if (!(participant = purple_media_add_participant(media, who))) {
		purple_media_remove_session(media, session);
		g_free(session);
		return FALSE;
	} else {
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_CHANGED_NEW,
				NULL, who);
	}

	stream = purple_media_get_stream(media, sess_id, who);

	if (!stream) {
		GError *err = NULL;
		FsStream *fsstream = NULL;
		const gchar *stun_ip = purple_network_get_stun_ip();
		const gchar *turn_ip = purple_network_get_turn_ip();

		if (stun_ip || turn_ip) {
			guint new_num_params = 
				stun_ip && turn_ip ? num_params + 2 : num_params + 1;
			guint next_param_index = num_params;
			GParameter *param = g_new0(GParameter, new_num_params);
			memcpy(param, params, sizeof(GParameter) * num_params);

			if (stun_ip) {
				purple_debug_info("media", 
					"setting property stun-ip on new stream: %s\n", stun_ip);

				param[next_param_index].name = "stun-ip";
				g_value_init(&param[next_param_index].value, G_TYPE_STRING);
				g_value_set_string(&param[next_param_index].value, stun_ip);
				next_param_index++;
			}

			if (turn_ip) {
				GValueArray *relay_info = g_value_array_new(0);
				GValue value;
				gint turn_port = 
					purple_prefs_get_int("/purple/network/turn_port");
				const gchar *username =
					purple_prefs_get_string("/purple/network/turn_username");
				const gchar *password =
					purple_prefs_get_string("/purple/network/turn_password");
				GstStructure *turn_setup = gst_structure_new("relay-info",
					"ip", G_TYPE_STRING, turn_ip, 
					"port", G_TYPE_UINT, turn_port,
					"username", G_TYPE_STRING, username,
					"password", G_TYPE_STRING, password,
					NULL);

				if (turn_setup) {
					memset(&value, 0, sizeof(GValue));
					g_value_init(&value, GST_TYPE_STRUCTURE);
					gst_value_set_structure(&value, turn_setup);
					relay_info = g_value_array_append(relay_info, &value);
					gst_structure_free(turn_setup);

					purple_debug_info("media",
						"setting property relay-info on new stream\n");
					param[next_param_index].name = "relay-info";
					g_value_init(&param[next_param_index].value, 
						G_TYPE_VALUE_ARRAY);
					g_value_set_boxed(&param[next_param_index].value,
						relay_info);
					g_value_array_free(relay_info);
				} else {
					purple_debug_error("media", "Error relay info");
					g_object_unref(participant);
					g_hash_table_remove(media->priv->participants, who);
					purple_media_remove_session(media, session);
					g_free(session);
					return FALSE;
				}
			}

			fsstream = fs_session_new_stream(session->session,
					participant, type_direction &
					FS_DIRECTION_RECV, transmitter,
					new_num_params, param, &err);
			g_free(param);
		} else {
			fsstream = fs_session_new_stream(session->session,
					participant, type_direction &
					FS_DIRECTION_RECV, transmitter,
					num_params, params, &err);
		}

		if (err) {
			purple_debug_error("media", "Error creating stream: %s\n",
					   err->message);
			g_error_free(err);
			g_object_unref(participant);
			g_hash_table_remove(media->priv->participants, who);
			purple_media_remove_session(media, session);
			g_free(session);
			return FALSE;
		}

		stream = purple_media_insert_stream(session, who, fsstream);

		/* callback for source pad added (new stream source ready) */
		g_signal_connect(G_OBJECT(fsstream),
				 "src-pad-added", G_CALLBACK(purple_media_src_pad_added_cb), stream);

		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_CHANGED_NEW,
				session->id, who);
	} else if (*direction != type_direction) {	
		/* change direction */
		g_object_set(stream->stream, "direction", type_direction, NULL);
	}

	return TRUE;
}

gboolean
purple_media_add_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who,
			PurpleMediaSessionType type,
			const gchar *transmitter,
			guint num_params, GParameter *params)
{
	FsStreamDirection type_direction;

	if (type & PURPLE_MEDIA_AUDIO) {
		type_direction = purple_media_to_fs_stream_direction(type & PURPLE_MEDIA_AUDIO);

		if (!purple_media_add_stream_internal(media, sess_id, who,
						      FS_MEDIA_TYPE_AUDIO, type_direction,
						      transmitter, num_params, params)) {
			return FALSE;
		}
	}
	else if (type & PURPLE_MEDIA_VIDEO) {
		type_direction = purple_media_to_fs_stream_direction(type & PURPLE_MEDIA_VIDEO);

		if (!purple_media_add_stream_internal(media, sess_id, who,
						      FS_MEDIA_TYPE_VIDEO, type_direction,
						      transmitter, num_params, params)) {
			return FALSE;
		}
	}
	return TRUE;
}

void
purple_media_remove_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who)
{
	/* Add state-changed end emits in here when this is implemented */
}

PurpleMediaSessionType
purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return session->type;
}
/* XXX: Should wait until codecs-ready is TRUE before using this function */
GList *
purple_media_get_codecs(PurpleMedia *media, const gchar *sess_id)
{
	GList *fscodecs;
	GList *codecs;
	g_object_get(G_OBJECT(purple_media_get_session(media, sess_id)->session),
		     "codecs", &fscodecs, NULL);
	codecs = purple_media_codec_list_from_fs(fscodecs);
	fs_codec_list_destroy(fscodecs);
	return codecs;
}

GList *
purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	PurpleMediaStream *stream = purple_media_get_stream(media, sess_id, name);
	return purple_media_candidate_list_from_fs(stream->local_candidates);
}

void
purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
				   const gchar *name, GList *remote_candidates)
{
	PurpleMediaStream *stream = purple_media_get_stream(media, sess_id, name);
	GError *err = NULL;

	stream->remote_candidates = g_list_concat(stream->remote_candidates,
			purple_media_candidate_list_to_fs(remote_candidates));

	fs_stream_set_remote_candidates(stream->stream,
			stream->remote_candidates, &err);

	if (err) {
		purple_debug_error("media", "Error adding remote"
				" candidates: %s\n", err->message);
		g_error_free(err);
	}
}

PurpleMediaCandidate *
purple_media_get_local_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	return purple_media_candidate_from_fs(purple_media_get_stream(
			media, sess_id, name)->local_candidate);
}

PurpleMediaCandidate *
purple_media_get_remote_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	return purple_media_candidate_from_fs(purple_media_get_stream(
			media, sess_id, name)->remote_candidate);
}

gboolean
purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id, const gchar *name, GList *codecs)
{
	FsStream *stream = purple_media_get_stream(media, sess_id, name)->stream;
	GList *fscodecs = purple_media_codec_list_to_fs(codecs);
	GError *err = NULL;

	fs_stream_set_remote_codecs(stream, fscodecs, &err);
	fs_codec_list_destroy(fscodecs);

	if (err) {
		purple_debug_error("media", "Error setting remote codecs: %s\n",
				   err->message);
		g_error_free(err);
		return FALSE;
	}
	return TRUE;
}

gboolean
purple_media_candidates_prepared(PurpleMedia *media, const gchar *name)
{
	GList *sessions = purple_media_get_session_names(media);

	for (; sessions; sessions = sessions->next) {
		const gchar *session = sessions->data;
		if (!purple_media_get_local_candidate(media, session, name) ||
				!purple_media_get_remote_candidate(media, session, name))
			return FALSE;
	}

	return TRUE;
}

gboolean
purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, PurpleMediaCodec *codec)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsCodec *fscodec = purple_media_codec_to_fs(codec);
	GError *err = NULL;

	fs_session_set_send_codec(session->session, fscodec, &err);
	fs_codec_destroy(fscodec);

	if (err) {
		purple_debug_error("media", "Error setting send codec\n");
		g_error_free(err);
		return FALSE;
	}
	return TRUE;
}

gboolean
purple_media_codecs_ready(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	gboolean ret;
	g_object_get(session->session, "codecs-ready", &ret, NULL);
	return ret;
}

void purple_media_mute(PurpleMedia *media, gboolean active)
{
	GList *sessions = g_hash_table_get_values(media->priv->sessions);
	purple_debug_info("media", "Turning mute %s\n", active ? "on" : "off");

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session = sessions->data;
		if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
			GstElement *volume = gst_bin_get_by_name(
					GST_BIN(session->src),
					"purpleaudioinputvolume");
			g_object_set(volume, "mute", active, NULL);
		}
	}
}

void purple_media_set_input_volume(PurpleMedia *media,
		const gchar *session_id, double level)
{
	GList *sessions;

	if (session_id == NULL)
		sessions = g_hash_table_get_values(media->priv->sessions);
	else
		sessions = g_list_append(NULL,
				purple_media_get_session(media, session_id));

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session = sessions->data;

		if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
			GstElement *volume = gst_bin_get_by_name(
					GST_BIN(session->src),
					"purpleaudioinputvolume");
			g_object_set(volume, "volume", level, NULL);
		}
	}
}

void purple_media_set_output_volume(PurpleMedia *media,
		const gchar *session_id, const gchar *participant,
		double level)
{
	GList *streams = purple_media_get_streams(media,
				session_id, participant);

	for (; streams; streams = g_list_delete_link(streams, streams)) {
		PurpleMediaStream *stream = streams->data;

		if (stream->session->type & PURPLE_MEDIA_RECV_AUDIO) {
			GstElement *volume = gst_bin_get_by_name(
					GST_BIN(stream->sink),
					"purpleaudiooutputvolume");
			g_object_set(volume, "volume", level, NULL);
		}
	}
}

typedef struct
{
	PurpleMedia *media;
	gchar *session_id;
	gchar *participant;
	gulong handler_id;
} PurpleMediaXOverlayData;

static void
window_id_cb(GstBus *bus, GstMessage *msg, PurpleMediaXOverlayData *data)
{
	gchar *name;
	gchar *cmpname;
	gulong window_id;
	gboolean set = FALSE;

	if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ELEMENT ||
			!gst_structure_has_name(msg->structure,
			"prepare-xwindow-id"))
		return;

	name = gst_object_get_name(GST_MESSAGE_SRC(msg));

	if (data->participant == NULL) {
		cmpname = g_strdup_printf("session-sink_%s",
				data->session_id);
		if (!strncmp(name, cmpname, strlen(cmpname))) {
			PurpleMediaSession *session =
					purple_media_get_session(
					data->media, data->session_id);
			if (session != NULL) {
				window_id = session->window_id;
				set = TRUE;
			}
		}
	} else {
		cmpname = g_strdup_printf("stream-sink_%s_%s",
				data->session_id, data->participant);

		if (!strncmp(name, cmpname, strlen(cmpname))) {
			PurpleMediaStream *stream =
					purple_media_get_stream(
					data->media, data->session_id,
					data->participant);
			if (stream != NULL) {
				window_id = stream->window_id;
				set = TRUE;
			}
		}
	}
	g_free(cmpname);
	g_free(name);

	if (set == TRUE) {
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(
				purple_media_get_pipeline(data->media)));
		g_signal_handler_disconnect(bus, data->handler_id);
		gst_object_unref(bus);

		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(
				GST_MESSAGE_SRC(msg)), window_id);

		g_free(data->session_id);
		g_free(data->participant);
		g_free(data);
	}
		
	return;
}

gboolean
purple_media_set_output_window(PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id)
{

	if (session_id != NULL && participant == NULL) {

		PurpleMediaSession *session;
		session = purple_media_get_session(media, session_id);

		if (session == NULL)
			return FALSE;

		session->window_id = window_id;

		if (session->sink == NULL) {
			GstBus *bus;
			GstElement *tee, *bin, *queue, *sink;
			GstPad *request_pad, *sinkpad, *ghostpad;
			gchar *name;

			/* Create callback for prepared-xwindow-id signal */
			PurpleMediaXOverlayData *data =
					g_new0(PurpleMediaXOverlayData, 1);
			data->media = media;
			data->session_id = g_strdup(session_id);
			data->participant = g_strdup(participant);

			bus = gst_pipeline_get_bus(GST_PIPELINE(
					purple_media_get_pipeline(media)));
			data->handler_id = g_signal_connect(bus,
					"sync-message::element",
					G_CALLBACK(window_id_cb), data);
			gst_object_unref(bus);

			/* Create sink */
			tee = gst_bin_get_by_name(GST_BIN(session->src),
					"purplevideosrctee");
			bin = gst_bin_new(NULL);
			gst_bin_add(GST_BIN(GST_ELEMENT_PARENT(tee)), bin);

			queue = gst_element_factory_make("queue", NULL);
			name = g_strdup_printf(
					"session-sink_%s", session_id);
			sink = gst_element_factory_make(
					"autovideosink", name);
			g_free(name);

			gst_bin_add_many(GST_BIN(bin), queue, sink, NULL);
			gst_element_link(queue, sink);

			sinkpad = gst_element_get_static_pad(queue, "sink");
			ghostpad = gst_ghost_pad_new("ghostsink", sinkpad);
			gst_object_unref(sinkpad);
			gst_element_add_pad(bin, ghostpad);

			gst_element_set_state(bin, GST_STATE_PLAYING);

			request_pad = gst_element_get_request_pad(
					tee, "src%d");
			gst_pad_link(request_pad, ghostpad);
			gst_object_unref(request_pad);

			session->sink = bin;
			return TRUE;
		} else {
			/* Changing the XOverlay output window */
			GstElement *xoverlay = gst_bin_get_by_interface(
					GST_BIN(session->sink),
					GST_TYPE_X_OVERLAY);
			if (xoverlay != NULL) {
				gst_x_overlay_set_xwindow_id(
					GST_X_OVERLAY(xoverlay),
					window_id);
			}
			return FALSE;
		}
	} else if (session_id != NULL && participant != NULL) {
		PurpleMediaStream *stream = purple_media_get_stream(
				media, session_id, participant);
		GstBus *bus;
		GstElement *bin, *sink;
		GstPad *pad, *peer = NULL, *ghostpad;
		PurpleMediaXOverlayData *data;
		gchar *name;

		if (stream == NULL)
			return FALSE;

		stream->window_id = window_id;

		if (stream->sink != NULL) {
			gboolean is_fakebin;
			name = gst_element_get_name(stream->sink);
			is_fakebin = !strcmp(name, "fakebin");
			g_free(name);

			if (is_fakebin) {
				pad = gst_element_get_static_pad(
						stream->sink, "ghostsink");
				peer = gst_pad_get_peer(pad);

				gst_pad_unlink(peer, pad);
				gst_object_unref(pad);
				gst_element_set_state(stream->sink,
						GST_STATE_NULL);
				gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(
						stream->sink)), stream->sink);
			} else {
				/* Changing the XOverlay output window */
				GstElement *xoverlay =
						gst_bin_get_by_interface(
						GST_BIN(stream->sink),
						GST_TYPE_X_OVERLAY);
				if (xoverlay != NULL) {
					gst_x_overlay_set_xwindow_id(
						GST_X_OVERLAY(xoverlay),
						window_id);
					return TRUE;
				}
				return FALSE;
			}
		}

		data = g_new0(PurpleMediaXOverlayData, 1);
		data->media = media;
		data->session_id = g_strdup(session_id);
		data->participant = g_strdup(participant);

		bus = gst_pipeline_get_bus(GST_PIPELINE(
				purple_media_get_pipeline(media)));
		data->handler_id = g_signal_connect(bus,
				"sync-message::element",
				G_CALLBACK(window_id_cb), data);
		gst_object_unref(bus);

		bin = gst_bin_new(NULL);

		if (stream->sink != NULL)
			gst_bin_add(GST_BIN(GST_ELEMENT_PARENT(
					stream->sink)), bin);

		name = g_strdup_printf("stream-sink_%s_%s",
				session_id, participant);
		sink = gst_element_factory_make("autovideosink", name);
		g_free(name);

		gst_bin_add(GST_BIN(bin), sink);
		pad = gst_element_get_static_pad(sink, "sink");
		ghostpad = gst_ghost_pad_new("ghostsink", pad);
		gst_element_add_pad(bin, ghostpad);

		if (stream->sink != NULL) {
			gst_element_set_state(bin, GST_STATE_PLAYING);
			gst_pad_link(peer, ghostpad);
		}

		stream->sink = bin;
		return TRUE;
	}
	return FALSE;
}

#endif  /* USE_VV */
