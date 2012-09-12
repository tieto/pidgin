#include "message-prpl.h"

#include <debug.h>

#include "chat.h"
#include "utils.h"

#define GGP_GG10_DEFAULT_FORMAT "<span style=\"color:#000000; " \
	"font-family:'MS Shell Dlg 2'; font-size:9pt; \">"
#define GGP_GG10_DEFAULT_FORMAT_REPLACEMENT "<span>"

typedef struct
{
	enum
	{
		GGP_MESSAGE_GOT_TYPE_IM,
		GGP_MESSAGE_GOT_TYPE_CHAT,
		GGP_MESSAGE_GOT_TYPE_MULTILOGON
	} type;

	uin_t user;
	gchar *text;
	time_t time;
	uint64_t chat_id;
} ggp_message_got_data;

static PurpleConversation * ggp_message_get_conv(PurpleConnection *gc,
	uin_t uin);
static void ggp_message_got_data_free(ggp_message_got_data *msg);
static void ggp_message_got_display(PurpleConnection *gc,
	ggp_message_got_data *msg);
static void ggp_message_format_from_gg(ggp_message_got_data *msg,
	const gchar *text);

/**************/

static PurpleConversation * ggp_message_get_conv(PurpleConnection *gc,
	uin_t uin)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleConversation *conv;
	const gchar *who = ggp_uin_to_str(uin);

	conv = purple_find_conversation_with_account(
		PURPLE_CONV_TYPE_IM, who, account);
	if (conv)
		return conv;
	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, who);
	return conv;
}

static void ggp_message_got_data_free(ggp_message_got_data *msg)
{
	g_free(msg->text);
	g_free(msg);
}

void ggp_message_got(PurpleConnection *gc, const struct gg_event_msg *ev)
{
	ggp_message_got_data *msg = g_new(ggp_message_got_data, 1);
	ggp_message_format_from_gg(msg, ev->xhtml_message);

	msg->time = ev->time;
	msg->user = ev->sender;

	if (ev->chat_id != 0)
	{
		msg->type = GGP_MESSAGE_GOT_TYPE_CHAT;
		msg->chat_id = ev->chat_id;
	}
	else
	{
		msg->type = GGP_MESSAGE_GOT_TYPE_IM;
	}

	ggp_message_got_display(gc, msg);
	ggp_message_got_data_free(msg);
}

void ggp_message_got_multilogon(PurpleConnection *gc, const struct gg_event_msg *ev)
{
	ggp_message_got_data *msg = g_new(ggp_message_got_data, 1);
	ggp_message_format_from_gg(msg, ev->xhtml_message);

	msg->time = ev->time;
	msg->user = ev->sender; /* not really a sender*/
	msg->type = GGP_MESSAGE_GOT_TYPE_MULTILOGON;

	ggp_message_got_display(gc, msg);
	ggp_message_got_data_free(msg);
}

static void ggp_message_got_display(PurpleConnection *gc,
	ggp_message_got_data *msg)
{
	if (msg->type == GGP_MESSAGE_GOT_TYPE_IM)
	{
		serv_got_im(gc, ggp_uin_to_str(msg->user), msg->text,
			PURPLE_MESSAGE_RECV, msg->time);
	}
	else if (msg->type == GGP_MESSAGE_GOT_TYPE_CHAT)
	{
		ggp_chat_got_message(gc, msg->chat_id, msg->text, msg->time,
			msg->user);
	}
	else if (msg->type == GGP_MESSAGE_GOT_TYPE_MULTILOGON)
	{
		PurpleConversation *conv = ggp_message_get_conv(gc, msg->user);
		const gchar *me = purple_account_get_username(
			purple_connection_get_account(gc));

		purple_conversation_write(conv, me, msg->text,
			PURPLE_MESSAGE_SEND, msg->time);
	}
	else
		purple_debug_error("gg", "ggp_message_got_display: "
			"unexpected message type: %d\n", msg->type);
}

static void ggp_message_format_from_gg(ggp_message_got_data *msg,
	const gchar *text)
{
	gchar *text_new, *tmp;

	if (text == NULL)
	{
		msg->text = g_strdup("");
		return;
	}

	text_new = g_strdup(text);
	purple_str_strip_char(text_new, '\r');

	tmp = text_new;
	text_new = purple_strreplace(text_new, GGP_GG10_DEFAULT_FORMAT,
		GGP_GG10_DEFAULT_FORMAT_REPLACEMENT);
	g_free(tmp);

	msg->text = text_new;
}
