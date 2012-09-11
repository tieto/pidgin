#include "chat.h"

#include <debug.h>

#include "gg.h"
#include "utils.h"

typedef struct _ggp_chat_local_info ggp_chat_local_info;

struct _ggp_chat_session_data
{
	ggp_chat_local_info *chats;
	int chats_count;
};

struct _ggp_chat_local_info
{
	int local_id;
	uint64_t id;
	
	PurpleConversation *conv;
	PurpleConnection *gc;
	
	gboolean previously_joined;
};

static ggp_chat_local_info * ggp_chat_new(PurpleConnection *gc, uint64_t id);
static ggp_chat_local_info * ggp_chat_get(PurpleConnection *gc, uint64_t id);
static ggp_chat_local_info * ggp_chat_get_local(PurpleConnection *gc,
	int local_id);
static void ggp_chat_joined(ggp_chat_local_info *chat, uin_t uin,
	gboolean new_arrival);
static void ggp_chat_left(ggp_chat_local_info *chat, uin_t uin);
static const gchar * ggp_chat_get_name_from_id(uint64_t id);
/*static uint64_t ggp_chat_get_id_from_name(const gchar * name);*/

static inline ggp_chat_session_data *
ggp_chat_get_sdata(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	return accdata->chat_data;
}

void ggp_chat_setup(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);
	ggp_chat_session_data *sdata = g_new0(ggp_chat_session_data, 1);

	accdata->chat_data = sdata;

	sdata->chats = NULL;
	sdata->chats_count = 0;
}

void ggp_chat_cleanup(PurpleConnection *gc)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);

	g_free(sdata->chats);
	g_free(sdata);
}

static ggp_chat_local_info * ggp_chat_new(PurpleConnection *gc, uint64_t id)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	int local_id;
	ggp_chat_local_info *chat;

	if (NULL == (chat = ggp_chat_get(gc, id)))
	{
		local_id = sdata->chats_count++;
		sdata->chats = realloc(sdata->chats,
			sdata->chats_count * sizeof(ggp_chat_local_info));
		chat = &sdata->chats[local_id];

		chat->local_id = local_id;
		chat->id = id;
		chat->conv = NULL;
		chat->gc = gc;
		chat->previously_joined = FALSE;
	}

	if (chat->conv != NULL)
	{
		purple_debug_warning("gg", "ggp_chat_new: "
			"chat %llu already exists\n", id);
		return chat;
	}

	chat->conv = serv_got_joined_chat(gc, local_id,
		ggp_chat_get_name_from_id(id));
	if (chat->previously_joined)
	{
		purple_conversation_write(chat->conv, NULL,
			_("You have re-joined the chat"), PURPLE_MESSAGE_SYSTEM,
			time(NULL));
	}
	chat->previously_joined = TRUE;

	return chat;
}

static ggp_chat_local_info * ggp_chat_get(PurpleConnection *gc, uint64_t id)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	int i;

	for (i = 0; i < sdata->chats_count; i++)
		if (sdata->chats[i].id == id)
			return &sdata->chats[i];

	return NULL;
}

static ggp_chat_local_info * ggp_chat_get_local(PurpleConnection *gc,
	int local_id)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	int i;

	for (i = 0; i < sdata->chats_count; i++)
		if (sdata->chats[i].local_id == local_id)
			return &sdata->chats[i];

	return NULL;
}

void ggp_chat_got_event(PurpleConnection *gc, const struct gg_event *ev)
{
	ggp_chat_local_info *chat;
	int i;

	if (ev->type == GG_EVENT_CHAT_INFO)
	{
		const struct gg_event_chat_info *eci = &ev->event.chat_info;
		chat = ggp_chat_new(gc, eci->id);
		for (i = 0; i < eci->participants_count; i++)
			ggp_chat_joined(chat, eci->participants[i], FALSE);
	}
	else if (ev->type == GG_EVENT_CHAT_INFO_UPDATE)
	{
		const struct gg_event_chat_info_update *eciu =
			&ev->event.chat_info_update;
		chat = ggp_chat_get(gc, eciu->id);
		if (!chat)
		{
			purple_debug_error("gg", "ggp_chat_got_event: "
				"chat %llu not found\n", eciu->id);
			return;
		}
		if (eciu->type == GG_CHAT_INFO_UPDATE_ENTERED)
			ggp_chat_joined(chat, eciu->participant, TRUE);
		else if (eciu->type == GG_CHAT_INFO_UPDATE_EXITED)
			ggp_chat_left(chat, eciu->participant);
		else
			purple_debug_warning("gg", "ggp_chat_got_event: "
				"unknown update type - %d", eciu->type);
	}
	else if (ev->type == GG_EVENT_CHAT_CREATED)
	{
		const struct gg_event_chat_created *ecc =
			&ev->event.chat_created;
		uin_t me = ggp_str_to_uin(purple_account_get_username(
			purple_connection_get_account(gc)));
		chat = ggp_chat_new(gc, ecc->id);
		ggp_chat_joined(chat, me, FALSE);
	}
	else if (ev->type == GG_EVENT_CHAT_INVITE_ACK ||
		ev->type == GG_EVENT_CHAT_SEND_MSG_ACK)
	{
		/* ignore */
	}
	else
	{
		purple_debug_fatal("gg", "ggp_chat_got_event: unexpected event "
			"- %d\n", ev->type);
	}
}

static void ggp_chat_joined(ggp_chat_local_info *chat, uin_t uin,
	gboolean new_arrival)
{
	purple_conv_chat_add_user(purple_conversation_get_chat_data(chat->conv),
		ggp_uin_to_str(uin), NULL, PURPLE_CBFLAGS_NONE, new_arrival);
}

static void ggp_chat_left(ggp_chat_local_info *chat, uin_t uin)
{
	uin_t me;

	me = ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(chat->gc)));

	if (me == uin)
	{
		purple_conversation_write(chat->conv, NULL,
			_("You have left the chat"), PURPLE_MESSAGE_SYSTEM,
			time(NULL));
		serv_got_chat_left(chat->gc, chat->local_id);
		chat->conv = NULL;
	}
	purple_conv_chat_remove_user(purple_conversation_get_chat_data(
		chat->conv), ggp_uin_to_str(uin), NULL);
}

GList * ggp_chat_info(PurpleConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Conference identifier:");
	pce->identifier = "id";
	pce->required = FALSE;
	m = g_list_append(m, pce);
	
	return m;
}

char * ggp_chat_get_name(GHashTable *components)
{
	return g_strdup((gchar*)g_hash_table_lookup(components, "id"));
}

static const gchar * ggp_chat_get_name_from_id(uint64_t id)
{
	static gchar buff[30];
	g_snprintf(buff, sizeof(buff), "%llu", id);
	return buff;
}

# if 0
static uint64_t ggp_chat_get_id_from_name(const gchar * name)
{
	uint64_t id;
	gchar *endptr;

	if (name == NULL)
		return 0;

	id = g_ascii_strtoull(name, &endptr, 10);

	if (*endptr != '\0' || id == G_MAXUINT64)
		return 0;

	return id;
}
#endif

void ggp_chat_join(PurpleConnection *gc, GHashTable *components)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	const gchar *id;

	id = g_hash_table_lookup(components, "id");
	if (id == NULL || id[0] != '\0')
	{
		purple_serv_got_join_chat_failed(gc, components);
		return;
	}

	if (gg_chat_create(info->session) < 0)
	{
		purple_serv_got_join_chat_failed(gc, components);
		return;
	}
}

void ggp_chat_leave(PurpleConnection *gc, int local_id)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	ggp_chat_local_info *chat;
	
	chat = ggp_chat_get_local(gc, local_id);
	if (!chat)
	{
		purple_debug_error("gg", "ggp_chat_leave: "
			"chat %u doesn't exists\n", local_id);
		return;
	}
	
	if (gg_chat_leave(info->session, chat->id) < 0)
	{
		purple_debug_error("gg", "ggp_chat_leave: "
			"unable to leave chat %llu\n", chat->id);
	}
	chat->conv = NULL;
}

void ggp_chat_invite(PurpleConnection *gc, int local_id, const char *message,
	const char *who)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	ggp_chat_local_info *chat;
	uin_t invited;
	
	chat = ggp_chat_get_local(gc, local_id);
	if (!chat)
	{
		purple_debug_error("gg", "ggp_chat_invite: "
			"chat %u doesn't exists\n", local_id);
		return;
	}
	
	invited = ggp_str_to_uin(who);
	if (gg_chat_invite(info->session, chat->id, &invited, 1) < 0)
	{
		purple_debug_error("gg", "ggp_chat_invite: "
			"unable to invite %s to chat %llu\n", who, chat->id);
	}
}

int ggp_chat_send(PurpleConnection *gc, int local_id, const char *message,
	PurpleMessageFlags flags)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	ggp_chat_local_info *chat;
	gboolean succ = TRUE;
	const gchar *me;
	
	chat = ggp_chat_get_local(gc, local_id);
	if (!chat)
	{
		purple_debug_error("gg", "ggp_chat_send: "
			"chat %u doesn't exists\n", local_id);
		return -1;
	}
	
	if (gg_chat_send_message(info->session, chat->id, message, TRUE) < 0)
		succ = FALSE;

	me = purple_account_get_username(purple_connection_get_account(gc));
	serv_got_chat_in(gc, chat->local_id, me, flags, message, time(NULL));

	return succ ? 0 : -1;
}

void ggp_chat_got_message(PurpleConnection *gc, uint64_t chat_id,
	const char *message, time_t time, uin_t who)
{
	ggp_chat_local_info *chat;
	uin_t me;
	
	me = ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(gc)));
	
	chat = ggp_chat_get(gc, chat_id);
	if (!chat)
	{
		purple_debug_error("gg", "ggp_chat_got_message: "
			"chat %llu doesn't exists\n", chat_id);
		return;
	}

	if (who == me)
	{
		purple_conversation_write(chat->conv, ggp_uin_to_str(who),
			message, PURPLE_MESSAGE_SEND, time);
	}
	else
	{
		serv_got_chat_in(gc, chat->local_id, ggp_uin_to_str(who),
			PURPLE_MESSAGE_RECV, message, time);
	}
}
