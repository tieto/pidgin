/**
 * @file gntmedia.c GNT Media API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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

#include "finch.h"
#include "mediamanager.h"

#include "gntconv.h"
#include "gntmedia.h"

#include "gnt.h"
#include "gntbutton.h"
#include "gntbox.h"
#include "gntlabel.h"

#include "cmds.h"
#include "conversation.h"
#include "debug.h"

/* An incredibly large part of the following is from gtkmedia.c */
#ifdef USE_VV

#undef hangup

struct _FinchMediaPrivate
{
	PurpleMedia *media;
	GstElement *send_level;
	GstElement *recv_level;

	GntWidget *accept;
	GntWidget *reject;
	GntWidget *hangup;
	GntWidget *calling;

	PurpleConversation *conv;
};

#define FINCH_MEDIA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), FINCH_TYPE_MEDIA, FinchMediaPrivate))

static void finch_media_class_init (FinchMediaClass *klass);
static void finch_media_init (FinchMedia *media);
static void finch_media_finalize (GObject *object);
static void finch_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void finch_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static GntBoxClass *parent_class = NULL;

enum {
	MESSAGE,
	LAST_SIGNAL
};
static guint finch_media_signals[LAST_SIGNAL] = {0};

enum {
	PROP_0,
	PROP_MEDIA,
	PROP_SEND_LEVEL,
	PROP_RECV_LEVEL
};

GType
finch_media_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(FinchMediaClass),
			NULL,
			NULL,
			(GClassInitFunc) finch_media_class_init,
			NULL,
			NULL,
			sizeof(FinchMedia),
			0,
			(GInstanceInitFunc) finch_media_init,
			NULL
		};
		type = g_type_register_static(GNT_TYPE_BOX, "FinchMedia", &info, 0);
	}
	return type;
}


static void
finch_media_class_init (FinchMediaClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = finch_media_finalize;
	gobject_class->set_property = finch_media_set_property;
	gobject_class->get_property = finch_media_get_property;

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

	finch_media_signals[MESSAGE] = g_signal_new("message", G_TYPE_FROM_CLASS(klass),
					G_SIGNAL_RUN_LAST, 0, NULL, NULL,
					g_cclosure_marshal_VOID__STRING,
					G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private(klass, sizeof(FinchMediaPrivate));
}


static void
finch_media_init (FinchMedia *media)
{
	media->priv = FINCH_MEDIA_GET_PRIVATE(media);

	media->priv->calling = gnt_label_new(_("Calling ... "));
	media->priv->hangup = gnt_button_new(_("Hangup"));
	media->priv->accept = gnt_button_new(_("Accept"));
	media->priv->reject = gnt_button_new(_("Reject"));

	gnt_box_set_alignment(GNT_BOX(media), GNT_ALIGN_MID);

	gnt_box_add_widget(GNT_BOX(media), media->priv->accept);
	gnt_box_add_widget(GNT_BOX(media), media->priv->reject);
}

static void
finch_media_finalize (GObject *media)
{
}

static void
finch_media_emit_message(FinchMedia *gntmedia, const char *msg)
{
	g_signal_emit(gntmedia, finch_media_signals[MESSAGE], 0, msg);
}

static gboolean
level_message_cb(GstBus *bus, GstMessage *message, FinchMedia *gntmedia)
{
	/* XXX: I am hesitant to just remove this function altogether, because I don't
	 * know how necessary it is to have a callback to 'message'. If it isn't essential,
	 * I suppose this should be removed.
	 */
	return TRUE;
#if 0
	const GstStructure *s;
	const gchar *name;

	int channels;
	gdouble rms_db, peak_db, decay_db;
	gdouble rms;
	const GValue *list;
	const GValue *value;

	GstElement *src = GST_ELEMENT(message);

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

	if (!strcmp(gst_element_get_name(src), "sendlevel"))
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gntmedia->priv->send_progress), pow(10, rms_db / 20) * 5);
	else
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gntmedia->priv->recv_progress), pow(10, rms_db / 20) * 5);

	return TRUE;
#endif
}

static void
finch_media_ready_cb(PurpleMedia *media, FinchMedia *gntmedia)
{
	GstElement *element = purple_media_get_pipeline(media);

	GstElement *sendbin, *sendlevel;
	GstElement *recvbin, *recvlevel;

	GList *sessions = purple_media_get_session_names(media);

	purple_media_audio_init_src(&sendbin, &sendlevel);
	purple_media_audio_init_recv(&recvbin, &recvlevel);

	for (; sessions; sessions = sessions->next) {
		purple_media_set_src(media, sessions->data, sendbin);
		purple_media_set_sink(media, sessions->data, recvbin);
	}
	g_list_free(sessions);

	g_object_set(gntmedia, "send-level", &sendlevel,
		     "recv-level", &recvlevel,
		     NULL);

	gst_bus_add_signal_watch(GST_BUS(gst_pipeline_get_bus(GST_PIPELINE(element))));
	g_signal_connect(G_OBJECT(gst_pipeline_get_bus(GST_PIPELINE(element))), "message", G_CALLBACK(level_message_cb), gntmedia);
}

static void
finch_media_accept_cb(PurpleMedia *media, FinchMedia *gntmedia)
{
	GntWidget *parent;

	finch_media_emit_message(gntmedia, _("Call in progress."));

	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->accept);
	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->reject);
	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->hangup);
	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->calling);

	gnt_box_add_widget(GNT_BOX(gntmedia), gntmedia->priv->hangup);

	gnt_widget_destroy(gntmedia->priv->accept);
	gnt_widget_destroy(gntmedia->priv->reject);
	gnt_widget_destroy(gntmedia->priv->calling);
	gntmedia->priv->accept = NULL;
	gntmedia->priv->reject = NULL;
	gntmedia->priv->calling = NULL;

	parent = GNT_WIDGET(gntmedia);
	while (parent->parent)
		parent = parent->parent;
	gnt_box_readjust(GNT_BOX(parent));
	gnt_widget_draw(parent);
}

static void
finch_media_wait_cb(PurpleMedia *media, FinchMedia *gntmedia)
{
	GntWidget *parent;

	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->accept);
	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->reject);
	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->hangup);
	gnt_box_remove(GNT_BOX(gntmedia), gntmedia->priv->calling);

	gnt_box_add_widget(GNT_BOX(gntmedia), gntmedia->priv->calling);
	gnt_box_add_widget(GNT_BOX(gntmedia), gntmedia->priv->hangup);

	gnt_widget_destroy(gntmedia->priv->accept);
	gnt_widget_destroy(gntmedia->priv->reject);
	gntmedia->priv->accept = NULL;
	gntmedia->priv->reject = NULL;

	parent = GNT_WIDGET(gntmedia);
	while (parent->parent)
		parent = parent->parent;
	gnt_box_readjust(GNT_BOX(parent));
	gnt_widget_draw(parent);
}

static void
finch_media_hangup_cb(PurpleMedia *media, FinchMedia *gntmedia)
{
	finch_media_emit_message(gntmedia, _("You have ended the call."));
	finch_conversation_set_info_widget(gntmedia->priv->conv, NULL);
	gnt_widget_destroy(GNT_WIDGET(gntmedia));
}

static void
finch_media_got_hangup_cb(PurpleMedia *media, FinchMedia *gntmedia)
{
	finch_media_emit_message(gntmedia, _("The call has been terminated."));
	finch_conversation_set_info_widget(gntmedia->priv->conv, NULL);
	gnt_widget_destroy(GNT_WIDGET(gntmedia));
}

static void
finch_media_reject_cb(PurpleMedia *media, FinchMedia *gntmedia)
{
	finch_media_emit_message(gntmedia, _("You have rejected the call."));
	finch_conversation_set_info_widget(gntmedia->priv->conv, NULL);
	gnt_widget_destroy(GNT_WIDGET(gntmedia));
}

static void
finch_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	FinchMedia *media;
	g_return_if_fail(FINCH_IS_MEDIA(object));

	media = FINCH_MEDIA(object);
	switch (prop_id) {
		case PROP_MEDIA:
			if (media->priv->media)
				g_object_unref(media->priv->media);
			media->priv->media = g_value_get_object(value);
			g_object_ref(media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->accept), "activate",
				 G_CALLBACK(purple_media_accept), media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->reject), "activate",
				 G_CALLBACK(purple_media_reject), media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->hangup), "activate",
				 G_CALLBACK(purple_media_hangup), media->priv->media);

			g_signal_connect(G_OBJECT(media->priv->media), "accepted",
				G_CALLBACK(finch_media_accept_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media) ,"ready",
				G_CALLBACK(finch_media_ready_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "wait",
				G_CALLBACK(finch_media_wait_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "hangup",
				G_CALLBACK(finch_media_hangup_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "reject",
				G_CALLBACK(finch_media_reject_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "got-hangup",
				G_CALLBACK(finch_media_got_hangup_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "got-accept",
				G_CALLBACK(finch_media_accept_cb), media);
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
finch_media_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	FinchMedia *media;
	g_return_if_fail(FINCH_IS_MEDIA(object));

	media = FINCH_MEDIA(object);

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

GntWidget *
finch_media_new(PurpleMedia *media)
{
	return GNT_WIDGET(g_object_new(finch_media_get_type(),
				"media", media,
				"vertical", FALSE,
				"homogeneous", FALSE,
				NULL));
}

static void
gntmedia_message_cb(FinchMedia *gntmedia, const char *msg, PurpleConversation *conv)
{
	if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM) {
		purple_conv_im_write(PURPLE_CONV_IM(conv), NULL, msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
	}
}

static void
finch_new_media(PurpleMediaManager *manager, PurpleMedia *media, gpointer null)
{
	GntWidget *gntmedia;
	PurpleConversation *conv;

	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM,
			purple_connection_get_account(purple_media_get_connection(media)),
			purple_media_get_screenname(media));

	gntmedia = finch_media_new(media);
	g_signal_connect(G_OBJECT(gntmedia), "message", G_CALLBACK(gntmedia_message_cb), conv);
	FINCH_MEDIA(gntmedia)->priv->conv = conv;
	finch_conversation_set_info_widget(conv, gntmedia);
}

static PurpleCmdRet
call_cmd_cb(PurpleConversation *conv, const char *cmd, char **args,
		char **eror, gpointer data)
{
	PurpleConnection *gc = purple_conversation_get_gc(conv);

	PurpleMedia *media =
		serv_initiate_media(gc,
							purple_conversation_get_name(conv),
							PURPLE_MEDIA_RECV_AUDIO & PURPLE_MEDIA_SEND_AUDIO);

	if (!media)
		return PURPLE_CMD_STATUS_FAILED;

	purple_media_wait(media);
	return PURPLE_CMD_STATUS_OK;
}

void finch_media_manager_init(void)
{
	PurpleMediaManager *manager = purple_media_manager_get();
	g_signal_connect(G_OBJECT(manager), "init-media", G_CALLBACK(finch_new_media), NULL);
	purple_cmd_register("call", "", PURPLE_CMD_P_DEFAULT,
			PURPLE_CMD_FLAG_IM, NULL,
			call_cmd_cb, _("call: Make an audio call."), NULL);
}

void finch_media_manager_uninit(void)
{
	PurpleMediaManager *manager = purple_media_manager_get();
	g_signal_handlers_disconnect_by_func(G_OBJECT(manager),
			G_CALLBACK(finch_new_media), NULL);
}

#endif  /* USE_VV */

