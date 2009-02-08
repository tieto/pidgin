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

struct _PurpleMediaManagerPrivate
{
	GList *medias;
	GList *elements;

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

#endif  /* USE_VV */
