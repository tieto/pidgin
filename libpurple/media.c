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

#include "account.h"
#include "media.h"
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
/** @copydoc _PurpleMediaCandidateClass */
typedef struct _PurpleMediaCandidateClass PurpleMediaCandidateClass;
/** @copydoc _PurpleMediaCandidatePrivate */
typedef struct _PurpleMediaCandidatePrivate PurpleMediaCandidatePrivate;
/** @copydoc _PurpleMediaCodecClass */
typedef struct _PurpleMediaCodecClass PurpleMediaCodecClass;
/** @copydoc _PurpleMediaCodecPrivate */
typedef struct _PurpleMediaCodecPrivate PurpleMediaCodecPrivate;

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
	FsConference *conference;
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
#define PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA_CANDIDATE, PurpleMediaCandidatePrivate))
#define PURPLE_MEDIA_CODEC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA_CODEC, PurpleMediaCodecPrivate))

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
	PROP_CONFERENCE,
	PROP_INITIATOR,
	PROP_PRPL_DATA,
};
#endif


/*
 * PurpleMediaElementType
 */

GType
purple_media_session_type_get_type()
{
	static GType type = 0;
	if (type == 0) {
		static const GFlagsValue values[] = {
			{ PURPLE_MEDIA_NONE,
				"PURPLE_MEDIA_NONE", "none" },
			{ PURPLE_MEDIA_RECV_AUDIO,
				"PURPLE_MEDIA_RECV_AUDIO", "recv-audio" },
			{ PURPLE_MEDIA_SEND_AUDIO,
				"PURPLE_MEDIA_SEND_AUDIO", "send-audio" },
			{ PURPLE_MEDIA_RECV_VIDEO,
				"PURPLE_MEDIA_RECV_VIDEO", "recv-video" },
			{ PURPLE_MEDIA_SEND_VIDEO,
				"PURPLE_MEDIA_SEND_VIDEO", "send-audio" },
			{ PURPLE_MEDIA_AUDIO,
				"PURPLE_MEDIA_AUDIO", "audio" },
			{ PURPLE_MEDIA_VIDEO,
				"PURPLE_MEDIA_VIDEO", "video" },
			{ 0, NULL, NULL }
		};
		type = g_flags_register_static(
				"PurpleMediaSessionType", values);
	}
	return type;
}

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
			{ PURPLE_MEDIA_INFO_ACCEPT,
					"PURPLE_MEDIA_INFO_ACCEPT", "accept" },
			{ PURPLE_MEDIA_INFO_REJECT,
					"PURPLE_MEDIA_INFO_REJECT", "reject" },
			{ PURPLE_MEDIA_INFO_MUTE,
					"PURPLE_MEDIA_INFO_MUTE", "mute" },
			{ PURPLE_MEDIA_INFO_UNMUTE,
					"PURPLE_MEDIA_INFO_UNMUTE", "unmute" },
			{ PURPLE_MEDIA_INFO_PAUSE,
					"PURPLE_MEDIA_INFO_PAUSE", "pause" },
			{ PURPLE_MEDIA_INFO_UNPAUSE,
					"PURPLE_MEDIA_INFO_UNPAUSE", "unpause" },
			{ PURPLE_MEDIA_INFO_HOLD,
					"PURPLE_MEDIA_INFO_HOLD", "hold" },
			{ PURPLE_MEDIA_INFO_UNHOLD,
					"PURPLE_MEDIA_INFO_HOLD", "unhold" },
			{ 0, NULL, NULL }
		};
		type = g_enum_register_static("PurpleMediaInfoType", values);
	}
	return type;
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

	g_object_class_install_property(gobject_class, PROP_CONFERENCE,
			g_param_spec_object("conference",
			"Farsight conference",
			"The FsConference associated with this media.",
			FS_TYPE_CONFERENCE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

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
		case PROP_ACCOUNT:
			media->priv->account = g_value_get_pointer(value);
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
		case PROP_ACCOUNT:
			g_value_set_pointer(value, media->priv->account);
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
#endif

/*
 * PurpleMediaCandidateType
 */

GType
purple_media_candidate_type_get_type()
{
	static GType type = 0;
	if (type == 0) {
		static const GEnumValue values[] = {
			{ PURPLE_MEDIA_CANDIDATE_TYPE_HOST,
					"PURPLE_MEDIA_CANDIDATE_TYPE_HOST",
					"host" },
			{ PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX,
					"PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX",
					"srflx" },
			{ PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX,
					"PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX",
					"prflx" },
			{ PURPLE_MEDIA_CANDIDATE_TYPE_RELAY,
					"PPURPLE_MEDIA_CANDIDATE_TYPE_RELAY",
					"relay" },
			{ PURPLE_MEDIA_CANDIDATE_TYPE_MULTICAST,
					"PURPLE_MEDIA_CANDIDATE_TYPE_MULTICAST",
					"multicast" },
			{ 0, NULL, NULL }
		};
		type = g_enum_register_static("PurpleMediaCandidateType",
				values);
	}
	return type;
}

/*
 * PurpleMediaNetworkProtocol
 */

GType
purple_media_network_protocol_get_type()
{
	static GType type = 0;
	if (type == 0) {
		static const GEnumValue values[] = {
			{ PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
					"PURPLE_MEDIA_NETWORK_PROTOCOL_UDP",
					"udp" },
			{ PURPLE_MEDIA_NETWORK_PROTOCOL_TCP,
					"PURPLE_MEDIA_NETWORK_PROTOCOL_TCP",
					"tcp" },
			{ 0, NULL, NULL }
		};
		type = g_enum_register_static("PurpleMediaNetworkProtocol",
				values);
	}
	return type;
}

/*
 * PurpleMediaCandidate
 */

struct _PurpleMediaCandidateClass
{
	GObjectClass parent_class;
};

struct _PurpleMediaCandidate
{
	GObject parent;
};

#ifdef USE_VV
struct _PurpleMediaCandidatePrivate
{
	gchar *foundation;
	guint component_id;
	gchar *ip;
	guint16 port;
	gchar *base_ip;
	guint16 base_port;
	PurpleMediaNetworkProtocol proto;
	guint32 priority;
	PurpleMediaCandidateType type;
	gchar *username;
	gchar *password;
	guint ttl;
};

enum {
	PROP_CANDIDATE_0,
	PROP_FOUNDATION,
	PROP_COMPONENT_ID,
	PROP_IP,
	PROP_PORT,
	PROP_BASE_IP,
	PROP_BASE_PORT,
	PROP_PROTOCOL,
	PROP_PRIORITY,
	PROP_TYPE,
	PROP_USERNAME,
	PROP_PASSWORD,
	PROP_TTL,
};

static void
purple_media_candidate_init(PurpleMediaCandidate *info)
{
	PurpleMediaCandidatePrivate *priv =
			PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(info);
	priv->foundation = NULL;
	priv->component_id = 0;
	priv->ip = NULL;
	priv->port = 0;
	priv->base_ip = NULL;
	priv->proto = PURPLE_MEDIA_NETWORK_PROTOCOL_UDP;
	priv->priority = 0;
	priv->type = PURPLE_MEDIA_CANDIDATE_TYPE_HOST;
	priv->username = NULL;
	priv->password = NULL;
	priv->ttl = 0;
}

static void
purple_media_candidate_finalize(GObject *info)
{
	PurpleMediaCandidatePrivate *priv =
			PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(info);

	g_free(priv->foundation);
	g_free(priv->ip);
	g_free(priv->base_ip);
	g_free(priv->username);
	g_free(priv->password);
}

static void
purple_media_candidate_set_property (GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	PurpleMediaCandidatePrivate *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_CANDIDATE(object));

	priv = PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_FOUNDATION:
			g_free(priv->foundation);
			priv->foundation = g_value_dup_string(value);
			break;
		case PROP_COMPONENT_ID:
			priv->component_id = g_value_get_uint(value);
			break;
		case PROP_IP:
			g_free(priv->ip);
			priv->ip = g_value_dup_string(value);
			break;
		case PROP_PORT:
			priv->port = g_value_get_uint(value);
			break;
		case PROP_BASE_IP:
			g_free(priv->base_ip);
			priv->base_ip = g_value_dup_string(value);
			break;
		case PROP_BASE_PORT:
			priv->base_port = g_value_get_uint(value);
			break;
		case PROP_PROTOCOL:
			priv->proto = g_value_get_enum(value);
			break;
		case PROP_PRIORITY:
			priv->priority = g_value_get_uint(value);
			break;
		case PROP_TYPE:
			priv->type = g_value_get_enum(value);
			break;
		case PROP_USERNAME:
			g_free(priv->username);
			priv->username = g_value_dup_string(value);
			break;
		case PROP_PASSWORD:
			g_free(priv->password);
			priv->password = g_value_dup_string(value);
			break;
		case PROP_TTL:
			priv->ttl = g_value_get_uint(value);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_candidate_get_property (GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	PurpleMediaCandidatePrivate *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_CANDIDATE(object));
	
	priv = PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_FOUNDATION:
			g_value_set_string(value, priv->foundation);
			break;
		case PROP_COMPONENT_ID:
			g_value_set_uint(value, priv->component_id);
			break;
		case PROP_IP:
			g_value_set_string(value, priv->ip);
			break;
		case PROP_PORT:
			g_value_set_uint(value, priv->port);
			break;
		case PROP_BASE_IP:
			g_value_set_string(value, priv->base_ip);
			break;
		case PROP_BASE_PORT:
			g_value_set_uint(value, priv->base_port);
			break;
		case PROP_PROTOCOL:
			g_value_set_enum(value, priv->proto);
			break;
		case PROP_PRIORITY:
			g_value_set_uint(value, priv->priority);
			break;
		case PROP_TYPE:
			g_value_set_enum(value, priv->type);
			break;
		case PROP_USERNAME:
			g_value_set_string(value, priv->username);
			break;
		case PROP_PASSWORD:
			g_value_set_string(value, priv->password);
			break;
		case PROP_TTL:
			g_value_set_uint(value, priv->ttl);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_candidate_class_init(PurpleMediaCandidateClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	
	gobject_class->finalize = purple_media_candidate_finalize;
	gobject_class->set_property = purple_media_candidate_set_property;
	gobject_class->get_property = purple_media_candidate_get_property;

	g_object_class_install_property(gobject_class, PROP_FOUNDATION,
			g_param_spec_string("foundation",
			"Foundation",
			"The foundation of the candidate.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_COMPONENT_ID,
			g_param_spec_uint("component-id",
			"Component ID",
			"The component id of the candidate.",
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_IP,
			g_param_spec_string("ip",
			"IP Address",
			"The IP address of the candidate.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PORT,
			g_param_spec_uint("port",
			"Port",
			"The port of the candidate.",
			0, G_MAXUINT16, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BASE_IP,
			g_param_spec_string("base-ip",
			"Base IP",
			"The internal IP address of the candidate.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BASE_PORT,
			g_param_spec_uint("base-port",
			"Base Port",
			"The internal port of the candidate.",
			0, G_MAXUINT16, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PROTOCOL,
			g_param_spec_enum("protocol",
			"Protocol",
			"The protocol of the candidate.",
			PURPLE_TYPE_MEDIA_NETWORK_PROTOCOL,
			PURPLE_MEDIA_NETWORK_PROTOCOL_UDP,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PRIORITY,
			g_param_spec_uint("priority",
			"Priority",
			"The priority of the candidate.",
			0, G_MAXUINT32, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_TYPE,
			g_param_spec_enum("type",
			"Type",
			"The type of the candidate.",
			PURPLE_TYPE_MEDIA_CANDIDATE_TYPE,
			PURPLE_MEDIA_CANDIDATE_TYPE_HOST,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_USERNAME,
			g_param_spec_string("username",
			"Username",
			"The username used to connect to the candidate.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PASSWORD,
			g_param_spec_string("password",
			"Password",
			"The password use to connect to the candidate.",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_TTL,
			g_param_spec_uint("ttl",
			"TTL",
			"The TTL of the candidate.",
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(PurpleMediaCandidatePrivate));
}

G_DEFINE_TYPE(PurpleMediaCandidate,
		purple_media_candidate, G_TYPE_OBJECT);
#else
GType
purple_media_candidate_get_type()
{
	return G_TYPE_NONE;
}
#endif

PurpleMediaCandidate *
purple_media_candidate_new(const gchar *foundation, guint component_id,
		PurpleMediaCandidateType type,
		PurpleMediaNetworkProtocol proto,
		const gchar *ip, guint port)
{
	return g_object_new(PURPLE_TYPE_MEDIA_CANDIDATE,
			"foundation", foundation,
			"component-id", component_id,
			"type", type,
			"protocol", proto,
			"ip", ip,
			"port", port, NULL);
}

static PurpleMediaCandidate *
purple_media_candidate_copy(PurpleMediaCandidate *candidate)
{
#ifdef USE_VV
	PurpleMediaCandidatePrivate *priv;
	PurpleMediaCandidate *new_candidate;

	if (candidate == NULL)
		return NULL;

	priv = PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(candidate);

	new_candidate = purple_media_candidate_new(priv->foundation,
			priv->component_id, priv->type, priv->proto,
			priv->ip, priv->port);
	g_object_set(new_candidate,
			"base-ip", priv->base_ip,
			"base-port", priv->base_port,
			"priority", priv->priority,
			"username", priv->username,
			"password", priv->password,
			"ttl", priv->ttl, NULL);
	return new_candidate;
#else
	return NULL;
#endif
}

#ifdef USE_VV
static FsCandidate *
purple_media_candidate_to_fs(PurpleMediaCandidate *candidate)
{
	PurpleMediaCandidatePrivate *priv;
	FsCandidate *fscandidate;

	if (candidate == NULL)
		return NULL;

	priv = PURPLE_MEDIA_CANDIDATE_GET_PRIVATE(candidate);

	fscandidate = fs_candidate_new(priv->foundation,
			priv->component_id, priv->type,
			priv->proto, priv->ip, priv->port);

	fscandidate->base_ip = g_strdup(priv->base_ip);
	fscandidate->base_port = priv->base_port;
	fscandidate->priority = priv->priority;
	fscandidate->username = g_strdup(priv->username);
	fscandidate->password = g_strdup(priv->password);
	fscandidate->ttl = priv->ttl;
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
#endif

GList *
purple_media_candidate_list_copy(GList *candidates)
{
	GList *new_list = NULL;

	for (; candidates; candidates = g_list_next(candidates)) {
		new_list = g_list_prepend(new_list,
				purple_media_candidate_copy(candidates->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

void
purple_media_candidate_list_free(GList *candidates)
{
	for (; candidates; candidates =
			g_list_delete_link(candidates, candidates)) {
		g_object_unref(candidates->data);
	}
}

gchar *
purple_media_candidate_get_foundation(PurpleMediaCandidate *candidate)
{
	gchar *foundation;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), NULL);
	g_object_get(candidate, "foundation", &foundation, NULL);
	return foundation;
}

guint
purple_media_candidate_get_component_id(PurpleMediaCandidate *candidate)
{
	guint component_id;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), 0);
	g_object_get(candidate, "component-id", &component_id, NULL);
	return component_id;
}

gchar *
purple_media_candidate_get_ip(PurpleMediaCandidate *candidate)
{
	gchar *ip;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), NULL);
	g_object_get(candidate, "ip", &ip, NULL);
	return ip;
}

guint16
purple_media_candidate_get_port(PurpleMediaCandidate *candidate)
{
	guint port;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), 0);
	g_object_get(candidate, "port", &port, NULL);
	return port;
}

gchar *
purple_media_candidate_get_base_ip(PurpleMediaCandidate *candidate)
{
	gchar *base_ip;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), NULL);
	g_object_get(candidate, "base-ip", &base_ip, NULL);
	return base_ip;
}

guint16
purple_media_candidate_get_base_port(PurpleMediaCandidate *candidate)
{
	guint base_port;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), 0);
	g_object_get(candidate, "base_port", &base_port, NULL);
	return base_port;
}

PurpleMediaNetworkProtocol
purple_media_candidate_get_protocol(PurpleMediaCandidate *candidate)
{
	PurpleMediaNetworkProtocol protocol;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate),
			PURPLE_MEDIA_NETWORK_PROTOCOL_UDP);
	g_object_get(candidate, "protocol", &protocol, NULL);
	return protocol;
}

guint32
purple_media_candidate_get_priority(PurpleMediaCandidate *candidate)
{
	guint priority;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), 0);
	g_object_get(candidate, "priority", &priority, NULL);
	return priority;
}

PurpleMediaCandidateType
purple_media_candidate_get_candidate_type(PurpleMediaCandidate *candidate)
{
	PurpleMediaCandidateType type;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate),
			PURPLE_MEDIA_CANDIDATE_TYPE_HOST);
	g_object_get(candidate, "type", &type, NULL);
	return type;
}

gchar *
purple_media_candidate_get_username(PurpleMediaCandidate *candidate)
{
	gchar *username;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), NULL);
	g_object_get(candidate, "username", &username, NULL);
	return username;
}

gchar *
purple_media_candidate_get_password(PurpleMediaCandidate *candidate)
{
	gchar *password;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), NULL);
	g_object_get(candidate, "password", &password, NULL);
	return password;
}

guint
purple_media_candidate_get_ttl(PurpleMediaCandidate *candidate)
{
	guint ttl;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CANDIDATE(candidate), 0);
	g_object_get(candidate, "ttl", &ttl, NULL);
	return ttl;
}

#ifdef USE_VV
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
#endif

/*
 * PurpleMediaCodec
 */

struct _PurpleMediaCodecClass
{
	GObjectClass parent_class;
};

struct _PurpleMediaCodec
{
	GObject parent;
};

#ifdef USE_VV
struct _PurpleMediaCodecPrivate
{
	gint id;
	char *encoding_name;
	PurpleMediaSessionType media_type;
	guint clock_rate;
	guint channels;
	GList *optional_params;
};

enum {
	PROP_CODEC_0,
	PROP_ID,
	PROP_ENCODING_NAME,
	PROP_MEDIA_TYPE,
	PROP_CLOCK_RATE,
	PROP_CHANNELS,
	PROP_OPTIONAL_PARAMS,
};

static void
purple_media_codec_init(PurpleMediaCodec *info)
{
	PurpleMediaCodecPrivate *priv =
			PURPLE_MEDIA_CODEC_GET_PRIVATE(info);
	priv->encoding_name = NULL;
	priv->optional_params = NULL;
}

static void
purple_media_codec_finalize(GObject *info)
{
	PurpleMediaCodecPrivate *priv =
			PURPLE_MEDIA_CODEC_GET_PRIVATE(info);
	g_free(priv->encoding_name);
	for (; priv->optional_params; priv->optional_params =
			g_list_delete_link(priv->optional_params,
			priv->optional_params)) {
		g_free(priv->optional_params->data);
	}
}

static void
purple_media_codec_set_property (GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	PurpleMediaCodecPrivate *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_CODEC(object));

	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_ID:
			priv->id = g_value_get_uint(value);
			break;
		case PROP_ENCODING_NAME:
			g_free(priv->encoding_name);
			priv->encoding_name = g_value_dup_string(value);
			break;
		case PROP_MEDIA_TYPE:
			priv->media_type = g_value_get_flags(value);
			break;
		case PROP_CLOCK_RATE:
			priv->clock_rate = g_value_get_uint(value);
			break;
		case PROP_CHANNELS:
			priv->channels = g_value_get_uint(value);
			break;
		case PROP_OPTIONAL_PARAMS:
			priv->optional_params = g_value_get_pointer(value);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_codec_get_property (GObject *object, guint prop_id,
		GValue *value, GParamSpec *pspec)
{
	PurpleMediaCodecPrivate *priv;
	g_return_if_fail(PURPLE_IS_MEDIA_CODEC(object));
	
	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(object);

	switch (prop_id) {
		case PROP_ID:
			g_value_set_uint(value, priv->id);
			break;
		case PROP_ENCODING_NAME:
			g_value_set_string(value, priv->encoding_name);
			break;
		case PROP_MEDIA_TYPE:
			g_value_set_flags(value, priv->media_type);
			break;
		case PROP_CLOCK_RATE:
			g_value_set_uint(value, priv->clock_rate);
			break;
		case PROP_CHANNELS:
			g_value_set_uint(value, priv->channels);
			break;
		case PROP_OPTIONAL_PARAMS:
			g_value_set_pointer(value, priv->optional_params);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID(
					object, prop_id, pspec);
			break;
	}
}

static void
purple_media_codec_class_init(PurpleMediaCodecClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	
	gobject_class->finalize = purple_media_codec_finalize;
	gobject_class->set_property = purple_media_codec_set_property;
	gobject_class->get_property = purple_media_codec_get_property;

	g_object_class_install_property(gobject_class, PROP_ID,
			g_param_spec_uint("id",
			"ID",
			"The numeric identifier of the codec.",
			0, G_MAXUINT, 0,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ENCODING_NAME,
			g_param_spec_string("encoding-name",
			"Encoding Name",
			"The name of the codec.",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_MEDIA_TYPE,
			g_param_spec_flags("media-type",
			"Media Type",
			"Whether this is an audio of video codec.",
			PURPLE_TYPE_MEDIA_SESSION_TYPE,
			PURPLE_MEDIA_NONE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CLOCK_RATE,
			g_param_spec_uint("clock-rate",
			"Create Callback",
			"The function called to create this element.",
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CHANNELS,
			g_param_spec_uint("channels",
			"Channels",
			"The number of channels in this codec.",
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class, PROP_OPTIONAL_PARAMS,
			g_param_spec_pointer("optional-params",
			"Optional Params",
			"A list of optional parameters for the codec.",
			G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(PurpleMediaCodecPrivate));
}

G_DEFINE_TYPE(PurpleMediaCodec,
		purple_media_codec, G_TYPE_OBJECT);
#else
GType
purple_media_codec_get_type()
{
	return G_TYPE_NONE;
}
#endif

guint
purple_media_codec_get_id(PurpleMediaCodec *codec)
{
	guint id;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CODEC(codec), 0);
	g_object_get(codec, "id", &id, NULL);
	return id;
}

gchar *
purple_media_codec_get_encoding_name(PurpleMediaCodec *codec)
{
	gchar *name;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CODEC(codec), NULL);
	g_object_get(codec, "encoding-name", &name, NULL);
	return name;
}

guint
purple_media_codec_get_clock_rate(PurpleMediaCodec *codec)
{
	guint clock_rate;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CODEC(codec), 0);
	g_object_get(codec, "clock-rate", &clock_rate, NULL);
	return clock_rate;
}

guint
purple_media_codec_get_channels(PurpleMediaCodec *codec)
{
	guint channels;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CODEC(codec), 0);
	g_object_get(codec, "channels", &channels, NULL);
	return channels;
}

GList *
purple_media_codec_get_optional_parameters(PurpleMediaCodec *codec)
{
	GList *optional_params;
	g_return_val_if_fail(PURPLE_IS_MEDIA_CODEC(codec), NULL);
	g_object_get(codec, "optional-params", &optional_params, NULL);
	return optional_params;
}

void
purple_media_codec_add_optional_parameter(PurpleMediaCodec *codec,
		const gchar *name, const gchar *value)
{
#ifdef USE_VV
	PurpleMediaCodecPrivate *priv;
	PurpleKeyValuePair *new_param;

	g_return_if_fail(codec != NULL);
	g_return_if_fail(name != NULL && value != NULL);

	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(codec);

	new_param = g_new0(PurpleKeyValuePair, 1);
	new_param->key = g_strdup(name);
	new_param->value = g_strdup(value);
	priv->optional_params = g_list_append(
			priv->optional_params, new_param);
#endif
}

void
purple_media_codec_remove_optional_parameter(PurpleMediaCodec *codec,
		PurpleKeyValuePair *param)
{
#ifdef USE_VV
	PurpleMediaCodecPrivate *priv;

	g_return_if_fail(codec != NULL && param != NULL);

	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(codec);

	g_free(param->key);
	g_free(param->value);
	g_free(param);

	priv->optional_params =
			g_list_remove(priv->optional_params, param);
#endif
}

PurpleKeyValuePair *
purple_media_codec_get_optional_parameter(PurpleMediaCodec *codec,
		const gchar *name, const gchar *value)
{
#ifdef USE_VV
	PurpleMediaCodecPrivate *priv;
	GList *iter;

	g_return_val_if_fail(codec != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(codec);

	for (iter = priv->optional_params; iter; iter = g_list_next(iter)) {
		PurpleKeyValuePair *param = iter->data;
		if (!g_ascii_strcasecmp(param->key, name) &&
				(value == NULL ||
				!g_ascii_strcasecmp(param->value, value)))
			return param;
	}
#endif

	return NULL;
}

PurpleMediaCodec *
purple_media_codec_new(int id, const char *encoding_name,
		PurpleMediaSessionType media_type, guint clock_rate)
{
	PurpleMediaCodec *codec =
			g_object_new(PURPLE_TYPE_MEDIA_CODEC,
			"id", id,
			"encoding_name", encoding_name,
			"media_type", media_type,
			"clock-rate", clock_rate, NULL);
	return codec;
}

static PurpleMediaCodec *
purple_media_codec_copy(PurpleMediaCodec *codec)
{
#ifdef USE_VV
	PurpleMediaCodecPrivate *priv;
	PurpleMediaCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(codec);

	new_codec = purple_media_codec_new(priv->id, priv->encoding_name,
			priv->media_type, priv->clock_rate);
	g_object_set(codec, "channels", priv->channels, NULL);

	for (iter = priv->optional_params; iter; iter = g_list_next(iter)) {
		PurpleKeyValuePair *param =
				(PurpleKeyValuePair*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->key, param->value);
	}

	return new_codec;
#else
	return NULL;
#endif
}

#ifdef USE_VV
static FsCodec *
purple_media_codec_to_fs(const PurpleMediaCodec *codec)
{
	PurpleMediaCodecPrivate *priv;
	FsCodec *new_codec;
	GList *iter;

	if (codec == NULL)
		return NULL;

	priv = PURPLE_MEDIA_CODEC_GET_PRIVATE(codec);

	new_codec = fs_codec_new(priv->id, priv->encoding_name,
			purple_media_to_fs_media_type(priv->media_type),
			priv->clock_rate);
	new_codec->channels = priv->channels;

	for (iter = priv->optional_params; iter; iter = g_list_next(iter)) {
		PurpleKeyValuePair *param = (PurpleKeyValuePair*)iter->data;
		fs_codec_add_optional_parameter(new_codec,
				param->key, param->value);
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
	g_object_set(new_codec, "channels", codec->channels, NULL);

	for (iter = codec->optional_params; iter; iter = g_list_next(iter)) {
		FsCodecParameter *param = (FsCodecParameter*)iter->data;
		purple_media_codec_add_optional_parameter(new_codec,
				param->name, param->value);
	}

	return new_codec;
}
#endif

gchar *
purple_media_codec_to_string(const PurpleMediaCodec *codec)
{
#ifdef USE_VV
	FsCodec *fscodec = purple_media_codec_to_fs(codec);
	gchar *str = fs_codec_to_string(fscodec);
	fs_codec_destroy(fscodec);
	return str;
#else
	return g_strdup("");
#endif
}

#ifdef USE_VV
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
#endif

GList *
purple_media_codec_list_copy(GList *codecs)
{
	GList *new_list = NULL;

	for (; codecs; codecs = g_list_next(codecs)) {
		new_list = g_list_prepend(new_list,
				purple_media_codec_copy(codecs->data));
	}

	new_list = g_list_reverse(new_list);
	return new_list;
}

void
purple_media_codec_list_free(GList *codecs)
{
	for (; codecs; codecs =
			g_list_delete_link(codecs, codecs)) {
		g_object_unref(codecs->data);
	}
}

#ifdef USE_VV
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
	g_object_unref(candidate);

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

static void
purple_media_element_added_cb(FsElementAddedNotifier *self,
		GstBin *bin, GstElement *element, gpointer user_data)
{
	/*
	 * Hack to make H264 work with Gmail video.
	 */
	if (!strncmp(GST_ELEMENT_NAME(element), "x264", 4)) {
		g_object_set(GST_OBJECT(element), "cabac", FALSE, NULL);
	}
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

	session = purple_media_get_session(media, sess_id);

	if (!session) {
		GError *err = NULL;
		GList *codec_conf = NULL, *iter = NULL;
		gchar *filename = NULL;
		PurpleMediaSessionType session_type;
		GstElement *src = NULL;

		session = g_new0(PurpleMediaSession, 1);

		session->session = fs_conference_new_session(
				media->priv->conference, media_type, &err);

		if (err != NULL) {
			purple_media_error(media, "Error creating session: %s\n", err->message);
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
					G_CALLBACK(purple_media_element_added_cb),
					stream);
			fs_element_added_notifier_add(notifier,
					GST_BIN(media->priv->conference));
		}

		fs_codec_list_destroy(codec_conf);

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

	fs_stream_set_remote_candidates(stream->stream,
			stream->remote_candidates, &err);

	if (err) {
		purple_debug_error("media", "Error adding remote"
				" candidates: %s\n", err->message);
		g_error_free(err);
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

