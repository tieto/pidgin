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

static gint
_connect_to_buddy(PurpleBuddy *gb)
{
	gint socket_fd;
	struct sockaddr_in buddy_address;
	BonjourBuddy *bb = gb->proto_data;

	purple_debug_info("bonjour", "Connecting to buddy %s at %s:%d.\n",
		purple_buddy_get_name(gb), bb->ip ? bb->ip : "(null)", bb->port_p2pj);

	/* Create a socket and make it non-blocking */
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		purple_debug_warning("bonjour", "Error opening socket: %s\n", strerror(errno));
		return -1;
	}

	buddy_address.sin_family = PF_INET;
	buddy_address.sin_port = htons(bb->port_p2pj);
	inet_aton(bb->ip, &(buddy_address.sin_addr));
	memset(&(buddy_address.sin_zero), '\0', 8);

	/* TODO: make this nonblocking before connecting */
	if (connect(socket_fd, (struct sockaddr*)&buddy_address, sizeof(struct sockaddr)) == 0)
		fcntl(socket_fd, F_SETFL, O_NONBLOCK);
	else {
		purple_debug_warning("bonjour", "Error connecting to buddy %s at %s:%d error: %s\n", purple_buddy_get_name(gb), bb->ip ? bb->ip : "(null)", bb->port_p2pj, strerror(errno));
		close(socket_fd);
		socket_fd = -1;
	}

	return socket_fd;
}

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
bonjour_jabber_conv_new(const char *name) {

	BonjourJabberConversation *bconv = g_new0(BonjourJabberConversation, 1);
	bconv->socket = -1;
	bconv->buddy_name = g_strdup(name);
	bconv->watcher_id = -1;

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
_jabber_parse_and_write_message_to_ui(xmlnode *message_node, PurpleConnection *connection, PurpleBuddy *gb)
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
		{
			composing_event = TRUE;
		}
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
	serv_got_im(connection, gb->name, body, 0, time(NULL));

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

static gint
_send_data(gint socket, char *message)
{
	gint message_len = strlen(message);
	gint partial_sent = 0;
	gchar *partial_message = message;

	while ((partial_sent = send(socket, partial_message, message_len, 0)) < message_len)
	{
		if (partial_sent != -1) {
			partial_message += partial_sent;
			message_len -= partial_sent;
		} else {
			return -1;
		}
	}

	return strlen(message);
}

static void
_client_socket_handler(gpointer data, gint socket, PurpleInputCondition condition)
{
	char *message = NULL;
	gint message_length;
	PurpleBuddy *pb = data;
	PurpleAccount *account = pb->account;
	PurpleConversation *conversation;
	BonjourBuddy *bb = pb->proto_data;
	gboolean closed_conversation = FALSE;
	xmlnode *message_node;

	/* Read the data from the socket */
	if ((message_length = _read_data(socket, &message)) == -1) {
		/* There have been an error reading from the socket */
		/* TODO: Shouldn't we handle the error if it isn't EAGAIN? */
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

	/* Parse the message into an XMLnode for analysis */
	message_node = xmlnode_from_str(message, strlen(message));

	/* Check if the start of the stream has been received, if not check that the current */
	/* data is the start of the stream */
	if (!(bb->conversation->stream_started))
	{
		/* Check if this is the start of the stream */
		if ((message_node != NULL) &&
			g_ascii_strcasecmp(xmlnode_get_attrib(message_node, "xmlns"), "jabber:client") &&
			(xmlnode_get_attrib(message_node,"xmlns:stream") != NULL))
		{
			bb->conversation->stream_started = TRUE;
		}
		else
		{
			char *stream_start = g_strdup_printf(DOCTYPE, purple_account_get_username(pb->account),
							     purple_buddy_get_name(pb));

			/* TODO: This needs to be nonblocking! */
			if (send(bb->conversation->socket, stream_start, strlen(stream_start), 0) == -1)
				purple_debug_error("bonjour", "Unable to start a conversation with %s\n", bb->name);
			else
				bb->conversation->stream_started = TRUE;

			g_free(stream_start);
		}
	}

	/*
	 * Check that this is not the end of the conversation.  This is
	 * using a magic string, but xmlnode won't play nice when just
	 * parsing an end tag
	 */
	if (closed_conversation || purple_str_has_prefix(message, STREAM_END)) {
		char *closed_conv_message;

		/* Close the socket, clear the watcher and free memory */
		if (bb->conversation != NULL)
			bonjour_jabber_close_conversation(pb);

		/* Inform the user that the conversation has been closed */
		conversation = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, pb->name, account);
		closed_conv_message = g_strdup_printf(_("%s has closed the conversation."), pb->name);
		purple_conversation_write(conversation, NULL, closed_conv_message, PURPLE_MESSAGE_SYSTEM, time(NULL));
		g_free(closed_conv_message);
	} else if (message_node != NULL) {
		/* Parse the message to get the data and send to the ui */
		_jabber_parse_and_write_message_to_ui(message_node, account->gc, pb);
	} else {
		/* TODO: Deal with receiving only a partial message */
	}

	g_free(message);
	if (message_node != NULL)
		xmlnode_free(message_node);
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
		bb->conversation = bonjour_jabber_conv_new(pb->name);
		bb->conversation->socket = client_socket;

		if (bb->conversation->stream_started == FALSE) {
			char *stream_start = g_strdup_printf(DOCTYPE, purple_account_get_username(pb->account),
							     purple_buddy_get_name(pb));
			/* Start the stream */
			send(bb->conversation->socket, stream_start, strlen(stream_start), 0);
			bb->conversation->stream_started = TRUE;
			g_free(stream_start);
		}

		/* Open a watcher for the client socket */
		bb->conversation->watcher_id = purple_input_add(client_socket, PURPLE_INPUT_READ,
								_client_socket_handler, pb);
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
		int socket = _connect_to_buddy(pb);
		if (socket < 0)
			return -10001;

		bb->conversation = bonjour_jabber_conv_new(pb->name);
		bb->conversation->socket = socket;
		bb->conversation->watcher_id = purple_input_add(bb->conversation->socket,
				PURPLE_INPUT_READ, _client_socket_handler, pb);
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

	/* Check if the stream for the conversation has been started */
	if (bb->conversation->stream_started == FALSE)
	{
		char *stream_start = g_strdup_printf(DOCTYPE, purple_account_get_username(pb->account),
						     purple_buddy_get_name(pb));
		/* Start the stream */
		if (send(bb->conversation->socket, stream_start, strlen(stream_start), 0) == -1)
		{
			PurpleConversation *conv;

			purple_debug_error("bonjour", "Unable to start a conversation\n");
			purple_debug_warning("bonjour", "send error: %s\n", strerror(errno));
			conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, bb->name, data->account);
			purple_conversation_write(conv, NULL,
						  _("Unable to send the message, the conversation couldn't be started."),
						  PURPLE_MESSAGE_SYSTEM, time(NULL));

			bonjour_jabber_close_conversation(pb);

			g_free(message);
			g_free(stream_start);
			return 0;
		}

		g_free(stream_start);
		bb->conversation->stream_started = TRUE;
	}

	/* Send the message */
	ret = (_send_data(bb->conversation->socket, message) == -1);
	g_free(message);

	if (ret == -1)
		return -10000;

	return 1;
}

void
bonjour_jabber_close_conversation(PurpleBuddy *pb)
{
	BonjourBuddy *bb = pb->proto_data;
	BonjourJabberConversation *bconv = bb->conversation;

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
		purple_input_remove(bconv->watcher_id);

		/* Free all the data related to the conversation */
		g_free(bconv->buddy_name);
		g_free(bconv);
		bb->conversation = NULL;
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
		for (l = buddies; l; l = l->next)
			bonjour_jabber_close_conversation(l->data);
		g_slist_free(buddies);
	}
}
