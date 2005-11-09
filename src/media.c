/**
 * @file media.h Voice and Video API
 * @ingroup core
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

#ifdef HAVE_VV

#include "media.h"
#include "mediastream.h"
#include "msilbcdec.h"


/* msrtpsend.o and msrtprecv.o aren't used within the core, so
 * the linker chooses not to link them. I want plugins to be able
 * to depend on them, so I reference symbols from them here. */
static void * dummy1 = ms_rtp_send_new;
static void * dummy2 = ms_rtp_recv_new;

struct _GaimVoiceChat {
	GaimConnection *gc;
	char *name;

	GaimMediaState state;

	void *proto_data;
	void *ui_data;

	MSFilter *speaker;
	MSFilter *microphone;
	MSSync   *timer;
};

static GaimMediaUiOps *media_ui_ops = NULL;

void gaim_media_init()
{
	ms_init();
	ms_ilbc_codec_init();
	ms_speex_codec_init();
	ortp_init();
}

GaimVoiceChat *gaim_voice_chat_new(GaimConnection *gc, const char *name)
{
	GaimVoiceChat *vc = g_new0(GaimVoiceChat, 1);
	SndCard *card;
	
	vc->gc = gc;
	vc->name = g_strdup(name);
	
	card = snd_card_manager_get_card(snd_card_manager,0);
	vc->speaker = snd_card_create_write_filter(card);
	vc->microphone = snd_card_create_read_filter(card);
	vc->timer = ms_timer_new();
	ms_sync_attach(vc->timer, vc->microphone);
	if (media_ui_ops)
		media_ui_ops->new_voice_chat(vc);
	return vc;
}



void gaim_voice_chat_destroy(GaimVoiceChat *vc)
{
	if (media_ui_ops)
		media_ui_ops->destroy(vc);
	g_free(vc);
}

const char *gaim_voice_chat_get_name(GaimVoiceChat *vc)
{
	return vc->name;
}

void gaim_voice_chat_set_name(GaimVoiceChat *vc, const char *name)
{
	g_free(vc->name);
	vc->name = g_strdup(name);
}

GaimConnection *gaim_voice_chat_get_connection(GaimVoiceChat *vc)
{
	return vc->gc;
}

void *gaim_voice_chat_get_ui_data(GaimVoiceChat *vc)
{
	return vc->ui_data;
}

void gaim_voice_chat_set_ui_data(GaimVoiceChat *vc, void *data)
{
	vc->ui_data = data;
}

void *gaim_voice_chat_get_proto_data(GaimVoiceChat *vc)
{
	return vc->proto_data;
}

void gaim_voice_chat_set_proto_data(GaimVoiceChat *vc, void *data)
{
	vc->proto_data = data;
}

void gaim_media_set_ui_ops(GaimMediaUiOps *ops)
{
	media_ui_ops = ops;
}

GaimMediaUiOps *gaim_media_get_ui_ops(void)
{
	return media_ui_ops;
}


GaimMediaState gaim_voice_chat_get_state(GaimVoiceChat *vc)
{
	return vc->state;
}

void gaim_voice_chat_accept(GaimVoiceChat *vc)
{
	GaimConnection *gc = gaim_voice_chat_get_connection(vc);
	GaimPluginProtocolInfo *prpl_info = NULL;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	
	if (!prpl_info->media_prpl_ops || !prpl_info->media_prpl_ops->accept)
		return;
	prpl_info->media_prpl_ops->accept(vc);
}

void gaim_voice_chat_reject(GaimVoiceChat *vc)
{
	GaimConnection *gc = gaim_voice_chat_get_connection(vc);
	GaimPluginProtocolInfo *prpl_info = NULL;
	
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	
	if (!prpl_info->media_prpl_ops || !prpl_info->media_prpl_ops->reject)
		return;
	prpl_info->media_prpl_ops->reject(vc);
}


void gaim_voice_chat_terminate(GaimVoiceChat *vc)
{
	GaimConnection *gc = gaim_voice_chat_get_connection(vc);
	GaimPluginProtocolInfo *prpl_info = NULL;
	
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	
	if (!prpl_info->media_prpl_ops || !prpl_info->media_prpl_ops->terminate)
		return;
	prpl_info->media_prpl_ops->terminate(vc);
}


void gaim_voice_chat_set_state(GaimVoiceChat *vc, GaimMediaState state)
{
	vc->state = state;
	printf("State: %d\n",vc);
	if (media_ui_ops)
		media_ui_ops->state_change(vc, state);
}

void gaim_voice_chat_get_filters(GaimVoiceChat *vc, MSFilter **microphone, MSFilter **speaker)
{
	if (microphone) *microphone = vc->microphone;
	if (speaker) *speaker = vc->speaker;
}

MSSync *gaim_voice_chat_get_timer(GaimVoiceChat *vc)
{
	return vc->timer;
}

void *gaim_voice_chat_start_streams(GaimVoiceChat *vc)
{
	GaimConnection *gc = gaim_voice_chat_get_connection(vc);
	GaimPluginProtocolInfo *prpl_info = NULL;
	
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(gc->prpl);
	
	if (prpl_info->media_prpl_ops && prpl_info->media_prpl_ops->init_filters)
		prpl_info->media_prpl_ops->init_filters(vc);
	
	ms_start(vc->timer);
}

#endif  /* HAVE_VV */
