/**
 * @file mediamanager.c Media Manager API
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

#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "marshallers.h"
#include "mediamanager.h"
#include "media.h"

#ifdef USE_VV

#include <gst/farsight/fs-conference-iface.h>
#include <gst/interfaces/xoverlay.h>

typedef struct _PurpleMediaOutputWindow PurpleMediaOutputWindow;

struct _PurpleMediaOutputWindow
{
	gulong id;
	PurpleMedia *media;
	gchar *session_id;
	gchar *participant;
	gulong window_id;
	GstElement *sink;
};

struct _PurpleMediaManagerPrivate
{
	GstElement *pipeline;
	GList *medias;
	GList *elements;
	GList *output_windows;
	gulong next_output_window_id;

	PurpleMediaElementInfo *video_src;
	PurpleMediaElementInfo *video_sink;
	PurpleMediaElementInfo *audio_src;
	PurpleMediaElementInfo *audio_sink;
};

#define PURPLE_MEDIA_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_MEDIA_MANAGER, PurpleMediaManagerPrivate))

static void purple_media_manager_class_init (PurpleMediaManagerClass *klass);
static void purple_media_manager_init (PurpleMediaManager *media);
static void purple_media_manager_finalize (GObject *object);

static GObjectClass *parent_class = NULL;



enum {
	INIT_MEDIA,
	LAST_SIGNAL
};
static guint purple_media_manager_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_FARSIGHT_SESSION,
	PROP_NAME,
	PROP_CONNECTION,
	PROP_MIC_ELEMENT,
	PROP_SPEAKER_ELEMENT,
};

GType
purple_media_manager_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMediaManagerClass),
			NULL,
			NULL,
			(GClassInitFunc) purple_media_manager_class_init,
			NULL,
			NULL,
			sizeof(PurpleMediaManager),
			0,
			(GInstanceInitFunc) purple_media_manager_init,
			NULL
		};
		type = g_type_register_static(G_TYPE_OBJECT, "PurpleMediaManager", &info, 0);
	}
	return type;
}


static void
purple_media_manager_class_init (PurpleMediaManagerClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);
	
	gobject_class->finalize = purple_media_manager_finalize;

	purple_media_manager_signals[INIT_MEDIA] = g_signal_new ("init-media",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		purple_smarshal_BOOLEAN__OBJECT_POINTER_STRING,
		G_TYPE_BOOLEAN, 3, PURPLE_TYPE_MEDIA,
		G_TYPE_POINTER, G_TYPE_STRING);
	g_type_class_add_private(klass, sizeof(PurpleMediaManagerPrivate));
}

static void
purple_media_manager_init (PurpleMediaManager *media)
{
	media->priv = PURPLE_MEDIA_MANAGER_GET_PRIVATE(media);
	media->priv->medias = NULL;
	media->priv->next_output_window_id = 1;
}

static void
purple_media_manager_finalize (GObject *media)
{
	PurpleMediaManagerPrivate *priv = PURPLE_MEDIA_MANAGER_GET_PRIVATE(media);
	for (; priv->medias; priv->medias =
			g_list_delete_link(priv->medias, priv->medias)) {
		g_object_unref(priv->medias->data);
	}
	for (; priv->elements; priv->elements =
			g_list_delete_link(priv->elements, priv->elements));
	parent_class->finalize(media);
}

PurpleMediaManager *
purple_media_manager_get()
{
	static PurpleMediaManager *manager = NULL;

	if (manager == NULL)
		manager = PURPLE_MEDIA_MANAGER(g_object_new(purple_media_manager_get_type(), NULL));
	return manager;
}

static gboolean
pipeline_bus_call(GstBus *bus, GstMessage *msg, PurpleMediaManager *manager)
{
	switch(GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_EOS:
			purple_debug_info("mediamanager", "End of Stream\n");
			break;
		case GST_MESSAGE_ERROR: {
			gchar *debug = NULL;
			GError *err = NULL;

			gst_message_parse_error(msg, &err, &debug);

			purple_debug_error("mediamanager",
					"gst pipeline error: %s\n",
					err->message);
			g_error_free(err);

			if (debug) {
				purple_debug_error("mediamanager",
						"Debug details: %s\n", debug);
				g_free (debug);
			}
			break;
		}
		default:
			break;
	}
	return TRUE;
}

GstElement *
purple_media_manager_get_pipeline(PurpleMediaManager *manager)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	if (manager->priv->pipeline == NULL) {
		GstBus *bus;
		manager->priv->pipeline = gst_pipeline_new(NULL);

		bus = gst_pipeline_get_bus(
				GST_PIPELINE(manager->priv->pipeline));
		gst_bus_add_signal_watch(GST_BUS(bus));
		g_signal_connect(G_OBJECT(bus), "message",
				G_CALLBACK(pipeline_bus_call), manager);
		gst_bus_set_sync_handler(bus,
				gst_bus_sync_signal_handler, NULL);
		gst_object_unref(bus);

		gst_element_set_state(manager->priv->pipeline,
				GST_STATE_PLAYING);
	}

	return manager->priv->pipeline;
}

PurpleMedia *
purple_media_manager_create_media(PurpleMediaManager *manager,
				  PurpleConnection *gc,
				  const char *conference_type,
				  const char *remote_user,
				  gboolean initiator)
{
	PurpleMedia *media;
	FsConference *conference = FS_CONFERENCE(gst_element_factory_make(conference_type, NULL));
	GstStateChangeReturn ret;
	gboolean signal_ret;

	if (conference == NULL) {
		purple_conv_present_error(remote_user,
					  purple_connection_get_account(gc),
					  _("Error creating conference."));
		purple_debug_error("media", "Conference == NULL\n");
		return NULL;
	}

	media = PURPLE_MEDIA(g_object_new(purple_media_get_type(),
			     "manager", manager,
			     "connection", gc,
			     "conference", conference,
			     "initiator", initiator,
			     NULL));

	ret = gst_element_set_state(GST_ELEMENT(conference), GST_STATE_PLAYING);

	if (ret == GST_STATE_CHANGE_FAILURE) {
		purple_conv_present_error(remote_user,
					  purple_connection_get_account(gc),
					  _("Error creating conference."));
		purple_debug_error("media", "Failed to start conference.\n");
		g_object_unref(media);
		return NULL;
	}

	g_signal_emit(manager, purple_media_manager_signals[INIT_MEDIA], 0,
			media, gc, remote_user, &signal_ret);

	if (signal_ret == FALSE) {
		g_object_unref(media);
		return NULL;
	}

	manager->priv->medias = g_list_append(manager->priv->medias, media);
	return media;
}

GList *
purple_media_manager_get_media(PurpleMediaManager *manager)
{
	return manager->priv->medias;
}

GList *
purple_media_manager_get_media_by_connection(PurpleMediaManager *manager,
		PurpleConnection *pc)
{
	GList *media = NULL;
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	iter = manager->priv->medias;
	for (; iter; iter = g_list_next(iter)) {
		if (purple_media_get_connection(iter->data) == pc) {
			media = g_list_prepend(media, iter->data);
		}
	}

	return media;
}

void
purple_media_manager_remove_media(PurpleMediaManager *manager,
				  PurpleMedia *media)
{
	GList *list = g_list_find(manager->priv->medias, media);
	if (list)
		manager->priv->medias =
			g_list_delete_link(manager->priv->medias, list);
}

GstElement *
purple_media_manager_get_element(PurpleMediaManager *manager,
		PurpleMediaSessionType type)
{
	GstElement *ret = NULL;

	/* TODO: If src, retrieve current src */
	/* TODO: Send a signal here to allow for overriding the source/sink */

	if (type & PURPLE_MEDIA_SEND_AUDIO
			&& manager->priv->audio_src != NULL)
		ret = manager->priv->audio_src->create();
	else if (type & PURPLE_MEDIA_RECV_AUDIO
			&& manager->priv->audio_sink != NULL)
		ret = manager->priv->audio_sink->create();
	else if (type & PURPLE_MEDIA_SEND_VIDEO
			&& manager->priv->video_src != NULL)
		ret = manager->priv->video_src->create();
	else if (type & PURPLE_MEDIA_RECV_VIDEO
			&& manager->priv->video_sink != NULL)
		ret = manager->priv->video_sink->create();

	if (ret == NULL)
		purple_debug_error("media", "Error creating source or sink\n");

	return ret;
}

PurpleMediaElementInfo *
purple_media_manager_get_element_info(PurpleMediaManager *manager,
		const gchar *id)
{
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	iter = manager->priv->elements;

	for (; iter; iter = g_list_next(iter)) {
		PurpleMediaElementInfo *info = iter->data;
		if (!strcmp(info->id, id))
			return info;
	}

	return NULL;
}

gboolean
purple_media_manager_register_element(PurpleMediaManager *manager,
		PurpleMediaElementInfo *info)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);
	g_return_val_if_fail(info != NULL, FALSE);

	if (purple_media_manager_get_element_info(manager, info->id) != NULL)
		return FALSE;

	manager->priv->elements =
			g_list_prepend(manager->priv->elements, info);
	return TRUE;
}

gboolean
purple_media_manager_unregister_element(PurpleMediaManager *manager,
		const gchar *id)
{
	PurpleMediaElementInfo *info;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);

	info = purple_media_manager_get_element_info(manager, id);

	if (info == NULL)
		return FALSE;

	if (manager->priv->audio_src == info)
		manager->priv->audio_src = NULL;
	if (manager->priv->audio_sink == info)
		manager->priv->audio_sink = NULL;
	if (manager->priv->video_src == info)
		manager->priv->video_src = NULL;
	if (manager->priv->video_sink == info)
		manager->priv->video_sink = NULL;

	manager->priv->elements = g_list_remove(
			manager->priv->elements, info);
	return TRUE;
}

gboolean
purple_media_manager_set_active_element(PurpleMediaManager *manager,
		PurpleMediaElementInfo *info)
{
	gboolean ret = FALSE;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);
	g_return_val_if_fail(info != NULL, FALSE);

	if (purple_media_manager_get_element_info(manager, info->id) == NULL)
		purple_media_manager_register_element(manager, info);

	if (info->type & PURPLE_MEDIA_ELEMENT_SRC) {
		if (info->type & PURPLE_MEDIA_ELEMENT_AUDIO) {
			manager->priv->audio_src = info;
			ret = TRUE;
		}
		if (info->type & PURPLE_MEDIA_ELEMENT_VIDEO) {
			manager->priv->video_src = info;
			ret = TRUE;
		}
	}
	if (info->type & PURPLE_MEDIA_ELEMENT_SINK) {
		if (info->type & PURPLE_MEDIA_ELEMENT_AUDIO) {
			manager->priv->audio_sink = info;
			ret = TRUE;
		}
		if (info->type & PURPLE_MEDIA_ELEMENT_VIDEO) {
			manager->priv->video_sink = info;
			ret = TRUE;
		}
	}

	return ret;
}

PurpleMediaElementInfo *
purple_media_manager_get_active_element(PurpleMediaManager *manager,
		PurpleMediaElementType type)
{
	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), NULL);

	if (type & PURPLE_MEDIA_ELEMENT_SRC) {
		if (type & PURPLE_MEDIA_ELEMENT_AUDIO)
			return manager->priv->audio_src;
		else if (type & PURPLE_MEDIA_ELEMENT_VIDEO)
			return manager->priv->video_src;
	} else if (type & PURPLE_MEDIA_ELEMENT_SINK) {
		if (type & PURPLE_MEDIA_ELEMENT_AUDIO)
			return manager->priv->audio_sink;
		else if (type & PURPLE_MEDIA_ELEMENT_VIDEO)
			return manager->priv->video_sink;
	}

	return NULL;
}

static void
window_id_cb(GstBus *bus, GstMessage *msg, PurpleMediaOutputWindow *ow)
{
	if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ELEMENT ||
			!gst_structure_has_name(msg->structure,
			"prepare-xwindow-id"))
		return;

	if (GST_ELEMENT_PARENT(GST_MESSAGE_SRC(msg)) == ow->sink) {
		g_signal_handlers_disconnect_matched(bus, G_SIGNAL_MATCH_FUNC
				| G_SIGNAL_MATCH_DATA, 0, 0, NULL,
				window_id_cb, ow);

		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(
				GST_MESSAGE_SRC(msg)), ow->window_id);
	}
}

gboolean
purple_media_manager_create_output_window(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant)
{
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	iter = manager->priv->output_windows;
	for(; iter; iter = g_list_next(iter)) {
		PurpleMediaOutputWindow *ow = iter->data;

		if (ow->sink == NULL && ow->media == media &&
				((participant != NULL &&
				ow->participant != NULL &&
				!strcmp(participant, ow->participant)) ||
				(participant == ow->participant)) &&
				!strcmp(session_id, ow->session_id)) {
			GstBus *bus;
			GstElement *queue;
			GstElement *tee = purple_media_get_tee(media,
					session_id, participant);

			if (tee == NULL)
				continue;

			queue = gst_element_factory_make(
					"queue", NULL);
			ow->sink = purple_media_manager_get_element(
					manager, PURPLE_MEDIA_RECV_VIDEO);

			if (participant == NULL) {
				/* aka this is a preview sink */
				GObjectClass *klass =
						G_OBJECT_GET_CLASS(ow->sink);
				if (g_object_class_find_property(klass,
						"sync"))
					g_object_set(G_OBJECT(ow->sink),
							"sync", "FALSE", NULL);
				if (g_object_class_find_property(klass,
						"async"))
					g_object_set(G_OBJECT(ow->sink),
							"async", FALSE, NULL);
			}

			gst_bin_add_many(GST_BIN(GST_ELEMENT_PARENT(tee)),
					queue, ow->sink, NULL);

			bus = gst_pipeline_get_bus(GST_PIPELINE(
					manager->priv->pipeline));
			g_signal_connect(bus, "sync-message::element",
					G_CALLBACK(window_id_cb), ow);
			gst_object_unref(bus);

			gst_element_sync_state_with_parent(ow->sink);
			gst_element_link(queue, ow->sink);
			gst_element_sync_state_with_parent(queue);
			gst_element_link(tee, queue);
		}
	}
	return TRUE;
}

gulong
purple_media_manager_set_output_window(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant, gulong window_id)
{
	PurpleMediaOutputWindow *output_window;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_MEDIA(media), FALSE);

	output_window = g_new0(PurpleMediaOutputWindow, 1);
	output_window->id = manager->priv->next_output_window_id++;
	output_window->media = media;
	output_window->session_id = g_strdup(session_id);
	output_window->participant = g_strdup(participant);
	output_window->window_id = window_id;

	manager->priv->output_windows = g_list_prepend(
			manager->priv->output_windows, output_window);

	if (purple_media_get_tee(media, session_id, participant) != NULL)
		purple_media_manager_create_output_window(manager,
				media, session_id, participant);

	return output_window->id;
}

gboolean
purple_media_manager_remove_output_window(PurpleMediaManager *manager,
		gulong output_window_id)
{
	PurpleMediaOutputWindow *output_window = NULL;
	GList *iter;

	g_return_val_if_fail(PURPLE_IS_MEDIA_MANAGER(manager), FALSE);

	iter = manager->priv->output_windows;
	for (; iter; iter = g_list_next(iter)) {
		PurpleMediaOutputWindow *ow = iter->data;
		if (ow->id == output_window_id) {
			manager->priv->output_windows = g_list_delete_link(
					manager->priv->output_windows, iter);
			output_window = ow;
			break;
		}
	}

	if (output_window == NULL)
		return FALSE;

	if (output_window->sink != NULL) {
		GstPad *pad = gst_element_get_static_pad(
				output_window->sink, "sink");
		GstPad *peer = gst_pad_get_peer(pad);
		GstElement *queue = GST_ELEMENT_PARENT(peer);
		gst_object_unref(pad);
		pad = gst_element_get_static_pad(queue, "sink");
		peer = gst_pad_get_peer(pad);
		gst_object_unref(pad);
		gst_element_release_request_pad(GST_ELEMENT_PARENT(peer), peer);
		gst_element_set_locked_state(queue, TRUE);
		gst_element_set_state(queue, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(queue)), queue);
		gst_element_set_locked_state(output_window->sink, TRUE);
		gst_element_set_state(output_window->sink, GST_STATE_NULL);
		gst_bin_remove(GST_BIN(GST_ELEMENT_PARENT(output_window->sink)),
				output_window->sink);
	}

	g_free(output_window->session_id);
	g_free(output_window->participant);
	g_free(output_window);

	return TRUE;
}

void
purple_media_manager_remove_output_windows(PurpleMediaManager *manager,
		PurpleMedia *media, const gchar *session_id,
		const gchar *participant)
{
	GList *iter;

	g_return_if_fail(PURPLE_IS_MEDIA(media));

	iter = manager->priv->output_windows;

	for (; iter;) {
		PurpleMediaOutputWindow *ow = iter->data;
		iter = g_list_next(iter);

	if (media == ow->media &&
			((session_id != NULL && ow->session_id != NULL &&
			!strcmp(session_id, ow->session_id)) ||
			(session_id == ow->session_id)) &&
			((participant != NULL && ow->participant != NULL &&
			!strcmp(participant, ow->participant)) ||
			(participant == ow->participant)))
		purple_media_manager_remove_output_window(
				manager, ow->id);
	}
}

#endif  /* USE_VV */
