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

struct _PurpleMediaPrivate
{
	FsConference *conference;

	char *name;
	PurpleConnection *connection;
	GstElement *audio_src;
	GstElement *audio_sink;
	GstElement *video_src;
	GstElement *video_sink;

	FsSession *audio_session;
	FsSession *video_session;

	GList *participants; 	/* FsParticipant list */
	GList *audio_streams;	/* FsStream list */
	GList *video_streams;	/* FsStream list */

	/* might be able to just combine these two */
	GstElement *audio_pipeline;
	GstElement *video_pipeline;

	/* this will need to be stored/handled per stream
	 * once having multiple streams is supported */
	GList *local_candidates;

	FsCandidate *local_candidate;
	FsCandidate *remote_candidate;
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
	GOT_HANGUP,
	GOT_ACCEPT,
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
	PROP_AUDIO_SRC,
	PROP_AUDIO_SINK,
	PROP_VIDEO_SRC,
	PROP_VIDEO_SINK,
	PROP_VIDEO_SESSION,
	PROP_AUDIO_SESSION
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

	g_object_class_install_property(gobject_class, PROP_AUDIO_SRC,
			g_param_spec_object("audio-src",
			"Audio source",
			"The GstElement used to source audio",
			GST_TYPE_ELEMENT,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_AUDIO_SINK,
			g_param_spec_object("audio-sink",
			"Audio sink",
			"The GstElement used to sink audio",
			GST_TYPE_ELEMENT,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_VIDEO_SRC,
			g_param_spec_object("video-src",
			"Video source",
			"The GstElement used to source video",
			GST_TYPE_ELEMENT,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_VIDEO_SINK,
			g_param_spec_object("video-sink",
			"Audio source",
			"The GstElement used to sink video",
			GST_TYPE_ELEMENT,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_VIDEO_SESSION,
			g_param_spec_object("video-session",
			"Video stream",
			"The FarsightStream used for video",
			FS_TYPE_SESSION,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_AUDIO_SESSION,
			g_param_spec_object("audio-session",
			"Audio stream",
			"The FarsightStream used for audio",
			FS_TYPE_SESSION,
			G_PARAM_READWRITE));

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
	purple_media_signals[GOT_HANGUP] = g_signal_new("got-hangup", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	purple_media_signals[GOT_ACCEPT] = g_signal_new("got-accept", G_TYPE_FROM_CLASS(klass),
					 G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
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
	GList *iter;
	purple_debug_info("media","purple_media_finalize\n");

	g_free(priv->name);

	if (priv->audio_pipeline) {
		gst_element_set_state(priv->audio_pipeline, GST_STATE_NULL);
		gst_object_unref(priv->audio_pipeline);
	}
	if (priv->video_pipeline) {
		gst_element_set_state(priv->video_pipeline, GST_STATE_NULL);
		gst_object_unref(priv->video_pipeline);
	}

	if (priv->audio_src)
		gst_object_unref(priv->audio_src);
	if (priv->audio_sink)
		gst_object_unref(priv->audio_sink);
	if (priv->video_src)
		gst_object_unref(priv->video_src);
	if (priv->video_sink)
		gst_object_unref(priv->video_sink);

	for (iter = priv->audio_streams; iter; iter = g_list_next(iter)) {
		g_object_unref(iter->data);
	}
	g_list_free(priv->audio_streams);

	for (iter = priv->video_streams; iter; iter = g_list_next(iter)) {
		g_object_unref(iter->data);
	}
	g_list_free(priv->video_streams);

	if (priv->audio_session)
		g_object_unref(priv->audio_session);
	if (priv->video_session)
		g_object_unref(priv->video_session);

	for (iter = priv->participants; iter; iter = g_list_next(iter)) {
		g_object_unref(iter->data);
	}
	g_list_free(priv->participants);

	for (iter = priv->local_candidates; iter; iter = g_list_next(iter)) {
		g_free(iter->data);
	}
	g_list_free(priv->local_candidates);

	if (priv->local_candidate)
		g_free(priv->local_candidate);
	if (priv->remote_candidate)
		g_free(priv->remote_candidate);

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
		case PROP_AUDIO_SRC:
			if (media->priv->audio_src)
				gst_object_unref(media->priv->audio_src);
			media->priv->audio_src = g_value_get_object(value);
			gst_object_ref(media->priv->audio_src);
			gst_bin_add(GST_BIN(purple_media_get_audio_pipeline(media)),
				    media->priv->audio_src);
			break;
		case PROP_AUDIO_SINK:
			if (media->priv->audio_sink)
				gst_object_unref(media->priv->audio_sink);
			media->priv->audio_sink = g_value_get_object(value);
			gst_object_ref(media->priv->audio_sink);
			gst_bin_add(GST_BIN(purple_media_get_audio_pipeline(media)),
				    media->priv->audio_sink);
			break;
		case PROP_VIDEO_SRC:
			if (media->priv->video_src)
				gst_object_unref(media->priv->video_src);
			media->priv->video_src = g_value_get_object(value);
			gst_object_ref(media->priv->video_src);
			break;
		case PROP_VIDEO_SINK:
			if (media->priv->video_sink)
				gst_object_unref(media->priv->video_sink);
			media->priv->video_sink = g_value_get_object(value);
			gst_object_ref(media->priv->video_sink);
			break;
		case PROP_VIDEO_SESSION:
			if (media->priv->video_session)
				g_object_unref(media->priv->video_session);
			media->priv->video_session = g_value_get_object(value);
			gst_object_ref(media->priv->video_session);
			break;
		case PROP_AUDIO_SESSION:
			if (media->priv->audio_session)
				g_object_unref(media->priv->audio_session);
			media->priv->audio_session = g_value_get_object(value);
			gst_object_ref(media->priv->audio_session);
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
		case PROP_AUDIO_SRC:
			g_value_set_object(value, media->priv->audio_src);
			break;
		case PROP_AUDIO_SINK:
			g_value_set_object(value, media->priv->audio_sink);
			break;
		case PROP_VIDEO_SRC:
			g_value_set_object(value, media->priv->video_src);
			break;
		case PROP_VIDEO_SINK:
			g_value_set_object(value, media->priv->video_sink);
			break;
		case PROP_VIDEO_SESSION:
			g_value_set_object(value, media->priv->video_session);
			break;
		case PROP_AUDIO_SESSION:
			g_value_set_object(value, media->priv->audio_session);
			break;

		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);	
			break;
	}

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
purple_media_set_audio_src(PurpleMedia *media, GstElement *audio_src)
{
	g_object_set(G_OBJECT(media), "audio-src", audio_src, NULL);
}

void 
purple_media_set_audio_sink(PurpleMedia *media, GstElement *audio_sink)
{
	g_object_set(G_OBJECT(media), "audio-sink", audio_sink, NULL);
}

void 
purple_media_set_video_src(PurpleMedia *media, GstElement *video_src)
{
	g_object_set(G_OBJECT(media), "video-src", video_src, NULL);
}

void 
purple_media_set_video_sink(PurpleMedia *media, GstElement *video_sink)
{
	g_object_set(G_OBJECT(media), "video-sink", video_sink, NULL);
}

GstElement *
purple_media_get_audio_src(PurpleMedia *media)
{
	GstElement *ret;
	g_object_get(G_OBJECT(media), "audio-src", &ret, NULL);
	return ret;
}

GstElement *
purple_media_get_audio_sink(PurpleMedia *media)
{
	GstElement *ret;
	g_object_get(G_OBJECT(media), "audio-sink", &ret, NULL);
	return ret;
}

GstElement *
purple_media_get_video_src(PurpleMedia *media)
{
	GstElement *ret;
	g_object_get(G_OBJECT(media), "video-src", &ret, NULL);
	return ret;
}

GstElement *
purple_media_get_video_sink(PurpleMedia *media)
{
	GstElement *ret;
	g_object_get(G_OBJECT(media), "video-sink", &ret, NULL);
	return ret;
}

GstElement *
purple_media_get_audio_pipeline(PurpleMedia *media)
{
	if (!media->priv->audio_pipeline) {
		media->priv->audio_pipeline = gst_pipeline_new(media->priv->name);
		gst_bin_add(GST_BIN(media->priv->audio_pipeline), GST_ELEMENT(media->priv->conference));
	}

	return media->priv->audio_pipeline;
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

	*sendbin = gst_bin_new("sendbin");
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
purple_media_audio_init_recv(GstElement **recvbin, GstElement **recvlevel)
{
	GstElement *sink;
	GstPad *pad, *ghost;

	purple_debug_info("media", "purple_media_audio_init_recv\n");

	*recvbin = gst_bin_new("pidginrecvbin");
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

static void
purple_media_new_local_candidate(FsStream *stream,
				  FsCandidate *local_candidate,
				  PurpleMedia *media)
{
	purple_debug_info("media", "got new local candidate: %s\n", local_candidate->candidate_id);
	media->priv->local_candidates = g_list_append(media->priv->local_candidates, 
						      fs_candidate_copy(local_candidate));
}

static void
purple_media_candidates_prepared(FsStream *stream, PurpleMedia *media)
{
	g_signal_emit(media, purple_media_signals[CANDIDATES_PREPARED], 0);
}

/* callback called when a pair of transport candidates (local and remote)
 * has been established */
static void
purple_media_candidate_pair_established(FsStream *stream,
					 FsCandidate *native_candidate,
					 FsCandidate *remote_candidate,
					 PurpleMedia *media)
{
	media->priv->local_candidate = fs_candidate_copy(native_candidate);
	media->priv->remote_candidate = fs_candidate_copy(remote_candidate);

	purple_debug_info("media", "candidate pair established\n");
	g_signal_emit(media, purple_media_signals[CANDIDATE_PAIR], 0,
		      media->priv->local_candidate,
		      media->priv->remote_candidate);
}

static void
purple_media_src_pad_added(FsStream *stream, GstPad *srcpad,
			    FsCodec *codec, PurpleMedia *media)
{
	GstElement *pipeline = purple_media_get_audio_pipeline(media);
	GstPad *sinkpad = gst_element_get_static_pad(purple_media_get_audio_sink(media), "ghostsink");
	purple_debug_info("media", "connecting new src pad: %s\n", 
			  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK ? "success" : "failure");
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

static gboolean
purple_media_add_stream_internal(PurpleMedia *media, FsSession **session, GList **streams,
				 GstElement *src, const gchar *who, FsMediaType type,
				 FsStreamDirection type_direction, const gchar *transmitter)
{
	char *cname = NULL;
	FsParticipant *participant = NULL;
	GList *l = NULL;
	FsStream *stream = NULL;
	FsParticipant *p = NULL;
	FsStreamDirection *direction = NULL;
	FsSession *s = NULL;

	if (!*session) {
		GError *err = NULL;
		*session = fs_conference_new_session(media->priv->conference, type, &err);

		if (err != NULL) {
			purple_debug_error("media", "Error creating session: %s\n", err->message);
			g_error_free(err);
			purple_conv_present_error(who,
						  purple_connection_get_account(purple_media_get_connection(media)),
						  _("Error creating session."));
			return FALSE;
		}

		if (src) {
			GstPad *sinkpad;
			GstPad *srcpad;
			g_object_get(*session, "sink-pad", &sinkpad, NULL);
			srcpad = gst_element_get_static_pad(src, "ghostsrc");
			purple_debug_info("media", "connecting pad: %s\n", 
					  gst_pad_link(srcpad, sinkpad) == GST_PAD_LINK_OK
					  ? "success" : "failure");
		}
	}
	
	for (l = media->priv->participants; l != NULL; l = g_list_next(l)) {
		g_object_get(l->data, "cname", cname, NULL);
		if (!strcmp(cname, who)) {
			g_free(cname);
			participant = l->data;
			break;
		}
		g_free(cname);
	}

	if (!participant) {
		participant = fs_conference_new_participant(media->priv->conference, (gchar*)who, NULL);
		media->priv->participants = g_list_prepend(media->priv->participants, participant);
	}
	
	for (l = *streams; l != NULL; l = g_list_next(l)) {
		g_object_get(l->data, "participant", &p, "direction", &direction, "session", &s, NULL);

		if (participant == p && *session == s) {
			stream = l->data;
			break;
		}
	}

	if (!stream) {
		stream = fs_session_new_stream(*session, participant, 
					       type_direction, transmitter, 0, NULL, NULL);
		*streams = g_list_prepend(*streams, stream);
		/* callback for new local candidate (new local candidate retreived) */
		g_signal_connect(G_OBJECT(stream),
				 "new-local-candidate", G_CALLBACK(purple_media_new_local_candidate), media);
		/* callback for source pad added (new stream source ready) */
		g_signal_connect(G_OBJECT(stream),
				 "src-pad-added", G_CALLBACK(purple_media_src_pad_added), media);
		/* callback for local candidates prepared (local candidates ready to send) */
		g_signal_connect(G_OBJECT(stream), 
				 "local-candidates-prepared", 
				 G_CALLBACK(purple_media_candidates_prepared), media);
		/* callback for new active candidate pair (established connection) */
		g_signal_connect(G_OBJECT(stream),
				 "new-active-candidate-pair", 
				 G_CALLBACK(purple_media_candidate_pair_established), media);
	} else if (*direction != type_direction) {	
		/* change direction */
		g_object_set(stream, "direction", type_direction, NULL);
	}

	return TRUE;
}

gboolean
purple_media_add_stream(PurpleMedia *media, const gchar *who,
			PurpleMediaStreamType type,
			const gchar *transmitter)
{
	FsStreamDirection type_direction;

	if (type & PURPLE_MEDIA_AUDIO) {
		if (type & PURPLE_MEDIA_SEND_AUDIO && type & PURPLE_MEDIA_RECV_AUDIO)
			type_direction = FS_DIRECTION_BOTH;
		else if (type & PURPLE_MEDIA_SEND_AUDIO)
			type_direction = FS_DIRECTION_SEND;
		else if (type & PURPLE_MEDIA_RECV_AUDIO)
			type_direction = FS_DIRECTION_RECV;
		else
			type_direction = FS_DIRECTION_NONE;

		if (!purple_media_add_stream_internal(media, &media->priv->audio_session,
						      &media->priv->audio_streams,
				 		      media->priv->audio_src, who,
						      FS_MEDIA_TYPE_AUDIO, type_direction,
						      transmitter)) {
			return FALSE;
		}
	}
	if (type & PURPLE_MEDIA_VIDEO) {
		if (type & PURPLE_MEDIA_SEND_VIDEO && type & PURPLE_MEDIA_RECV_VIDEO)
			type_direction = FS_DIRECTION_BOTH;
		else if (type & PURPLE_MEDIA_SEND_VIDEO)
			type_direction = FS_DIRECTION_SEND;
		else if (type & PURPLE_MEDIA_RECV_VIDEO)
			type_direction = FS_DIRECTION_RECV;
		else
			type_direction = FS_DIRECTION_NONE;

		if (!purple_media_add_stream_internal(media, &media->priv->video_session,
						      &media->priv->video_streams,
				 		      media->priv->video_src, who,
						      FS_MEDIA_TYPE_VIDEO, type_direction,
						      transmitter)) {
			return FALSE;
		}
	}
	return TRUE;
}

void
purple_media_remove_stream(PurpleMedia *media, const gchar *who, PurpleMediaStreamType type)
{
	
}

static FsStream *
purple_media_get_audio_stream(PurpleMedia *media, const gchar *name)
{
	GList *streams = media->priv->audio_streams;
	for (; streams; streams = streams->next) {
		FsParticipant *participant;
		gchar *cname;
		g_object_get(streams->data, "participant", &participant, NULL);
		g_object_get(participant, "cname", &cname, NULL);

		if (!strcmp(cname, name)) {
			return streams->data;
		}
	}

	return NULL;
}

GList *
purple_media_get_local_audio_codecs(PurpleMedia *media)
{
	GList *codecs;
	g_object_get(G_OBJECT(media->priv->audio_session), "local-codecs", &codecs, NULL);
	return codecs;
}

GList *
purple_media_get_local_audio_candidates(PurpleMedia *media)
{
	return media->priv->local_candidates;
}

GList *
purple_media_get_negotiated_audio_codecs(PurpleMedia *media)
{
	GList *codec_intersection;
	g_object_get(media->priv->audio_session, "negotiated-codecs", &codec_intersection, NULL);
	return codec_intersection;
}

void
purple_media_add_remote_audio_candidates(PurpleMedia *media, const gchar *name, GList *remote_candidates)
{
	FsStream *stream = purple_media_get_audio_stream(media, name);
	GList *candidates = remote_candidates;
	for (; candidates; candidates = candidates->next)
		fs_stream_add_remote_candidate(stream, candidates->data, NULL);
}

FsCandidate *
purple_media_get_local_candidate(PurpleMedia *media)
{
	return media->priv->local_candidate;
}

FsCandidate *
purple_media_get_remote_candidate(PurpleMedia *media)
{
	return media->priv->remote_candidate;
}

void
purple_media_set_remote_audio_codecs(PurpleMedia *media, const gchar *name, GList *codecs)
{
	fs_stream_set_remote_codecs(purple_media_get_audio_stream(media, name), codecs, NULL);
}

#endif  /* USE_VV */
