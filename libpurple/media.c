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
	GstElement *tee;
	FsSession *session;

	PurpleMediaSessionType type;

	gulong window_id;
};

struct _PurpleMediaStream
{
	PurpleMediaSession *session;
	gchar *participant;
	FsStream *stream;
	GstElement *src;
	GstElement *tee;

	GList *local_candidates;
	GList *remote_candidates;

	gboolean accepted;
	gboolean candidates_prepared;

	GList *active_local_candidates;
	GList *active_remote_candidates;

	gulong window_id;
	guint connected_cb_id;
};

struct _PurpleMediaPrivate
{
	PurpleMediaManager *manager;
	PurpleConnection *pc;
	FsConference *conference;
	gboolean initiator;
	gpointer prpl_data;

	GHashTable *sessions;	/* PurpleMediaSession table */
	GHashTable *participants; /* FsParticipant table */

	GList *streams;		/* PurpleMediaStream table */

	GstElement *confbin;
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
static gboolean media_bus_call(GstBus *bus,
		GstMessage *msg, PurpleMedia *media);

static GObjectClass *parent_class = NULL;



enum {
	ERROR,
	ACCEPTED,
	CANDIDATES_PREPARED,
	CODECS_CHANGED,
	NEW_CANDIDATE,
	STATE_CHANGED,
	STREAM_INFO,
	LAST_SIGNAL
};
static guint purple_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_MANAGER,
	PROP_CONNECTION,
	PROP_CONFERENCE,
	PROP_INITIATOR,
	PROP_PRPL_DATA,
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
			{ PURPLE_MEDIA_STATE_NEW,
				"PURPLE_MEDIA_STATE_NEW", "new" },
			{ PURPLE_MEDIA_STATE_CONNECTED,
				"PURPLE_MEDIA_STATE_CONNECTED", "connected" },
			{ PURPLE_MEDIA_STATE_END,
				"PURPLE_MEDIA_STATE_END", "end" },
			{ 0, NULL, NULL }
		};
		type = g_enum_register_static("PurpleMediaState", values);
	}
	return type;
}

GType
purple_media_info_type_get_type()
{
	static GType type = 0;
	if (type == 0) {
		static const GEnumValue values[] = {
			{ PURPLE_MEDIA_INFO_HANGUP,
					"PURPLE_MEDIA_INFO_HANGUP", "hangup" },
			{ PURPLE_MEDIA_INFO_REJECT,
					"PURPLE_MEDIA_INFO_REJECT", "reject" },
			{ PURPLE_MEDIA_INFO_MUTE,
					"PURPLE_MEDIA_INFO_MUTE", "mute" },
			{ PURPLE_MEDIA_INFO_HOLD,
					"PURPLE_MEDIA_INFO_HOLD", "hold" },
			{ 0, NULL, NULL }
		};
		type = g_enum_register_static("PurpleMediaInfoType", values);
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

	g_object_class_install_property(gobject_class, PROP_MANAGER,
			g_param_spec_object("manager",
			"Purple Media Manager",
			"The media manager that contains this media session.",
			PURPLE_TYPE_MEDIA_MANAGER,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CONNECTION,
			g_param_spec_pointer("connection",
			"PurpleConnection",
			"The connection this media session is on.",
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

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

	g_object_class_install_property(gobject_class, PROP_PRPL_DATA,
			g_param_spec_pointer("prpl-data",
			"gpointer",
			"Data the prpl plugin set on the media session.",
			G_PARAM_READWRITE));

	purple_media_signals[ERROR] = g_signal_new("error", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_signals[ACCEPTED] = g_signal_new("accepted", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	purple_media_signals[CANDIDATES_PREPARED] = g_signal_new("candidates-prepared", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING,
					 G_TYPE_STRING);
	purple_media_signals[CODECS_CHANGED] = g_signal_new("codecs-changed", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_signals[NEW_CANDIDATE] = g_signal_new("new-candidate", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__POINTER_POINTER_OBJECT,
					 G_TYPE_NONE, 3, G_TYPE_POINTER,
					 G_TYPE_POINTER, PURPLE_TYPE_MEDIA_CANDIDATE);
	purple_media_signals[STATE_CHANGED] = g_signal_new("state-changed", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__ENUM_STRING_STRING,
					 G_TYPE_NONE, 3, PURPLE_MEDIA_TYPE_STATE,
					 G_TYPE_STRING, G_TYPE_STRING);
	purple_media_signals[STREAM_INFO] = g_signal_new("stream-info", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__ENUM_STRING_STRING,
					 G_TYPE_NONE, 3, PURPLE_MEDIA_TYPE_INFO_TYPE,
					 G_TYPE_STRING, G_TYPE_STRING);
	g_type_class_add_private(klass, sizeof(PurpleMediaPrivate));
}


static void
purple_media_init (PurpleMedia *media)
{
	media->priv = PURPLE_MEDIA_GET_PRIVATE(media);
	memset(media->priv, 0, sizeof(*media->priv));
}

static void
purple_media_stream_free(PurpleMediaStream *stream)
{
	if (stream == NULL)
		return;

	/* Remove the connected_cb timeout */
	if (stream->connected_cb_id != 0)
		purple_timeout_remove(stream->connected_cb_id);

	g_free(stream->participant);

	if (stream->local_candidates)
		fs_candidate_list_destroy(stream->local_candidates);
	if (stream->remote_candidates)
		fs_candidate_list_destroy(stream->remote_candidates);

	if (stream->active_local_candidates)
		fs_candidate_list_destroy(stream->active_local_candidates);
	if (stream->active_remote_candidates)
		fs_candidate_list_destroy(stream->active_remote_candidates);

	g_free(stream);
}

static void
purple_media_session_free(PurpleMediaSession *session)
{
	if (session == NULL)
		return;

	g_free(session->id);
	g_free(session);
}

static void
purple_media_dispose(GObject *media)
{
	PurpleMediaPrivate *priv = PURPLE_MEDIA_GET_PRIVATE(media);
	GList *iter = NULL;

	purple_debug_info("media","purple_media_dispose\n");

	purple_media_manager_remove_media(priv->manager, PURPLE_MEDIA(media));

	if (priv->confbin) {
		gst_element_set_locked_state(priv->confbin, TRUE);
		gst_element_set_state(GST_ELEMENT(priv->confbin),
				GST_STATE_NULL);
		gst_bin_remove(GST_BIN(purple_media_manager_get_pipeline(
				priv->manager)), priv->confbin);
		priv->confbin = NULL;
		priv->conference = NULL;
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

	if (priv->manager) {
		GstElement *pipeline = purple_media_manager_get_pipeline(
				priv->manager);
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		g_signal_handlers_disconnect_matched(G_OBJECT(bus),
				G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
				0, 0, 0, media_bus_call, media);
		gst_object_unref(bus);

		g_object_unref(priv->manager);
		priv->manager = NULL;
	}

	G_OBJECT_CLASS(parent_class)->dispose(media);
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

	G_OBJECT_CLASS(parent_class)->finalize(media);
}

static void
purple_media_setup_pipeline(PurpleMedia *media)
{
	GstBus *bus;
	gchar *name;
	GstElement *pipeline;

	if (media->priv->conference == NULL || media->priv->manager == NULL)
		return;

	pipeline = purple_media_manager_get_pipeline(media->priv->manager);

	name = g_strdup_printf("conf_%p",
			media->priv->conference);
	media->priv->confbin = gst_bin_new(name);
	g_free(name);

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	g_signal_connect(G_OBJECT(bus), "message",
			G_CALLBACK(media_bus_call), media);
	gst_object_unref(bus);

	gst_bin_add(GST_BIN(pipeline),
			media->priv->confbin);
	gst_bin_add(GST_BIN(media->priv->confbin),
			GST_ELEMENT(media->priv->conference));
	gst_element_set_state(GST_ELEMENT(media->priv->confbin),
			GST_STATE_PLAYING);
}

static void
purple_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PurpleMedia *media;
	g_return_if_fail(PURPLE_IS_MEDIA(object));

	media = PURPLE_MEDIA(object);

	switch (prop_id) {
		case PROP_MANAGER:
			media->priv->manager = g_value_get_object(value);
			g_object_ref(media->priv->manager);

			purple_media_setup_pipeline(media);
			break;
		case PROP_CONNECTION:
			media->priv->pc = g_value_get_pointer(value);
			break;
		case PROP_CONFERENCE: {
			if (media->priv->conference)
				gst_object_unref(media->priv->conference);
			media->priv->conference = g_value_get_object(value);
			gst_object_ref(media->priv->conference);

			purple_media_setup_pipeline(media);
			break;
		}
		case PROP_INITIATOR:
			media->priv->initiator = g_value_get_boolean(value);
			break;
		case PROP_PRPL_DATA:
			media->priv->prpl_data = g_value_get_pointer(value);
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
		case PROP_MANAGER:
			g_value_set_object(value, media->priv->manager);
			break;
		case PROP_CONNECTION:
			g_value_set_pointer(value, media->priv->pc);
			break;
		case PROP_CONFERENCE:
			g_value_set_object(value, media->priv->conference);
			break;
		case PROP_INITIATOR:
			g_value_set_boolean(value, media->priv->initiator);
			break;
		case PROP_PRPL_DATA:
			g_value_set_pointer(value, media->priv->prpl_data);
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

PurpleMediaCandidate *
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

	g_return_if_fail(codec != NULL);
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
	g_return_if_fail(codec != NULL && param != NULL);

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

PurpleMediaCodec *
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

static PurpleMediaSession*
purple_media_get_session(PurpleMedia *media, const gchar *sess_id)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	return (PurpleMediaSession*) (media->priv->sessions) ?
			g_hash_table_lookup(media->priv->sessions, sess_id) : NULL;
}

static FsParticipant*
purple_media_get_participant(PurpleMedia *media, const gchar *name)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	return (FsParticipant*) (media->priv->participants) ?
			g_hash_table_lookup(media->priv->participants, name) : NULL;
}

static PurpleMediaStream*
purple_media_get_stream(PurpleMedia *media, const gchar *session, const gchar *participant)
{
	GList *streams;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);

	streams = media->priv->streams;

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
	GList *streams;
	GList *ret = NULL;
	
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);

	streams = media->priv->streams;

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
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	g_return_if_fail(session != NULL);

	if (!media->priv->sessions) {
		purple_debug_info("media", "Creating hash table for sessions\n");
		media->priv->sessions = g_hash_table_new(g_str_hash, g_str_equal);
	}
	g_hash_table_insert(media->priv->sessions, g_strdup(session->id), session);
}

static gboolean
purple_media_remove_session(PurpleMedia *media, PurpleMediaSession *session)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);
	return g_hash_table_remove(media->priv->sessions, session->id);
}

static FsParticipant *
purple_media_add_participant(PurpleMedia *media, const gchar *name)
{
	FsParticipant *participant;
	GError *err = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);

	participant = purple_media_get_participant(media, name);

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
	PurpleMediaStream *media_stream;
	
	g_return_val_if_fail(session != NULL, NULL);

	media_stream = g_new0(PurpleMediaStream, 1);
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
	PurpleMediaStream *stream;

	g_return_if_fail(session != NULL);

	stream = purple_media_get_stream(session->media, session->id, name);
	stream->local_candidates = g_list_append(stream->local_candidates, candidate);
}

GList *
purple_media_get_session_names(PurpleMedia *media)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	return media->priv->sessions != NULL ?
			g_hash_table_get_keys(media->priv->sessions) : NULL;
}

static void 
purple_media_set_src(PurpleMedia *media, const gchar *sess_id, GstElement *src)
{
	PurpleMediaSession *session;
	GstPad *sinkpad;
	GstPad *srcpad;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	session = purple_media_get_session(media, sess_id);

	if (session == NULL) {
		purple_debug_warning("media", "purple_media_set_src: trying"
				" to set src on non-existent session\n");
		return;
	}

	if (session->src)
		gst_object_unref(session->src);
	session->src = src;
	gst_element_set_locked_state(session->src, TRUE);
	gst_bin_add(GST_BIN(session->media->priv->confbin),
		    session->src);

	session->tee = gst_element_factory_make("tee", NULL);
	gst_bin_add(GST_BIN(session->media->priv->confbin), session->tee);
	gst_element_link(session->src, session->tee);
	gst_element_set_state(session->tee, GST_STATE_PLAYING);

	g_object_get(session->session, "sink-pad", &sinkpad, NULL);
	srcpad = gst_element_get_request_pad(session->tee, "src%d");
	purple_debug_info("media", "connecting pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK
			  ? "success" : "failure");
	gst_element_set_locked_state(session->src, FALSE);
}

#if 0
static void 
purple_media_set_sink(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant, GstElement *sink)
{
	PurpleMediaStream *stream;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	stream = purple_media_get_stream(media, sess_id, participant);

	if (stream == NULL) {
		purple_debug_warning("media", "purple_media_set_sink: trying"
				" to set sink on non-existent stream\n");
		return;
	}

	if (stream->sink)
		gst_object_unref(stream->sink);
	stream->sink = sink;
	gst_bin_add(GST_BIN(stream->session->media->priv->confbin),
		    stream->sink);
}
#endif

GstElement *
purple_media_get_src(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	session = purple_media_get_session(media, sess_id);
	return (session != NULL) ? session->src : NULL;
}

static PurpleMediaSession *
purple_media_session_from_fs_stream(PurpleMedia *media, FsStream *stream)
{
	FsSession *fssession;
	GList *values;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	g_return_val_if_fail(FS_IS_STREAM(stream), NULL);

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

static gboolean
media_bus_call(GstBus *bus, GstMessage *msg, PurpleMedia *media)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ELEMENT: {
			if (!FS_IS_CONFERENCE(GST_MESSAGE_SRC(msg)) ||
					!PURPLE_IS_MEDIA(media) ||
					media->priv->conference !=
					FS_CONFERENCE(GST_MESSAGE_SRC(msg)))
				break;

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
				FsStreamState fsstate = g_value_get_enum(gst_structure_get_value(msg->structure, "state"));
				guint component = g_value_get_uint(gst_structure_get_value(msg->structure, "component"));
				const gchar *state;
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
				purple_debug_info("media", "farsight-component-state-changed: component: %u state: %s\n", component, state);
			} else if (gst_structure_has_name(msg->structure,
					"farsight-send-codec-changed")) {
				FsCodec *codec = g_value_get_boxed(gst_structure_get_value(msg->structure, "codec"));
				gchar *codec_str = fs_codec_to_string(codec);
				purple_debug_info("media", "farsight-send-codec-changed: codec: %s\n", codec_str);
				g_free(codec_str);
			} else if (gst_structure_has_name(msg->structure,
					"farsight-codecs-changed")) {
				GList *sessions = g_hash_table_get_values(PURPLE_MEDIA(media)->priv->sessions);
				FsSession *fssession = g_value_get_object(gst_structure_get_value(msg->structure, "session"));
				for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
					PurpleMediaSession *session = sessions->data;
					if (session->session == fssession) {
						gchar *session_id = g_strdup(session->id);
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
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);

	return purple_media_manager_get_pipeline(media->priv->manager);
}

PurpleConnection *
purple_media_get_connection(PurpleMedia *media)
{
	PurpleConnection *pc;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	g_object_get(G_OBJECT(media), "connection", &pc, NULL);
	return pc;
}

gpointer
purple_media_get_prpl_data(PurpleMedia *media)
{
	gpointer prpl_data;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	g_object_get(G_OBJECT(media), "prpl-data", &prpl_data, NULL);
	return prpl_data;
}

void
purple_media_set_prpl_data(PurpleMedia *media, gpointer prpl_data)
{
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	g_object_set(G_OBJECT(media), "prpl-data", prpl_data, NULL);
}

void
purple_media_error(PurpleMedia *media, const gchar *error, ...)
{
	va_list args;
	gchar *message;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

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
	GList *streams;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	streams = media->priv->streams;

	for (; streams; streams = g_list_next(streams)) {
		PurpleMediaStream *stream = streams->data;
		g_object_set(G_OBJECT(stream->stream), "direction",
				purple_media_to_fs_stream_direction(
				stream->session->type), NULL);
		stream->accepted = TRUE;
	}

	g_signal_emit(media, purple_media_signals[ACCEPTED],
			0, NULL, NULL);
}

void
purple_media_hangup(PurpleMedia *media)
{
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	g_signal_emit(media, purple_media_signals[STREAM_INFO],
			0, PURPLE_MEDIA_INFO_HANGUP,
			NULL, NULL);
	purple_media_end(media, NULL, NULL);
}

void
purple_media_reject(PurpleMedia *media)
{
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	g_signal_emit(media, purple_media_signals[STREAM_INFO],
			0, PURPLE_MEDIA_INFO_REJECT,
			NULL, NULL);
	purple_media_end(media, NULL, NULL);
}

void
purple_media_end(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	if (session_id == NULL && participant == NULL) {
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_END,
				NULL, NULL);
		g_object_unref(media);
	}
}

static void
purple_media_new_local_candidate_cb(FsStream *stream,
				    FsCandidate *local_candidate,
				    PurpleMediaSession *session)
{
	gchar *name;
	FsParticipant *participant;
	PurpleMediaCandidate *candidate;

	g_return_if_fail(FS_IS_STREAM(stream));
	g_return_if_fail(session != NULL);

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

	g_return_if_fail(FS_IS_STREAM(stream));
	g_return_if_fail(session != NULL);

	g_object_get(stream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	stream_data = purple_media_get_stream(session->media, session->id, name);
	stream_data->candidates_prepared = TRUE;

	g_signal_emit(session->media,
			purple_media_signals[CANDIDATES_PREPARED],
			0, session->id, name);

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
	GList *iter;

	g_return_if_fail(FS_IS_STREAM(fsstream));
	g_return_if_fail(session != NULL);

	g_object_get(fsstream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	stream = purple_media_get_stream(session->media, session->id, name);

	iter = stream->active_local_candidates;
	for(; iter; iter = g_list_next(iter)) {
		FsCandidate *c = iter->data;
		if (native_candidate->component_id == c->component_id) {
			fs_candidate_destroy(c);
			stream->active_local_candidates =
					g_list_delete_link(iter, iter);
			stream->active_local_candidates = g_list_prepend(
					stream->active_local_candidates,
					fs_candidate_copy(native_candidate));
			break;
		}
	}
	if (iter == NULL)
		stream->active_local_candidates = g_list_prepend(
				stream->active_local_candidates,
				fs_candidate_copy(native_candidate));

	iter = stream->active_remote_candidates;
	for(; iter; iter = g_list_next(iter)) {
		FsCandidate *c = iter->data;
		if (native_candidate->component_id == c->component_id) {
			fs_candidate_destroy(c);
			stream->active_remote_candidates =
					g_list_delete_link(iter, iter);
			stream->active_remote_candidates = g_list_prepend(
					stream->active_remote_candidates,
					fs_candidate_copy(remote_candidate));
			break;
		}
	}
	if (iter == NULL)
		stream->active_remote_candidates = g_list_prepend(
				stream->active_remote_candidates,
				fs_candidate_copy(remote_candidate));

	purple_debug_info("media", "candidate pair established\n");
}

static gboolean
purple_media_connected_cb(PurpleMediaStream *stream)
{
	g_return_val_if_fail(stream != NULL, FALSE);

	stream->connected_cb_id = 0;

	purple_media_manager_create_output_window(
			stream->session->media->priv->manager,
			stream->session->media,
			stream->session->id, stream->participant);

	g_signal_emit(stream->session->media,
			purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CONNECTED,
			stream->session->id, stream->participant);
	return FALSE;
}

static void
purple_media_src_pad_added_cb(FsStream *fsstream, GstPad *srcpad,
			      FsCodec *codec, PurpleMediaStream *stream)
{
	PurpleMediaPrivate *priv;
	GstPad *sinkpad;

	g_return_if_fail(FS_IS_STREAM(fsstream));
	g_return_if_fail(stream != NULL);

	priv = stream->session->media->priv;

	if (stream->src == NULL) {
		GstElement *sink = NULL;

		if (codec->media_type == FS_MEDIA_TYPE_AUDIO) {
			/*
			 * Should this instead be:
			 *  audioconvert ! audioresample ! liveadder !
			 *   audioresample ! audioconvert ! realsink
			 */
			stream->src = gst_element_factory_make(
					"liveadder", NULL);
			sink = purple_media_manager_get_element(priv->manager,
					PURPLE_MEDIA_RECV_AUDIO);
		} else if (codec->media_type == FS_MEDIA_TYPE_VIDEO) {
			stream->src = gst_element_factory_make(
					"fsfunnel", NULL);
			sink = gst_element_factory_make(
					"fakesink", NULL);
			g_object_set(G_OBJECT(sink), "async", FALSE, NULL);
		}
		stream->tee = gst_element_factory_make("tee", NULL);
		gst_bin_add_many(GST_BIN(priv->confbin),
				stream->src, stream->tee, sink, NULL);
		gst_element_sync_state_with_parent(sink);
		gst_element_sync_state_with_parent(stream->tee);
		gst_element_sync_state_with_parent(stream->src);
		gst_element_link_many(stream->src, stream->tee, sink, NULL);
	}

	sinkpad = gst_element_get_request_pad(stream->src, "sink%d");
	gst_pad_link(srcpad, sinkpad);
	gst_object_unref(sinkpad);

	stream->connected_cb_id = purple_timeout_add(0,
			(GSourceFunc)purple_media_connected_cb, stream);
}

static gboolean
purple_media_add_stream_internal(PurpleMedia *media, const gchar *sess_id,
				 const gchar *who, FsMediaType type,
				 FsStreamDirection type_direction,
				 const gchar *transmitter,
				 guint num_params, GParameter *params)
{
	PurpleMediaSession *session;
	FsParticipant *participant = NULL;
	PurpleMediaStream *stream = NULL;
	FsStreamDirection *direction = NULL;
	PurpleMediaSessionType session_type;
	gboolean is_nice = !strcmp(transmitter, "nice");

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	session = purple_media_get_session(media, sess_id);

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

		fs_session_set_codec_preferences(session->session, codec_conf, NULL);

		/*
		 * Removes a 5-7 second delay before
		 * receiving the src-pad-added signal.
		 * Only works for non-multicast FsRtpSessions.
		 */
		if (is_nice || !strcmp(transmitter, "rawudp"))
			g_object_set(G_OBJECT(session->session),
					"no-rtcp-timeout", 0, NULL);

		fs_codec_list_destroy(codec_conf);

		session->id = g_strdup(sess_id);
		session->media = media;
		session->type = purple_media_from_fs(type, type_direction);

		purple_media_add_session(media, session);
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_NEW,
				session->id, NULL);

		session_type = purple_media_from_fs(type, FS_DIRECTION_SEND);
		purple_media_set_src(media, session->id,
				purple_media_manager_get_element(
				media->priv->manager, session_type));
		gst_element_set_state(session->src, GST_STATE_PLAYING);

		purple_media_manager_create_output_window(
				media->priv->manager,
				session->media,
				session->id, NULL);
	}

	if (!(participant = purple_media_add_participant(media, who))) {
		purple_media_remove_session(media, session);
		g_free(session);
		return FALSE;
	} else {
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_NEW,
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
					(stun_ip && is_nice) && turn_ip ?
					num_params + 2 : num_params + 1;
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

			if (turn_ip && is_nice) {
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
				0, PURPLE_MEDIA_STATE_NEW,
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

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

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
	PurpleMediaSession *session;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), PURPLE_MEDIA_NONE);
	session = purple_media_get_session(media, sess_id);
	return session->type;
}
/* XXX: Should wait until codecs-ready is TRUE before using this function */
GList *
purple_media_get_codecs(PurpleMedia *media, const gchar *sess_id)
{
	GList *fscodecs;
	GList *codecs;
	PurpleMediaSession *session;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);

	session = purple_media_get_session(media, sess_id);

	if (session == NULL)
		return NULL;

	g_object_get(G_OBJECT(session->session),
		     "codecs", &fscodecs, NULL);
	codecs = purple_media_codec_list_from_fs(fscodecs);
	fs_codec_list_destroy(fscodecs);
	return codecs;
}

GList *
purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	PurpleMediaStream *stream;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	stream = purple_media_get_stream(media, sess_id, name);
	return purple_media_candidate_list_from_fs(stream->local_candidates);
}

void
purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
				   const gchar *name, GList *remote_candidates)
{
	PurpleMediaStream *stream;
	GError *err = NULL;

	g_return_if_fail(PURPLE_IS_MEDIA(media));
	stream = purple_media_get_stream(media, sess_id, name);

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

GList *
purple_media_get_active_local_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *name)
{
	PurpleMediaStream *stream;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	stream = purple_media_get_stream(media, sess_id, name);
	return purple_media_candidate_list_from_fs(
			stream->active_local_candidates);
}

GList *
purple_media_get_active_remote_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *name)
{
	PurpleMediaStream *stream;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	stream = purple_media_get_stream(media, sess_id, name);
	return purple_media_candidate_list_from_fs(
			stream->active_remote_candidates);
}

gboolean
purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id, const gchar *name, GList *codecs)
{
	PurpleMediaStream *stream;
	FsStream *fsstream;
	GList *fscodecs;
	GError *err = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);
	stream = purple_media_get_stream(media, sess_id, name);

	if (stream == NULL)
		return FALSE;

	fsstream = stream->stream;
	fscodecs = purple_media_codec_list_to_fs(codecs);
	fs_stream_set_remote_codecs(fsstream, fscodecs, &err);
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
purple_media_candidates_prepared(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GList *streams;
	gboolean prepared = TRUE;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	streams = purple_media_get_streams(media, session_id, participant);

	for (; streams; streams = g_list_delete_link(streams, streams)) {
		PurpleMediaStream *stream = streams->data;
		if (stream->candidates_prepared == FALSE) {
			g_list_free(streams);
			prepared = FALSE;
			break;
		}
	}

	return prepared;
}

gboolean
purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, PurpleMediaCodec *codec)
{
	PurpleMediaSession *session;
	FsCodec *fscodec;
	GError *err = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	session = purple_media_get_session(media, sess_id);

	if (session != NULL)
		return FALSE;

	fscodec = purple_media_codec_to_fs(codec);
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
	gboolean ret;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	if (sess_id != NULL) {
		PurpleMediaSession *session;
		session = purple_media_get_session(media, sess_id);

		if (session == NULL)
			return FALSE;

		g_object_get(session->session, "codecs-ready", &ret, NULL);
	} else {
		GList *values = g_hash_table_get_values(media->priv->sessions);
		for (; values; values = g_list_delete_link(values, values)) {
			PurpleMediaSession *session = values->data;
			g_object_get(session->session,
					"codecs-ready", &ret, NULL);
			if (ret == FALSE)
				break;
		}
		if (values != NULL)
			g_list_free(values);
	}
	return ret;
}

gboolean
purple_media_accepted(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant)
{
	gboolean accepted = TRUE;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	if (sess_id == NULL && participant == NULL) {
		GList *streams = media->priv->streams;

		for (; streams; streams = g_list_next(streams)) {
			PurpleMediaStream *stream = streams->data;
			if (stream->accepted == FALSE) {
				accepted = FALSE;
				break;
			}
		}
	} else if (sess_id != NULL && participant == NULL) {
		GList *streams = purple_media_get_streams(
				media, sess_id, NULL);
		for (; streams; streams =
				g_list_delete_link(streams, streams)) {
			PurpleMediaStream *stream = streams->data;
			if (stream->accepted == FALSE) {
				g_list_free(streams);
				accepted = FALSE;
				break;
			}
		}
	} else if (sess_id != NULL && participant != NULL) {
		PurpleMediaStream *stream = purple_media_get_stream(
				media, sess_id, participant);
		if (stream == NULL || stream->accepted == FALSE)
			accepted = FALSE;
	}

	return accepted;
}

void purple_media_mute(PurpleMedia *media, gboolean active)
{
	GList *sessions;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	sessions = g_hash_table_get_values(media->priv->sessions);
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

	g_return_if_fail(PURPLE_IS_MEDIA(media));

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
	GList *streams;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	streams = purple_media_get_streams(media,
			session_id, participant);

	for (; streams; streams = g_list_delete_link(streams, streams)) {
		PurpleMediaStream *stream = streams->data;

		if (stream->session->type & PURPLE_MEDIA_RECV_AUDIO) {
			GstElement *tee = stream->tee;
			GstIterator *iter = gst_element_iterate_src_pads(tee);
			GstPad *sinkpad;
			while (gst_iterator_next(iter, (gpointer)&sinkpad)
					 == GST_ITERATOR_OK) {
				GstPad *peer = gst_pad_get_peer(sinkpad);
				GstElement *volume;

				if (peer == NULL) {
					gst_object_unref(sinkpad);
					continue;
				}
					
				volume = gst_bin_get_by_name(GST_BIN(
						GST_OBJECT_PARENT(peer)),
						"purpleaudiooutputvolume");
				g_object_set(volume, "volume", level, NULL);
				gst_object_unref(peer);
				gst_object_unref(sinkpad);
			}
			gst_iterator_free(iter);
		}
	}
}

gulong
purple_media_set_output_window(PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	return purple_media_manager_set_output_window(media->priv->manager,
			media, session_id, participant, window_id);
}

void
purple_media_remove_output_windows(PurpleMedia *media)
{
	GList *iter = media->priv->streams;
	for (; iter; iter = g_list_next(iter)) {
		PurpleMediaStream *stream = iter->data;
		purple_media_manager_remove_output_windows(
				media->priv->manager, media,
				stream->session->id, stream->participant);
	}

	iter = purple_media_get_session_names(media);
	for (; iter; iter = g_list_delete_link(iter, iter)) {
		gchar *session_name = iter->data;
		purple_media_manager_remove_output_windows(
				media->priv->manager, media,
				session_name, NULL);
	}
}

GstElement *
purple_media_get_tee(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);

	if (session_id != NULL && participant == NULL) {
		PurpleMediaSession *session =
				purple_media_get_session(media, session_id);
		return (session != NULL) ? session->tee : NULL;
	} else if (session_id != NULL && participant != NULL) {
		PurpleMediaStream *stream =
				purple_media_get_stream(media,
				session_id, participant);
		return (stream != NULL) ? stream->tee : NULL;
	}
	g_return_val_if_reached(NULL);
}

#endif  /* USE_VV */
