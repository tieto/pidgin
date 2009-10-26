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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <string.h>

#include "internal.h"

#include "account.h"
#include "media.h"
#include "media/backend-fs2.h"
#include "media/backend-iface.h"
#include "mediamanager.h"
#include "network.h"

#include "debug.h"

#ifdef USE_GSTREAMER
#include "marshallers.h"
#include "media-gst.h"
#endif

#ifdef USE_VV

#include <gst/farsight/fs-conference-iface.h>
#include <gst/farsight/fs-element-added-notifier.h>

/** @copydoc _PurpleMediaSession */
typedef struct _PurpleMediaSession PurpleMediaSession;
/** @copydoc _PurpleMediaStream */
typedef struct _PurpleMediaStream PurpleMediaStream;
/** @copydoc _PurpleMediaClass */
typedef struct _PurpleMediaClass PurpleMediaClass;
/** @copydoc _PurpleMediaPrivate */
typedef struct _PurpleMediaPrivate PurpleMediaPrivate;

/** The media class */
struct _PurpleMediaClass
{
	GObjectClass parent_class;     /**< The parent class. */
};

/** The media class's private data */
struct _PurpleMedia
{
	GObject parent;                /**< The parent of this object. */
	PurpleMediaPrivate *priv;      /**< The private data of this object. */
};

struct _PurpleMediaSession
{
	gchar *id;
	PurpleMedia *media;
	GstElement *src;
	GstElement *tee;
	FsSession *session;

	PurpleMediaSessionType type;
	gboolean initiator;
};

struct _PurpleMediaStream
{
	PurpleMediaSession *session;
	gchar *participant;
	FsStream *stream;
	GstElement *src;
	GstElement *tee;
	GstElement *volume;
	GstElement *level;

	GList *local_candidates;
	GList *remote_candidates;

	gboolean initiator;
	gboolean accepted;
	gboolean candidates_prepared;

	GList *active_local_candidates;
	GList *active_remote_candidates;

	guint connected_cb_id;
};
#endif

struct _PurpleMediaPrivate
{
#ifdef USE_VV
	PurpleMediaManager *manager;
	PurpleAccount *account;
	PurpleMediaBackend *backend;
	FsConference *conference;
	gchar *conference_type;
	gulong gst_bus_handler_id;
	gboolean initiator;
	gpointer prpl_data;

	GHashTable *sessions;	/* PurpleMediaSession table */
	GHashTable *participants; /* FsParticipant table */

	GList *streams;		/* PurpleMediaStream table */

	GstElement *confbin;
#else
	gpointer dummy;
#endif
};

#ifdef USE_VV
#define PURPLE_MEDIA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA, PurpleMediaPrivate))

static void purple_media_class_init (PurpleMediaClass *klass);
static void purple_media_init (PurpleMedia *media);
static void purple_media_dispose (GObject *object);
static void purple_media_finalize (GObject *object);
static void purple_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void purple_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static void purple_media_new_local_candidate_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, const gchar *participant,
		PurpleMediaCandidate *candidate, PurpleMedia *media);
static void purple_media_candidates_prepared_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, const gchar *name, PurpleMedia *media);
static void purple_media_candidate_pair_established_cb(
		PurpleMediaBackend *backend,
		const gchar *sess_id, const gchar *name,
		PurpleMediaCandidate *local_candidate,
		PurpleMediaCandidate *remote_candidate,
		PurpleMedia *media);
static void purple_media_codecs_changed_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, PurpleMedia *media);
static gboolean media_bus_call(GstBus *bus,
		GstMessage *msg, PurpleMedia *media);

static GObjectClass *parent_class = NULL;



enum {
	S_ERROR,
	CANDIDATES_PREPARED,
	CODECS_CHANGED,
	LEVEL,
	NEW_CANDIDATE,
	STATE_CHANGED,
	STREAM_INFO,
	LAST_SIGNAL
};
static guint purple_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_MANAGER,
	PROP_ACCOUNT,
#ifndef PURPLE_DISABLE_DEPRECATED
	PROP_CONFERENCE,
#endif
	PROP_CONFERENCE_TYPE,
	PROP_INITIATOR,
	PROP_PRPL_DATA,
};
#endif


GType
purple_media_get_type()
{
#ifdef USE_VV
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
#else
	return G_TYPE_NONE;
#endif
}

#ifdef USE_VV
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

	g_object_class_install_property(gobject_class, PROP_ACCOUNT,
			g_param_spec_pointer("account",
			"PurpleAccount",
			"The account this media session is on.",
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

#ifndef PURPLE_DISABLE_DEPRECATED
	g_object_class_install_property(gobject_class, PROP_CONFERENCE,
			g_param_spec_object("conference",
			"Farsight conference",
			"The FsConference associated with this media.",
			FS_TYPE_CONFERENCE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
#endif

	g_object_class_install_property(gobject_class, PROP_CONFERENCE_TYPE,
			g_param_spec_string("conference-type",
			"Conference Type",
			"The type of conference that this media object "
			"has been created to provide.",
			NULL,
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

	purple_media_signals[S_ERROR] = g_signal_new("error", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_signals[CANDIDATES_PREPARED] = g_signal_new("candidates-prepared", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING,
					 G_TYPE_STRING);
	purple_media_signals[CODECS_CHANGED] = g_signal_new("codecs-changed", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_signals[LEVEL] = g_signal_new("level", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING_DOUBLE,
					 G_TYPE_NONE, 3, G_TYPE_STRING,
					 G_TYPE_STRING, G_TYPE_DOUBLE);
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
					 purple_smarshal_VOID__ENUM_STRING_STRING_BOOLEAN,
					 G_TYPE_NONE, 4, PURPLE_MEDIA_TYPE_INFO_TYPE,
					 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
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

	if (priv->backend) {
		g_object_unref(priv->backend);
		priv->backend = NULL;
	}

	for (iter = priv->streams; iter; iter = g_list_next(iter)) {
		PurpleMediaStream *stream = iter->data;
		if (stream->stream) {
			g_object_unref(stream->stream);
			stream->stream = NULL;
		}
	}

	if (priv->participants) {
		GList *participants = g_hash_table_get_values(priv->participants);
		for (; participants; participants = g_list_delete_link(participants, participants))
			g_object_unref(participants->data);
	}

	if (priv->gst_bus_handler_id != 0) {
		GstElement *pipeline = purple_media_manager_get_pipeline(
				priv->manager);
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
		g_signal_handler_disconnect(bus, priv->gst_bus_handler_id);
		gst_object_unref(bus);
		priv->gst_bus_handler_id = 0;
	}

	if (priv->manager) {
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
purple_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PurpleMedia *media;
	g_return_if_fail(PURPLE_IS_MEDIA(object));

	media = PURPLE_MEDIA(object);

	switch (prop_id) {
		case PROP_MANAGER:
			media->priv->manager = g_value_dup_object(value);
			break;
		case PROP_ACCOUNT:
			media->priv->account = g_value_get_pointer(value);
			break;
#ifndef PURPLE_DISABLE_DEPRECATED
		case PROP_CONFERENCE: {
			if (media->priv->conference)
				gst_object_unref(media->priv->conference);
			media->priv->conference = g_value_dup_object(value);
			break;
		}
#endif
		case PROP_CONFERENCE_TYPE:
			media->priv->conference_type =
					g_value_dup_string(value);
			/* Will eventually get this type from the media manager */
			media->priv->backend = g_object_new(
					PURPLE_TYPE_MEDIA_BACKEND_FS2,
					"conference-type",
					media->priv->conference_type,
					"media", media,
					NULL);
			g_signal_connect(media->priv->backend,
					"active-candidate-pair",
					G_CALLBACK(
					purple_media_candidate_pair_established_cb),
					media);
			g_signal_connect(media->priv->backend,
					"candidates-prepared",
					G_CALLBACK(
					purple_media_candidates_prepared_cb),
					media);
			g_signal_connect(media->priv->backend,
					"codecs-changed",
					G_CALLBACK(
					purple_media_codecs_changed_cb),
					media);
			g_signal_connect(media->priv->backend,
					"new-candidate",
					G_CALLBACK(
					purple_media_new_local_candidate_cb),
					media);
			break;
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
		case PROP_ACCOUNT:
			g_value_set_pointer(value, media->priv->account);
			break;
#ifndef PURPLE_DISABLE_DEPRECATED
		case PROP_CONFERENCE:
			g_value_set_object(value, media->priv->conference);
			break;
#endif
		case PROP_CONFERENCE_TYPE:
			g_value_set_string(value,
					media->priv->conference_type);
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

static FsCodec *
purple_media_codec_to_fs(const PurpleMediaCodec *codec)
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
			purple_media_to_fs_media_type(media_type),
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
	g_object_set(new_codec, "channels", codec->channels, NULL);

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		FsCodecParameter *param = (FsCodecParameter*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
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
#endif

GList *
purple_media_get_session_ids(PurpleMedia *media)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	return media->priv->sessions != NULL ?
			g_hash_table_get_keys(media->priv->sessions) : NULL;
#else
	return NULL;
#endif
}

#ifdef USE_VV
static void 
purple_media_set_src(PurpleMedia *media, const gchar *sess_id, GstElement *src)
{
	PurpleMediaSession *session;
	GstPad *sinkpad;
	GstPad *srcpad;

	g_return_if_fail(PURPLE_IS_MEDIA(media));
	g_return_if_fail(GST_IS_ELEMENT(src));

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

	session->tee = gst_element_factory_make("tee", NULL);
	gst_bin_add(GST_BIN(session->media->priv->confbin), session->tee);

	/* This supposedly isn't necessary, but it silences some warnings */
	if (GST_ELEMENT_PARENT(session->media->priv->confbin)
			== GST_ELEMENT_PARENT(session->src)) {
		GstPad *pad = gst_element_get_static_pad(session->tee, "sink");
		GstPad *ghost = gst_ghost_pad_new(NULL, pad);
		gst_object_unref(pad);
		gst_pad_set_active(ghost, TRUE);
		gst_element_add_pad(session->media->priv->confbin, ghost);
	}

	gst_element_set_state(session->tee, GST_STATE_PLAYING);
	gst_element_link(session->src, session->media->priv->confbin);

	g_object_get(session->session, "sink-pad", &sinkpad, NULL);
	if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
		gchar *name = g_strdup_printf("volume_%s", session->id);
		GstElement *level;
		GstElement *volume = gst_element_factory_make("volume", name);
		double input_volume = purple_prefs_get_int(
				"/purple/media/audio/volume/input")/10.0;
		g_free(name);
		name = g_strdup_printf("sendlevel_%s", session->id);
		level = gst_element_factory_make("level", name);
		g_free(name);
		gst_bin_add(GST_BIN(session->media->priv->confbin), volume);
		gst_bin_add(GST_BIN(session->media->priv->confbin), level);
		gst_element_set_state(level, GST_STATE_PLAYING);
		gst_element_set_state(volume, GST_STATE_PLAYING);
		gst_element_link(volume, level);
		gst_element_link(session->tee, volume);
		srcpad = gst_element_get_static_pad(level, "src");
		g_object_set(volume, "volume", input_volume, NULL);
	} else {
		srcpad = gst_element_get_request_pad(session->tee, "src%d");
	}
	purple_debug_info("media", "connecting pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK
			  ? "success" : "failure");
	gst_element_set_locked_state(session->src, FALSE);
	gst_object_unref(session->src);
}
#endif

#ifdef USE_GSTREAMER
GstElement *
purple_media_get_src(PurpleMedia *media, const gchar *sess_id)
{
#ifdef USE_VV
	PurpleMediaSession *session;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	session = purple_media_get_session(media, sess_id);
	return (session != NULL) ? session->src : NULL;
#else
	return NULL;
#endif
}
#endif /* USE_GSTREAMER */

#ifdef USE_VV
static gboolean
media_bus_call(GstBus *bus, GstMessage *msg, PurpleMedia *media)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_ELEMENT: {
			if (g_signal_has_handler_pending(media,
					purple_media_signals[LEVEL], 0, FALSE)
					&& gst_structure_has_name(
					gst_message_get_structure(msg), "level")) {
				GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
				gchar *name;
				gchar *participant = NULL;
				PurpleMediaSession *session = NULL;
				gdouble rms_db;
				gdouble percent;
				const GValue *list;
				const GValue *value;

				if (!PURPLE_IS_MEDIA(media) ||
						GST_ELEMENT_PARENT(src) !=
						media->priv->confbin)
					break;

				name = gst_element_get_name(src);
				if (!strncmp(name, "sendlevel_", 10)) {
					session = purple_media_get_session(
							media, name+10);
				} else {
					GList *iter = media->priv->streams;
					for (; iter; iter = g_list_next(iter)) {
						PurpleMediaStream *stream = iter->data;
						if (stream->level == src) {
							session = stream->session;
							participant = stream->participant;
							break;
						}
					}
				}
				g_free(name);
				if (!session)
					break;

				list = gst_structure_get_value(
						gst_message_get_structure(msg), "rms");
				value = gst_value_list_get_value(list, 0);
				rms_db = g_value_get_double(value);
				percent = pow(10, rms_db / 20) * 5;
				if(percent > 1.0)
					percent = 1.0;

				g_signal_emit(media, purple_media_signals[LEVEL],
						0, session->id, participant, percent);
				break;
			}
			break;
		}
		default:
			break;
	}

	return TRUE;
}
#endif

PurpleAccount *
purple_media_get_account(PurpleMedia *media)
{
#ifdef USE_VV
	PurpleAccount *account;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	g_object_get(G_OBJECT(media), "account", &account, NULL);
	return account;
#else
	return NULL;
#endif
}

gpointer
purple_media_get_prpl_data(PurpleMedia *media)
{
#ifdef USE_VV
	gpointer prpl_data;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	g_object_get(G_OBJECT(media), "prpl-data", &prpl_data, NULL);
	return prpl_data;
#else
	return NULL;
#endif
}

void
purple_media_set_prpl_data(PurpleMedia *media, gpointer prpl_data)
{
#ifdef USE_VV
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	g_object_set(G_OBJECT(media), "prpl-data", prpl_data, NULL);
#endif
}

void
purple_media_error(PurpleMedia *media, const gchar *error, ...)
{
#ifdef USE_VV
	va_list args;
	gchar *message;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	va_start(args, error);
	message = g_strdup_vprintf(error, args);
	va_end(args);

	purple_debug_error("media", "%s\n", message);
	g_signal_emit(media, purple_media_signals[S_ERROR], 0, message);

	g_free(message);
#endif
}

void
purple_media_end(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
#ifdef USE_VV
	g_return_if_fail(PURPLE_IS_MEDIA(media));
	if (session_id == NULL && participant == NULL) {
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_END,
				NULL, NULL);
		g_object_unref(media);
	}
#endif
}

void
purple_media_stream_info(PurpleMedia *media, PurpleMediaInfoType type,
		const gchar *session_id, const gchar *participant,
		gboolean local)
{
#ifdef USE_VV
	g_return_if_fail(PURPLE_IS_MEDIA(media));

	if (type == PURPLE_MEDIA_INFO_ACCEPT) {
		GList *streams;

		g_return_if_fail(PURPLE_IS_MEDIA(media));

		streams = purple_media_get_streams(media,
				session_id, participant);

		for (; streams; streams =
				g_list_delete_link(streams, streams)) {
			PurpleMediaStream *stream = streams->data;
			g_object_set(G_OBJECT(stream->stream), "direction",
					purple_media_to_fs_stream_direction(
					stream->session->type), NULL);
			stream->accepted = TRUE;

			if (stream->remote_candidates != NULL) {
				GError *err = NULL;
				fs_stream_set_remote_candidates(stream->stream,
						stream->remote_candidates, &err);

				if (err) {
					purple_debug_error("media", "Error adding remote"
							" candidates: %s\n", err->message);
					g_error_free(err);
				}
			}
		}
	} else if (local == TRUE && (type == PURPLE_MEDIA_INFO_MUTE ||
			type == PURPLE_MEDIA_INFO_UNMUTE)) {
		GList *sessions;
		gboolean active = (type == PURPLE_MEDIA_INFO_MUTE);

		g_return_if_fail(PURPLE_IS_MEDIA(media));

		if (session_id == NULL)
			sessions = g_hash_table_get_values(
					media->priv->sessions);
		else
			sessions = g_list_prepend(NULL,
					purple_media_get_session(
					media, session_id));

		purple_debug_info("media", "Turning mute %s\n",
				active ? "on" : "off");

		for (; sessions; sessions = g_list_delete_link(
				sessions, sessions)) {
			PurpleMediaSession *session = sessions->data;
			if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
				gchar *name = g_strdup_printf("volume_%s",
						session->id);
				GstElement *volume = gst_bin_get_by_name(
						GST_BIN(session->media->
						priv->confbin), name);
				g_free(name);
				g_object_set(volume, "mute", active, NULL);
			}
		}
	} else if (local == TRUE && (type == PURPLE_MEDIA_INFO_PAUSE ||
			type == PURPLE_MEDIA_INFO_UNPAUSE)) {
		gboolean active = (type == PURPLE_MEDIA_INFO_PAUSE);
		GList *streams = purple_media_get_streams(media,
				session_id, participant);
		for (; streams; streams = g_list_delete_link(streams, streams)) {
			PurpleMediaStream *stream = streams->data;
			if (stream->session->type & PURPLE_MEDIA_SEND_VIDEO) {
				g_object_set(stream->stream, "direction",
						purple_media_to_fs_stream_direction(
						stream->session->type & ((active) ?
						~PURPLE_MEDIA_SEND_VIDEO :
						PURPLE_MEDIA_VIDEO)), NULL);
			}
		}
	}

	g_signal_emit(media, purple_media_signals[STREAM_INFO],
			0, type, session_id, participant, local);

	if (type == PURPLE_MEDIA_INFO_HANGUP ||
			type == PURPLE_MEDIA_INFO_REJECT) {
		purple_media_end(media, session_id, participant);
	}
#endif
}

#ifdef USE_VV
static void
purple_media_new_local_candidate_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, const gchar *participant,
		PurpleMediaCandidate *candidate, PurpleMedia *media)
{
	PurpleMediaSession *session =
			purple_media_get_session(media, sess_id);

	purple_media_insert_local_candidate(session, participant,
			purple_media_candidate_to_fs(candidate));

	g_signal_emit(session->media, purple_media_signals[NEW_CANDIDATE],
		      0, session->id, participant, candidate);
}

static void
purple_media_candidates_prepared_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, const gchar *name, PurpleMedia *media)
{
	PurpleMediaStream *stream_data;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	stream_data = purple_media_get_stream(media, sess_id, name);
	stream_data->candidates_prepared = TRUE;

	g_signal_emit(media, purple_media_signals[CANDIDATES_PREPARED],
			0, sess_id, name);
}

/* callback called when a pair of transport candidates (local and remote)
 * has been established */
static void
purple_media_candidate_pair_established_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, const gchar *name,
		PurpleMediaCandidate *local_candidate,
		PurpleMediaCandidate *remote_candidate,
		PurpleMedia *media)
{
	PurpleMediaStream *stream;
	GList *iter;
	guint id;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	stream = purple_media_get_stream(media, sess_id, name);
	id = purple_media_candidate_get_component_id(local_candidate);

	iter = stream->active_local_candidates;
	for(; iter; iter = g_list_next(iter)) {
		FsCandidate *c = iter->data;
		if (id == c->component_id) {
			fs_candidate_destroy(c);
			stream->active_local_candidates =
					g_list_delete_link(iter, iter);
			stream->active_local_candidates = g_list_prepend(
					stream->active_local_candidates,
					purple_media_candidate_to_fs(
					local_candidate));
			break;
		}
	}
	if (iter == NULL)
		stream->active_local_candidates = g_list_prepend(
				stream->active_local_candidates,
				purple_media_candidate_to_fs(
				local_candidate));

	id = purple_media_candidate_get_component_id(local_candidate);

	iter = stream->active_remote_candidates;
	for(; iter; iter = g_list_next(iter)) {
		FsCandidate *c = iter->data;
		if (id == c->component_id) {
			fs_candidate_destroy(c);
			stream->active_remote_candidates =
					g_list_delete_link(iter, iter);
			stream->active_remote_candidates = g_list_prepend(
					stream->active_remote_candidates,
					purple_media_candidate_to_fs(
					remote_candidate));
			break;
		}
	}
	if (iter == NULL)
		stream->active_remote_candidates = g_list_prepend(
				stream->active_remote_candidates,
				purple_media_candidate_to_fs(
				remote_candidate));

	purple_debug_info("media", "candidate pair established\n");
}

static void
purple_media_codecs_changed_cb(PurpleMediaBackend *backend,
		const gchar *sess_id, PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[CODECS_CHANGED], 0, sess_id);
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
			GstElement *queue = NULL;
			double output_volume = purple_prefs_get_int(
					"/purple/media/audio/volume/output")/10.0;
			/*
			 * Should this instead be:
			 *  audioconvert ! audioresample ! liveadder !
			 *   audioresample ! audioconvert ! realsink
			 */
			queue = gst_element_factory_make("queue", NULL);
			stream->volume = gst_element_factory_make(
					"volume", NULL);
			g_object_set(stream->volume, "volume",
					output_volume, NULL);
			stream->level = gst_element_factory_make(
					"level", NULL);
			stream->src = gst_element_factory_make(
					"liveadder", NULL);
			sink = purple_media_manager_get_element(priv->manager,
					PURPLE_MEDIA_RECV_AUDIO,
					stream->session->media,
					stream->session->id,
					stream->participant);
			gst_bin_add(GST_BIN(priv->confbin), queue);
			gst_bin_add(GST_BIN(priv->confbin), stream->volume);
			gst_bin_add(GST_BIN(priv->confbin), stream->level);
			gst_bin_add(GST_BIN(priv->confbin), sink);
			gst_element_set_state(sink, GST_STATE_PLAYING);
			gst_element_set_state(stream->level, GST_STATE_PLAYING);
			gst_element_set_state(stream->volume, GST_STATE_PLAYING);
			gst_element_set_state(queue, GST_STATE_PLAYING);
			gst_element_link(stream->level, sink);
			gst_element_link(stream->volume, stream->level);
			gst_element_link(queue, stream->volume);
			sink = queue;
		} else if (codec->media_type == FS_MEDIA_TYPE_VIDEO) {
			stream->src = gst_element_factory_make(
					"fsfunnel", NULL);
			sink = gst_element_factory_make(
					"fakesink", NULL);
			g_object_set(G_OBJECT(sink), "async", FALSE, NULL);
			gst_bin_add(GST_BIN(priv->confbin), sink);
			gst_element_set_state(sink, GST_STATE_PLAYING);
		}
		stream->tee = gst_element_factory_make("tee", NULL);
		gst_bin_add_many(GST_BIN(priv->confbin),
				stream->src, stream->tee, NULL);
		gst_element_set_state(stream->tee, GST_STATE_PLAYING);
		gst_element_set_state(stream->src, GST_STATE_PLAYING);
		gst_element_link_many(stream->src, stream->tee, sink, NULL);
	}

	sinkpad = gst_element_get_request_pad(stream->src, "sink%d");
	gst_pad_link(srcpad, sinkpad);
	gst_object_unref(sinkpad);

	stream->connected_cb_id = purple_timeout_add(0,
			(GSourceFunc)purple_media_connected_cb, stream);
}
#endif  /* USE_VV */

gboolean
purple_media_add_stream(PurpleMedia *media, const gchar *sess_id,
		const gchar *who, PurpleMediaSessionType type,
		gboolean initiator, const gchar *transmitter,
		guint num_params, GParameter *params)
{
#ifdef USE_VV
	PurpleMediaSession *session;
	FsParticipant *participant = NULL;
	PurpleMediaStream *stream = NULL;
	FsMediaType media_type = purple_media_to_fs_media_type(type);
	FsStreamDirection type_direction =
			purple_media_to_fs_stream_direction(type);
	gboolean is_nice = !strcmp(transmitter, "nice");

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	if (media->priv->gst_bus_handler_id == 0) {
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(
				purple_media_manager_get_pipeline(
				purple_media_manager_get())));
		media->priv->gst_bus_handler_id =
				g_signal_connect(G_OBJECT(bus), "message",
				G_CALLBACK(media_bus_call), media);
		gst_object_unref(bus);
	}

	if (!purple_media_backend_add_stream(media->priv->backend,
			sess_id, who, type, initiator, transmitter,
			num_params, params)) {
		purple_debug_error("media", "Error adding stream.\n");
		return FALSE;
	}

	/* XXX: Temporary call while integrating with backend */
	if (media->priv->conference == NULL) {
		media->priv->conference =
				purple_media_backend_fs2_get_conference(
				PURPLE_MEDIA_BACKEND_FS2(
				media->priv->backend));
		media->priv->confbin = GST_ELEMENT_PARENT(
				media->priv->conference);
	}

	session = purple_media_get_session(media, sess_id);

	if (!session) {
		PurpleMediaSessionType session_type;
		GstElement *src = NULL;

		session = g_new0(PurpleMediaSession, 1);
		session->session = purple_media_backend_fs2_get_session(
				PURPLE_MEDIA_BACKEND_FS2(media->priv->backend),
				sess_id);
		session->id = g_strdup(sess_id);
		session->media = media;
		session->type = type;
		session->initiator = initiator;

		purple_media_add_session(media, session);
		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_NEW,
				session->id, NULL);

		if (type_direction & FS_DIRECTION_SEND) {
			session_type = purple_media_from_fs(media_type,
					FS_DIRECTION_SEND);
			src = purple_media_manager_get_element(
					media->priv->manager, session_type,
					media, session->id, who);
			if (!GST_IS_ELEMENT(src)) {
				purple_debug_error("media",
						"Error creating src for session %s\n",
						session->id);
				purple_media_end(media, session->id, NULL);
				return FALSE;
			}

			purple_media_set_src(media, session->id, src);
			gst_element_set_state(session->src, GST_STATE_PLAYING);
			purple_media_manager_create_output_window(
					media->priv->manager,
					session->media,
					session->id, NULL);
		}
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

		if (fsstream == NULL) {
			purple_debug_error("media",
					"Error creating stream: %s\n",
					err && err->message ?
					err->message : "NULL");
			if (err)
				g_error_free(err);
			g_object_unref(participant);
			g_hash_table_remove(media->priv->participants, who);
			purple_media_remove_session(media, session);
			g_free(session);
			return FALSE;
		}

		stream = purple_media_insert_stream(session, who, fsstream);
		stream->initiator = initiator;

		/* callback for source pad added (new stream source ready) */
		g_signal_connect(G_OBJECT(fsstream),
				 "src-pad-added", G_CALLBACK(purple_media_src_pad_added_cb), stream);

		g_signal_emit(media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_NEW,
				session->id, who);
	} else {
		if (purple_media_to_fs_stream_direction(stream->session->type)
				!= type_direction) {
			/* change direction */
			g_object_set(stream->stream, "direction",
					type_direction, NULL);
		}
	}

	return TRUE;
#else
	return FALSE;
#endif  /* USE_VV */
}

PurpleMediaManager *
purple_media_get_manager(PurpleMedia *media)
{
	PurpleMediaManager *ret;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	g_object_get(media, "manager", &ret, NULL);
	return ret;
}

PurpleMediaSessionType
purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id)
{
#ifdef USE_VV
	PurpleMediaSession *session;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), PURPLE_MEDIA_NONE);
	session = purple_media_get_session(media, sess_id);
	return session->type;
#else
	return PURPLE_MEDIA_NONE;
#endif
}
/* XXX: Should wait until codecs-ready is TRUE before using this function */
GList *
purple_media_get_codecs(PurpleMedia *media, const gchar *sess_id)
{
#ifdef USE_VV
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
#else
	return NULL;
#endif
}

GList *
purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id,
                                  const gchar *participant)
{
#ifdef USE_VV
	PurpleMediaStream *stream;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	stream = purple_media_get_stream(media, sess_id, participant);
	return stream ? purple_media_candidate_list_from_fs(
			stream->local_candidates) : NULL;
#else
	return NULL;
#endif
}

void
purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
                                   const gchar *participant,
                                   GList *remote_candidates)
{
#ifdef USE_VV
	PurpleMediaStream *stream;
	GError *err = NULL;

	g_return_if_fail(PURPLE_IS_MEDIA(media));
	stream = purple_media_get_stream(media, sess_id, participant);

	if (stream == NULL) {
		purple_debug_error("media",
				"purple_media_add_remote_candidates: "
				"couldn't find stream %s %s.\n",
				sess_id ? sess_id : "(null)",
				participant ? participant : "(null)");
		return;
	}

	stream->remote_candidates = g_list_concat(stream->remote_candidates,
			purple_media_candidate_list_to_fs(remote_candidates));

	if (stream->accepted == TRUE) {
		fs_stream_set_remote_candidates(stream->stream,
				stream->remote_candidates, &err);

		if (err) {
			purple_debug_error("media", "Error adding remote"
					" candidates: %s\n", err->message);
			g_error_free(err);
		}
	}
#endif
}

#if 0
/*
 * These two functions aren't being used and I'd rather not lock in the API
 * until they are needed. If they ever are.
 */

GList *
purple_media_get_active_local_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant)
{
#ifdef USE_VV
	PurpleMediaStream *stream;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	stream = purple_media_get_stream(media, sess_id, participant);
	return purple_media_candidate_list_from_fs(
			stream->active_local_candidates);
#else
	return NULL;
#endif
}

GList *
purple_media_get_active_remote_candidates(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant)
{
#ifdef USE_VV
	PurpleMediaStream *stream;
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), NULL);
	stream = purple_media_get_stream(media, sess_id, participant);
	return purple_media_candidate_list_from_fs(
			stream->active_remote_candidates);
#else
	return NULL;
#endif
}
#endif

gboolean
purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id,
                               const gchar *participant, GList *codecs)
{
#ifdef USE_VV
	PurpleMediaStream *stream;
	FsStream *fsstream;
	GList *fscodecs;
	GError *err = NULL;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);
	stream = purple_media_get_stream(media, sess_id, participant);

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
#else
	return FALSE;
#endif
}

gboolean
purple_media_candidates_prepared(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
#ifdef USE_VV
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
#else
	return FALSE;
#endif
}

gboolean
purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, PurpleMediaCodec *codec)
{
#ifdef USE_VV
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
#else
	return FALSE;
#endif
}

gboolean
purple_media_codecs_ready(PurpleMedia *media, const gchar *sess_id)
{
#ifdef USE_VV
	gboolean ret;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	if (sess_id != NULL) {
		PurpleMediaSession *session;
		session = purple_media_get_session(media, sess_id);

		if (session == NULL)
			return FALSE;
		if (session->type & (PURPLE_MEDIA_SEND_AUDIO |
				PURPLE_MEDIA_SEND_VIDEO))
			g_object_get(session->session,
					"codecs-ready", &ret, NULL);
		else
			ret = TRUE;
	} else {
		GList *values = g_hash_table_get_values(media->priv->sessions);
		for (; values; values = g_list_delete_link(values, values)) {
			PurpleMediaSession *session = values->data;
			if (session->type & (PURPLE_MEDIA_SEND_AUDIO |
					PURPLE_MEDIA_SEND_VIDEO))
				g_object_get(session->session,
						"codecs-ready", &ret, NULL);
			else
				ret = TRUE;

			if (ret == FALSE)
				break;
		}
		if (values != NULL)
			g_list_free(values);
	}
	return ret;
#else
	return FALSE;
#endif
}

gboolean
purple_media_is_initiator(PurpleMedia *media,
		const gchar *sess_id, const gchar *participant)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	if (sess_id == NULL && participant == NULL)
		return media->priv->initiator;
	else if (sess_id != NULL && participant == NULL) {
		PurpleMediaSession *session =
				purple_media_get_session(media, sess_id);
		return session != NULL ? session->initiator : FALSE;
	} else if (sess_id != NULL && participant != NULL) {
		PurpleMediaStream *stream = purple_media_get_stream(
				media, sess_id, participant);
		return stream != NULL ? stream->initiator : FALSE;
	}
#endif
	return FALSE;
}

gboolean
purple_media_accepted(PurpleMedia *media, const gchar *sess_id,
		const gchar *participant)
{
#ifdef USE_VV
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
#else
	return FALSE;
#endif
}

void purple_media_set_input_volume(PurpleMedia *media,
		const gchar *session_id, double level)
{
#ifdef USE_VV
	GList *sessions;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	purple_prefs_set_int("/purple/media/audio/volume/input", level);

	if (session_id == NULL)
		sessions = g_hash_table_get_values(media->priv->sessions);
	else
		sessions = g_list_append(NULL,
				purple_media_get_session(media, session_id));

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session = sessions->data;

		if (session->type & PURPLE_MEDIA_SEND_AUDIO) {
			gchar *name = g_strdup_printf("volume_%s",
					session->id);
			GstElement *volume = gst_bin_get_by_name(
					GST_BIN(session->media->priv->confbin),
					name);
			g_free(name);
			g_object_set(volume, "volume", level/10.0, NULL);
		}
	}
#endif
}

void purple_media_set_output_volume(PurpleMedia *media,
		const gchar *session_id, const gchar *participant,
		double level)
{
#ifdef USE_VV
	GList *streams;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	purple_prefs_set_int("/purple/media/audio/volume/output", level);

	streams = purple_media_get_streams(media,
			session_id, participant);

	for (; streams; streams = g_list_delete_link(streams, streams)) {
		PurpleMediaStream *stream = streams->data;

		if (stream->session->type & PURPLE_MEDIA_RECV_AUDIO
				&& GST_IS_ELEMENT(stream->volume)) {
			g_object_set(stream->volume, "volume", level/10.0, NULL);
		}
	}
#endif
}

gulong
purple_media_set_output_window(PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id)
{
#ifdef USE_VV
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	return purple_media_manager_set_output_window(media->priv->manager,
			media, session_id, participant, window_id);
#else
	return 0;
#endif
}

void
purple_media_remove_output_windows(PurpleMedia *media)
{
#ifdef USE_VV
	GList *iter = media->priv->streams;
	for (; iter; iter = g_list_next(iter)) {
		PurpleMediaStream *stream = iter->data;
		purple_media_manager_remove_output_windows(
				media->priv->manager, media,
				stream->session->id, stream->participant);
	}

	iter = purple_media_get_session_ids(media);
	for (; iter; iter = g_list_delete_link(iter, iter)) {
		gchar *session_name = iter->data;
		purple_media_manager_remove_output_windows(
				media->priv->manager, media,
				session_name, NULL);
	}
#endif
}

#ifdef USE_GSTREAMER
GstElement *
purple_media_get_tee(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
#ifdef USE_VV
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
#else
	return NULL;
#endif
}
#endif /* USE_GSTREAMER */

