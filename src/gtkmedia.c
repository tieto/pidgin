/**
 * @file gtkmedia.h Voice and Video UI
 * @ingroup gtkui
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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

#ifdef HAVE_VV

#include "debug.h"
#include "gtkconv.h"
#include "gtkmedia.h"

typedef struct {
	GtkWidget *call_pane;
	GtkWidget *bbox;
} GaimGtkVoiceChat;


static void gaim_gtk_media_new_voice_chat(GaimVoiceChat *vc)
{
	GaimGtkVoiceChat *gvc = g_new0(GaimGtkVoiceChat, 1);
	gaim_voice_chat_set_ui_data(vc, gvc);

}

static void gaim_gtk_media_destroy(GaimVoiceChat *vc)
{
	GaimConversation *conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, gaim_connection_get_account(gaim_voice_chat_get_connection(vc)),
						       gaim_voice_chat_get_name(vc));
	GaimGtkVoiceChat *gvc = (GaimGtkVoiceChat*)gaim_voice_chat_get_ui_data(vc);
	gaim_conversation_write(conv, NULL, _("Call ended."), GAIM_MESSAGE_SYSTEM, time(NULL));
	gtk_widget_destroy(gvc->call_pane);
	g_free(gvc);
}

static void gaim_gtk_media_state_change(GaimVoiceChat *vc, GaimMediaState state)
{
	GaimGtkVoiceChat *gvc = (GaimGtkVoiceChat*)gaim_voice_chat_get_ui_data(vc);
       	GaimConversation *conv = gaim_conversation_new(GAIM_CONV_TYPE_IM, gaim_connection_get_account(gaim_voice_chat_get_connection(vc)),
						       gaim_voice_chat_get_name(vc));
	GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(conv);
	GtkWidget *hbox, *bbox;
	GtkWidget *button;
	char *msg = NULL;
	
	switch (state) {
	case GAIM_MEDIA_STATE_CALLING:
		msg = g_strdup_printf(_("Calling %s"), gaim_voice_chat_get_name(vc));
		gaim_conversation_write(conv, NULL, msg, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
		hbox = gtk_hbox_new(FALSE, 6);
		button = gtk_button_new_with_label(_("End Call"));
		g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gaim_voice_chat_terminate), vc);
		gvc->call_pane = hbox;
		gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, 0, 0);
		gtk_box_pack_end(GTK_BOX(gtkconv->tab_cont), hbox, FALSE, 0, 0);
		gtk_widget_show_all(hbox);
		break;
	case GAIM_MEDIA_STATE_RECEIVING:
		msg = g_strdup_printf(_("Receiving call from %s"), gaim_voice_chat_get_name(vc));
		gaim_conversation_write(conv, NULL, msg, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
		hbox = gtk_hbox_new(FALSE, 6);
		bbox = gvc->bbox = gtk_hbutton_box_new();
		gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
		gtk_box_set_spacing(GTK_BOX(bbox), 6);
		gtk_container_add(GTK_CONTAINER(hbox), bbox);
		button = gtk_button_new_with_label(_("Reject Call"));
		g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gaim_voice_chat_reject), vc);
		gvc->call_pane = hbox;
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, 0, 0);
		gtk_box_pack_end(GTK_BOX(gtkconv->tab_cont), hbox, FALSE, 0, 0);
		
		button = gtk_button_new_with_label(_("Accept call"));
		g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gaim_voice_chat_accept), vc);
		gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, 0, 0);
		
		gtk_widget_show_all(hbox);
		break;
	case GAIM_MEDIA_STATE_IN_PROGRESS:
		msg = g_strdup_printf(_("Connected to %s"), gaim_voice_chat_get_name(vc));
		gaim_conversation_write(conv, NULL, msg, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(msg);
		if (gvc->bbox) {
			gtk_widget_destroy(gvc->bbox);
			gvc->bbox = NULL;
			button = gtk_button_new_with_label(_("End Call"));
			g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gaim_voice_chat_terminate), vc);
			gtk_box_pack_end(GTK_BOX(gvc->call_pane), button, FALSE, 0, 0);
			gtk_widget_show(button);
		}
		button = gtk_check_button_new_with_mnemonic(_("_Mute"));
		gtk_box_pack_start(GTK_BOX(gvc->call_pane), button, FALSE, 0, 0);
		gtk_widget_show(button);
		break;
	default:
		break;
	}
}


static GaimMediaUiOps ui_ops =
{
	gaim_gtk_media_new_voice_chat,
	gaim_gtk_media_destroy,
	gaim_gtk_media_state_change
};

void gaim_gtk_media_init(void)
{
	gaim_debug_info("gtkmedia","Initialising\n");
	gaim_media_set_ui_ops(&ui_ops);
}

#endif  /* HAVE_VV */
