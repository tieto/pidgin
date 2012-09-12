#include "message-prpl.h"

#include <debug.h>

#include "gg.h"
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
static gchar * ggp_message_format_to_gg(const gchar *text);

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

static gchar * ggp_message_format_to_gg(const gchar *text)
{
	gchar *text_new, *tmp;
	GRegex *regex;

	/* TODO: do it via xml parser*/

	text_new = purple_strreplace(text, "<hr>", "<br>---<br>");

	tmp = text_new;
	text_new = purple_strreplace(text_new, "</a>&nbsp;", " ");
	g_free(tmp);

	tmp = text_new;
	text_new = purple_strreplace(text_new, "</a>", "");
	g_free(tmp);
	
	tmp = text_new;
	text_new = purple_strreplace(text_new, "&nbsp;<a ", " <a ");
	g_free(tmp);
	
	regex = g_regex_new("<a href=[^>]+>", G_REGEX_CASELESS /*| G_REGEX_OPTIMIZE */, 0, NULL);
	tmp = text_new;
	text_new = g_regex_replace_literal(regex, text_new, -1, 0, "", 0, NULL);
	g_free(tmp);
	g_regex_unref(regex);
	
	/* TODO: sent images */
	return text_new;
}

/* sending */

int ggp_message_send_im(PurpleConnection *gc, const char *who,
	const char *message, PurpleMessageFlags flags)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	ggp_buddy_data *buddy_data;
	gchar *gg_msg;
	gboolean succ;

	/* TODO: return -ENOTCONN, if not connected */

	if (message == NULL || message[0] == '\0')
		return 0;

	buddy_data = ggp_buddy_get_data(purple_find_buddy(
		purple_connection_get_account(gc), who));

	if (buddy_data->blocked)
		return -1;

	gg_msg = ggp_message_format_to_gg(message);

	/* TODO: splitting messages */
	if (strlen(gg_msg) > GG_MSG_MAXSIZE)
	{
		g_free(gg_msg);
		return -E2BIG;
	}

	succ = (gg_send_message_html(info->session, GG_CLASS_CHAT,
		ggp_str_to_uin(who), (unsigned char *)gg_msg) >= 0);

	g_free(gg_msg);

	return succ ? 1 : -1;
}
