/*
 * purple - Bonjour Protocol Plugin
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include "libc_interface.h"
#endif
#include <sys/types.h>
#include <glib.h>
#include <unistd.h>
#include <fcntl.h>

#include "internal.h"
#include "network.h"
#include "eventloop.h"
#include "connection.h"
#include "blist.h"
#include "xmlnode.h"
#include "debug.h"
#include "notify.h"
#include "util.h"

#include "jabber.h"
#include "bonjour.h"
#include "buddy.h"

#define STREAM_END "</stream:stream>"
/* TODO: specify version='1.0' and send stream features */
#define DOCTYPE "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n" \
		"<stream:stream xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\" from=\"%s\" to=\"%s\">"

#if 0 /* this isn't used anywhere... */
static const char *
_font_size_purple_to_ichat(int size)
{
	switch (size) {
		case 1:
			return "8";
		case 2:
			return "10";
		case 3:
			return "12";
		case 4:
			return "14";
		case 5:
			return "17";
		case 6:
			return "21";
		case 7:
			return "24";
	}

	return "12";
}
#endif

static BonjourJabberConversation *
bonjour_jabber_conv_new() {

	BonjourJabberConversation *bconv = g_new0(BonjourJabberConversation, 1);
	bconv->socket = -1;
	bconv->tx_buf = purple_circ_buffer_new(512);
	bconv->tx_handler = -1;
	bconv->rx_handler = -1;

	return bconv;
}


static const char *
_font_size_ichat_to_purple(int size)
{
	if (size > 24) {
		return "7";
	} else if (size >= 21) {
		return "6";
	} else if (size >= 17) {
		return "5";
	} else if (size >= 14) {
		return "4";
	} else if (size >= 12) {
		return "3";
	} else if (size >= 10) {
		return "2";
	}

	return "1";
}

static void
_jabber_parse_and_write_message_to_ui(xmlnode *message_node, PurpleConnection *connection, PurpleBuddy *pb)
{
	xmlnode *body_node, *html_node, *events_node;
	char *body, *html_body = NULL;
	const char *ichat_balloon_color = NULL;
	const char *ichat_text_color = NULL;
	const char *font_face = NULL;
	const char *font_size = NULL;
	const char *font_color = NULL;
	gboolean composing_event = FALSE;

	body_node = xmlnode_get_child(message_node, "body");
	if (body_node == NULL)
		return;
	body = xmlnode_get_data(body_node);

	html_node = xmlnode_get_child(message_node, "html");
	if (html_node != NULL)
	{
		xmlnode *html_body_node;

		html_body_node = xmlnode_get_child(html_node, "body");
		if (html_body_node != NULL)
		{
			xmlnode *html_body_font_node;

			ichat_balloon_color = xmlnode_get_attrib(html_body_node, "ichatballooncolor");
			ichat_text_color = xmlnode_get_attrib(html_body_node, "ichattextcolor");
			html_body_font_node = xmlnode_get_child(html_body_node, "font");
			if (html_body_font_node != NULL)
			{ /* Types of messages sent by iChat */
				font_face = xmlnode_get_attrib(html_body_font_node, "face");
				/* The absolute iChat font sizes should be converted to 1..7 range */
				font_size = xmlnode_get_attrib(html_body_font_node, "ABSZ");
				if (font_size != NULL)
					font_size = _font_size_ichat_to_purple(atoi(font_size));
				font_color = xmlnode_get_attrib(html_body_font_node, "color");
				html_body = xmlnode_get_data(html_body_font_node);
				if (html_body == NULL)
					/* This is the kind of formated messages that Purple creates */
					html_body = xmlnode_to_str(html_body_font_node, NULL);
			}
		}
	}

	events_node = xmlnode_get_child_with_namespace(message_node, "x", "jabber:x:event");
	if (events_node != NULL)
	{
		if (xmlnode_get_child(events_node, "composing") != NULL)
			composing_event = TRUE;
		if (xmlnode_get_child(events_node, "id") != NULL)
		{
			/* The user is just typing */
			/* TODO: Deal with typing notification */
			g_free(body);
			g_free(html_body);
			return;
		}
	}

	/* Compose the message */
	if (html_body != NULL)
	{
		g_free(body);

		if (font_face == NULL) font_face = "Helvetica";
		if (font_size == NULL) font_size = "3";
		if (ichat_text_color == NULL) ichat_text_color = "#000000";
		if (ichat_balloon_color == NULL) ichat_balloon_color = "#FFFFFF";
		body = g_strdup_printf("<font face='%s' size='%s' color='%s' back='%s'>%s</font>",
				       font_face, font_size, ichat_text_color, ichat_balloon_color,
				       html_body);
	}

	/* TODO: Should we do something with "composing_event" here? */

	/* Send the message to the UI */
	serv_got_im(connection, pb->name, body, 0, time(NULL));

	g_free(body);
	g_free(html_body);
}

struct _check_buddy_by_address_t {
	const char *address;
	PurpleBuddy **pb;
	BonjourJabber *bj;
};

static void
_check_buddy_by_address(gpointer key, gpointer value, gpointer data)
{
	PurpleBuddy *pb = value;
	BonjourBuddy *bb;
	struct _check_buddy_by_address_t *cbba = data;

	/*
	 * If the current PurpleBuddy's data is not null and the PurpleBuddy's account
	 * is the same as the account requesting the check then continue to determine
	 * whether the buddies IP matches the target IP.
	 */
	if (cbba->bj->account == pb->account)
	{
		bb = pb->proto_data;
		if ((bb != NULL) && (g_ascii_strcasecmp(bb->ip, cbba->address) == 0))
			*(cbba->pb) = pb;
	}
}

static gint
_read_data(gint socket, char **message)
{
	GString *data = g_string_new("");
	char partial_data[512];
	gint total_message_length = 0;
	gint partial_message_length = 0;

	/* Read chunks of 512 bytes till the end of the data */
	while ((partial_message_length = recv(socket, partial_data, 512, 0)) > 0)
	{
		g_string_append_len(data, partial_data, partial_message_length);
		total_message_length += partial_message_length;
	}

	if (partial_message_length == -1)
	{
		if (errno != EAGAIN)
			purple_debug_warning("bonjour", "receive error: %s\n", strerror(errno));
		if (total_message_length == 0) {
			return -1;
		}
	}

	*message = g_string_free(data, FALSE);
	if (total_message_length != 0)
		purple_debug_info("bonjour", "Receive: -%s- %d bytes\n", *message, total_message_length);

	return total_message_length;
}

static void
_send_data_write_cb(gpointer data, gint source, PurpleInputCondition cond)
{
	PurpleBuddy *pb = data;
	BonjourBuddy *bb = pb->proto_data;
	BonjourJabberConversation *bconv = bb->conversation;
	int ret, writelen;

	/* TODO: Make sure that the stream has been established before sending */

	writelen = purple_circ_buffer_get_max_read(bconv->tx_buf);

	if (writelen == 0) {
		purple_input_remove(bconv->tx_handler);
		bconv->tx_handler = -1;
		return;
	}

	ret = send(bconv->socket, bconv->tx_buf->outptr, writelen, 0);

	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0) {
		PurpleConversation *conv;
		const char *error = strerror(errno);

		purple_debug_error("bonjour", "Error sending message to buddy %s error: %s\n",
				   purple_buddy_get_name(pb), error ? error : "(null)");

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, bb->name, pb->account);
		if (conv != NULL)
			purple_conversation_write(conv, NULL,
				  _("Unable to send message."),
				  PURPLE_MESSAGE_SYSTEM, time(NULL));

		bonjour_jabber_close_conversation(bb->conversation);
		bb->conversation = NULL;
		return;
	}

	purple_circ_buffer_mark_read(bconv->tx_buf, ret);
}

static gint
_send_data(PurpleBuddy *pb, char *message)
{
	gint ret;
	int len = strlen(message);
	BonjourBuddy *bb = pb->proto_data;
	BonjourJabberConversation *bconv = bb->conversation;

	/* If we're not ready to actually send, append it to the buffer */
	if (bconv->tx_handler != -1
			|| bconv->connect_data != NULL
			|| !bconv->stream_started
			|| purple_circ_buffer_get_max_read(bconv->tx_buf) > 0) {
		ret = -1;
		errno = EAGAIN;
	} else {
		ret = send(bconv->socket, message, len, 0);
	}

	if (ret == -1 && errno == EAGAIN)
		ret = 0;
	else if (ret <= 0) {
		PurpleConversation *conv;
		const char *error = strerror(errno);

		purple_debug_error("bonjour", "Error sending message to buddy %s error: %s\n",
				   purple_buddy_get_name(pb), error ? error : "(null)");

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, bb->name, pb->account);
		if (conv != NULL)
			purple_conversation_write(conv, NULL,
				  _("Unable to send message."),
				  PURPLE_MESSAGE_SYSTEM, time(NULL));

		bonjour_jabber_close_conversation(bb->conversation);
		bb->conversation = NULL;
		return -1;
	}

	if (ret < len) {
		if (bconv->tx_handler == -1)
			bconv->tx_handler = purple_input_add(bconv->socket, PURPLE_INPUT_WRITE,
				_send_data_write_cb, pb);
		purple_circ_buffer_append(bconv->tx_buf, message + ret, len - ret);
	}

	return ret;
}

static void
_client_socket_handler(gpointer data, gint socket, PurpleInputCondition condition)
{
	char *message = NULL;
	gint message_length;
	PurpleBuddy *pb = data;
	PurpleAccount *account = pb->account;
	BonjourBuddy *bb = pb->proto_data;
	gboolean closed_conversation = FALSE;

	/* Read the data from the socket */
	if ((message_length = _read_data(socket, &message)) == -1) {
		/* There have been an error reading from the socket */
		if (errno != EAGAIN) {
			bonjour_jabber_close_conversation(bb->conversation);
			bb->conversation = NULL;

			/* I guess we really don't need to notify the user.
			 * If they try to send another message it'll reconnect */
		}
		return;
	} else if (message_length == 0) { /* The other end has closed the socket */
		closed_conversation = TRUE;
	} else {
		message[message_length] = '\0';

		while (g_ascii_iscntrl(message[message_length - 1])) {
			message[message_length - 1] = '\0';
			message_length--;
		}
	}

	/*
	 * Check that this is not the end of the conversation.  This is
	 * using a magic string, but xmlnode won't play nice when just
	 * parsing an end tag
	 */
	if (closed_conversation || purple_str_has_prefix(message, STREAM_END)) {
		PurpleConversation *conv;

		/* Close the socket, clear the watcher and free memory */
		bonjour_jabber_close_conversation(bb->conversation);
		bb->conversation = NULL;

		/* Inform the user that the conversation has been closed */
		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, pb->name, account);
		if (conv != NULL) {
			char *tmp = g_strdup_printf(_("%s has closed the conversation."), pb->name);
			purple_conversation_write(conv, NULL, tmp, PURPLE_MESSAGE_SYSTEM, time(NULL));
			g_free(tmp);
		}
	} else {
		xmlnode *message_node;

		/* Parse the message into an XMLnode for analysis */
		message_node = xmlnode_from_str(message, strlen(message));

		if (message_node != NULL) {
			/* Parse the message to get the data and send to the ui */
			_jabber_parse_and_write_message_to_ui(message_node, account->gc, pb);
			xmlnode_free(message_node);
		} else {
			/* TODO: Deal with receiving only a partial message */
		}
	}

	g_free(message);
}

struct _stream_start_data {
	char *msg;
	PurpleInputFunction tx_handler_cb;
};

static void
_start_stream(gpointer data, gint source, PurpleInputCondition condition)
{
	PurpleBuddy *pb = data;
	BonjourBuddy *bb = pb->proto_data;
	struct _stream_start_data *ss = bb->conversation->stream_data;
	int len, ret;

	len = strlen(ss->msg);

	/* Start Stream */
	ret = send(source, ss->msg, len, 0);

	if (ret == -1 && errno == EAGAIN)
		return;
	else if (ret <= 0) {
		const char *err = strerror(errno);
		PurpleConversation *conv;

		purple_debug_error("bonjour", "Error starting stream with buddy %s at %s:%d error: %s\n",
				   purple_buddy_get_name(pb), bb->ip ? bb->ip : "(null)", bb->port_p2pj, err ? err : "(null)");

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, bb->name, pb->account);
		if (conv != NULL)
			purple_conversation_write(conv, NULL,
				  _("Unable to send the message, the conversation couldn't be started."),
				  PURPLE_MESSAGE_SYSTEM, time(NULL));

		bonjour_jabber_close_conversation(bb->conversation);
		bb->conversation = NULL;

		return;
	}

	/* This is EXTREMELY unlikely to happen */
	if (ret < len) {
		char *tmp = g_strdup(ss->msg + ret);
		g_free(ss->msg);
		ss->msg = tmp;
		return;
	}

	/* Stream started; process the send buffer if there is one*/
	purple_input_remove(bb->conversation->tx_handler);
	bb->conversation->tx_handler= -1;

	bb->conversation->stream_started = TRUE;

	g_free(ss->msg);
	g_free(ss);
	bb->conversation->stream_data = NULL;

	if (ss->tx_handler_cb) {
		bb->conversation->tx_handler = purple_input_add(source, PURPLE_INPUT_WRITE,
			ss->tx_handler_cb, pb);
		/* We can probably write the data now. */
		(ss->tx_handler_cb)(pb, source, PURPLE_INPUT_WRITE);
	}
}

static void
_server_socket_handler(gpointer data, int server_socket, PurpleInputCondition condition)
{
	PurpleBuddy *pb = NULL;
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size = sizeof(struct sockaddr);
	int client_socket;
	BonjourBuddy *bb;
	char *address_text = NULL;
	PurpleBuddyList *bl = purple_get_blist();
	struct _check_buddy_by_address_t *cbba;

	/* Check that it is a read condition */
	if (condition != PURPLE_INPUT_READ)
		return;

	if ((client_socket = accept(server_socket, (struct sockaddr *)&their_addr, &sin_size)) == -1)
		return;

	fcntl(client_socket, F_SETFL, O_NONBLOCK);

	/* Look for the buddy that has opened the conversation and fill information */
	address_text = inet_ntoa(their_addr.sin_addr);
	cbba = g_new0(struct _check_buddy_by_address_t, 1);
	cbba->address = address_text;
	cbba->pb = &pb;
	cbba->bj = data;
	g_hash_table_foreach(bl->buddies, _check_buddy_by_address, cbba);
	g_free(cbba);
	if (pb == NULL)
	{
		purple_debug_info("bonjour", "We don't like invisible buddies, this is not a superheros comic\n");
		close(client_socket);
		return;
	}
	bb = pb->proto_data;

	/* Check if the conversation has been previously started */
	if (bb->conversation == NULL)
	{
		int ret, len;
		char *stream_start = g_strdup_printf(DOCTYPE, purple_account_get_username(pb->account),
			purple_buddy_get_name(pb));

		len = strlen(stream_start);

		/* Start the stream */
		ret = send(client_socket, stream_start, len, 0);

		if (ret == -1 && errno == EAGAIN)
			ret = 0;
		else if (ret <= 0) {
			const char *err = strerror(errno);

			purple_debug_error("bonjour", "Error starting stream with buddy %s at %s:%d error: %s\n",
					   purple_buddy_get_name(pb), bb->ip ? bb->ip : "(null)", bb->port_p2pj, err ? err : "(null)");

			close(client_socket);
			g_free(stream_start);

			return;
		}

		bb->conversation = bonjour_jabber_conv_new();
		bb->conversation->socket = client_socket;
		bb->conversation->rx_handler = purple_input_add(client_socket,
			PURPLE_INPUT_READ, _client_socket_handler, pb);

		/* This is unlikely to happen */
		if (ret < len) {
			struct _stream_start_data *ss = g_new(struct _stream_start_data, 1);
			ss->msg = g_strdup(stream_start + ret);
			ss->tx_handler_cb = NULL; /* We have nothing to write yet */
			bb->conversation->stream_data = ss;
			/* Finish sending the stream start */
			bb->conversation->tx_handler = purple_input_add(client_socket,
				PURPLE_INPUT_WRITE, _start_stream, pb);
		} else {
			bb->conversation->stream_started = TRUE;
		}

		g_free(stream_start);
	} else {
		close(client_socket);
	}
}

gint
bonjour_jabber_start(BonjourJabber *data)
{
	struct sockaddr_in my_addr;
	int yes = 1;
	int i;
	gboolean bind_successful;

	/* Open a listening socket for incoming conversations */
	if ((data->socket = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		purple_debug_error("bonjour", "Cannot open socket: %s\n", strerror(errno));
		purple_connection_error(data->account->gc, _("Cannot open socket"));
		return -1;
	}

	/* Make the socket reusable */
	if (setsockopt(data->socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0)
	{
		purple_debug_error("bonjour", "Error setting socket options: %s\n", strerror(errno));
		purple_connection_error(data->account->gc, _("Error setting socket options"));
		return -1;
	}

	memset(&my_addr, 0, sizeof(struct sockaddr_in));
	my_addr.sin_family = PF_INET;

	/* Attempt to find a free port */
	bind_successful = FALSE;
	for (i = 0; i < 10; i++)
	{
		my_addr.sin_port = htons(data->port);
		if (bind(data->socket, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == 0)
		{
			bind_successful = TRUE;
			break;
		}
		data->port++;
	}

	/* On no!  We tried 10 ports and could not bind to ANY of them */
	if (!bind_successful)
	{
		purple_debug_error("bonjour", "Cannot bind socket: %s\n", strerror(errno));
		purple_connection_error(data->account->gc, _("Could not bind socket to port"));
		return -1;
	}

	/* Attempt to listen on the bound socket */
	if (listen(data->socket, 10) != 0)
	{
		purple_debug_error("bonjour", "Cannot listen on socket: %s\n", strerror(errno));
		purple_connection_error(data->account->gc, _("Could not listen on socket"));
		return -1;
	}

#if 0
	/* TODO: Why isn't this being used? */
	data->socket = purple_network_listen(data->port, SOCK_STREAM);

	if (data->socket == -1)
	{
		purple_debug_error("bonjour", "No se ha podido crear el socket\n");
	}
#endif

	/* Open a watcher in the socket we have just opened */
	data->watcher_id = purple_input_add(data->socket, PURPLE_INPUT_READ, _server_socket_handler, data);

	return data->port;
}

static void
_connected_to_buddy(gpointer data, gint source, const gchar *error)
{
	PurpleBuddy *pb = data;
	BonjourBuddy *bb = pb->proto_data;
	int len, ret;
	char *stream_start;

	bb->conversation->connect_data = NULL;

	if (source < 0) {
		PurpleConversation *conv;

		purple_debug_error("bonjour", "Error connecting to buddy %s at %s:%d error: %s\n",
				   purple_buddy_get_name(pb), bb->ip ? bb->ip : "(null)", bb->port_p2pj, error ? error : "(null)");

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, bb->name, pb->account);
		if (conv != NULL)
			purple_conversation_write(conv, NULL,
				  _("Unable to send the message, the conversation couldn't be started."),
				  PURPLE_MESSAGE_SYSTEM, time(NULL));

		bonjour_jabber_close_conversation(bb->conversation);
		bb->conversation = NULL;
		return;
	}

	stream_start = g_strdup_printf(DOCTYPE, purple_account_get_username(pb->account), purple_buddy_get_name(pb));
	len = strlen(stream_start);

	/* Start the stream and send queued messages */
	ret = send(source, stream_start, len, 0);

	if (ret == -1 && errno == EAGAIN)
		ret = 0;
	else if (ret <= 0) {
		const char *err = strerror(errno);
		PurpleConversation *conv;

		purple_debug_error("bonjour", "Error starting stream with buddy %s at %s:%d error: %s\n",
				   purple_buddy_get_name(pb), bb->ip ? bb->ip : "(null)", bb->port_p2pj, err ? err : "(null)");

		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, bb->name, pb->account);
		if (conv != NULL)
			purple_conversation_write(conv, NULL,
				  _("Unable to send the message, the conversation couldn't be started."),
				  PURPLE_MESSAGE_SYSTEM, time(NULL));

		close(source);
		bonjour_jabber_close_conversation(bb->conversation);
		bb->conversation = NULL;

		g_free(stream_start);

		return;
	}

	bb->conversation->socket = source;
	bb->conversation->rx_handler = purple_input_add(source,
		PURPLE_INPUT_READ, _client_socket_handler, pb);

	/* This is unlikely to happen */
	if (ret < len) {
		struct _stream_start_data *ss = g_new(struct _stream_start_data, 1);
		ss->msg = g_strdup(stream_start + ret);
		ss->tx_handler_cb = _send_data_write_cb;
		bb->conversation->stream_data = ss;
		/* Finish sending the stream start */
		bb->conversation->tx_handler = purple_input_add(source,
			PURPLE_INPUT_WRITE, _start_stream, pb);
	}
	/* Process the send buffer */
	else {
		bb->conversation->stream_started = TRUE;
		/* Watch for when we can write the buffered messages */
		bb->conversation->tx_handler = purple_input_add(source, PURPLE_INPUT_WRITE,
			_send_data_write_cb, pb);
		/* We can probably write the data now. */
		_send_data_write_cb(pb, source, PURPLE_INPUT_WRITE);
	}

	g_free(stream_start);
}

int
bonjour_jabber_send_message(BonjourJabber *data, const gchar *to, const gchar *body)
{
	xmlnode *message_node, *node, *node2;
	gchar *message;
	PurpleBuddy *pb;
	BonjourBuddy *bb;
	int ret;

	pb = purple_find_buddy(data->account, to);
	if (pb == NULL) {
		purple_debug_info("bonjour", "Can't send a message to an offline buddy (%s).\n", to);
		/* You can not send a message to an offline buddy */
		return -10000;
	}

	bb = pb->proto_data;

	/* Check if there is a previously open conversation */
	if (bb->conversation == NULL)
	{
		PurpleProxyConnectData *connect_data;
		PurpleProxyInfo *proxy_info;

		/* Make sure that the account always has a proxy of "none".
		 * This is kind of dirty, but proxy_connect_none() isn't exposed. */
		proxy_info = purple_account_get_proxy_info(data->account);
		if (proxy_info == NULL) {
			proxy_info = purple_proxy_info_new();
			purple_account_set_proxy_info(data->account, proxy_info);
		}
		purple_proxy_info_set_type(proxy_info, PURPLE_PROXY_NONE);

		connect_data =
			purple_proxy_connect(data->account->gc, data->account, bb->ip,
					     bb->port_p2pj, _connected_to_buddy, pb);

		if (connect_data == NULL) {
			purple_debug_error("bonjour", "Unable to connect to buddy (%s).\n", to);
			return -10001;
		}

		bb->conversation = bonjour_jabber_conv_new();
		bb->conversation->connect_data = connect_data;
		/* We don't want _send_data() to register the tx_handler;
		 * that neeeds to wait until we're actually connected. */
		bb->conversation->tx_handler = 0;
	}

	message_node = xmlnode_new("message");
	xmlnode_set_attrib(message_node, "to", bb->name);
	xmlnode_set_attrib(message_node, "from", purple_account_get_username(data->account));
	xmlnode_set_attrib(message_node, "type", "chat");

	/* Enclose the message from the UI within a "font" node */
	node = xmlnode_new_child(message_node, "body");
	message = purple_markup_strip_html(body);
	xmlnode_insert_data(node, message, strlen(message));
	g_free(message);

	node = xmlnode_new_child(message_node, "html");
	xmlnode_set_namespace(node, "http://www.w3.org/1999/xhtml");

	node = xmlnode_new_child(node, "body");
	message = g_strdup_printf("<font>%s</font>", body);
	node2 = xmlnode_from_str(message, strlen(message));
	g_free(message);
	xmlnode_insert_child(node, node2);

	node = xmlnode_new_child(message_node, "x");
	xmlnode_set_namespace(node, "jabber:x:event");
	xmlnode_insert_child(node, xmlnode_new("composing"));

	message = xmlnode_to_str(message_node, NULL);
	xmlnode_free(message_node);

	ret = _send_data(pb, message) >= 0;

	g_free(message);

	return ret;
}

void
bonjour_jabber_close_conversation(BonjourJabberConversation *bconv)
{
	if (bconv != NULL)
	{
		/* Close the socket and remove the watcher */
		if (bconv->socket >= 0) {
			/* Send the end of the stream to the other end of the conversation */
			if (bconv->stream_started)
				send(bconv->socket, STREAM_END, strlen(STREAM_END), 0);
			/* TODO: We're really supposed to wait for "</stream:stream>" before closing the socket */
			close(bconv->socket);
		}
		if (bconv->rx_handler != -1)
			purple_input_remove(bconv->rx_handler);
		if (bconv->tx_handler > 0)
			purple_input_remove(bconv->tx_handler);

		/* Free all the data related to the conversation */
		purple_circ_buffer_destroy(bconv->tx_buf);
		if (bconv->connect_data != NULL)
			purple_proxy_connect_cancel(bconv->connect_data);
		if (bconv->stream_data != NULL) {
			struct _stream_start_data *ss = bconv->stream_data;
			g_free(ss->msg);
			g_free(ss);
		}
		g_free(bconv);
	}
}

void
bonjour_jabber_stop(BonjourJabber *data)
{
	/* Close the server socket and remove the watcher */
	if (data->socket >= 0)
		close(data->socket);
	if (data->watcher_id > 0)
		purple_input_remove(data->watcher_id);

	/* Close all the conversation sockets and remove all the watchers after sending end streams */
	if (data->account->gc != NULL)
	{
		GSList *buddies, *l;

		buddies = purple_find_buddies(data->account, purple_account_get_username(data->account));
		for (l = buddies; l; l = l->next) {
			BonjourBuddy *bb = ((PurpleBuddy*) l->data)->proto_data;
			bonjour_jabber_close_conversation(bb->conversation);
			bb->conversation = NULL;
		}

		g_slist_free(buddies);
	}
}
