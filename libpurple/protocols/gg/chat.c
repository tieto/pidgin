#include "chat.h"

#include <debug.h>

#include "gg.h"
#include "utils.h"

typedef struct _ggp_chat_local_info ggp_chat_local_info;

struct _ggp_chat_session_data
{
	ggp_chat_local_info *chats;
	int chats_count;

	gboolean got_all_chats_info;
	GSList *pending_joins;
};

struct _ggp_chat_local_info
{
	int local_id;
	uint64_t id;
	
	PurpleConversation *conv;
	PurpleConnection *gc;
	
	gboolean left;
	gboolean previously_joined;
	
	uin_t *participants;
	int participants_count;
};

static ggp_chat_local_info * ggp_chat_new(PurpleConnection *gc, uint64_t id);
static ggp_chat_local_info * ggp_chat_get(PurpleConnection *gc, uint64_t id);
static void ggp_chat_open_conv(ggp_chat_local_info *chat);
static ggp_chat_local_info * ggp_chat_get_local(PurpleConnection *gc,
	int local_id);
static void ggp_chat_joined(ggp_chat_local_info *chat, uin_t uin);
static void ggp_chat_left(ggp_chat_local_info *chat, uin_t uin);
static const gchar * ggp_chat_get_name_from_id(uint64_t id);
static uint64_t ggp_chat_get_id_from_name(const gchar * name);
static void ggp_chat_join_id(PurpleConnection *gc, uint64_t id);

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
}

void ggp_chat_cleanup(PurpleConnection *gc)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	int i;

	g_slist_free_full(sdata->pending_joins, g_free);
	for (i = 0; i < sdata->chats_count; i++)
		g_free(sdata->chats[i].participants);
	g_free(sdata->chats);
	g_free(sdata);
}

static ggp_chat_local_info * ggp_chat_new(PurpleConnection *gc, uint64_t id)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	int local_id;
	ggp_chat_local_info *chat;

	if (NULL != (chat = ggp_chat_get(gc, id)))
		return chat;

	local_id = sdata->chats_count++;
	sdata->chats = g_realloc(sdata->chats,
		sdata->chats_count * sizeof(ggp_chat_local_info));
	chat = &sdata->chats[local_id];

	chat->local_id = local_id;
	chat->id = id;
	chat->conv = NULL;
	chat->gc = gc;
	chat->left = FALSE;
	chat->previously_joined = FALSE;
	chat->participants = NULL;
	chat->participants_count = 0;

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

static void ggp_chat_open_conv(ggp_chat_local_info *chat)
{
	PurpleConvChat *pcchat;
	int i;

	if (chat->conv != NULL)
		return;

	chat->conv = serv_got_joined_chat(chat->gc, chat->local_id,
		ggp_chat_get_name_from_id(chat->id));
	if (chat->previously_joined)
	{
		purple_conversation_write(chat->conv, NULL,
			_("You have re-joined the chat"), PURPLE_MESSAGE_SYSTEM,
			time(NULL));
	}
	chat->previously_joined = TRUE;

	pcchat = purple_conversation_get_chat_data(chat->conv);
	purple_conv_chat_clear_users(pcchat);
	for (i = 0; i < chat->participants_count; i++)
		purple_conv_chat_add_user(pcchat,
			ggp_uin_to_str(chat->participants[i]), NULL,
			PURPLE_CBFLAGS_NONE, FALSE);
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
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	ggp_chat_local_info *chat;
	int i;

	if (ev->type == GG_EVENT_CHAT_INFO)
	{
		const struct gg_event_chat_info *eci = &ev->event.chat_info;
		chat = ggp_chat_new(gc, eci->id);
		for (i = 0; i < eci->participants_count; i++)
			ggp_chat_joined(chat, eci->participants[i]);
	}
	else if (ev->type == GG_EVENT_CHAT_INFO_GOT_ALL)
	{
		GSList *it = sdata->pending_joins;
		sdata->got_all_chats_info = TRUE;
		while (it)
		{
			uint64_t *id_p = it->data;
			ggp_chat_join_id(gc, *id_p);
			it = g_slist_next(it);
		}
		g_slist_free_full(sdata->pending_joins, g_free);
		sdata->pending_joins = NULL;
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
			ggp_chat_joined(chat, eciu->participant);
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
		ggp_chat_joined(chat, me);
		ggp_chat_open_conv(chat);
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

static int ggp_chat_participant_find(ggp_chat_local_info *chat, uin_t uin)
{
	int i;
	for (i = 0; i < chat->participants_count; i++)
		if (chat->participants[i] == uin)
			return i;
	return -1;
}

static void ggp_chat_joined(ggp_chat_local_info *chat, uin_t uin)
{
	int idx = ggp_chat_participant_find(chat, uin);
	if (idx >= 0)
	{
		purple_debug_warning("gg", "ggp_chat_joined: "
			"user %u is already present in chat %llu\n",
			uin, chat->id);
		return;
	}
	chat->participants_count++;
	chat->participants = g_realloc(chat->participants,
		sizeof(uin) * chat->participants_count);
	chat->participants[chat->participants_count - 1] = uin;
	
	if (!chat->conv)
		return;
	purple_conv_chat_add_user(purple_conversation_get_chat_data(chat->conv),
		ggp_uin_to_str(uin), NULL, PURPLE_CBFLAGS_NONE, TRUE);
}

static void ggp_chat_left(ggp_chat_local_info *chat, uin_t uin)
{
	uin_t me;
	int idx = ggp_chat_participant_find(chat, uin);

	if (idx < 0)
	{
		purple_debug_warning("gg", "ggp_chat_joined: "
			"user %u isn't present in chat %llu\n", uin, chat->id);
		return;
	}
	chat->participants[idx] =
		chat->participants[chat->participants_count - 1];
	chat->participants_count--;
	chat->participants = g_realloc(chat->participants,
		sizeof(uin) * chat->participants_count);

	if (chat->conv == NULL)
		return;

	me = ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(chat->gc)));

	if (me == uin)
	{
		purple_conversation_write(chat->conv, NULL,
			_("You have left the chat"), PURPLE_MESSAGE_SYSTEM,
			time(NULL));
		serv_got_chat_left(chat->gc, chat->local_id);
		chat->conv = NULL;
		chat->left = TRUE;
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

GHashTable * ggp_chat_info_defaults(PurpleConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL && ggp_chat_get_id_from_name(chat_name) != 0)
		g_hash_table_insert(defaults, "id", g_strdup(chat_name));

	return defaults;
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

void ggp_chat_join(PurpleConnection *gc, GHashTable *components)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	const gchar *id_cs;
	gchar *id_s;
	uint64_t id;

	id_cs = g_hash_table_lookup(components, "id");
	id_s = g_strdup(id_cs);
	if (id_s)
		g_strstrip(id_s);
	if (id_s == NULL || id_s[0] == '\0')
	{
		g_free(id_s);
		if (gg_chat_create(info->session) < 0)
		{
			purple_debug_error("gg", "ggp_chat_join; "
				"cannot create\n");
			purple_serv_got_join_chat_failed(gc, components);
		}
		return;
	}
	id = ggp_chat_get_id_from_name(id_s);
	g_free(id_s);

	if (!id)
	{
		char *buff = g_strdup_printf(
			_("%s is not a valid room identifier"), id_cs);
		purple_notify_error(gc, _("Invalid Room Identifier"),
			_("Invalid Room Identifier"), buff);
		g_free(buff);
		purple_serv_got_join_chat_failed(gc, components);
		return;
	}

	if (sdata->got_all_chats_info)
		ggp_chat_join_id(gc, id);
	else
	{
		uint64_t *id_p = g_new(uint64_t, 1);
		*id_p = id;
		sdata->pending_joins = g_slist_append(sdata->pending_joins, id_p);
	}

}

static void ggp_chat_join_id(PurpleConnection *gc, uint64_t id)
{
	GHashTable *components;
	ggp_chat_local_info *chat = ggp_chat_get(gc, id);

	if (chat && !chat->left)
	{
		ggp_chat_open_conv(chat);
		return;
	}
	
	if (!chat)
	{
		char *buff = g_strdup_printf(
			_("%llu is not a valid room identifier"), id);
		purple_notify_error(gc, _("Invalid Room Identifier"),
			_("Invalid Room Identifier"), buff);
		g_free(buff);
	}
	else /* if (chat->left) */
	{
		purple_notify_error(gc, _("Could not join chat room"),
			_("Could not join chat room"),
			_("You have to ask for invitation from another chat "
			"participant"));
	}

	components = ggp_chat_info_defaults(gc, ggp_chat_get_name_from_id(id));
	purple_serv_got_join_chat_failed(gc, components);
	g_hash_table_destroy(components);
}

void ggp_chat_leave(PurpleConnection *gc, int local_id)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	ggp_chat_local_info *chat;
	uin_t me;
	
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

	me = ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(chat->gc)));

	ggp_chat_left(chat, me);
	chat->left = TRUE;
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

	ggp_chat_open_conv(chat);
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

static gboolean ggp_chat_roomlist_get_list_finish(gpointer roomlist)
{
	purple_roomlist_set_in_progress((PurpleRoomlist*)roomlist, FALSE);
	return FALSE;
}

PurpleRoomlist * ggp_chat_roomlist_get_list(PurpleConnection *gc)
{
	ggp_chat_session_data *sdata = ggp_chat_get_sdata(gc);
	PurpleRoomlist *roomlist;
	GList *fields = NULL;
	int i;

	purple_debug_info("gg", "ggp_chat_roomlist_get_list\n");

	roomlist = purple_roomlist_new(purple_connection_get_account(gc));

	fields = g_list_append(fields, purple_roomlist_field_new(
		PURPLE_ROOMLIST_FIELD_STRING, _("Conference identifier"), "id",
		TRUE));

	fields = g_list_append(fields, purple_roomlist_field_new(
		PURPLE_ROOMLIST_FIELD_STRING, _("Start Date"), "date",
		FALSE));

	fields = g_list_append(fields, purple_roomlist_field_new(
		PURPLE_ROOMLIST_FIELD_INT, _("User Count"), "users",
		FALSE));

	fields = g_list_append(fields, purple_roomlist_field_new(
		PURPLE_ROOMLIST_FIELD_STRING, _("Status"), "status",
		FALSE));

	purple_roomlist_set_fields(roomlist, fields);

	for (i = sdata->chats_count - 1; i >= 0 ; i--)
	{
		PurpleRoomlistRoom *room;
		ggp_chat_local_info *chat = &sdata->chats[i];
		const gchar *name;
		time_t date;
		const gchar *status;
		int count = chat->participants_count;
		
		date = (uint32_t)(chat->id >> 32);
		
		if (chat->conv)
			status = _("Joined");
		else if (chat->left)
			status = _("Chat left");
		else
		{
			status = _("Can join chat");
			count--;
		}
		
		name = ggp_chat_get_name_from_id(chat->id);
		room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,
			name, NULL);
		purple_roomlist_room_add_field(roomlist, room, name);
		purple_roomlist_room_add_field(roomlist, room, purple_date_format_full(localtime(&date)));
		purple_roomlist_room_add_field(roomlist, room, GINT_TO_POINTER(count));
		purple_roomlist_room_add_field(roomlist, room, status);
		purple_roomlist_room_add(roomlist, room);
	}

	//TODO
	//purple_roomlist_set_in_progress(roomlist, FALSE);
	purple_timeout_add(1, ggp_chat_roomlist_get_list_finish, roomlist);
	return roomlist;
}
