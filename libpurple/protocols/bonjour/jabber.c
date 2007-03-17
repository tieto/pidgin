/*
 * gaim - Bonjour Protocol Plugin
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
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include "libc_interface.h"
#endif
#include <sys/types.h>
#include <glib.h>
#include <glib/gprintf.h>
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

static gint
_connect_to_buddy(GaimBuddy *gb)
{
	gint socket_fd;
	gint retorno = 0;
	struct sockaddr_in buddy_address;

	/* Create a socket and make it non-blocking */
	socket_fd = socket(PF_INET, SOCK_STREAM, 0);

	buddy_address.sin_family = PF_INET;
	buddy_address.sin_port = htons(((BonjourBuddy*)(gb->proto_data))->port_p2pj);
	inet_aton(((BonjourBuddy*)(gb->proto_data))->ip, &(buddy_address.sin_addr));
	memset(&(buddy_address.sin_zero), '\0', 8);

	retorno = connect(socket_fd, (struct sockaddr*)&buddy_address, sizeof(struct sockaddr));
	if (retorno == -1) {
		gaim_debug_warning("bonjour", "connect error: %s\n", strerror(errno));
	}
	fcntl(socket_fd, F_SETFL, O_NONBLOCK);

	return socket_fd;
}

#if 0 /* this isn't used anywhere... */
static const char *
_font_size_gaim_to_ichat(int size)
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

static const char *
_font_size_ichat_to_gaim(int size)
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
_jabber_parse_and_write_message_to_ui(char *message, GaimConnection *connection, GaimBuddy *gb)
{
	xmlnode *body_node = NULL;
	char *body = NULL;
	xmlnode *html_node = NULL;
	gboolean isHTML = FALSE;
	xmlnode *html_body_node = NULL;
	const char *ichat_balloon_color = NULL;
	const char *ichat_text_color = NULL;
	xmlnode *html_body_font_node = NULL;
	const char *font_face = NULL;
	const char *font_size = NULL;
	const char *font_color = NULL;
	char *html_body = NULL;
	xmlnode *events_node = NULL;
	gboolean composing_event = FALSE;
	gint garbage = -1;
	xmlnode *message_node = NULL;

	/* Parsing of the message */
	message_node = xmlnode_from_str(message, strlen(message));
	if (message_node == NULL) {
		return;
	}

	body_node = xmlnode_get_child(message_node, "body");
	if (body_node != NULL) {
		body = xmlnode_get_data(body_node);
	} else {
		return;
	}

	html_node = xmlnode_get_child(message_node, "html");
	if (html_node != NULL)
	{
		isHTML = TRUE;
		html_body_node = xmlnode_get_child(html_node, "body");
		if (html_body_node != NULL)
		{
			ichat_balloon_color = xmlnode_get_attrib(html_body_node, "ichatballooncolor");
			ichat_text_color = xmlnode_get_attrib(html_body_node, "ichattextcolor");
			html_body_font_node = xmlnode_get_child(html_body_node, "font");
			if (html_body_font_node != NULL)
			{ /* Types of messages sent by iChat */
				font_face = xmlnode_get_attrib(html_body_font_node, "face");
				/* The absolute iChat font sizes should be converted to 1..7 range */
				font_size = xmlnode_get_attrib(html_body_font_node, "ABSZ");
				if (font_size != NULL)
				{
					font_size = _font_size_ichat_to_gaim(atoi(font_size));
				}
				font_color = xmlnode_get_attrib(html_body_font_node, "color");
				html_body = xmlnode_get_data(html_body_font_node);
				if (html_body == NULL)
				{
					/* This is the kind of formated messages that Gaim creates */
					html_body = xmlnode_to_str(html_body_font_node, &garbage);
				}
			} else {
				isHTML = FALSE;
			}
		} else {
			isHTML = FALSE;
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
			xmlnode_free(message_node);
			g_free(body);
			g_free(html_body);
			return;
		}
	}

	/* Compose the message */
	if (isHTML)
	{
		if (font_face == NULL) font_face = "Helvetica";
		if (font_size == NULL) font_size = "3";
		if (ichat_text_color == NULL) ichat_text_color = "#000000";
		if (ichat_balloon_color == NULL) ichat_balloon_color = "#FFFFFF";
		body = g_strconcat("<font face='", font_face, "' size='", font_size, "' color='", ichat_text_color,
							"' back='", ichat_balloon_color, "'>", html_body, "</font>", NULL);
	}

	/* Send the message to the UI */
	serv_got_im(connection, gb->name, body, 0, time(NULL));

	/* Free all the strings and nodes (the attributes are freed with their nodes) */
	xmlnode_free(message_node);
	g_free(body);
	g_free(html_body);
}

struct _check_buddy_by_address_t {
	char *address;
	GaimBuddy **gb;
	BonjourJabber *bj;
};

static void
_check_buddy_by_address(gpointer key, gpointer value, gpointer data)
{
	GaimBuddy *gb = (GaimBuddy*)value;
	BonjourBuddy *bb;
	struct _check_buddy_by_address_t *cbba;

	gb = value;
	cbba = data;

	/*
	 * If the current GaimBuddy's data is not null and the GaimBuddy's account
	 * is the same as the account requesting the check then continue to determine
	 * whether the buddies IP matches the target IP.
	 */
	if (cbba->bj->account == gb->account)
	{
		bb = gb->proto_data;
		if ((bb != NULL) && (g_strcasecmp(bb->ip, cbba->address) == 0))
			*(cbba->gb) = gb;
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
		gaim_debug_warning("bonjour", "receive error: %s\n", strerror(errno));
		if (total_message_length == 0) {
			return -1;
		}
	}

	*message = data->str;
	g_string_free(data, FALSE);
	if (total_message_length != 0)
		gaim_debug_info("bonjour", "Receive: -%s- %d bytes\n", *message, total_message_length);

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
_client_socket_handler(gpointer data, gint socket, GaimInputCondition condition)
{
	char *message = NULL;
	gint message_length;
	GaimBuddy *gb = (GaimBuddy*)data;
	GaimAccount *account = gb->account;
	GaimConversation *conversation;
	char *closed_conv_message;
	BonjourBuddy *bb = (BonjourBuddy*)gb->proto_data;
	gboolean closed_conversation = FALSE;
	xmlnode *message_node = NULL;

	/* Read the data from the socket */
	if ((message_length = _read_data(socket, &message)) == -1) {
		/* There have been an error reading from the socket */
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
			/* TODO: This needs to be nonblocking! */
			if (send(bb->conversation->socket, DOCTYPE, strlen(DOCTYPE), 0) == -1)
			{
				gaim_debug_error("bonjour", "Unable to start a conversation with %s\n", bb->name);
			}
			else
			{
				bb->conversation->stream_started = TRUE;
			}
		}
	}

	/*
	 * Check that this is not the end of the conversation.  This is
	 * using a magic string, but xmlnode won't play nice when just
	 * parsing an end tag
	 */
	if (gaim_str_has_prefix(message, STREAM_END) || (closed_conversation == TRUE)) {
		/* Close the socket, clear the watcher and free memory */
		if (bb->conversation != NULL) {
			close(bb->conversation->socket);
			gaim_input_remove(bb->conversation->watcher_id);
			g_free(bb->conversation->buddy_name);
			g_free(bb->conversation);
			bb->conversation = NULL;
		}

		/* Inform the user that the conversation has been closed */
		conversation = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, gb->name, account);
		closed_conv_message = g_strdup_printf(_("%s has closed the conversation."), gb->name);
		gaim_conversation_write(conversation, NULL, closed_conv_message, GAIM_MESSAGE_SYSTEM, time(NULL));
		g_free(closed_conv_message);
	} else {
		/* Parse the message to get the data and send to the ui */
		_jabber_parse_and_write_message_to_ui(message, account->gc, gb);
	}

	if (message_node != NULL)
		xmlnode_free(message_node);
}

static void
_server_socket_handler(gpointer data, int server_socket, GaimInputCondition condition)
{
	GaimBuddy *gb = NULL;
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size = sizeof(struct sockaddr);
	int client_socket;
	BonjourBuddy *bb = NULL;
	BonjourJabber *bj = data;
	char *address_text = NULL;
	GaimBuddyList *bl = gaim_get_blist();
	struct _check_buddy_by_address_t *cbba;

	/* Check that it is a read condition */
	if (condition != GAIM_INPUT_READ) {
		return;
	}

	if ((client_socket = accept(server_socket, (struct sockaddr *)&their_addr, &sin_size)) == -1)
	{
		return;
	}
	fcntl(client_socket, F_SETFL, O_NONBLOCK);

	/* Look for the buddy that has opened the conversation and fill information */
	address_text = inet_ntoa(their_addr.sin_addr);
	cbba = g_new0(struct _check_buddy_by_address_t, 1);
	cbba->address = address_text;
	cbba->gb = &gb;
	cbba->bj = bj;
	g_hash_table_foreach(bl->buddies, _check_buddy_by_address, cbba);
	g_free(cbba);
	if (gb == NULL)
	{
		gaim_debug_info("bonjour", "We don't like invisible buddies, this is not a superheros comic\n");
		close(client_socket);
		return;
	}
	bb = (BonjourBuddy*)gb->proto_data;

	/* Check if the conversation has been previously started */
	if (bb->conversation == NULL)
	{
		bb->conversation = g_new(BonjourJabberConversation, 1);
		bb->conversation->socket = client_socket;
		bb->conversation->stream_started = FALSE;
		bb->conversation->buddy_name = g_strdup(gb->name);
		bb->conversation->message_id = 1;

		if (bb->conversation->stream_started == FALSE) {
			/* Start the stream */
			send(bb->conversation->socket, DOCTYPE, strlen(DOCTYPE), 0);
			bb->conversation->stream_started = TRUE;
		}

		/* Open a watcher for the client socket */
		bb->conversation->watcher_id = gaim_input_add(client_socket, GAIM_INPUT_READ,
													_client_socket_handler, gb);
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
		gaim_debug_error("bonjour", "Cannot open socket: %s\n", strerror(errno));
		gaim_connection_error(data->account->gc, _("Cannot open socket"));
		return -1;
	}

	/* Make the socket reusable */
	if (setsockopt(data->socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0)
	{
		gaim_debug_error("bonjour", "Error setting socket options: %s\n", strerror(errno));
		gaim_connection_error(data->account->gc, _("Error setting socket options"));
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
		gaim_debug_error("bonjour", "Cannot bind socket: %s\n", strerror(errno));
		gaim_connection_error(data->account->gc, _("Could not bind socket to port"));
		return -1;
	}

	/* Attempt to listen on the bound socket */
	if (listen(data->socket, 10) != 0)
	{
		gaim_debug_error("bonjour", "Cannot listen on socket: %s\n", strerror(errno));
		gaim_connection_error(data->account->gc, _("Could not listen on socket"));
		return -1;
	}

#if 0
	/* TODO: Why isn't this being used? */
	data->socket = gaim_network_listen(data->port, SOCK_STREAM);

	if (data->socket == -1)
	{
		gaim_debug_error("bonjour", "No se ha podido crear el socket\n");
	}
#endif

	/* Open a watcher in the socket we have just opened */
	data->watcher_id = gaim_input_add(data->socket, GAIM_INPUT_READ, _server_socket_handler, data);

	return data->port;
}

int
bonjour_jabber_send_message(BonjourJabber *data, const gchar *to, const gchar *body)
{
	xmlnode *message_node = NULL;
	gchar *message = NULL;
	gint message_length = -1;
	xmlnode *message_body_node = NULL;
	xmlnode *message_html_node = NULL;
	xmlnode *message_html_body_node = NULL;
	xmlnode *message_html_body_font_node = NULL;
	xmlnode *message_x_node = NULL;
	GaimBuddy *gb = NULL;
	BonjourBuddy *bb = NULL;
	GaimConversation *conversation = NULL;
	char *message_from_ui = NULL;
	char *stripped_message = NULL;
	gint ret;

	gb = gaim_find_buddy(data->account, to);
	if (gb == NULL)
		/* You can not send a message to an offline buddy */
		return -10000;

	bb = (BonjourBuddy *)gb->proto_data;

	/* Enclose the message from the UI within a "font" node */
	message_body_node = xmlnode_new("body");
	stripped_message = gaim_markup_strip_html(body);
	xmlnode_insert_data(message_body_node, stripped_message, strlen(stripped_message));
	g_free(stripped_message);

	message_from_ui = g_strconcat("<font>", body, "</font>", NULL);
	message_html_body_font_node = xmlnode_from_str(message_from_ui, strlen(message_from_ui));
	g_free(message_from_ui);

	message_html_body_node = xmlnode_new("body");
	xmlnode_insert_child(message_html_body_node, message_html_body_font_node);

	message_html_node = xmlnode_new("html");
	xmlnode_set_attrib(message_html_node, "xmlns", "http://www.w3.org/1999/xhtml");
	xmlnode_insert_child(message_html_node, message_html_body_node);

	message_x_node = xmlnode_new("x");
	xmlnode_set_attrib(message_x_node, "xmlns", "jabber:x:event");
	xmlnode_insert_child(message_x_node, xmlnode_new("composing"));

	message_node = xmlnode_new("message");
	xmlnode_set_attrib(message_node, "to", ((BonjourBuddy*)(gb->proto_data))->name);
	xmlnode_set_attrib(message_node, "from", data->account->username);
	xmlnode_set_attrib(message_node, "type", "chat");
	xmlnode_insert_child(message_node, message_body_node);
	xmlnode_insert_child(message_node, message_html_node);
	xmlnode_insert_child(message_node, message_x_node);

	message = xmlnode_to_str(message_node, &message_length);
	xmlnode_free(message_node);

	/* Check if there is a previously open conversation */
	if (bb->conversation == NULL)
	{
		bb->conversation = g_new(BonjourJabberConversation, 1);
		bb->conversation->socket = _connect_to_buddy(gb);
		bb->conversation->stream_started = FALSE;
		bb->conversation->buddy_name = g_strdup(gb->name);
		bb->conversation->watcher_id = gaim_input_add(bb->conversation->socket,
				GAIM_INPUT_READ, _client_socket_handler, gb);
	}

	/* Check if the stream for the conversation has been started */
	if (bb->conversation->stream_started == FALSE)
	{
		/* Start the stream */
		if (send(bb->conversation->socket, DOCTYPE, strlen(DOCTYPE), 0) == -1)
		{
				gaim_debug_error("bonjour", "Unable to start a conversation\n");
				gaim_debug_warning("bonjour", "send error: %s\n", strerror(errno));
				conversation = gaim_find_conversation_with_account(GAIM_CONV_TYPE_IM, bb->name, data->account);
				gaim_conversation_write(conversation, NULL, 
										_("Unable to send the message, the conversation couldn't be started."),
										GAIM_MESSAGE_SYSTEM, time(NULL));
				close(bb->conversation->socket);
				gaim_input_remove(bb->conversation->watcher_id);

				/* Free all the data related to the conversation */
				g_free(bb->conversation->buddy_name);
				g_free(bb->conversation);
				bb->conversation = NULL;
				g_free(message);
				return 0;
		}

		bb->conversation->stream_started = TRUE;
	}

	/* Send the message */
	ret = _send_data(bb->conversation->socket, message) == -1;
	g_free(message);

	if (ret == -1)
		return -10000;

	return 1;
}

void
bonjour_jabber_close_conversation(BonjourJabber *data, GaimBuddy *gb)
{
	BonjourBuddy *bb = (BonjourBuddy*)gb->proto_data;

	if (bb->conversation != NULL)
	{
		/* Send the end of the stream to the other end of the conversation */
		send(bb->conversation->socket, STREAM_END, strlen(STREAM_END), 0);

		/* Close the socket and remove the watcher */
		close(bb->conversation->socket);
		gaim_input_remove(bb->conversation->watcher_id);

		/* Free all the data related to the conversation */
		g_free(bb->conversation->buddy_name);
		g_free(bb->conversation);
		bb->conversation = NULL;
	}
}

void
bonjour_jabber_stop(BonjourJabber *data)
{
	GaimBuddy *gb = NULL;
	BonjourBuddy *bb = NULL;
	GSList *buddies;
	GSList *l;

	/* Close the server socket and remove all the watcher */
	close(data->socket);
	gaim_input_remove(data->watcher_id);

	/* Close all the sockets and remove all the watchers after sending end streams */
	if (data->account->gc != NULL)
	{
		buddies = gaim_find_buddies(data->account, data->account->username);
		for (l = buddies; l; l = l->next)
		{
			gb = (GaimBuddy*)l->data;
			bb = (BonjourBuddy*)gb->proto_data;
			if (bb->conversation != NULL)
			{
				send(bb->conversation->socket, STREAM_END, strlen(STREAM_END), 0);
				close(bb->conversation->socket);
				gaim_input_remove(bb->conversation->watcher_id);
			}
		}
		g_slist_free(buddies);
	}
}
