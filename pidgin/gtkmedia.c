/**
 * @file media.c Account API
 * @ingroup core
 *
 * Pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#include "debug.h"
#include "internal.h"
#include "connection.h"
#include "media.h"
#include "pidgin.h"

#include "gtkmedia.h"

#ifdef USE_VV

#include <gst/interfaces/xoverlay.h>

typedef enum
{
	/* Waiting for response */
	PIDGIN_MEDIA_WAITING = 1,
	/* Got request */
	PIDGIN_MEDIA_REQUESTED,
	/* Accepted call */
	PIDGIN_MEDIA_ACCEPTED,
	/* Rejected call */
	PIDGIN_MEDIA_REJECTED,
} PidginMediaState;

struct _PidginMediaPrivate
{
	PurpleMedia *media;
	GstElement *send_level;
	GstElement *recv_level;

	GtkWidget *calling;
	GtkWidget *accept;
	GtkWidget *reject;
	GtkWidget *hangup;
	GtkWidget *mute;

	GtkWidget *send_progress;
	GtkWidget *recv_progress;

	PidginMediaState state;

	GtkWidget *display;
	GtkWidget *local_video;
	GtkWidget *remote_video;
};

#define PIDGIN_MEDIA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), PIDGIN_TYPE_MEDIA, PidginMediaPrivate))

static void pidgin_media_class_init (PidginMediaClass *klass);
static void pidgin_media_init (PidginMedia *media);
static void pidgin_media_finalize (GObject *object);
static void pidgin_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void pidgin_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void pidgin_media_set_state(PidginMedia *gtkmedia, PidginMediaState state);

static GtkHBoxClass *parent_class = NULL;



enum {
	MESSAGE,
	LAST_SIGNAL
};
static guint pidgin_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_MEDIA,
	PROP_SEND_LEVEL,
	PROP_RECV_LEVEL
};

GType
pidgin_media_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(PidginMediaClass),
			NULL,
			NULL,
			(GClassInitFunc) pidgin_media_class_init,
			NULL,
			NULL,
			sizeof(PidginMedia),
			0,
			(GInstanceInitFunc) pidgin_media_init,
			NULL
		};
		type = g_type_register_static(GTK_TYPE_HBOX, "PidginMedia", &info, 0);
	}
	return type;
}


static void
pidgin_media_class_init (PidginMediaClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
/*	GtkContainerClass *container_class = (GtkContainerClass*)klass; */
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = pidgin_media_finalize;
	gobject_class->set_property = pidgin_media_set_property;
	gobject_class->get_property = pidgin_media_get_property;

	g_object_class_install_property(gobject_class, PROP_MEDIA,
			g_param_spec_object("media",
			"PurpleMedia",
			"The PurpleMedia associated with this media.",
			PURPLE_TYPE_MEDIA,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class, PROP_SEND_LEVEL,
			g_param_spec_object("send-level",
			"Send level",
			"The GstElement of this media's send 'level'",
			GST_TYPE_ELEMENT,
			G_PARAM_READWRITE));
	g_object_class_install_property(gobject_class, PROP_RECV_LEVEL,
			g_param_spec_object("recv-level",
			"Receive level",
			"The GstElement of this media's recv 'level'",
			GST_TYPE_ELEMENT,
			G_PARAM_READWRITE));

	pidgin_media_signals[MESSAGE] = g_signal_new("message", G_TYPE_FROM_CLASS(klass),
					G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					g_cclosure_marshal_VOID__STRING,
					G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private(klass, sizeof(PidginMediaPrivate));
}

static void
pidgin_media_mute_toggled(GtkToggleButton *toggle, PidginMedia *media)
{
	purple_media_mute(media->priv->media,
			gtk_toggle_button_get_active(toggle));
}

static void
pidgin_media_init (PidginMedia *media)
{
	media->priv = PIDGIN_MEDIA_GET_PRIVATE(media);
	media->priv->calling = gtk_label_new_with_mnemonic("Calling...");
	media->priv->hangup = gtk_button_new_with_label("Hangup");
	media->priv->accept = gtk_button_new_with_label("Accept");
	media->priv->reject = gtk_button_new_with_label("Reject");
	media->priv->mute = gtk_toggle_button_new_with_label("Mute");

	g_signal_connect(media->priv->mute, "toggled",
			G_CALLBACK(pidgin_media_mute_toggled), media);

	gtk_box_pack_start(GTK_BOX(media), media->priv->calling, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(media), media->priv->hangup, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(media), media->priv->accept, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(media), media->priv->reject, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(media), media->priv->mute, FALSE, FALSE, 0);

	gtk_widget_show_all(media->priv->accept);
	gtk_widget_show_all(media->priv->reject);

	media->priv->display = gtk_vbox_new(TRUE, PIDGIN_HIG_BOX_SPACE);
}

static gboolean
level_message_cb(GstBus *bus, GstMessage *message, PidginMedia *gtkmedia)
{
	const GstStructure *s;
	const gchar *name;

	gdouble rms_db;
	gdouble percent;
	const GValue *list;
	const GValue *value;

	GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(message));

	if (message->type != GST_MESSAGE_ELEMENT)
		return TRUE;

	s = gst_message_get_structure(message);
	name = gst_structure_get_name(s);

	if (strcmp(name, "level"))
		return TRUE;

	list = gst_structure_get_value(s, "rms");

	/* Only bother with the first channel. */
	value = gst_value_list_get_value(list, 0);
	rms_db = g_value_get_double(value);

	percent = pow(10, rms_db / 20) * 5;

	if(percent > 1.0)
		percent = 1.0;

	if (!strcmp(gst_element_get_name(src), "sendlevel"))	
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gtkmedia->priv->send_progress), percent);
	else
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gtkmedia->priv->recv_progress), percent);

	return TRUE;
}


static void
pidgin_media_disconnect_levels(PurpleMedia *media, PidginMedia *gtkmedia)
{
	GstElement *element = purple_media_get_pipeline(media);
	gulong handler_id = g_signal_handler_find(G_OBJECT(gst_pipeline_get_bus(GST_PIPELINE(element))),
						  G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, 
						  NULL, G_CALLBACK(level_message_cb), gtkmedia);
	if (handler_id)
		g_signal_handler_disconnect(G_OBJECT(gst_pipeline_get_bus(GST_PIPELINE(element))),
					    handler_id);
}

static void
pidgin_media_finalize (GObject *media)
{
	PidginMedia *gtkmedia = PIDGIN_MEDIA(media);
	if (gtkmedia->priv->media) {
		pidgin_media_disconnect_levels(gtkmedia->priv->media, gtkmedia);
		g_object_unref(gtkmedia->priv->media);
	}
	if (gtkmedia->priv->send_level)
		gst_object_unref(gtkmedia->priv->send_level);
	if (gtkmedia->priv->recv_level)
		gst_object_unref(gtkmedia->priv->recv_level);
	if (gtkmedia->priv->display)
		gtk_widget_destroy(gtkmedia->priv->display);
}

static void
pidgin_media_emit_message(PidginMedia *gtkmedia, const char *msg)
{
	g_signal_emit(gtkmedia, pidgin_media_signals[MESSAGE], 0, msg);
}

GtkWidget *
pidgin_media_get_display_widget(GtkWidget *gtkmedia)
{
	return PIDGIN_MEDIA_GET_PRIVATE(gtkmedia)->display;
}

static gboolean
create_window (GstBus *bus, GstMessage *message, PidginMedia *gtkmedia)
{
	char *name;

	if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
		return TRUE;

	if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
		return TRUE;

	name = gst_object_get_name(GST_MESSAGE_SRC (message));
	purple_debug_info("gtkmedia", "prepare-xwindow-id object name: %s\n", name);

	/* The XOverlay's name is the sink's name with a suffix */
	if (!strncmp(name, "purplevideosink", strlen("purplevideosink")))
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(GST_MESSAGE_SRC(message)),
					     GDK_WINDOW_XWINDOW(gtkmedia->priv->remote_video->window));
	else if (!strncmp(name, "purplelocalvideosink", strlen("purplelocalvideosink")))
		gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(GST_MESSAGE_SRC(message)),
					     GDK_WINDOW_XWINDOW(gtkmedia->priv->local_video->window));
	g_free(name);
	return TRUE;
}

static void
pidgin_media_ready_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	GstElement *audiosendbin = NULL, *audiosendlevel = NULL;
	GstElement *audiorecvbin = NULL, *audiorecvlevel = NULL;
	GstElement *videosendbin = NULL;
	GstElement *videorecvbin = NULL;

	GList *sessions = purple_media_get_session_names(media);

	for (; sessions; sessions = g_list_delete_link(sessions, sessions)) {
		PurpleMediaSessionType type = purple_media_get_session_type(media, sessions->data);
		if (type & PURPLE_MEDIA_AUDIO) {
			if (!audiosendbin && (type & PURPLE_MEDIA_SEND_AUDIO)) {
				purple_media_audio_init_src(&audiosendbin, &audiosendlevel);
				purple_media_set_src(media, sessions->data, audiosendbin);
				gst_element_set_state(audiosendbin, GST_STATE_READY);
			}
			if (!audiorecvbin && (type & PURPLE_MEDIA_RECV_AUDIO)) {
				purple_media_audio_init_recv(&audiorecvbin, &audiorecvlevel);
				purple_media_set_sink(media, sessions->data, audiorecvbin);
				gst_element_set_state(audiorecvbin, GST_STATE_READY);
			}
		} else if (purple_media_get_session_type(media, sessions->data) & PURPLE_MEDIA_VIDEO) {
			if (!videosendbin && (type & PURPLE_MEDIA_SEND_VIDEO)) {
				purple_media_video_init_src(&videosendbin);
				purple_media_set_src(media, sessions->data, videosendbin);
				gst_element_set_state(videosendbin, GST_STATE_READY);
			}
			if (!videorecvbin && (type & PURPLE_MEDIA_RECV_VIDEO)) {
				purple_media_video_init_recv(&videorecvbin);
				purple_media_set_sink(media, sessions->data, videorecvbin);
				gst_element_set_state(videorecvbin, GST_STATE_READY);
			}
		}
	}

	if (audiosendlevel || audiorecvlevel) {
		g_object_set(gtkmedia, "send-level", audiosendlevel,
				       "recv-level", audiorecvlevel,
				       NULL);
	}
}

static void
pidgin_media_wait_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	pidgin_media_set_state(gtkmedia, PIDGIN_MEDIA_WAITING);
}

/* maybe we should have different callbacks for when we received the accept
    and we accepted ourselves */
static void
pidgin_media_accept_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	GtkWidget *send_widget = NULL, *recv_widget = NULL;

	GstElement *pipeline = purple_media_get_pipeline(media);
	GstElement *audiosendbin = NULL;
	GstElement *audiorecvbin = NULL;
	GstElement *videosendbin = NULL;
	GstElement *videorecvbin = NULL;
	GstBus *bus;

	pidgin_media_emit_message(gtkmedia, _("Call in progress."));
	pidgin_media_set_state(gtkmedia, PIDGIN_MEDIA_ACCEPTED);

	purple_media_get_elements(media, &audiosendbin, &audiorecvbin,
				  &videosendbin, &videorecvbin);

	if (videorecvbin || audiorecvbin) {
		recv_widget = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(gtkmedia->priv->display),
				recv_widget, TRUE, TRUE, 0);
		gtk_widget_show(recv_widget);
	}
	if (videosendbin || audiosendbin) {
		send_widget = gtk_hbox_new(FALSE, PIDGIN_HIG_BOX_SPACE);
		gtk_box_pack_start(GTK_BOX(gtkmedia->priv->display),
				send_widget, TRUE, TRUE, 0);
		gtk_widget_show(send_widget);
	}

	if (videorecvbin) {
		GtkWidget *aspect;
		GtkWidget *remote_video;

		aspect = gtk_aspect_frame_new(NULL, 0.5, 0.5, 4.0/3.0, FALSE);
		gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(recv_widget), aspect, TRUE, TRUE, 0);

		remote_video = gtk_drawing_area_new();
		gtk_container_add(GTK_CONTAINER(aspect), remote_video);
		gtk_widget_set_size_request (GTK_WIDGET(remote_video), 100, -1);
		gtk_widget_show(remote_video);
		gtk_widget_show(aspect);

		gtkmedia->priv->remote_video = remote_video;
		gst_element_set_state(videorecvbin, GST_STATE_PLAYING);
	}
	if (videosendbin) {
		GtkWidget *aspect;
		GtkWidget *local_video;

		aspect = gtk_aspect_frame_new(NULL, 0.5, 0.5, 4.0/3.0, FALSE);
		gtk_frame_set_shadow_type(GTK_FRAME(aspect), GTK_SHADOW_IN);
		gtk_box_pack_start(GTK_BOX(send_widget), aspect, TRUE, TRUE, 0);

		local_video = gtk_drawing_area_new();
		gtk_container_add(GTK_CONTAINER(aspect), local_video);
		gtk_widget_set_size_request (GTK_WIDGET(local_video), 100, -1);
		gtk_widget_show(local_video);
		gtk_widget_show(aspect);

		gtkmedia->priv->local_video = local_video;

		gst_element_set_state(videosendbin, GST_STATE_PLAYING);
	}

	if (audiorecvbin) {
		gtkmedia->priv->recv_progress = gtk_progress_bar_new();
		gtk_widget_set_size_request(gtkmedia->priv->recv_progress, 10, 70);
		gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(gtkmedia->priv->recv_progress),
						 GTK_PROGRESS_BOTTOM_TO_TOP);
		gtk_box_pack_end(GTK_BOX(recv_widget),
				   gtkmedia->priv->recv_progress, FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->recv_progress);
		gst_element_set_state(audiorecvbin, GST_STATE_PLAYING);
	}
	if (audiosendbin) {
		gtkmedia->priv->send_progress = gtk_progress_bar_new();
		gtk_widget_set_size_request(gtkmedia->priv->send_progress, 10, 70);
		gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(gtkmedia->priv->send_progress),
						 GTK_PROGRESS_BOTTOM_TO_TOP);
		gtk_box_pack_end(GTK_BOX(send_widget),
				   gtkmedia->priv->send_progress, FALSE, FALSE, 0);
		gtk_widget_show(gtkmedia->priv->send_progress);
		gst_element_set_state(audiosendbin, GST_STATE_PLAYING);

		gtk_widget_show(gtkmedia->priv->mute);
	}

	if (audiorecvbin || audiosendbin || videorecvbin || videosendbin)
		gtk_widget_show(gtkmedia->priv->display);

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	if (audiorecvbin || audiosendbin) {
		gst_bus_add_signal_watch(GST_BUS(bus));
		g_signal_connect(G_OBJECT(bus), "message",
				G_CALLBACK(level_message_cb), gtkmedia);
	}
	if (videorecvbin || videosendbin)
		gst_bus_set_sync_handler(bus, (GstBusSyncHandler)create_window, gtkmedia);
	gst_object_unref(bus);
}

static void
pidgin_media_hangup_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	pidgin_media_emit_message(gtkmedia, _("You have ended the call."));
	gtk_widget_destroy(GTK_WIDGET(gtkmedia));
}

static void
pidgin_media_got_request_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	PurpleMediaSessionType type = purple_media_get_overall_type(media);
	gchar *message;

	if (type & PURPLE_MEDIA_AUDIO && type & PURPLE_MEDIA_VIDEO) {
		message = g_strdup_printf(_("%s wishes to start an audio/video session with you."),
					  purple_media_get_screenname(media));
	} else if (type & PURPLE_MEDIA_AUDIO) {
		message = g_strdup_printf(_("%s wishes to start an audio session with you."),
					  purple_media_get_screenname(media));
	} else if (type & PURPLE_MEDIA_VIDEO) {
		message = g_strdup_printf(_("%s wishes to start a video session with you."),
					  purple_media_get_screenname(media));
	} else {
		return;
	}

	pidgin_media_emit_message(gtkmedia, message);
	g_free(message);
}

static void
pidgin_media_got_hangup_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	pidgin_media_emit_message(gtkmedia, _("The call has been terminated."));
	gtk_widget_destroy(GTK_WIDGET(gtkmedia));
}

static void
pidgin_media_reject_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	pidgin_media_emit_message(gtkmedia, _("You have rejected the call."));
	gtk_widget_destroy(GTK_WIDGET(gtkmedia));
}

static void
pidgin_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	PidginMedia *media;
	g_return_if_fail(PIDGIN_IS_MEDIA(object));

	media = PIDGIN_MEDIA(object);
	switch (prop_id) {
		case PROP_MEDIA:
			if (media->priv->media)
				g_object_unref(media->priv->media);
			media->priv->media = g_value_get_object(value);
			g_object_ref(media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->accept), "clicked", 
				 G_CALLBACK(purple_media_accept), media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->reject), "clicked",
				 G_CALLBACK(purple_media_reject), media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->hangup), "clicked",
				 G_CALLBACK(purple_media_hangup), media->priv->media);

			g_signal_connect(G_OBJECT(media->priv->media), "accepted",
				G_CALLBACK(pidgin_media_accept_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media) ,"ready",
				G_CALLBACK(pidgin_media_ready_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media) ,"wait",
				G_CALLBACK(pidgin_media_wait_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "hangup",
				G_CALLBACK(pidgin_media_hangup_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "reject",
				G_CALLBACK(pidgin_media_reject_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "got-request",
				G_CALLBACK(pidgin_media_got_request_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "got-hangup",
				G_CALLBACK(pidgin_media_got_hangup_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "got-accept",
				G_CALLBACK(pidgin_media_accept_cb), media);
			break;
		case PROP_SEND_LEVEL:
			if (media->priv->send_level)
				gst_object_unref(media->priv->send_level);
			media->priv->send_level = g_value_get_object(value);
			g_object_ref(media->priv->send_level);
			break;
		case PROP_RECV_LEVEL:
			if (media->priv->recv_level)
				gst_object_unref(media->priv->recv_level);
			media->priv->recv_level = g_value_get_object(value);
			g_object_ref(media->priv->recv_level);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
pidgin_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	PidginMedia *media;
	g_return_if_fail(PIDGIN_IS_MEDIA(object));

	media = PIDGIN_MEDIA(object);

	switch (prop_id) {
		case PROP_MEDIA:
			g_value_set_object(value, media->priv->media);
			break;
		case PROP_SEND_LEVEL:
			g_value_set_object(value, media->priv->send_level);
			break;
		case PROP_RECV_LEVEL:
			g_value_set_object(value, media->priv->recv_level);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

GtkWidget *
pidgin_media_new(PurpleMedia *media)
{
	PidginMedia *gtkmedia = g_object_new(pidgin_media_get_type(),
					     "media", media, NULL);
	return GTK_WIDGET(gtkmedia);
}

static void
pidgin_media_set_state(PidginMedia *gtkmedia, PidginMediaState state)
{
	gtkmedia->priv->state = state;
	switch (state) {
		case PIDGIN_MEDIA_WAITING:
			gtk_widget_show(gtkmedia->priv->calling);
			gtk_widget_hide(gtkmedia->priv->accept);
			gtk_widget_hide(gtkmedia->priv->reject);
			gtk_widget_show(gtkmedia->priv->hangup);
			break;
		case PIDGIN_MEDIA_REQUESTED:
			gtk_widget_hide(gtkmedia->priv->calling);
			gtk_widget_show(gtkmedia->priv->accept);
			gtk_widget_show(gtkmedia->priv->reject);
			gtk_widget_hide(gtkmedia->priv->hangup);
			break;
		case PIDGIN_MEDIA_ACCEPTED:
			gtk_widget_show(gtkmedia->priv->hangup);
			gtk_widget_hide(gtkmedia->priv->calling);
			gtk_widget_hide(gtkmedia->priv->accept);
			gtk_widget_hide(gtkmedia->priv->reject);
			break;
		default:
			break;
	}
}

#endif  /* USE_VV */
