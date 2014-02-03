/**
 * @file gg.c Gadu-Gadu protocol plugin
 *
 * purple
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
 *
 * Some parts of the code are adapted or taken from the previous implementation
 * of this plugin written by Arkadiusz Miskiewicz <misiek@pld.org.pl>
 * Some parts Copyright (C) 2009  Krzysztof Klinikowski <grommasher@gmail.com>
 *
 * Thanks to Google's Summer of Code Program.
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

#include "plugins.h"
#include "version.h"
#include "notify.h"
#include "buddylist.h"
#include "accountopt.h"
#include "debug.h"
#include "util.h"
#include "request.h"
#include "xmlnode.h"

#include "gg.h"
#include "chat.h"
#include "search.h"
#include "blist.h"
#include "utils.h"
#include "resolver-purple.h"
#include "deprecated.h"
#include "purplew.h"
#include "libgadu-events.h"
#include "multilogon.h"
#include "status.h"
#include "servconn.h"
#include "tcpsocket.h"
#include "pubdir-prpl.h"
#include "message-prpl.h"
#include "html.h"
#include "libgaduw.h"

/* ---------------------------------------------------------------------- */
static PurpleProtocol *my_protocol = NULL;
static PurpleAccountOption *ggp_server_option;

/* ---------------------------------------------------------------------- */

ggp_buddy_data * ggp_buddy_get_data(PurpleBuddy *buddy)
{
	ggp_buddy_data *buddy_data = purple_buddy_get_protocol_data(buddy);
	if (buddy_data)
		return buddy_data;

	buddy_data = g_new0(ggp_buddy_data, 1); /* TODO: leak */
	purple_buddy_set_protocol_data(buddy, buddy_data);
	return buddy_data;
}

static void ggp_buddy_free(PurpleBuddy *buddy)
{
	ggp_buddy_data *buddy_data = purple_buddy_get_protocol_data(buddy);

	if (!buddy_data)
		return;

	g_free(buddy_data);
	purple_buddy_set_protocol_data(buddy, NULL);
}

const gchar * ggp_get_imtoken(PurpleConnection *gc)
{
	GGPInfo *accdata = purple_connection_get_protocol_data(gc);

	if (accdata->imtoken)
		return accdata->imtoken;

	if (accdata->imtoken_warned)
		return NULL;
	accdata->imtoken_warned = TRUE;

	purple_notify_error(gc, _("Authentication failed"),
		_("IMToken value has not been received."),
		_("Some features will be disabled. "
		"You may try again after a while."),
		purple_request_cpar_from_connection(gc));
	return NULL;
}

uin_t ggp_own_uin(PurpleConnection *gc)
{
	return ggp_str_to_uin(purple_account_get_username(
		purple_connection_get_account(gc)));
}

/* ---------------------------------------------------------------------- */
/* buddy list import/export from/to file */

static void ggp_callback_buddylist_save_ok(PurpleConnection *gc, const char *filename)
{
	PurpleAccount *account = purple_connection_get_account(gc);

	char *buddylist = ggp_buddylist_dump(account);

	purple_debug_info("gg", "Saving...\n");
	purple_debug_info("gg", "file = %s\n", filename);

	if (buddylist == NULL) {
		purple_notify_info(account, _("Save Buddylist..."), _("Your "
			"buddylist is empty, nothing was written to the file."),
			NULL, purple_request_cpar_from_connection(gc));
		return;
	}

	if (purple_util_write_data_to_file_absolute(filename, buddylist, -1)) {
		purple_notify_info(account, _("Save Buddylist..."),
			_("Buddylist saved successfully!"), NULL,
			purple_request_cpar_from_connection(gc));
	} else {
		gchar *primary = g_strdup_printf(
			_("Couldn't write buddy list for %s to %s"),
			purple_account_get_username(account), filename);
		purple_notify_error(account, _("Save Buddylist..."),
			primary, NULL, purple_request_cpar_from_connection(gc));
		g_free(primary);
	}

	g_free(buddylist);
}

static void ggp_callback_buddylist_load_ok(PurpleConnection *gc, gchar *file)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GError *error = NULL;
	char *buddylist = NULL;
	gsize length;

	purple_debug_info("gg", "file_name = %s\n", file);

	if (!g_file_get_contents(file, &buddylist, &length, &error)) {
		purple_notify_error(account, _("Couldn't load buddylist"),
			_("Couldn't load buddylist"), error->message,
			purple_request_cpar_from_connection(gc));

		purple_debug_error("gg",
			"Couldn't load buddylist. file = %s; error = %s\n",
			file, error->message);

		g_error_free(error);

		return;
	}

	ggp_buddylist_load(gc, buddylist);
	g_free(buddylist);

	purple_notify_info(account, _("Load Buddylist..."),
		_("Buddylist loaded successfully!"), NULL,
		purple_request_cpar_from_connection(gc));
}
/* }}} */

/*
 */
/* static void ggp_action_buddylist_save(PurpleProtocolAction *action) {{{ */
static void ggp_action_buddylist_save(PurpleProtocolAction *action)
{
	PurpleConnection *gc = action->connection;

	purple_request_file(action, _("Save buddylist..."), NULL, TRUE,
		G_CALLBACK(ggp_callback_buddylist_save_ok), NULL,
		purple_request_cpar_from_connection(gc), gc);
}

static void ggp_action_buddylist_load(PurpleProtocolAction *action)
{
	PurpleConnection *gc = action->connection;

	purple_request_file(action, _("Load buddylist from file..."), NULL,
		FALSE, G_CALLBACK(ggp_callback_buddylist_load_ok), NULL,
		purple_request_cpar_from_connection(gc), gc);
}

/* ----- BLOCK BUDDIES -------------------------------------------------- */

static void ggp_add_deny(PurpleConnection *gc, const char *who)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	uin_t uin = ggp_str_to_uin(who);

	purple_debug_info("gg", "ggp_add_deny: %u\n", uin);

	gg_remove_notify_ex(info->session, uin, GG_USER_NORMAL);
	gg_add_notify_ex(info->session, uin, GG_USER_BLOCKED);
}

static void ggp_rem_deny(PurpleConnection *gc, const char *who)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	uin_t uin = ggp_str_to_uin(who);

	purple_debug_info("gg", "ggp_rem_deny: %u\n", uin);

	gg_remove_notify_ex(info->session, uin, GG_USER_BLOCKED);
	gg_add_notify_ex(info->session, uin, GG_USER_NORMAL);
}

/* ---------------------------------------------------------------------- */
/* ----- INTERNAL CALLBACKS --------------------------------------------- */
/* ---------------------------------------------------------------------- */

static void ggp_typing_notification_handler(PurpleConnection *gc, uin_t uin, int length) {
	gchar *from;

	from = g_strdup_printf("%u", uin);
	if (length)
		serv_got_typing(gc, from, 0, PURPLE_IM_TYPING);
	else
		serv_got_typing_stopped(gc, from);
	g_free(from);
}

/**
 * Handling of XML events.
 *
 * @param gc PurpleConnection.
 * @param data Raw XML contents.
 *
 * @see http://toxygen.net/libgadu/protocol/#ch1.13
 * @todo: this may not be necessary anymore
 */
static void ggp_xml_event_handler(PurpleConnection *gc, char *data)
{
	PurpleXmlNode *xml = NULL;
	PurpleXmlNode *xmlnode_next_event;

	xml = purple_xmlnode_from_str(data, -1);
	if (xml == NULL)
	{
		purple_debug_error("gg", "ggp_xml_event_handler: "
			"invalid xml: [%s]\n", data);
		goto out;
	}

	xmlnode_next_event = purple_xmlnode_get_child(xml, "event");
	while (xmlnode_next_event != NULL)
	{
		PurpleXmlNode *xmlnode_current_event = xmlnode_next_event;

		PurpleXmlNode *xmlnode_type;
		char *event_type_raw;
		int event_type = 0;

		PurpleXmlNode *xmlnode_sender;
		char *event_sender_raw;
		uin_t event_sender = 0;

		xmlnode_next_event = purple_xmlnode_get_next_twin(xmlnode_next_event);

		xmlnode_type = purple_xmlnode_get_child(xmlnode_current_event, "type");
		if (xmlnode_type == NULL)
			continue;
		event_type_raw = purple_xmlnode_get_data(xmlnode_type);
		if (event_type_raw != NULL)
			event_type = atoi(event_type_raw);
		g_free(event_type_raw);

		xmlnode_sender = purple_xmlnode_get_child(xmlnode_current_event, "sender");
		if (xmlnode_sender != NULL)
		{
			event_sender_raw = purple_xmlnode_get_data(xmlnode_sender);
			if (event_sender_raw != NULL)
				event_sender = ggp_str_to_uin(event_sender_raw);
			g_free(event_sender_raw);
		}

		switch (event_type)
		{
			case 28: /* avatar update */
				purple_debug_info("gg",
					"ggp_xml_event_handler: avatar updated (uid: %u)\n",
					event_sender);
				break;
			default:
				purple_debug_error("gg",
					"ggp_xml_event_handler: unsupported event type=%d from=%u\n",
					event_type, event_sender);
		}
	}

	out:
		if (xml)
			purple_xmlnode_free(xml);
}

static void ggp_callback_recv(gpointer _gc, gint fd, PurpleInputCondition cond)
{
	PurpleConnection *gc = _gc;
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	struct gg_event *ev;

	if (!(ev = gg_watch_fd(info->session))) {
		purple_debug_error("gg",
			"ggp_callback_recv: gg_watch_fd failed -- CRITICAL!\n");
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to read from socket"));
		return;
	}

#if GGP_ENABLE_GG11
	if (purple_debug_is_verbose()) {
		purple_debug_misc("gg", "ggp_callback_recv: got event %s",
			gg_debug_event(ev->type));
	}
#endif

	purple_input_remove(info->inpa);
	info->inpa = purple_input_add(info->session->fd,
		ggp_tcpsocket_inputcond_gg_to_purple(info->session->check),
		ggp_callback_recv, gc);

	switch (ev->type) {
		case GG_EVENT_NONE:
			/* Nothing happened. */
			break;
		case GG_EVENT_CONN_FAILED:
			purple_connection_error (gc,
				PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				_("Server disconnected"));
			break;
		case GG_EVENT_MSG:
			ggp_message_got(gc, &ev->event.msg);
			break;
		case GG_EVENT_ACK:
#if GGP_ENABLE_GG11
		case GG_EVENT_ACK110:
#endif
			break;
		case GG_EVENT_IMAGE_REPLY:
			ggp_image_recv(gc, &ev->event.image_reply);
			break;
		case GG_EVENT_IMAGE_REQUEST:
			ggp_image_send(gc, &ev->event.image_request);
			break;
		case GG_EVENT_NOTIFY60:
		case GG_EVENT_STATUS60:
			ggp_status_got_others(gc, ev);
			break;
		case GG_EVENT_TYPING_NOTIFICATION:
			ggp_typing_notification_handler(gc, ev->event.typing_notification.uin,
				ev->event.typing_notification.length);
			break;
		case GG_EVENT_XML_EVENT:
			purple_debug_info("gg", "GG_EVENT_XML_EVENT\n");
			ggp_xml_event_handler(gc, ev->event.xml_event.data);
			break;
		case GG_EVENT_USER_DATA:
			ggp_events_user_data(gc, &ev->event.user_data);
			break;
#if GGP_ENABLE_GG11
		case GG_EVENT_JSON_EVENT:
			ggp_events_json(gc, &ev->event.json_event);
			break;
#endif
		case GG_EVENT_USERLIST100_VERSION:
			ggp_roster_version(gc, &ev->event.userlist100_version);
			break;
		case GG_EVENT_USERLIST100_REPLY:
			ggp_roster_reply(gc, &ev->event.userlist100_reply);
			break;
		case GG_EVENT_MULTILOGON_MSG:
			ggp_message_got_multilogon(gc, &ev->event.multilogon_msg);
			break;
		case GG_EVENT_MULTILOGON_INFO:
			ggp_multilogon_info(gc, &ev->event.multilogon_info);
			break;
#if GGP_ENABLE_GG11
		case GG_EVENT_IMTOKEN:
			purple_debug_info("gg", "gg11: got IMTOKEN\n");
			g_free(info->imtoken);
			info->imtoken = g_strdup(ev->event.imtoken.imtoken);
			break;
		case GG_EVENT_PONG110:
			purple_debug_info("gg", "gg11: got PONG110 %lu\n",
				(long unsigned)ev->event.pong110.time);
			break;
		case GG_EVENT_CHAT_INFO:
		case GG_EVENT_CHAT_INFO_GOT_ALL:
		case GG_EVENT_CHAT_INFO_UPDATE:
		case GG_EVENT_CHAT_CREATED:
		case GG_EVENT_CHAT_INVITE_ACK:
			ggp_chat_got_event(gc, ev);
			break;
#endif
		case GG_EVENT_DISCONNECT:
			ggp_servconn_remote_disconnect(gc);
			break;
		default:
			purple_debug_warning("gg",
				"unsupported event type=%d\n", ev->type);
			break;
	}

	gg_free_event(ev);
}

void ggp_async_login_handler(gpointer _gc, gint fd, PurpleInputCondition cond)
{
	PurpleConnection *gc = _gc;
	GGPInfo *info;
	struct gg_event *ev;

	g_return_if_fail(PURPLE_CONNECTION_IS_VALID(gc));

	info = purple_connection_get_protocol_data(gc);

	purple_debug_info("gg", "login_handler: session: check = %d; state = %d;\n",
			info->session->check, info->session->state);

	switch (info->session->state) {
		case GG_STATE_ERROR:
			purple_debug_info("gg", "GG_STATE_ERROR\n");
			break;
		case GG_STATE_RESOLVING:
			purple_debug_info("gg", "GG_STATE_RESOLVING\n");
			break;
		case GG_STATE_RESOLVING_GG:
			purple_debug_info("gg", "GG_STATE_RESOLVING_GG\n");
			break;
		case GG_STATE_CONNECTING_HUB:
			purple_debug_info("gg", "GG_STATE_CONNECTING_HUB\n");
			break;
		case GG_STATE_READING_DATA:
			purple_debug_info("gg", "GG_STATE_READING_DATA\n");
			break;
		case GG_STATE_CONNECTING_GG:
			purple_debug_info("gg", "GG_STATE_CONNECTING_GG\n");
			break;
		case GG_STATE_READING_KEY:
			purple_debug_info("gg", "GG_STATE_READING_KEY\n");
			break;
		case GG_STATE_READING_REPLY:
			purple_debug_info("gg", "GG_STATE_READING_REPLY\n");
			break;
		case GG_STATE_TLS_NEGOTIATION:
			purple_debug_info("gg", "GG_STATE_TLS_NEGOTIATION\n");
			break;
#if GGP_ENABLE_GG11
		case GG_STATE_RESOLVING_HUB:
			purple_debug_info("gg", "GG_STATE_RESOLVING_HUB\n");
			break;
		case GG_STATE_READING_HUB:
			purple_debug_info("gg", "GG_STATE_READING_HUB\n");
			break;
#endif
		default:
			purple_debug_error("gg", "unknown state = %d\n",
					 info->session->state);
		break;
	}

	if (!(ev = gg_watch_fd(info->session))) {
		purple_debug_error("gg", "login_handler: gg_watch_fd failed!\n");
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to read from socket"));
		return;
	}
	purple_debug_info("gg", "login_handler: session->fd = %d\n", info->session->fd);
	purple_debug_info("gg", "login_handler: session: check = %d; state = %d;\n",
			info->session->check, info->session->state);

	purple_input_remove(info->inpa);
	info->inpa = 0;

	/** XXX I think that this shouldn't be done if ev->type is GG_EVENT_CONN_FAILED or GG_EVENT_CONN_SUCCESS -datallah */
	if (info->session->fd >= 0)
		info->inpa = purple_input_add(info->session->fd,
			(info->session->check == 1) ? PURPLE_INPUT_WRITE :
				PURPLE_INPUT_READ,
			ggp_async_login_handler, gc);

	switch (ev->type) {
		case GG_EVENT_NONE:
			/* Nothing happened. */
			purple_debug_info("gg", "GG_EVENT_NONE\n");
			break;
		case GG_EVENT_CONN_SUCCESS:
			{
#if GGP_ENABLE_GG11
				purple_debug_info("gg", "GG_EVENT_CONN_SUCCESS:"
					" successfully connected to %s\n",
					info->session->connect_host);
				ggp_servconn_add_server(info->session->
					connect_host);
#endif
				purple_input_remove(info->inpa);
				info->inpa = purple_input_add(info->session->fd,
							  PURPLE_INPUT_READ,
							  ggp_callback_recv, gc);

				purple_connection_update_progress(gc, _("Connected"), 1, 2);
				purple_connection_set_state(gc, PURPLE_CONNECTION_CONNECTED);

				ggp_buddylist_send(gc);
				ggp_roster_request_update(gc);
			}
			break;
		case GG_EVENT_CONN_FAILED:
			if (info->inpa > 0)
			{
				purple_input_remove(info->inpa);
				info->inpa = 0;
			}
			purple_debug_info("gg", "Connection failure: %d\n",
				ev->event.failure);
			switch (ev->event.failure) {
				case GG_FAILURE_RESOLVING:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Unable to resolve "
						"hostname"));
					break;
				case GG_FAILURE_PASSWORD:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
						_("Incorrect password"));
					break;
				case GG_FAILURE_TLS:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR,
						_("SSL Connection Failed"));
					break;
				case GG_FAILURE_INTRUDER:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
						_("Your account has been "
						"disabled because too many "
						"incorrect passwords were "
						"entered"));
					break;
				case GG_FAILURE_UNAVAILABLE:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Service temporarily "
						"unavailable"));
					break;
				case GG_FAILURE_PROXY:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Error connecting to proxy "
						"server"));
					break;
				case GG_FAILURE_HUB:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Error connecting to master "
						"server"));
					break;
#if GGP_ENABLE_GG11
				case GG_FAILURE_INTERNAL:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_OTHER_ERROR,
						_("Internal error"));
					break;
#endif
				default:
					purple_connection_error(gc,
						PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
						_("Connection failed"));
			}
			break;
		case GG_EVENT_MSG:
			if (ev->event.msg.sender == 0)
			{
				if (ev->event.msg.message == NULL)
					break;

				/* system messages are mostly ads */
				purple_debug_info("gg", "System message:\n%s\n",
					ev->event.msg.message);
			}
			else
				purple_debug_warning("gg", "GG_EVENT_MSG: message from user %u "
					"unexpected while connecting:\n%s\n",
					ev->event.msg.sender,
					ev->event.msg.message);
			break;
		default:
			purple_debug_error("gg", "strange event: %d\n", ev->type);
			break;
	}

	gg_free_event(ev);
}

/* ---------------------------------------------------------------------- */
/* ----- PurpleProtocol ----------------------------------------- */
/* ---------------------------------------------------------------------- */

static const char *ggp_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "gadu-gadu";
}

static const char *ggp_normalize(const PurpleAccount *account, const char *who)
{
	static char normalized[21]; /* maximum unsigned long long int size */

	uin_t uin = ggp_str_to_uin(who);
	if (uin <= 0)
		return NULL;

	g_snprintf(normalized, sizeof(normalized), "%u", uin);

	return normalized;
}

/* TODO:
 * - move to status.c ?
 * - add information about not adding to his buddy list (not_a_friend)
 */
static void ggp_tooltip_text(PurpleBuddy *b, PurpleNotifyUserInfo *user_info, gboolean full)
{
	PurpleStatus *status;
	char *tmp;
	const char *name, *alias;
	gchar *msg;

	g_return_if_fail(b != NULL);

	status = purple_presence_get_active_status(purple_buddy_get_presence(b));
	name = purple_status_get_name(status);
	alias = purple_buddy_get_alias(b);
	ggp_status_from_purplestatus(status, &msg);

	purple_notify_user_info_add_pair_plaintext(user_info, _("Alias"), alias);

	if (msg != NULL) {
		if (PURPLE_BUDDY_IS_ONLINE(b)) {
			tmp = g_strdup_printf("%s: %s", name, msg);
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), tmp);
			g_free(tmp);
		} else {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Message"), msg);
		}
		g_free(msg);
	/* We don't want to duplicate 'Status: Offline'. */
	} else if (PURPLE_BUDDY_IS_ONLINE(b)) {
		purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), name);
	}
}

static void ggp_login(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	struct gg_login_params *glp;
	GGPInfo *info;
#if GGP_ENABLE_GG11
	const char *address;
#endif
	const gchar *encryption_type, *protocol_version;

	if (!ggp_deprecated_setup_proxy(gc))
		return;

	purple_connection_set_flags(gc, PURPLE_CONNECTION_FLAG_HTML |
			PURPLE_CONNECTION_FLAG_NO_URLDESC);

	glp = g_new0(struct gg_login_params, 1);
#if GGP_ENABLE_GG11
	glp->struct_size = sizeof(struct gg_login_params);
#endif
	info = g_new0(GGPInfo, 1);

	purple_connection_set_protocol_data(gc, info);

	ggp_tcpsocket_setup(gc, glp);
	ggp_image_setup(gc);
	ggp_avatar_setup(gc);
	ggp_roster_setup(gc);
	ggp_multilogon_setup(gc);
	ggp_status_setup(gc);
	ggp_chat_setup(gc);
	ggp_message_setup(gc);
	ggp_edisc_setup(gc);

	glp->uin = ggp_str_to_uin(purple_account_get_username(account));
	glp->password =
		ggp_convert_to_cp1250(purple_connection_get_password(gc));

	if (glp->uin == 0) {
		purple_connection_error(gc,
			PURPLE_CONNECTION_ERROR_INVALID_USERNAME,
			_("The username specified is invalid."));
		purple_str_wipe(glp->password);
		g_free(glp);
		return;
	}

	glp->image_size = 255;
	glp->status_flags = GG_STATUS_FLAG_UNKNOWN;

	if (purple_account_get_bool(account, "show_links_from_strangers", 1))
		glp->status_flags |= GG_STATUS_FLAG_SPAM;

	glp->encoding = GG_ENCODING_UTF8;
	glp->protocol_features = (GG_FEATURE_DND_FFC |
		GG_FEATURE_TYPING_NOTIFICATION | GG_FEATURE_MULTILOGON |
		GG_FEATURE_USER_DATA);

	glp->async = 1;

	encryption_type = purple_account_get_string(account, "encryption",
		"opportunistic_tls");
	purple_debug_info("gg", "Requested encryption type: %s\n",
		encryption_type);
	if (strcmp(encryption_type, "opportunistic_tls") == 0)
		glp->tls = GG_SSL_ENABLED;
	else if (strcmp(encryption_type, "require_tls") == 0) {
		if (gg_libgadu_check_feature(GG_LIBGADU_FEATURE_SSL))
			glp->tls = GG_SSL_REQUIRED;
		else {
			purple_connection_error(gc,
				PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
				_("SSL support unavailable"));
			purple_str_wipe(glp->password);
			g_free(glp);
			return;
		}
	}
	else /* encryption_type == "none" */
		glp->tls = GG_SSL_DISABLED;
	purple_debug_misc("gg", "TLS mode: %d\n", glp->tls);

	protocol_version = purple_account_get_string(account,
		"protocol_version", "default");
	purple_debug_info("gg", "Requested protocol version: %s\n",
		protocol_version);
#if GGP_ENABLE_GG11
	if (g_strcmp0(protocol_version, "gg10") == 0)
		glp->protocol_version = GG_PROTOCOL_VERSION_100;
	else if (g_strcmp0(protocol_version, "gg11") == 0)
		glp->protocol_version = GG_PROTOCOL_VERSION_110;
	glp->compatibility = GG_COMPAT_1_12_0;
#else
	glp->protocol_version = 0x2e;
#endif

	ggp_status_set_initial(gc, glp);

#if GGP_ENABLE_GG11
	address = purple_account_get_string(account, "gg_server", "");
	if (address && *address)
		glp->connect_host = g_strdup(address);
#endif

	info->session = gg_login(glp);
#if GGP_ENABLE_GG11
	g_free(glp->connect_host);
#endif
	purple_str_wipe(glp->password);
	g_free(glp);

	purple_connection_update_progress(gc, _("Connecting"), 0, 2);
	if (info->session == NULL) {
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Connection failed"));
		return;
	}

	if (info->session->fd > 0) {
		info->inpa = purple_input_add(info->session->fd,
			PURPLE_INPUT_READ, ggp_async_login_handler, gc);
	}
}

static void ggp_close(PurpleConnection *gc)
{
	PurpleAccount *account;
	GGPInfo *info;;

	g_return_if_fail(gc != NULL);

	account = purple_connection_get_account(gc);
	info = purple_connection_get_protocol_data(gc);

	purple_notify_close_with_handle(gc);

	if (info) {
		if (info->session != NULL)
		{
			ggp_status_set_disconnected(account);
			gg_logoff(info->session);
			gg_free_session(info->session);
		}

		ggp_image_cleanup(gc);
		ggp_avatar_cleanup(gc);
		ggp_roster_cleanup(gc);
		ggp_multilogon_cleanup(gc);
		ggp_status_cleanup(gc);
		ggp_chat_cleanup(gc);
		ggp_message_cleanup(gc);
		ggp_edisc_cleanup(gc);

		if (info->inpa > 0)
			purple_input_remove(info->inpa);
		g_free(info->imtoken);

		purple_connection_set_protocol_data(gc, NULL);
		g_free(info);
	}

	purple_debug_info("gg", "Connection closed.\n");
}

static unsigned int ggp_send_typing(PurpleConnection *gc, const char *name, PurpleIMTypingState state)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	int dummy_length; /* we don't send real length of typed message */

	if (state == PURPLE_IM_TYPED) /* not supported */
		return 1;

	if (state == PURPLE_IM_TYPING)
		dummy_length = (int)g_random_int();
	else /* PURPLE_IM_NOT_TYPING */
		dummy_length = 0;

	gg_typing_notification(
		info->session,
		ggp_str_to_uin(name),
		dummy_length);

	return 1; /* wait 1 second before another notification */
}

static void ggp_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group, const char *message)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	GGPInfo *info = purple_connection_get_protocol_data(gc);
	const gchar *name = purple_buddy_get_name(buddy);

	gg_add_notify(info->session, ggp_str_to_uin(name));

	/* gg server won't tell us our status here */
	if (strcmp(purple_account_get_username(account), name) == 0)
		ggp_status_fake_to_self(gc);

	ggp_roster_add_buddy(gc, buddy, group, message);
	ggp_pubdir_request_buddy_alias(gc, buddy);
}

static void ggp_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
						 PurpleGroup *group)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);

	gg_remove_notify(info->session, ggp_str_to_uin(purple_buddy_get_name(buddy)));
	ggp_roster_remove_buddy(gc, buddy, group);
}

static void ggp_keepalive(PurpleConnection *gc)
{
	GGPInfo *info = purple_connection_get_protocol_data(gc);

	/* purple_debug_info("gg", "Keeping connection alive....\n"); */

	if (gg_ping(info->session) < 0) {
		purple_debug_info("gg", "Not connected to the server "
				"or gg_session is not correct\n");
		purple_connection_error (gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Not connected to the server"));
	}
}

static void ggp_action_multilogon(PurpleProtocolAction *action)
{
	ggp_multilogon_dialog(action->connection);
}

static void ggp_action_status_broadcasting(PurpleProtocolAction *action)
{
	ggp_status_broadcasting_dialog(action->connection);
}

static void ggp_action_search(PurpleProtocolAction *action)
{
	ggp_pubdir_search(action->connection, NULL);
}

static void ggp_action_set_info(PurpleProtocolAction *action)
{
	ggp_pubdir_set_info(action->connection);
}

static GList *ggp_get_actions(PurpleConnection *gc)
{
	GList *m = NULL;
	PurpleProtocolAction *act;

	act = purple_protocol_action_new(_("Show other sessions"),
		ggp_action_multilogon);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Show status only for buddies"),
		ggp_action_status_broadcasting);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);

	act = purple_protocol_action_new(_("Find buddies..."),
		ggp_action_search);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Set User Info"),
		ggp_action_set_info);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);

	act = purple_protocol_action_new(_("Save buddylist to file..."),
				     ggp_action_buddylist_save);
	m = g_list_append(m, act);

	act = purple_protocol_action_new(_("Load buddylist from file..."),
				     ggp_action_buddylist_load);
	m = g_list_append(m, act);

	return m;
}

static const char* ggp_list_emblem(PurpleBuddy *buddy)
{
	ggp_buddy_data *buddy_data = ggp_buddy_get_data(buddy);

	if (buddy_data->blocked)
		return "not-authorized";
	if (buddy_data->not_a_friend)
		return "unavailable";

	return NULL;
}

static gboolean ggp_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

static GHashTable * ggp_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("GG number..."));
	return table;
}

static gssize
ggp_get_max_message_size(PurpleConversation *conv)
{
	/* TODO: it may depend on protocol version or other factors */
	return 1200; /* no more than 1232 */
}

static void
ggp_protocol_init(PurpleProtocol *protocol)
{
	PurpleAccountOption *option;
	GList *encryption_options = NULL;
	GList *protocol_version = NULL;

	protocol->id        = "prpl-gg";
	protocol->name      = "Gadu-Gadu";
	protocol->options   = OPT_PROTO_IM_IMAGE;
	protocol->icon_spec = purple_buddy_icon_spec_new("png",
	                                                 1, 1, 200, 200, 0,
	                                                 PURPLE_ICON_SCALE_DISPLAY |
	                                                 PURPLE_ICON_SCALE_SEND);

	option = purple_account_option_string_new(_("GG server"),
			"gg_server", "");
	protocol->account_options = g_list_append(protocol->account_options,
			option);
	ggp_server_option = option;

#define ADD_VALUE(list, desc, v) { \
	PurpleKeyValuePair *kvp = g_new0(PurpleKeyValuePair, 1); \
	kvp->key = g_strdup((desc)); \
	kvp->value = g_strdup((v)); \
	list = g_list_append(list, kvp); \
}

	ADD_VALUE(encryption_options, _("Use encryption if available"),
		"opportunistic_tls");
	ADD_VALUE(encryption_options, _("Require encryption"), "require_tls");
	ADD_VALUE(encryption_options, _("Don't use encryption"), "none");

	option = purple_account_option_list_new(_("Connection security"),
		"encryption", encryption_options);
	protocol->account_options = g_list_append(protocol->account_options,
		option);

	ADD_VALUE(protocol_version, _("Default"), "default");
	ADD_VALUE(protocol_version, "GG 10", "gg10");
	ADD_VALUE(protocol_version, "GG 11", "gg11");

	option = purple_account_option_list_new(_("Protocol version"),
		"protocol_version", protocol_version);
	protocol->account_options = g_list_append(protocol->account_options,
		option);

	option = purple_account_option_bool_new(_("Show links from strangers"),
		"show_links_from_strangers", 1);
	protocol->account_options = g_list_append(protocol->account_options,
		option);
}

static void
ggp_protocol_class_init(PurpleProtocolClass *klass)
{
	klass->login        = ggp_login;
	klass->close        = ggp_close;
	klass->status_types = ggp_status_types;
	klass->list_icon    = ggp_list_icon;
}

static void
ggp_protocol_client_iface_init(PurpleProtocolClientIface *client_iface)
{
	client_iface->get_actions            = ggp_get_actions;
	client_iface->list_emblem            = ggp_list_emblem;
	client_iface->status_text            = ggp_status_buddy_text;
	client_iface->tooltip_text           = ggp_tooltip_text;
	client_iface->buddy_free             = ggp_buddy_free;
	client_iface->normalize              = ggp_normalize;
	client_iface->offline_message        = ggp_offline_message;
	client_iface->get_account_text_table = ggp_get_account_text_table;
	client_iface->get_max_message_size   = ggp_get_max_message_size;
}

static void
ggp_protocol_server_iface_init(PurpleProtocolServerIface *server_iface)
{
	server_iface->get_info       = ggp_pubdir_get_info_protocol;
	server_iface->set_status     = ggp_status_set_purplestatus;
	server_iface->add_buddy      = ggp_add_buddy;
	server_iface->remove_buddy   = ggp_remove_buddy;
	server_iface->keepalive      = ggp_keepalive;
	server_iface->alias_buddy    = ggp_roster_alias_buddy;
	server_iface->group_buddy    = ggp_roster_group_buddy;
	server_iface->rename_group   = ggp_roster_rename_group;
	server_iface->set_buddy_icon = ggp_avatar_own_set;
}

static void
ggp_protocol_im_iface_init(PurpleProtocolIMIface *im_iface)
{
	im_iface->send        = ggp_message_send_im;
	im_iface->send_typing = ggp_send_typing;
}

#if GGP_ENABLE_GG11
static void
ggp_protocol_chat_iface_init(PurpleProtocolChatIface *chat_iface)
{
	chat_iface->info          = ggp_chat_info;
	chat_iface->info_defaults = ggp_chat_info_defaults;
	chat_iface->join          = ggp_chat_join;
	chat_iface->get_name      = ggp_chat_get_name;
	chat_iface->invite        = ggp_chat_invite;
	chat_iface->leave         = ggp_chat_leave;
	chat_iface->send          = ggp_chat_send;

	chat_iface->reject        = NULL; /* TODO */
}

static void
ggp_protocol_roomlist_iface_init(PurpleProtocolRoomlistIface *roomlist_iface)
{
	roomlist_iface->get_list = ggp_chat_roomlist_get_list;
}
#endif

static void
ggp_protocol_privacy_iface_init(PurpleProtocolPrivacyIface *privacy_iface)
{
	privacy_iface->add_deny = ggp_add_deny;
	privacy_iface->rem_deny = ggp_rem_deny;
}

static void
ggp_protocol_xfer_iface_init(PurpleProtocolXferIface *xfer_iface)
{
	xfer_iface->can_receive = ggp_edisc_xfer_can_receive_file;
	xfer_iface->send        = ggp_edisc_xfer_send_file;
	xfer_iface->new_xfer    = ggp_edisc_xfer_send_new;
}

PURPLE_DEFINE_TYPE_EXTENDED(
	GGPProtocol, ggp_protocol, PURPLE_TYPE_PROTOCOL, 0,

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CLIENT_IFACE,
	                                  ggp_protocol_client_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_SERVER_IFACE,
	                                  ggp_protocol_server_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_IM_IFACE,
	                                  ggp_protocol_im_iface_init)
#if GGP_ENABLE_GG11
	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_CHAT_IFACE,
	                                  ggp_protocol_chat_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_ROOMLIST_IFACE,
	                                  ggp_protocol_roomlist_iface_init)
#endif
	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_PRIVACY_IFACE,
	                                  ggp_protocol_privacy_iface_init)

	PURPLE_IMPLEMENT_INTERFACE_STATIC(PURPLE_TYPE_PROTOCOL_XFER_IFACE,
	                                  ggp_protocol_xfer_iface_init)
);

static gchar *
plugin_extra(PurplePlugin *plugin)
{
	return g_strdup_printf("Using libgadu version %s", gg_libgadu_version());
}

static PurplePluginInfo *
plugin_query(GError **error)
{
	const gchar * const authors[] = {
		"boler@sourceforge.net",
		NULL
	};

	return purple_plugin_info_new(
		"id",           "prpl-gg",
		"name",         "Gadu-Gadu Protocol",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("Gadu-Gadu Protocol Plugin"),
		"description",  N_("Polish popular IM"),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"extra-cb",     plugin_extra,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	ggp_protocol_register_type(plugin);

	my_protocol = purple_protocols_add(GGP_TYPE_PROTOCOL, error);
	if (!my_protocol)
		return FALSE;

	purple_prefs_add_none("/plugins/prpl/gg");

	purple_debug_info("gg", "Loading Gadu-Gadu protocol plugin with "
		"libgadu %s...\n", gg_libgadu_version());

	ggp_libgaduw_setup();
	ggp_resolver_purple_setup();
	ggp_servconn_setup(ggp_server_option);
	ggp_html_setup();
	ggp_message_setup_global();

	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	ggp_servconn_cleanup();
	ggp_html_cleanup();
	ggp_message_cleanup_global();
	ggp_libgaduw_cleanup();

	if (!purple_protocols_remove(my_protocol, error))
		return FALSE;

	return TRUE;
}

PURPLE_PLUGIN_INIT(gg, plugin_query, plugin_load, plugin_unload);

/* vim: set ts=8 sts=0 sw=8 noet: */
