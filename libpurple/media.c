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

struct _PurpleMediaSession
{
	gchar *id;
	PurpleMedia *media;
	GstElement *src;
	GstElement *sink;
	FsSession *session;
	/* FsStream table. Mapped by participant's name */
	GHashTable *streams;
	PurpleMediaSessionType type;
	/* GList of FsCandidates table. Mapped by participant's name */
	GHashTable *local_candidates;

	/*
	 * These will need to be per stream when sessions with multiple
	 * streams are supported.
	 */
	FsCandidate *local_candidate;
	FsCandidate *remote_candidate;
};

struct _PurpleMediaPrivate
{
	FsConference *conference;

	char *name;
	PurpleConnection *connection;

	GHashTable *sessions;	/* PurpleMediaSession table */
	GHashTable *participants; /* FsParticipant table */

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
	LAST_SIGNAL
};
static guint purple_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_FS_CONFERENCE,
	PROP_NAME,
	PROP_CONNECTION,
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

static void
purple_media_class_init (PurpleMediaClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);
	
	gobject_class->finalize = purple_media_finalize;
	gobject_class->set_property = purple_media_set_property;
	gobject_class->get_property = purple_media_get_property;

	g_object_class_install_property(gobject_class, PROP_FS_CONFERENCE,
			g_param_spec_object("farsight-conference",
			"Farsight conference",
			"The FsConference associated with this media.",
			FS_TYPE_CONFERENCE,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_NAME,
			g_param_spec_string("screenname",
			"Screenname",
			"The screenname of the remote user",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CONNECTION,
			g_param_spec_pointer("connection",
			"Connection",
			"The PurpleConnection associated with this session",
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

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
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[CANDIDATE_PAIR] = g_signal_new("candidate-pair", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 purple_smarshal_VOID__BOXED_BOXED,
					 G_TYPE_NONE, 2, FS_TYPE_CANDIDATE, FS_TYPE_CANDIDATE);

	g_type_class_add_private(klass, sizeof(PurpleMediaPrivate));
}


static void
purple_media_init (PurpleMedia *media)
{
	media->priv = PURPLE_MEDIA_GET_PRIVATE(media);
	memset(media->priv, 0, sizeof(media->priv));
}

static void
purple_media_finalize (GObject *media)
{
	PurpleMediaPrivate *priv = PURPLE_MEDIA_GET_PRIVATE(media);
	purple_debug_info("media","purple_media_finalize\n");

	purple_media_manager_remove_media(purple_media_manager_get(),
			PURPLE_MEDIA(media));

	g_free(priv->name);

	if (priv->sessions) {
		GList *sessions = g_hash_table_get_values(priv->sessions);
		for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
			PurpleMediaSession *session = sessions->data;
			g_free(session->id);

			if (session->streams) {
				GList *streams = g_hash_table_get_values(session->streams);
				for (; streams; streams = g_list_delete_link(streams, streams))
					g_object_unref(streams->data);
				g_hash_table_destroy(session->streams);
			}

			if (session->local_candidates) {
				GList *candidates = g_hash_table_get_values(session->local_candidates);
				for (; candidates; candidates =
						g_list_delete_link(candidates, candidates))
					fs_candidate_list_destroy(candidates->data);
				g_hash_table_destroy(session->local_candidates);
			}

			if (session->local_candidate)
				fs_candidate_destroy(session->local_candidate);
			if (session->remote_candidate)
				fs_candidate_destroy(session->remote_candidate);

			g_free(session);
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
		case PROP_FS_CONFERENCE:
			if (media->priv->conference)
				g_object_unref(media->priv->conference);
			media->priv->conference = g_value_get_object(value);
			g_object_ref(media->priv->conference);
			break;
		case PROP_NAME:
			g_free(media->priv->name);
			media->priv->name = g_value_dup_string(value);
			break;
		case PROP_CONNECTION:
			media->priv->connection = g_value_get_pointer(value);
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
		case PROP_FS_CONFERENCE:
			g_value_set_object(value, media->priv->conference);
			break;
		case PROP_NAME:
			g_value_set_string(value, media->priv->name);
			break;
		case PROP_CONNECTION:
			g_value_set_pointer(value, media->priv->connection);
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

static FsStream*
purple_media_session_get_stream(PurpleMediaSession *session, const gchar *name)
{
	return (FsStream*) (session->streams) ?
			g_hash_table_lookup(session->streams, name) : NULL;
}

static GList*
purple_media_session_get_local_candidates(PurpleMediaSession *session, const gchar *name)
{
	return (GList*) (session->local_candidates) ?
			g_hash_table_lookup(session->local_candidates, name) : NULL;
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

static void
purple_media_insert_stream(PurpleMediaSession *session, const gchar *name, FsStream *stream)
{
	if (!session->streams) {
		purple_debug_info("media", "Creating hash table for streams\n");
		session->streams = g_hash_table_new_full(g_str_hash,
				g_str_equal, g_free, NULL);
	}

	g_hash_table_insert(session->streams, g_strdup(name), stream);
}

static void
purple_media_insert_local_candidate(PurpleMediaSession *session, const gchar *name,
				     FsCandidate *candidate)
{
	GList *candidates = purple_media_session_get_local_candidates(session, name);

	candidates = g_list_append(candidates, candidate);

	if (!session->local_candidates) {
		purple_debug_info("media", "Creating hash table for local candidates\n");
		session->local_candidates = g_hash_table_new_full(g_str_hash,
				g_str_equal, g_free, NULL);
	}

	g_hash_table_insert(session->local_candidates, g_strdup(name), candidates);
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
		if (session->type & PURPLE_MEDIA_RECV_AUDIO && audio_sink)
			*audio_sink = session->sink;
		if (session->type & PURPLE_MEDIA_SEND_VIDEO && video_src)
			*video_src = session->src;
		if (session->type & PURPLE_MEDIA_RECV_VIDEO && video_sink)
			*video_sink = session->sink;
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
purple_media_set_sink(PurpleMedia *media, const gchar *sess_id, GstElement *sink)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	if (session->sink)
		gst_object_unref(session->sink);
	session->sink = sink;
	gst_bin_add(GST_BIN(purple_media_get_pipeline(media)),
		    session->sink);
}

GstElement *
purple_media_get_src(PurpleMedia *media, const gchar *sess_id)
{
	return purple_media_get_session(media, sess_id)->src;
}

GstElement *
purple_media_get_sink(PurpleMedia *media, const gchar *sess_id)
{
	return purple_media_get_session(media, sess_id)->sink;
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
		media->priv->pipeline = gst_pipeline_new(media->priv->name);
		bus = gst_pipeline_get_bus(GST_PIPELINE(media->priv->pipeline));
		gst_bus_add_signal_watch(GST_BUS(bus));
		g_signal_connect(G_OBJECT(bus), "message",
				G_CALLBACK(media_bus_call), media);
		gst_object_unref(bus);

		gst_bin_add(GST_BIN(media->priv->pipeline), GST_ELEMENT(media->priv->conference));
	}

	return media->priv->pipeline;
}

PurpleConnection *
purple_media_get_connection(PurpleMedia *media)
{
	PurpleConnection *gc;
	g_object_get(G_OBJECT(media), "connection", &gc, NULL);
	return gc;
}

char *
purple_media_get_screenname(PurpleMedia *media)
{
	char *ret;
	g_object_get(G_OBJECT(media), "screenname", &ret, NULL);
	return ret;
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
	g_signal_emit(media, purple_media_signals[ACCEPTED], 0);
}

void
purple_media_hangup(PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[HANGUP], 0);
}

void
purple_media_reject(PurpleMedia *media)
{
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
	g_signal_emit(media, purple_media_signals[GOT_HANGUP], 0);
}

void
purple_media_got_accept(PurpleMedia *media)
{
    g_signal_emit(media, purple_media_signals[GOT_ACCEPT], 0);
}

gchar*
purple_media_get_device_name(GstElement *element, GValue *device)
{
	gchar *name;

	GstElementFactory *factory = gst_element_get_factory(element);
	GstElement *temp = gst_element_factory_create(factory, "tmp_src");

	g_object_set_property (G_OBJECT (temp), "device", device);
	g_object_get (G_OBJECT (temp), "device-name", &name, NULL);
	gst_object_unref(temp);

	return name;
}

GList*
purple_media_get_devices(GstElement *element)
{
	GObjectClass *klass;
	GstPropertyProbe *probe;
	const GParamSpec *pspec;

	const gchar *longname = NULL;

	GstElementFactory *factory =
		gst_element_get_factory(element);

	GList *ret = NULL;

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
				GValue *location = g_new0(GValue, 1);
				gst_element_set_state (element, GST_STATE_NULL);
				location = g_value_init(location, G_TYPE_STRING);

				g_value_copy(device, location);
				ret = g_list_append(ret, location);

				name = purple_media_get_device_name(GST_ELEMENT(element), device);
				purple_debug_info("media", "Found source '%s' (%s) - device '%s' (%s)\n",
						  longname, GST_PLUGIN_FEATURE (factory)->name,
						  name, g_value_get_string(device));
				g_free(name);
			}
			g_value_array_free(array);
		}
	}

	return ret;
}

void
purple_media_element_set_device(GstElement *element, GValue *device)
{
	g_object_set_property(G_OBJECT(element), "device", device); 
}

GValue *
purple_media_element_get_device(GstElement *element)
{
	GValue *device;
	g_object_get(G_OBJECT(element), "device", &device, NULL);
	return device;
}

GstElement *
purple_media_get_element(const gchar *factory_name)
{
	GstElementFactory *factory = gst_element_factory_find(factory_name);
	GstElement *element;

	if (factory == NULL)
		return NULL;

	element = gst_element_factory_create(factory, NULL);
	gst_object_unref(factory);
	return element;
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

	/* set current audio device on "src"... */
	if (audio_device) {
		GList *devices = purple_media_get_devices(src);
		GList *dev = devices;
		purple_debug_info("media", "Setting device of GstElement src to %s\n",
				audio_device);
		for (; dev ; dev = dev->next) {
			GValue *device = (GValue *) dev->data;
			char *name = purple_media_get_device_name(src, device);
			if (strcmp(name, audio_device) == 0) {
				purple_media_element_set_device(src, device);
			}
			g_free(name);
		}
	}
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

	tee = gst_element_factory_make("tee", NULL);
	gst_bin_add(GST_BIN(*sendbin), tee);
	gst_element_link(src, tee);

	queue = gst_element_factory_make("queue", NULL);
	gst_bin_add(GST_BIN(*sendbin), queue);
	gst_element_link(tee, queue);

	if (!strcmp(video_plugin, "videotestsrc")) {
		/* unless is-live is set to true it doesn't throttle videotestsrc */
		g_object_set (G_OBJECT(src), "is-live", TRUE, NULL);
	}

	pad = gst_element_get_pad(queue, "src");
	ghost = gst_ghost_pad_new("ghostsrc", pad);
	gst_element_add_pad(*sendbin, ghost);

	queue = gst_element_factory_make("queue", NULL);
	gst_bin_add(GST_BIN(*sendbin), queue);
	gst_element_link(tee, queue);

	local_sink = gst_element_factory_make("autovideosink", "purplelocalvideosink");
	gst_bin_add(GST_BIN(*sendbin), local_sink);
	gst_element_link(queue, local_sink);

	/* set current video device on "src"... */
	if (video_device) {
		GList *devices = purple_media_get_devices(src);
		GList *dev = devices;
		purple_debug_info("media", "Setting device of GstElement src to %s\n",
				video_device);
		for (; dev ; dev = dev->next) {
			GValue *device = (GValue *) dev->data;
			char *name = purple_media_get_device_name(src, device);
			if (strcmp(name, video_device) == 0) {
				purple_media_element_set_device(src, device);
			}
			g_free(name);
		}
	}
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
	g_object_get(stream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);
	g_signal_emit(session->media, purple_media_signals[CANDIDATES_PREPARED], 0);
	g_free(name);
}

/* callback called when a pair of transport candidates (local and remote)
 * has been established */
static void
purple_media_candidate_pair_established_cb(FsStream *stream,
					   FsCandidate *native_candidate,
					   FsCandidate *remote_candidate,
					   PurpleMediaSession *session)
{
	FsCandidate *local = fs_candidate_copy(native_candidate);
	FsCandidate *remote = fs_candidate_copy(remote_candidate);

	session->local_candidate = fs_candidate_copy(native_candidate);
	session->remote_candidate = fs_candidate_copy(remote_candidate);

	purple_debug_info("media", "candidate pair established\n");
	g_signal_emit(session->media, purple_media_signals[CANDIDATE_PAIR], 0,
		      local, remote);

	fs_candidate_destroy(local);
	fs_candidate_destroy(remote);
}

static void
purple_media_src_pad_added_cb(FsStream *stream, GstPad *srcpad,
			      FsCodec *codec, PurpleMediaSession *session)
{
	GstPad *sinkpad = gst_element_get_static_pad(session->sink, "ghostsink");
	purple_debug_info("media", "connecting new src pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK ? "success" : "failure");
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
				 const gchar *transmitter)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsParticipant *participant = NULL;
	FsStream *stream = NULL;
	FsStreamDirection *direction = NULL;

	if (!session) {
		GError *err = NULL;
		GList *codec_conf = NULL;

		session = g_new0(PurpleMediaSession, 1);

		session->session = fs_conference_new_session(media->priv->conference, type, &err);

		if (err != NULL) {
			purple_debug_error("media", "Error creating session: %s\n", err->message);
			g_error_free(err);
			purple_conv_present_error(who,
						  purple_connection_get_account(purple_media_get_connection(media)),
						  _("Error creating session."));
			g_free(session);
			return FALSE;
		}

	/*
	 * The MPV codec didn't work for me.
	 * MPV may not work yet as of Farsight2 0.0.3
	 */
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_DISABLE,
				"MPV", FS_MEDIA_TYPE_VIDEO, 90000));

	/* XXX: SPEEX has a latency of 5 or 6 seconds for me */
#if 0
	/* SPEEX is added through the configuration */
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 8000));
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 16000));
#endif

		fs_session_set_codec_preferences(session->session, codec_conf, NULL);

	/*
	 * Temporary fix to remove a 5-7 second delay before
	 * receiving the src-pad-added signal.
	 * Only works for one-to-one sessions.
	 * Specific to FsRtpSession.
	 */
		g_object_set(G_OBJECT(session->session), "no-rtcp-timeout", 0, NULL);


		fs_codec_list_destroy(codec_conf);

		session->id = g_strdup(sess_id);
		session->media = media;
		session->type = purple_media_from_fs(type, type_direction);

		purple_media_add_session(media, session);
	}

	if (!(participant = purple_media_add_participant(media, who))) {
		purple_media_remove_session(media, session);
		g_free(session);
		return FALSE;
	}

	stream = purple_media_session_get_stream(session, who);

	if (!stream) {
		GError *err = NULL;
		gchar *stun_ip = NULL;

		if (!strcmp(transmitter, "rawudp") &&
				(stun_ip = purple_media_get_stun_pref_ip())) {
			GParameter param[2];
			memset(param, 0, sizeof(GParameter) * 2);

			param[0].name = "stun-ip";
			g_value_init(&param[0].value, G_TYPE_STRING);
			g_value_take_string(&param[0].value, stun_ip);

			param[1].name = "stun-timeout";
			g_value_init(&param[1].value, G_TYPE_UINT);
			g_value_set_uint(&param[1].value, 5);

			stream = fs_session_new_stream(session->session,
					participant, type_direction,
					transmitter, 2, param, &err);
			g_free(stun_ip);
		} else {
			stream = fs_session_new_stream(session->session,
					participant, type_direction,
					transmitter, 0, NULL, &err);
		}

		if (err) {
			purple_debug_error("media", "Error creating stream: %s\n",
					   err->message);
			g_error_free(err);
			g_object_unref(participant);
			purple_media_remove_session(media, session);
			g_free(session);
			return FALSE;
		}

		purple_media_insert_stream(session, who, stream);

		/* callback for source pad added (new stream source ready) */
		g_signal_connect(G_OBJECT(stream),
				 "src-pad-added", G_CALLBACK(purple_media_src_pad_added_cb), session);

	} else if (*direction != type_direction) {	
		/* change direction */
		g_object_set(stream, "direction", type_direction, NULL);
	}

	return TRUE;
}

gboolean
purple_media_add_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who,
			PurpleMediaSessionType type,
			const gchar *transmitter)
{
	FsStreamDirection type_direction;

	if (type & PURPLE_MEDIA_AUDIO) {
		type_direction = purple_media_to_fs_stream_direction(type & PURPLE_MEDIA_AUDIO);

		if (!purple_media_add_stream_internal(media, sess_id, who,
						      FS_MEDIA_TYPE_AUDIO, type_direction,
						      transmitter)) {
			return FALSE;
		}
	}
	if (type & PURPLE_MEDIA_VIDEO) {
		type_direction = purple_media_to_fs_stream_direction(type & PURPLE_MEDIA_VIDEO);

		if (!purple_media_add_stream_internal(media, sess_id, who,
						      FS_MEDIA_TYPE_VIDEO, type_direction,
						      transmitter)) {
			return FALSE;
		}
	}
	return TRUE;
}

void
purple_media_remove_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who)
{
	
}

PurpleMediaSessionType
purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return session->type;
}
/* XXX: Should wait until codecs-ready is TRUE before using this function */
GList *
purple_media_get_local_codecs(PurpleMedia *media, const gchar *sess_id)
{
	GList *codecs;
	g_object_get(G_OBJECT(purple_media_get_session(media, sess_id)->session),
		     "codecs", &codecs, NULL);
	return codecs;
}

GList *
purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return fs_candidate_list_copy(
			purple_media_session_get_local_candidates(session, name));
}
/* XXX: Should wait until codecs-ready is TRUE before using this function */
GList *
purple_media_get_negotiated_codecs(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	GList *codec_intersection;
	g_object_get(session->session, "codecs", &codec_intersection, NULL);
	return codec_intersection;
}

void
purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
				   const gchar *name, GList *remote_candidates)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsStream *stream = purple_media_session_get_stream(session, name);
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
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return session->local_candidate;
}

FsCandidate *
purple_media_get_remote_candidate(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return session->remote_candidate;
}

gboolean
purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id, const gchar *name, GList *codecs)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsStream *stream = purple_media_session_get_stream(session, name);
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

#endif  /* USE_VV */
