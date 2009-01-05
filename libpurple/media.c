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

#include "debug.h"

#ifdef USE_VV

#include <gst/interfaces/propertyprobe.h>
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
};

struct _PurpleMediaStream
{
	PurpleMediaSession *session;
	gchar *participant;
	FsStream *stream;
	GstElement *sink;

	GList *local_candidates;

	gboolean candidates_prepared;

	FsCandidate *local_candidate;
	FsCandidate *remote_candidate;
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
	READY,
	WAIT,
	ACCEPTED,
	HANGUP,
	REJECT,
	GOT_REQUEST,
	GOT_HANGUP,
	GOT_ACCEPT,
	NEW_CANDIDATE,
	CANDIDATES_PREPARED,
	CANDIDATE_PAIR,
	CODECS_READY,
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
	purple_media_signals[READY] = g_signal_new("ready", G_TYPE_FROM_CLASS(klass),
				 	 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[WAIT] = g_signal_new("wait", G_TYPE_FROM_CLASS(klass),
				 	 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[ACCEPTED] = g_signal_new("accepted", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[HANGUP] = g_signal_new("hangup", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[REJECT] = g_signal_new("reject", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[GOT_REQUEST] = g_signal_new("got-request", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[GOT_HANGUP] = g_signal_new("got-hangup", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[GOT_ACCEPT] = g_signal_new("got-accept", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[NEW_CANDIDATE] = g_signal_new("new-candidate", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__POINTER_POINTER_OBJECT,
					 G_TYPE_NONE, 3, G_TYPE_POINTER,
					 G_TYPE_POINTER, FS_TYPE_CANDIDATE);
	purple_media_signals[CANDIDATES_PREPARED] = g_signal_new("candidates-prepared", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__STRING_STRING,
					 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	purple_media_signals[CANDIDATE_PAIR] = g_signal_new("candidate-pair", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__BOXED_BOXED,
					 G_TYPE_NONE, 2, FS_TYPE_CANDIDATE, FS_TYPE_CANDIDATE);
	purple_media_signals[CODECS_READY] = g_signal_new("codecs-ready", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1, G_TYPE_STRING);
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
	g_object_unref(stream->stream);

	if (stream->local_candidates)
		fs_candidate_list_destroy(stream->local_candidates);

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
	g_object_unref(session->session);
	g_free(session);
}

static void
purple_media_finalize (GObject *media)
{
	PurpleMediaPrivate *priv = PURPLE_MEDIA_GET_PRIVATE(media);
	purple_debug_info("media","purple_media_finalize\n");

	purple_media_manager_remove_media(purple_media_manager_get(),
			PURPLE_MEDIA(media));

	for (; priv->streams; priv->streams = g_list_delete_link(priv->streams, priv->streams))
		purple_media_stream_free(priv->streams->data);

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);
		for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
			purple_media_session_free(sessions->data);
		}
		g_hash_table_destroy(priv->sessions);
	}

	if (priv->participants) {
		GList *participants = g_hash_table_get_values(priv->participants);
		for (; participants; participants = g_list_delete_link(participants, participants))
			g_object_unref(participants->data);
		g_hash_table_destroy(priv->participants);
	}

	if (priv->pipeline) {
		GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(priv->pipeline));
		gst_bus_remove_signal_watch(bus);
		gst_object_unref(bus);
		gst_element_set_state(priv->pipeline, GST_STATE_NULL);
		gst_object_unref(priv->pipeline);
	}

	gst_object_unref(priv->conference);

	parent_class->finalize(media);
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

FsMediaType
purple_media_to_fs_media_type(PurpleMediaSessionType type)
{
	if (type & PURPLE_MEDIA_AUDIO)
		return FS_MEDIA_TYPE_AUDIO;
	else if (type & PURPLE_MEDIA_VIDEO)
		return FS_MEDIA_TYPE_VIDEO;
	else
		return 0;
}

FsStreamDirection
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

PurpleMediaSessionType
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
						g_object_get(session->session, "codecs-ready", &ready, NULL);
						if (session->codecs_ready == FALSE && ready == TRUE) {
							session->codecs_ready = ready;
							g_signal_emit(session->media,
									purple_media_signals[CODECS_READY],
									0, session->id);
							purple_media_emit_ready(media, session, NULL);
						}

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
purple_media_ready(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[READY], 0);
}

void
purple_media_wait(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[WAIT], 0);
}

void
purple_media_accept(PurpleMedia *media)
{
	GList *sessions;

	g_signal_emit(media, purple_media_signals[ACCEPTED], 0);

	sessions = g_hash_table_get_values(media->priv->sessions);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session = sessions->data;
		session->accepted = TRUE;

		purple_media_emit_ready(media, session, NULL);
	}
}

void
purple_media_hangup(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CHANGED_END,
			NULL, NULL);
	g_signal_emit(media, purple_media_signals[HANGUP], 0);
}

void
purple_media_reject(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CHANGED_END,
			NULL, NULL);
	g_signal_emit(media, purple_media_signals[REJECT], 0);
}

void
purple_media_got_request(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[GOT_REQUEST], 0);
}

void
purple_media_got_hangup(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[STATE_CHANGED],
			0, PURPLE_MEDIA_STATE_CHANGED_END,
			NULL, NULL);
	g_signal_emit(media, purple_media_signals[GOT_HANGUP], 0);
}

void
purple_media_got_accept(PurpleMedia *media)
{
	GList *sessions;

	g_signal_emit(media, purple_media_signals[GOT_ACCEPT], 0);

	sessions = g_hash_table_get_values(media->priv->sessions);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSession *session = sessions->data;
		session->accepted = TRUE;
	}
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

	purple_debug_info("media", "purple_media_audio_init_src\n");

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
	GstElement *src, *tee, *queue, *local_sink;
	GstPad *pad;
	GstPad *ghost;
	const gchar *video_plugin = purple_prefs_get_string(
			"/purple/media/video/plugin");
	const gchar *video_device = purple_prefs_get_string(
			"/purple/media/video/device");

	purple_debug_info("media", "purple_media_video_init_src\n");

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

	queue = gst_element_factory_make("queue", "purplelocalvideoqueue");
	gst_bin_add(GST_BIN(*sendbin), queue);
	gst_element_link(tee, queue);

	local_sink = gst_element_factory_make("autovideosink", "purplelocalvideosink");
	gst_bin_add(GST_BIN(*sendbin), local_sink);
	gst_element_link(queue, local_sink);

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

	purple_debug_info("media", "purple_media_audio_init_recv\n");

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

	purple_debug_info("media", "purple_media_audio_init_recv end\n");
}

void
purple_media_video_init_recv(GstElement **recvbin)
{
	GstElement *sink;
	GstPad *pad, *ghost;

	purple_debug_info("media", "purple_media_video_init_recv\n");

	*recvbin = gst_bin_new("pidginrecvvideobin");
	sink = gst_element_factory_make("autovideosink", "purplevideosink");
	gst_bin_add(GST_BIN(*recvbin), sink);
	pad = gst_element_get_pad(sink, "sink");
	ghost = gst_ghost_pad_new("ghostsink", pad);
	gst_element_add_pad(*recvbin, ghost);

	purple_debug_info("media", "purple_media_video_init_recv end\n");
}

static void
purple_media_new_local_candidate_cb(FsStream *stream,
				    FsCandidate *local_candidate,
				    PurpleMediaSession *session)
{
	gchar *name;
	FsParticipant *participant;
	FsCandidate *candidate;
	purple_debug_info("media", "got new local candidate: %s\n", local_candidate->foundation);
	g_object_get(stream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	purple_media_insert_local_candidate(session, name, fs_candidate_copy(local_candidate));

	candidate = fs_candidate_copy(local_candidate);
	g_signal_emit(session->media, purple_media_signals[NEW_CANDIDATE],
		      0, session->id, name, candidate);
	fs_candidate_destroy(candidate);

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

	g_signal_emit(session->media, purple_media_signals[CANDIDATES_PREPARED],
			0, session->id, name);

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
	FsCandidate *local = fs_candidate_copy(native_candidate);
	FsCandidate *remote = fs_candidate_copy(remote_candidate);
	PurpleMediaStream *stream;

	g_object_get(fsstream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	stream = purple_media_get_stream(session->media, session->id, name);

	stream->local_candidate = fs_candidate_copy(native_candidate);
	stream->remote_candidate = fs_candidate_copy(remote_candidate);

	purple_debug_info("media", "candidate pair established\n");
	g_signal_emit(session->media, purple_media_signals[CANDIDATE_PAIR], 0,
		      local, remote);

	fs_candidate_destroy(local);
	fs_candidate_destroy(remote);
}

static void
purple_media_src_pad_added_cb(FsStream *fsstream, GstPad *srcpad,
			      FsCodec *codec, PurpleMediaStream *stream)
{
	PurpleMediaSessionType type = purple_media_from_fs(codec->media_type, FS_DIRECTION_RECV);
	GstPad *sinkpad = NULL;

	stream->sink = purple_media_manager_get_element(
			purple_media_manager_get(), type);

	gst_bin_add(GST_BIN(purple_media_get_pipeline(stream->session->media)),
		    stream->sink);
	sinkpad = gst_element_get_static_pad(stream->sink, "ghostsink");
	purple_debug_info("media", "connecting new src pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK ? "success" : "failure");
	gst_element_set_state(stream->sink, GST_STATE_PLAYING);

	g_signal_emit(stream->session->media, purple_media_signals[STATE_CHANGED],
				0, PURPLE_MEDIA_STATE_CHANGED_CONNECTED,
				stream->session->id, stream->participant);
}

static gchar *
purple_media_get_stun_pref_ip()
{
	const gchar *stun_pref =
			purple_prefs_get_string("/purple/network/stun_server");
	struct hostent *host;

	if ((host = gethostbyname(stun_pref)) && host->h_addr) {
		gchar *stun_ip = g_strdup_printf("%hhu.%hhu.%hhu.%hhu",
				host->h_addr[0], host->h_addr[1],
				host->h_addr[2], host->h_addr[3]);
		purple_debug_info("media", "IP address for %s found: %s\n",
				stun_pref, stun_ip);
		return stun_ip;
	} else {
		purple_debug_info("media", "Unable to resolve %s IP address\n",
				stun_pref);
		return NULL;
	}
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
		gchar *stun_ip = NULL;
		FsStream *fsstream = NULL;

		if ((stun_ip = purple_media_get_stun_pref_ip())) {
			GParameter *param = g_new0(GParameter, num_params+1);
			memcpy(param, params, sizeof(GParameter) * num_params);

			param[num_params].name = "stun-ip";
			g_value_init(&param[num_params].value, G_TYPE_STRING);
			g_value_take_string(&param[num_params].value, stun_ip);

			fsstream = fs_session_new_stream(session->session,
					participant, type_direction,
					transmitter, num_params+1, param, &err);
			g_free(param);
			g_free(stun_ip);
		} else {
			fsstream = fs_session_new_stream(session->session,
					participant, type_direction,
					transmitter, num_params, params, &err);
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
	GList *codecs;
	g_object_get(G_OBJECT(purple_media_get_session(media, sess_id)->session),
		     "codecs", &codecs, NULL);
	return codecs;
}

GList *
purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	PurpleMediaStream *stream = purple_media_get_stream(media, sess_id, name);
	return fs_candidate_list_copy(stream->local_candidates);
}

void
purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
				   const gchar *name, GList *remote_candidates)
{
	FsStream *stream = purple_media_get_stream(media, sess_id, name)->stream;
	GError *err = NULL;

	fs_stream_set_remote_candidates(stream, remote_candidates, &err);

	if (err) {
		purple_debug_error("media", "Error adding remote candidates: %s\n",
				   err->message);
		g_error_free(err);
	}
}

FsCandidate *
purple_media_get_local_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	return purple_media_get_stream(media, sess_id, name)->local_candidate;
}

FsCandidate *
purple_media_get_remote_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	return purple_media_get_stream(media, sess_id, name)->remote_candidate;
}

gboolean
purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id, const gchar *name, GList *codecs)
{
	FsStream *stream = purple_media_get_stream(media, sess_id, name)->stream;
	GError *err = NULL;

	fs_stream_set_remote_codecs(stream, codecs, &err);

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
purple_media_set_send_codec(PurpleMedia *media, const gchar *sess_id, FsCodec *codec)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	GError *err = NULL;

	fs_session_set_send_codec(session->session, codec, &err);

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

#endif  /* USE_VV */
