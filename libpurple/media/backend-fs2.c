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

#include "backend-iface.h"
#include "debug.h"
#include "media-gst.h"

#include <gst/farsight/fs-conference-iface.h>

/** @copydoc _PurpleMediaBackendFs2Class */
typedef struct _PurpleMediaBackendFs2Class PurpleMediaBackendFs2Class;
/** @copydoc _PurpleMediaBackendFs2Private */
typedef struct _PurpleMediaBackendFs2Private PurpleMediaBackendFs2Private;

#define PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj) \
		(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
		PURPLE_TYPE_MEDIA_BACKEND_FS2, PurpleMediaBackendFs2Private))

static void purple_media_backend_iface_init(PurpleMediaBackendIface *iface);

static gboolean
_gst_bus_cb(GstBus *bus, GstMessage *msg, PurpleMediaBackend *self);

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

struct _PurpleMediaBackendFs2Private
{
	PurpleMedia *media;
	GstElement *confbin;
	FsConference *conference;
	gchar *conference_type;
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

	if (priv->media) {
		g_object_unref(priv->media);
		priv->media = NULL;
	}
}

static void
purple_media_backend_fs2_finalize(GObject *obj)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(obj);
	g_free(priv->conference_type);
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
			priv->media = g_value_dup_object(value);
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

static gboolean
_gst_bus_cb(GstBus *bus, GstMessage *msg, PurpleMediaBackend *self)
{
	return TRUE;
}

static gboolean
_init_conference(PurpleMediaBackend *self)
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
			purple_media_manager_get());

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

static gboolean
purple_media_backend_fs2_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who,
		PurpleMediaSessionType type, gboolean initiator,
		const gchar *transmitter,
		guint num_params, GParameter *params)
{
	PurpleMediaBackendFs2Private *priv =
			PURPLE_MEDIA_BACKEND_FS2_GET_PRIVATE(self);

	if (priv->conference == NULL || !_init_conference(self)) {
		purple_debug_error("backend-fs2",
				"Error initializing the conference.\n");
		return FALSE;
	}

	return TRUE;
}

static void
purple_media_backend_fs2_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates)
{
}

static GList *
purple_media_backend_fs2_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	return NULL;
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

