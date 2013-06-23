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

#include <internal.h>
#include "finch.h"
#include "gntconv.h"
#include "gntmedia.h"

#include "gnt.h"
#include "gntbutton.h"
#include "gntbox.h"
#include "gntlabel.h"

#include "cmds.h"
#include "conversation.h"
#include "debug.h"
#include "mediamanager.h"

/* An incredibly large part of the following is from gtkmedia.c */
#ifdef USE_VV
#include "media-gst.h"

#undef hangup

#define FINCH_TYPE_MEDIA            (finch_media_get_type())
#define FINCH_MEDIA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), FINCH_TYPE_MEDIA, FinchMedia))
#define FINCH_MEDIA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), FINCH_TYPE_MEDIA, FinchMediaClass))
#define FINCH_IS_MEDIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), FINCH_TYPE_MEDIA))
#define FINCH_IS_MEDIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FINCH_TYPE_MEDIA))
#define FINCH_MEDIA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), FINCH_TYPE_MEDIA, FinchMediaClass))

typedef struct _FinchMedia FinchMedia;
typedef struct _FinchMediaClass FinchMediaClass;
typedef struct _FinchMediaPrivate FinchMediaPrivate;
typedef enum _FinchMediaState FinchMediaState;

struct _FinchMediaClass
{
	GntBoxClass parent_class;
};

struct _FinchMedia
{
	GntBox parent;
	FinchMediaPrivate *priv;
};

struct _FinchMediaPrivate
{
	PurpleMedia *media;

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
};

static GType
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

	media->priv->calling = gnt_label_new(_("Calling..."));
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
	FinchMedia *gntmedia = FINCH_MEDIA(media);
	purple_debug_info("gntmedia", "finch_media_finalize\n");
	if (gntmedia->priv->media)
		g_object_unref(gntmedia->priv->media);
}

static void
finch_media_emit_message(FinchMedia *gntmedia, const char *msg)
{
	g_signal_emit(gntmedia, finch_media_signals[MESSAGE], 0, msg);
}

static void
finch_media_connected_cb(PurpleMedia *media, FinchMedia *gntmedia)
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

	parent = GNT_WIDGET(gntmedia);
	while (parent->parent)
		parent = parent->parent;
	gnt_box_readjust(GNT_BOX(parent));
	gnt_widget_draw(parent);
}

static void
finch_media_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
		gchar *sid, gchar *name, FinchMedia *gntmedia)
{
	purple_debug_info("gntmedia", "state: %d sid: %s name: %s\n",
			state, sid, name);
	if (sid == NULL && name == NULL) {
		if (state == PURPLE_MEDIA_STATE_END) {
			finch_media_emit_message(gntmedia,
					_("The call has been terminated."));
			finch_conversation_set_info_widget(
					gntmedia->priv->conv, NULL);
			gnt_widget_destroy(GNT_WIDGET(gntmedia));
			/*
			 * XXX: This shouldn't have to be here
			 * to free the FinchMedia widget.
			 */
			g_object_unref(gntmedia);
		}
	} else if (state == PURPLE_MEDIA_STATE_CONNECTED) {
		finch_media_connected_cb(media, gntmedia);
	} else if (state == PURPLE_MEDIA_STATE_NEW &&
			sid != NULL && name != NULL &&
			purple_media_is_initiator(media, sid, name) == FALSE) {
		PurpleAccount *account;
		PurpleBuddy *buddy;
		const gchar *alias;
		PurpleMediaSessionType type =
				purple_media_get_session_type(media, sid);
		gchar *message = NULL;

		account = purple_media_get_account(gntmedia->priv->media);
		buddy = purple_find_buddy(account, name);
		alias = buddy ? purple_buddy_get_contact_alias(buddy) :	name;

		if (type & PURPLE_MEDIA_AUDIO) {
			message = g_strdup_printf(
					_("%s wishes to start an audio session with you."),
					alias);
		} else {
			message = g_strdup_printf(
					_("%s is trying to start an unsupported media session type with you."),
					alias);
		}
		finch_media_emit_message(gntmedia, message);
		g_free(message);
	}
}

static void
finch_media_stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
		gchar *sid, gchar *name, gboolean local, FinchMedia *gntmedia)
{
	if (type == PURPLE_MEDIA_INFO_REJECT) {
		finch_media_emit_message(gntmedia,
				_("You have rejected the call."));
	}
}

static void
finch_media_accept_cb(PurpleMedia *media, GntWidget *widget)
{
	purple_media_stream_info(media, PURPLE_MEDIA_INFO_ACCEPT,
			NULL, NULL, TRUE);
}

static void
finch_media_hangup_cb(PurpleMedia *media, GntWidget *widget)
{
	purple_media_stream_info(media, PURPLE_MEDIA_INFO_HANGUP,
			NULL, NULL, TRUE);
}

static void
finch_media_reject_cb(PurpleMedia *media, GntWidget *widget)
{
	purple_media_stream_info(media, PURPLE_MEDIA_INFO_REJECT,
			NULL, NULL, TRUE);
}

static void
finch_media_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	FinchMedia *media;
	g_return_if_fail(FINCH_IS_MEDIA(object));

	media = FINCH_MEDIA(object);
	switch (prop_id) {
		case PROP_MEDIA:
		{
			if (media->priv->media)
				g_object_unref(media->priv->media);
			media->priv->media = g_value_get_object(value);
			g_object_ref(media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->accept), "activate",
				 G_CALLBACK(finch_media_accept_cb), media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->reject), "activate",
				 G_CALLBACK(finch_media_reject_cb), media->priv->media);
			g_signal_connect_swapped(G_OBJECT(media->priv->hangup), "activate",
				 G_CALLBACK(finch_media_hangup_cb), media->priv->media);

			if (purple_media_is_initiator(media->priv->media,
					NULL, NULL) == TRUE) {
				finch_media_wait_cb(media->priv->media, media);
			}
			g_signal_connect(G_OBJECT(media->priv->media), "state-changed",
				G_CALLBACK(finch_media_state_changed_cb), media);
			g_signal_connect(G_OBJECT(media->priv->media), "stream-info",
				G_CALLBACK(finch_media_stream_info_cb), media);
			break;
		}
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
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static GntWidget *
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
		purple_im_conversation_write_message(PURPLE_CONV_IM(conv), NULL, msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
	}
}

static gboolean
finch_new_media(PurpleMediaManager *manager, PurpleMedia *media,
		PurpleAccount *account, gchar *name, gpointer null)
{
	GntWidget *gntmedia;
	PurpleConversation *conv;

	conv = purple_im_conversation_new(account, name);

	gntmedia = finch_media_new(media);
	g_signal_connect(G_OBJECT(gntmedia), "message", G_CALLBACK(gntmedia_message_cb), conv);
	FINCH_MEDIA(gntmedia)->priv->conv = conv;
	finch_conversation_set_info_widget(conv, gntmedia);
	return TRUE;
}

static PurpleCmdRet
call_cmd_cb(PurpleConversation *conv, const char *cmd, char **args,
		char **eror, gpointer data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);

	if (!purple_prpl_initiate_media(account,
			purple_conversation_get_name(conv),
			PURPLE_MEDIA_AUDIO))
		return PURPLE_CMD_STATUS_FAILED;

	return PURPLE_CMD_STATUS_OK;
}

static GstElement *
create_default_audio_src(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GstElement *src;
	src = gst_element_factory_make("gconfaudiosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("autoaudiosrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("alsasrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("osssrc", NULL);
	if (src == NULL)
		src = gst_element_factory_make("dshowaudiosrc", NULL);
	if (src == NULL) {
		purple_debug_error("gntmedia", "Unable to find a suitable "
				"element for the default audio source.\n");
		return NULL;
	}
	gst_element_set_name(src, "finchdefaultaudiosrc");
	return src;
}

static GstElement *
create_default_audio_sink(PurpleMedia *media,
		const gchar *session_id, const gchar *participant)
{
	GstElement *sink;
	sink = gst_element_factory_make("gconfaudiosink", NULL);
	if (sink == NULL)
		sink = gst_element_factory_make("autoaudiosink",NULL);
	if (sink == NULL) {
		purple_debug_error("gntmedia", "Unable to find a suitable "
				"element for the default audio sink.\n");
		return NULL;
	}
	return sink;
}
#endif  /* USE_VV */

void finch_media_manager_init(void)
{
#ifdef USE_VV
	PurpleMediaManager *manager = purple_media_manager_get();
	PurpleMediaElementInfo *default_audio_src =
			g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "finchdefaultaudiosrc",
			"name", "Finch Default Audio Source",
			"type", PURPLE_MEDIA_ELEMENT_AUDIO
					| PURPLE_MEDIA_ELEMENT_SRC
					| PURPLE_MEDIA_ELEMENT_ONE_SRC
					| PURPLE_MEDIA_ELEMENT_UNIQUE,
			"create-cb", create_default_audio_src, NULL);
	PurpleMediaElementInfo *default_audio_sink =
			g_object_new(PURPLE_TYPE_MEDIA_ELEMENT_INFO,
			"id", "finchdefaultaudiosink",
			"name", "Finch Default Audio Sink",
			"type", PURPLE_MEDIA_ELEMENT_AUDIO
					| PURPLE_MEDIA_ELEMENT_SINK
					| PURPLE_MEDIA_ELEMENT_ONE_SINK,
			"create-cb", create_default_audio_sink, NULL);

	g_signal_connect(G_OBJECT(manager), "init-media", G_CALLBACK(finch_new_media), NULL);
	purple_cmd_register("call", "", PURPLE_CMD_P_DEFAULT,
			PURPLE_CMD_FLAG_IM, NULL,
			call_cmd_cb, _("call: Make an audio call."), NULL);

	purple_media_manager_set_ui_caps(manager,
			PURPLE_MEDIA_CAPS_AUDIO |
			PURPLE_MEDIA_CAPS_AUDIO_SINGLE_DIRECTION);

	purple_debug_info("gntmedia", "Registering media element types\n");
	purple_media_manager_set_active_element(manager, default_audio_src);
	purple_media_manager_set_active_element(manager, default_audio_sink);
#endif
}

void finch_media_manager_uninit(void)
{
#ifdef USE_VV
	PurpleMediaManager *manager = purple_media_manager_get();
	g_signal_handlers_disconnect_by_func(G_OBJECT(manager),
			G_CALLBACK(finch_new_media), NULL);
#endif
}


