/**
 * @file media.c Media API
 * @ingroup core
 *
 * purple
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
	GHashTable *streams;		/* FsStream list map to participant's name */
	PurpleMediaStreamType type;
	GHashTable *local_candidates;	/* map to participant's name? */
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

	g_free(priv->name);

	if (priv->pipeline) {
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
purple_media_to_fs_media_type(PurpleMediaStreamType type)
{
	if (type & PURPLE_MEDIA_AUDIO)
		return FS_MEDIA_TYPE_AUDIO;
	else if (type & PURPLE_MEDIA_VIDEO)
		return FS_MEDIA_TYPE_VIDEO;
	else
		return FS_MEDIA_TYPE_APPLICATION;
}

FsStreamDirection
purple_media_to_fs_stream_direction(PurpleMediaStreamType type)
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

PurpleMediaStreamType
purple_media_from_fs(FsMediaType type, FsStreamDirection direction)
{
	PurpleMediaStreamType result = PURPLE_MEDIA_NONE;
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

PurpleMediaStreamType
purple_media_get_overall_type(PurpleMedia *media)
{
	GList *values = g_hash_table_get_values(media->priv->sessions);
	PurpleMediaStreamType type = PURPLE_MEDIA_NONE;

	for (; values; values = values->next) {
		PurpleMediaSession *session = values->data;
		type |= session->type;
	}

	g_list_free(values);
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

static FsParticipant *
purple_media_add_participant(PurpleMedia *media, const gchar *name)
{
	FsParticipant *participant = purple_media_get_participant(media, name);

	if (participant)
		return participant;

	participant = fs_conference_new_participant(media->priv->conference, g_strdup(name), NULL);

	if (!media->priv->participants) {
		purple_debug_info("media", "Creating hash table for participants\n");
		media->priv->participants = g_hash_table_new(g_str_hash, g_str_equal);
	}

	g_hash_table_insert(media->priv->participants, g_strdup(name), participant);

	return participant;
}

static void
purple_media_insert_stream(PurpleMediaSession *session, const gchar *name, FsStream *stream)
{
	if (!session->streams) {
		purple_debug_info("media", "Creating hash table for streams\n");
		session->streams = g_hash_table_new(g_str_hash, g_str_equal);
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
		session->local_candidates = g_hash_table_new(g_str_hash, g_str_equal);
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
	 if (audio_src) 
		g_object_get(G_OBJECT(media), "audio-src", *audio_src, NULL);
	 if (audio_sink) 
		g_object_get(G_OBJECT(media), "audio-sink", *audio_sink, NULL);
	 if (video_src) 
		g_object_get(G_OBJECT(media), "video-src", *video_src, NULL);
	 if (video_sink) 
		g_object_get(G_OBJECT(media), "video-sink", *video_sink, NULL);

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
	return purple_media_get_session(media, sess_id)->src;
}

GstElement *
purple_media_get_pipeline(PurpleMedia *media)
{
	if (!media->priv->pipeline) {
		media->priv->pipeline = gst_pipeline_new(media->priv->name);
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

const char *
purple_media_get_screenname(PurpleMedia *media)
{
	const char *ret;
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
				gst_element_set_state (element, GST_STATE_NULL);

				ret = g_list_append(ret, device);

				name = purple_media_get_device_name(GST_ELEMENT(element), device);
				purple_debug_info("media", "Found source '%s' (%s) - device '%s' (%s)\n",
						  longname, GST_PLUGIN_FEATURE (factory)->name,
						  name, g_value_get_string(device));
				g_free(name);
			}
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
	GstElement *element = gst_element_factory_create(factory, "video_src");
	gst_object_unref(factory);
	return element;
}

void
purple_media_audio_init_src(GstElement **sendbin, GstElement **sendlevel)
{
	GstElement *src;
	GstPad *pad;
	GstPad *ghost;
	const gchar *audio_device = purple_prefs_get_string("/purple/media/audio/device");

	purple_debug_info("media", "purple_media_audio_init_src\n");

	*sendbin = gst_bin_new("purplesendaudiobin");
	src = gst_element_factory_make("alsasrc", "asrc");
	*sendlevel = gst_element_factory_make("level", "sendlevel");
	gst_bin_add_many(GST_BIN(*sendbin), src, *sendlevel, NULL);
	gst_element_link(src, *sendlevel);
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
	const gchar *video_plugin = purple_prefs_get_string("/purple/media/video/plugin");
	const gchar *video_device = purple_prefs_get_string("/purple/media/video/device");

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
	GstElement *sink;
	GstPad *pad, *ghost;

	purple_debug_info("media", "purple_media_audio_init_recv\n");

	*recvbin = gst_bin_new("pidginrecvaudiobin");
	sink = gst_element_factory_make("alsasink", "asink");
	g_object_set(G_OBJECT(sink), "sync", FALSE, NULL);
	*recvlevel = gst_element_factory_make("level", "recvlevel");
	gst_bin_add_many(GST_BIN(*recvbin), sink, *recvlevel, NULL);
	gst_element_link(*recvlevel, sink);
	pad = gst_element_get_pad(*recvlevel, "sink");
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
	purple_debug_info("media", "got new local candidate: %s\n", local_candidate->candidate_id);
	g_object_get(stream, "participant", &participant, NULL);
	g_object_get(participant, "cname", &name, NULL);
	g_object_unref(participant);

	purple_media_insert_local_candidate(session, name, fs_candidate_copy(local_candidate));

	g_signal_emit(session->media, purple_media_signals[NEW_CANDIDATE],
		      0, session->id, name, fs_candidate_copy(local_candidate));

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
	session->local_candidate = fs_candidate_copy(native_candidate);
	session->remote_candidate = fs_candidate_copy(remote_candidate);

	purple_debug_info("media", "candidate pair established\n");
	g_signal_emit(session->media, purple_media_signals[CANDIDATE_PAIR], 0,
		      session->local_candidate,
		      session->remote_candidate);
}

static void
purple_media_src_pad_added_cb(FsStream *stream, GstPad *srcpad,
			      FsCodec *codec, PurpleMediaSession *session)
{
	GstElement *pipeline = purple_media_get_pipeline(session->media);
	GstPad *sinkpad = gst_element_get_static_pad(session->sink, "ghostsink");
	purple_debug_info("media", "connecting new src pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK ? "success" : "failure");
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
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
		GList *codec_conf;

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
	 * None of these three worked for me. THEORA is known to
	 * not work as of at least Farsight2 0.0.2
	 */
		codec_conf = g_list_prepend(NULL, fs_codec_new(FS_CODEC_ID_DISABLE,
				"THEORA", FS_MEDIA_TYPE_VIDEO, 90000));
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_DISABLE,
				"MPV", FS_MEDIA_TYPE_VIDEO, 90000));
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_DISABLE,
				"H264", FS_MEDIA_TYPE_VIDEO, 90000));

	/* XXX: SPEEX has a latency of 5 or 6 seconds for me */
#if 0
	/* SPEEX is added through the configuration */
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 8000));
		codec_conf = g_list_prepend(codec_conf, fs_codec_new(FS_CODEC_ID_ANY,
				"SPEEX", FS_MEDIA_TYPE_AUDIO, 16000));
#endif

		g_object_set(G_OBJECT(session->session), "local-codecs-config",
			     codec_conf, NULL);

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

	participant = purple_media_add_participant(media, who);

	stream = purple_media_session_get_stream(session, who);

	if (!stream) {
		stream = fs_session_new_stream(session->session, participant, 
					       type_direction, transmitter, 0, NULL, NULL);
		purple_media_insert_stream(session, who, stream);
		/* callback for new local candidate (new local candidate retreived) */
		g_signal_connect(G_OBJECT(stream),
				 "new-local-candidate", G_CALLBACK(purple_media_new_local_candidate_cb), session);
		/* callback for source pad added (new stream source ready) */
		g_signal_connect(G_OBJECT(stream),
				 "src-pad-added", G_CALLBACK(purple_media_src_pad_added_cb), session);
		/* callback for local candidates prepared (local candidates ready to send) */
		g_signal_connect(G_OBJECT(stream), 
				 "local-candidates-prepared", 
				 G_CALLBACK(purple_media_candidates_prepared_cb), session);
		/* callback for new active candidate pair (established connection) */
		g_signal_connect(G_OBJECT(stream),
				 "new-active-candidate-pair", 
				 G_CALLBACK(purple_media_candidate_pair_established_cb), session);
	} else if (*direction != type_direction) {	
		/* change direction */
		g_object_set(stream, "direction", type_direction, NULL);
	}

	return TRUE;
}

gboolean
purple_media_add_stream(PurpleMedia *media, const gchar *sess_id, const gchar *who,
			PurpleMediaStreamType type,
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

PurpleMediaStreamType
purple_media_get_session_type(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return session->type;
}

GList *
purple_media_get_local_codecs(PurpleMedia *media, const gchar *sess_id)
{
	GList *codecs;
	g_object_get(G_OBJECT(purple_media_get_session(media, sess_id)->session),
		     "local-codecs", &codecs, NULL);
	return codecs;
}

GList *
purple_media_get_local_candidates(PurpleMedia *media, const gchar *sess_id, const gchar *name)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	return purple_media_session_get_local_candidates(session, name);
}

GList *
purple_media_get_negotiated_codecs(PurpleMedia *media, const gchar *sess_id)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	GList *codec_intersection;
	g_object_get(session->session, "negotiated-codecs", &codec_intersection, NULL);
	return codec_intersection;
}

void
purple_media_add_remote_candidates(PurpleMedia *media, const gchar *sess_id,
				   const gchar *name, GList *remote_candidates)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsStream *stream = purple_media_session_get_stream(session, name);
	GList *candidates = remote_candidates;
	for (; candidates; candidates = candidates->next)
		fs_stream_add_remote_candidate(stream, candidates->data, NULL);
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

void
purple_media_set_remote_codecs(PurpleMedia *media, const gchar *sess_id, const gchar *name, GList *codecs)
{
	PurpleMediaSession *session = purple_media_get_session(media, sess_id);
	FsStream *stream = purple_media_session_get_stream(session, name);
	fs_stream_set_remote_codecs(stream, codecs, NULL);
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

#endif  /* USE_VV */
