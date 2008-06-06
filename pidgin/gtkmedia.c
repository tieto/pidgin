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
#include "internal.h"
#include "connection.h"
#include "media.h"

#include "gtkmedia.h"

#ifdef USE_VV

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

	GtkWidget *send_progress;
	GtkWidget *recv_progress;

	PidginMediaState state;
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
	GtkContainerClass *container_class = (GtkContainerClass*)klass;
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
pidgin_media_init (PidginMedia *media)
{
	media->priv = PIDGIN_MEDIA_GET_PRIVATE(media);
	media->priv->calling = gtk_label_new_with_mnemonic("Calling...");
	media->priv->hangup = gtk_button_new_with_label("Hangup");
	media->priv->accept = gtk_button_new_with_label("Accept");
	media->priv->reject = gtk_button_new_with_label("Reject");
	media->priv->send_progress = gtk_progress_bar_new();
	media->priv->recv_progress = gtk_progress_bar_new();

	gtk_widget_set_size_request(media->priv->send_progress, 70, 5);
	gtk_widget_set_size_request(media->priv->recv_progress, 70, 5);

	gtk_box_pack_start(GTK_BOX(media), media->priv->calling, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(media), media->priv->hangup, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(media), media->priv->accept, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(media), media->priv->reject, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(media), media->priv->send_progress, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(media), media->priv->recv_progress, FALSE, FALSE, 6);

	gtk_widget_show(media->priv->send_progress);
	gtk_widget_show(media->priv->recv_progress);

	gtk_widget_show_all(media->priv->accept);
	gtk_widget_show_all(media->priv->reject);
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
}

static void
pidgin_media_emit_message(PidginMedia *gtkmedia, const char *msg)
{
	g_signal_emit(gtkmedia, pidgin_media_signals[MESSAGE], 0, msg);
}

static void
pidgin_media_ready_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	GstElement *element = purple_media_get_pipeline(media);

	GstElement *audiosendbin, *audiosendlevel;
	GstElement *audiorecvbin, *audiorecvlevel;
	GstElement *videosendbin;
	GstElement *videorecvbin;

	GList *sessions = purple_media_get_session_names(media);

	purple_media_audio_init_src(&audiosendbin, &audiosendlevel);
	purple_media_audio_init_recv(&audiorecvbin, &audiorecvlevel);

	purple_media_video_init_src(&videosendbin);
	purple_media_video_init_recv(&videorecvbin);

	for (; sessions; sessions = sessions->next) {
		if (purple_media_get_session_type(media, sessions->data) == FS_MEDIA_TYPE_AUDIO) {
			purple_media_set_src(media, sessions->data, audiosendbin);
			purple_media_set_sink(media, sessions->data, audiorecvbin);
		} else if (purple_media_get_session_type(media, sessions->data) == FS_MEDIA_TYPE_VIDEO) {
			purple_media_set_src(media, sessions->data, videosendbin);
			purple_media_set_sink(media, sessions->data, videorecvbin);
		}
	}
	g_list_free(sessions);

	if (audiosendlevel && audiorecvlevel) {
		g_object_set(gtkmedia, "send-level", audiosendlevel,
				       "recv-level", audiorecvlevel,
				       NULL);
	}

	gst_bus_add_signal_watch(GST_BUS(gst_pipeline_get_bus(GST_PIPELINE(element))));
	g_signal_connect(G_OBJECT(gst_pipeline_get_bus(GST_PIPELINE(element))), "message", G_CALLBACK(level_message_cb), gtkmedia);
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
	pidgin_media_emit_message(gtkmedia, _("Call in progress."));
	pidgin_media_set_state(gtkmedia, PIDGIN_MEDIA_ACCEPTED);
}

static void
pidgin_media_hangup_cb(PurpleMedia *media, PidginMedia *gtkmedia)
{
	pidgin_media_emit_message(gtkmedia, _("You have ended the call."));
	gtk_widget_destroy(GTK_WIDGET(gtkmedia));
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
