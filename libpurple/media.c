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

#include "debug.h"

#ifdef USE_FARSIGHT
#ifdef USE_GSTPROPS

#include <gst/interfaces/propertyprobe.h>
#include <farsight/farsight.h>

struct _PurpleMediaPrivate
{
	FarsightSession *farsight_session;

	char *name;
	PurpleConnection *connection;
	GstElement *audio_src;
	GstElement *audio_sink;
	GstElement *video_src;
	GstElement *video_sink;

	FarsightStream *audio_stream;
	FarsightStream *video_stream;
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
	ACCEPTED,
	HANGUP,
	REJECT,
	GOT_HANGUP,
	GOT_ACCEPT,
	LAST_SIGNAL
};
static guint purple_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_FARSIGHT_SESSION,
	PROP_NAME,
	PROP_CONNECTION,
	PROP_AUDIO_SRC,
	PROP_AUDIO_SINK,
	PROP_VIDEO_SRC,
	PROP_VIDEO_SINK,
	PROP_VIDEO_STREAM,
	PROP_AUDIO_STREAM
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

	g_object_class_install_property(gobject_class, PROP_FARSIGHT_SESSION,
			g_param_spec_object("farsight-session",
			"Farsight session",
			"The FarsightSession associated with this media.",
			FARSIGHT_TYPE_SESSION,
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

	g_object_class_install_property(gobject_class, PROP_VIDEO_STREAM,
			g_param_spec_object("video-stream",
			"Video stream",
			"The FarsightStream used for video",
			FARSIGHT_TYPE_STREAM,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_AUDIO_STREAM,
			g_param_spec_object("audio-stream",
			"Audio stream",
			"The FarsightStream used for audio",
			FARSIGHT_TYPE_STREAM,
			G_PARAM_READWRITE));

	purple_media_signals[READY] = g_signal_new("ready", G_TYPE_FROM_CLASS(klass),
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
	parent_class->finalize(media);
}

static void
purple_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PurpleMedia *media;
	g_return_if_fail(PURPLE_IS_MEDIA(object));

	media = PURPLE_MEDIA(object);

	switch (prop_id) {
		case PROP_FARSIGHT_SESSION:
			if (media->priv->farsight_session)
				g_object_unref(media->priv->farsight_session);
			media->priv->farsight_session = g_value_get_object(value);
			g_object_ref(media->priv->farsight_session);
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
			break;
		case PROP_AUDIO_SINK:
			if (media->priv->audio_sink)
				gst_object_unref(media->priv->audio_sink);
			media->priv->audio_sink = g_value_get_object(value);
			gst_object_ref(media->priv->audio_sink);
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
		case PROP_VIDEO_STREAM:
			if (media->priv->video_stream)
				g_object_unref(media->priv->video_stream);
			media->priv->video_stream = g_value_get_object(value);
			gst_object_ref(media->priv->video_stream);
			break;
		case PROP_AUDIO_STREAM:
			if (media->priv->audio_stream)
				g_object_unref(media->priv->audio_stream);
			media->priv->audio_stream = g_value_get_object(value);
			gst_object_ref(media->priv->audio_stream);
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
		case PROP_FARSIGHT_SESSION:
			g_value_set_object(value, media->priv->farsight_session);
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
		case PROP_VIDEO_STREAM:
			g_value_set_object(value, media->priv->video_stream);
			break;
		case PROP_AUDIO_STREAM:
			g_value_set_object(value, media->priv->audio_stream);
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
	FarsightStream *stream;
	g_object_get(G_OBJECT(media), "audio-stream", &stream, NULL);
printf("stream: %d\n\n\n", stream);
GstElement *l = farsight_stream_get_pipeline(stream);
printf("Element: %d\n", l);
	return farsight_stream_get_pipeline(stream);
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
		purple_debug_info("Found source '%s' (%s) - no device",
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

#endif  /* USE_GSTPROPS */
#endif  /* USE_FARSIGHT */
