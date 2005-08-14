/*
 * gaim - Napster Protocol Plugin
 *
 * Copyright (C) 2000-2001, Rob Flynn <rob@marko.net>
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

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "proxy.h"
#include "util.h"
#include "version.h"

#define NAP_SERVER "64.124.41.187"
#define NAP_PORT 8888

#define NAPSTER_CONNECT_STEPS 2

GSList *nap_connections = NULL;

struct nap_data {
	int fd;
	gchar *email;
};

static GaimConversation *nap_find_chat(GaimConnection *gc, const char *name)
{
	GSList *bcs = gc->buddy_chats;

	while (bcs) {
		GaimConversation *b = bcs->data;
		if (!gaim_utf8_strcasecmp(b->name, name))
			return b;
		bcs = bcs->next;
	}

	return NULL;
}

static void nap_write_packet(GaimConnection *gc, unsigned short command, const char *format, ...)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	va_list ap;
	gchar *message;
	unsigned short size;

	va_start(ap, format);
	message = g_strdup_vprintf(format, ap);
	va_end(ap);

	size = strlen(message);
	gaim_debug(GAIM_DEBUG_MISC, "napster", "S %3hd: %s\n", command, message);

	write(ndata->fd, &size, 2);
	write(ndata->fd, &command, 2);
	write(ndata->fd, message, size);

	g_free(message);
}

static int nap_do_irc_style(GaimConnection *gc, const char *message, const char *name)
{
	gchar **res;

        gaim_debug(GAIM_DEBUG_MISC, "napster", "C %s\n", message);

	res = g_strsplit(message, " ", 2);

	if (!g_ascii_strcasecmp(res[0], "/ME")) { /* MSG_CLIENT_PUBLIC */
		nap_write_packet(gc, 824, "%s \"%s\"", name, res[1]);

	} else if (!g_ascii_strcasecmp(res[0], "/MSG")) { /* MSG_CLIENT_PUBLIC */
		nap_write_packet(gc, 205, "%s", res[1]);

	} else if (!g_ascii_strcasecmp(res[0], "/JOIN")) { /* join chatroom MSG_CLIENT_JOIN */
		if (!res[1]) {
			g_strfreev(res);
			return 1;
		}
		if (res[1][0] != '#')
			nap_write_packet(gc, 400, "#%s", res[1]);
		else
			nap_write_packet(gc, 400, "%s", res[1]);

	} else if (!g_ascii_strcasecmp(res[0], "/PART")) { /* partchatroom MSG_CLIENT_PART */
		nap_write_packet(gc, 401, "%s", res[1] ? res[1] : name);

	} else if (!g_ascii_strcasecmp(res[0], "/TOPIC")) { /* set topic MSG_SERVER_TOPIC */
		nap_write_packet(gc, 410, "%s", res[1] ? res[1] : name);

	} else if (!g_ascii_strcasecmp(res[0], "/WHOIS")) { /* whois request MSG_CLIENT_WHOIS */
		nap_write_packet(gc, 603, "%s", res[1]);

	} else if (!g_ascii_strcasecmp(res[0], "/PING")) { /* send ping MSG_CLIENT_PING */
		nap_write_packet(gc, 751, "%s", res[1]);

	} else if (!g_ascii_strcasecmp(res[0], "/KICK")) { /* kick asswipe MSG_CLIENT_KICK */
		nap_write_packet(gc, 829, "%s", res[1]);

	} else {
		g_strfreev(res);
		return 1;
	}

	g_strfreev(res);
	return 0;
}

/* 205 - MSG_CLIENT_PRIVMSG */
static int nap_send_im(GaimConnection *gc, const char *who, const char *message, GaimConvImFlags flags)
{

	if ((strlen(message) < 2) || (message[0] != '/' ) || (message[1] == '/')) {
		/* Actually send a chat message */
		nap_write_packet(gc, 205, "%s %s", who, message);
	} else {
		/* user typed an IRC-style command */
		nap_do_irc_style(gc, message, who);
	}

	return 1;
}

/* 207 - MSG_CLIENT_ADD_HOTLIST */
static void nap_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	nap_write_packet(gc, 207, "%s", buddy->name);
}

/* 208 - MSG_CLIENT_ADD_HOTLIST_SEQ */
static void nap_send_buddylist(GaimConnection *gc)
{
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;

	if ((blist = gaim_get_blist()) != NULL)
	{
		for (gnode = blist->root; gnode != NULL; gnode = gnode->next)
		{
			if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for (cnode = gnode->child; cnode != NULL; cnode = cnode->next)
			{
				if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
					continue;
				for (bnode = cnode->child; bnode != NULL; bnode = bnode->next)
				{
					if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;
					buddy = (GaimBuddy *)bnode;
					nap_write_packet(gc, 208, "%s", buddy->name);
				}
			}
		}
	}
}

/* 303 - MSG_CLIENT_REMOVE_HOTLIST */
static void nap_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	nap_write_packet(gc, 303, "%s", buddy->name);
}

static char *nap_get_chat_name(GHashTable *data) {
	char *name = g_hash_table_lookup(data, "group");

	/* Make sure the name has a # preceding it */
	if (name[0] != '#') {
		return g_strdup_printf("#%s", name);
	}

	return g_strdup(name);
}

/* 400 - MSG_CLIENT_JOIN */
static void nap_join_chat(GaimConnection *gc, GHashTable *data)
{
	char *name;

	if (!data)
		return;

	name = nap_get_chat_name(data);

	if (name) {
		nap_write_packet(gc, 400, "%s", name);
		g_free(name);
	}
}

/* 401 - MSG_CLIENT_PART */
static void nap_chat_leave(GaimConnection *gc, int id)
{
	GaimConversation *c = gaim_find_chat(gc, id);

	if (!c)
		return;

	nap_write_packet(gc, 401, "%s", c->name);
}

/* 402 - MSG_CLIENT_PUBLIC */
static int nap_chat_send(GaimConnection *gc, int id, const char *message)
{
	GaimConversation *c = gaim_find_chat(gc, id);

	if (!c)
		return -EINVAL;

	if ((strlen(message) < 2) || (message[0] != '/' ) || (message[1] == '/')) {
		/* Actually send a chat message */
		nap_write_packet(gc, 402, "%s %s", c->name, message);
	} else {
		/* user typed an IRC-style command */
		nap_do_irc_style(gc, message, c->name);
	}

	return 0;
}

/* 603 - MSG_CLIENT_WHOIS */
static void nap_get_info(GaimConnection *gc, const char *who)
{
	nap_write_packet(gc, 603, "%s", who);
}

static void nap_callback(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	struct nap_data *ndata = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimConversation *c;
	gchar *buf, *buf2, *buf3, **res;
	unsigned short header[2];
	int len;
	int command;
	int i;

	if (read(source, (void*)header, 4) != 4) {
		gaim_connection_error(gc, _("Unable to read header from server"));
		return;
	}

	len = header[0];
	command = header[1];
	buf = (gchar *)g_malloc((len + 1) * sizeof(gchar));
	buf[len] = '\0';

	i = 0;
	do {
		int tmp = read(source, buf + i, len - i);
		if (tmp <= 0) {
			g_free(buf);
			buf = g_strdup_printf(_("Unable to read message from server: %s.  Command is %hd, length is %hd."), strerror(errno), len, command); 
			gaim_connection_error(gc, buf);
			g_free(buf);
			return;
		}
		i += tmp;
	} while (i != len);

	gaim_debug(GAIM_DEBUG_MISC, "napster", "R %3hd: %s\n", command, buf);

	switch (command) {
	case 000: /* MSG_SERVER_ERROR */
		gaim_notify_error(gc, NULL, buf, NULL);
		gaim_input_remove(gc->inpa);
		gc->inpa = 0;
		close(source);
		gaim_connection_error(gc, _("Unknown server error."));
		break;

	case 003: /* MSG_SERVER_EMAIL */
		gaim_debug(GAIM_DEBUG_MISC, "napster", "Registered with e-mail address: %s\n", buf);
		ndata->email = g_strdup(buf);

		/* Our signon is complete */
		gaim_connection_set_state(gc, GAIM_CONNECTED);

		/* Send the server our buddy list */
		nap_send_buddylist(gc);

		break;

	case 201: /* MSG_SERVER_SEARCH_RESULT */
		res = g_strsplit(buf, " ", 0);
		gaim_prpl_got_user_status(account, res[0], "online", NULL);
		g_strfreev(res);
		break;

	case 202: /* MSG_SERVER_SEARCH_END */
		gaim_prpl_got_user_status(account, buf, "offline", NULL);
		break;

	case 205: /* MSG_CLIENT_PRIVMSG */
		res = g_strsplit(buf, " ", 2);
		buf2 = g_markup_escape_text(res[1], -1);
		serv_got_im(gc, res[0], buf2, 0, time(NULL));
		g_free(buf2);
		g_strfreev(res);
		break;

	case 209: /* MSG_SERVER_USER_SIGNON */
		/* USERNAME SPEED */
		res = g_strsplit(buf, " ", 2);
		gaim_prpl_got_user_status(account, res[0], "online", NULL);
		g_strfreev(res);
		break;

	case 210: /* MSG_SERVER_USER_SIGNOFF */
		/* USERNAME SPEED */
		res = g_strsplit(buf, " ", 2);
		gaim_prpl_got_user_status(account, res[0], "offline", NULL);
		g_strfreev(res);
		break;

	case 214: /* MSG_SERVER_STATS */
		res = g_strsplit(buf, " ", 3);
		buf2 = g_strdup_printf(_("users: %s, files: %s, size: %sGB"), res[0], res[1], res[2]);
		serv_got_im(gc, "server", buf2, 0, time(NULL));
		g_free(buf2);
		g_strfreev(res);
		break;

	case 301: /* MSG_SERVER_HOTLIST_ACK */
		/* Our buddy was added successfully */
		break;

	case 302: /* MSG_SERVER_HOTLIST_ERROR */
		buf2 = g_strdup_printf(_("Unable to add \"%s\" to your Napster hotlist"), buf);
		gaim_notify_error(gc, NULL, buf2, NULL);
		g_free(buf2);
		break;

	case 316: /* MSG_SERVER_DISCONNECTING */
		/* we have been kicked off =^( */
		gaim_connection_error(gc, _("You were disconnected from the server."));
		break;

	case 401: /* MSG_CLIENT_PART */
		c = nap_find_chat(gc, buf);
		if (c)
			serv_got_chat_left(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(c)));
		break;

	case 403: /* MSG_SERVER_PUBLIC */
		res = g_strsplit(buf, " ", 3);
		c = nap_find_chat(gc, res[0]);
		if (c)
			serv_got_chat_in(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(c)), res[1], 0, res[2], time((time_t)NULL));
		g_strfreev(res);
		break;

	case 404: /* MSG_SERVER_NOSUCH */
		/* abused by opennap servers to broadcast stuff */
		buf2 = g_markup_escape_text(buf, -1);
		serv_got_im(gc, "server", buf2, 0, time(NULL));
		g_free(buf2);
		break;

	case 405: /* MSG_SERVER_JOIN_ACK */
		c = nap_find_chat(gc, buf);
		if (!c)
			serv_got_joined_chat(gc, gaim_conv_chat_get_id(GAIM_CONV_CHAT(c)), buf);
		break;

	case 407: /* MSG_SERVER_PART */
		res = g_strsplit(buf, " ", 0);
		c = nap_find_chat(gc, res[0]);
		gaim_conv_chat_remove_user(GAIM_CONV_CHAT(c), res[1], NULL);
		g_strfreev(res);
		break;

	case 406: /* MSG_SERVER_JOIN */
	case 408: /* MSG_SERVER_CHANNEL_USER_LIST */
		res = g_strsplit(buf, " ", 4);
		c = nap_find_chat(gc, res[0]);
		gaim_conv_chat_add_user(GAIM_CONV_CHAT(c), res[1], NULL, GAIM_CBFLAGS_NONE, TRUE);
		g_strfreev(res);
		break;

	case 409: /* MSG_SERVER_CHANNEL_USER_LIST_END */
		break;

	case 410: /* MSG_SERVER_TOPIC */
		/* display the topic in the channel */
		res = g_strsplit(buf, " ", 2);
		c = nap_find_chat(gc, res[0]);
		gaim_conv_chat_set_topic(GAIM_CONV_CHAT(c), res[0], res[1]);
		g_strfreev(res);
		break;

	case 603: /* MSG_CLIENT_WHOIS */
		buf2 = g_strdup_printf(_("%s requested your information"), buf);
		serv_got_im(gc, "server", buf2, 0, time(NULL));
		g_free(buf2);
		break;

	case 604: /* MSG_SERVER_WHOIS_RESPONSE */
		/* XXX - Format is:   "Elite" 37 " " "Active" 0 0 0 0 "gaim 0.63cvs" 0 0 192.168.1.41 32798 0 unknown flounder */
		res = g_strsplit(buf, " ", 2);
		/* res[0] == username */
		gaim_notify_userinfo(gc, res[0], NULL, _("Buddy Information"), NULL,
							  res[1], NULL, NULL);
		g_strfreev(res);
		break;

	case 621:
	case 622: /* MSG_CLIENT_MOTD */
		/* also replaces MSG_SERVER_MOTD, so we should display it */
		buf2 = g_markup_escape_text(buf, -1);
		serv_got_im(gc, "motd", buf2, 0, time(NULL));
		g_free(buf2);
		break;

	case 627: /* MSG_CLIENT_WALLOP */
		/* abused by opennap server maintainers to broadcast stuff */
		buf2 = g_markup_escape_text(buf, -1);
		serv_got_im(gc, "wallop", buf2, 0, time(NULL));
		g_free(buf2);
		break;

	case 628: /* MSG_CLIENT_ANNOUNCE */
		buf2 = g_markup_escape_text(buf, -1);
		serv_got_im(gc, "announce", buf2, 0, time(NULL));
		g_free(buf);
		break;

	case 748: /* MSG_SERVER_GHOST */
		/* Looks like someone logged in as us! =-O */
		gaim_connection_error(gc, _("You have signed on from another location."));
		break;

	case 751: /* MSG_CLIENT_PING */
		buf2 = g_strdup_printf(_("%s requested a PING"), buf);
		serv_got_im(gc, "server", buf2, 0, time(NULL));
		g_free(buf2);
		/* send back a pong */
		/* MSG_CLIENT_PONG */
		nap_write_packet(gc, 752, "%s", buf);
		break;

	case 752: /* MSG_CLIENT_PONG */
		buf2 = g_strdup_printf("Received pong from %s", buf);
		gaim_notify_info(gc, NULL, buf2, NULL);
		g_free(buf2);
		break;

	case 824: /* MSG_CLIENT_EMOTE */
		res = g_strsplit(buf, " ", 3);
		buf2 = g_strndup(res[2]+1, strlen(res[2]) - 2); /* chomp off the surround quotes */
		buf3 = g_strdup_printf("/me %s", buf2);
		g_free(buf2);
		if ((c = nap_find_chat(gc, res[0]))) {
			gaim_conv_chat_write(GAIM_CONV_CHAT(c), res[1], buf3, GAIM_MESSAGE_NICK, time(NULL));
		}
		g_free(buf3);
		g_strfreev(res);
		break;

	default:
	        gaim_debug(GAIM_DEBUG_MISC, "napster", "Unknown packet %hd: %s\n", command, buf);
		break;
	}

	g_free(buf);
}

/* 002 - MSG_CLIENT_LOGIN */
static void nap_login_connect(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;
	gchar *buf;

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	if (source < 0) {
		gaim_connection_error(gc, _("Unable to connect."));
		return;
	}

	ndata->fd = source;

	/* Update the login progress status display */
	buf = g_strdup_printf("Logging in: %s", gaim_account_get_username(gc->account));
	gaim_connection_update_progress(gc, buf, 1, NAPSTER_CONNECT_STEPS);
	g_free(buf);

	/* Write our signon data */
	nap_write_packet(gc, 2, "%s %s 0 \"gaim %s\" 0",
			gaim_account_get_username(gc->account),
			gaim_connection_get_password(gc), VERSION);

	/* And set up the input watcher */
	gc->inpa = gaim_input_add(ndata->fd, GAIM_INPUT_READ, nap_callback, gc);
}

static void nap_login(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);

	gaim_connection_update_progress(gc, _("Connecting"), 0, NAPSTER_CONNECT_STEPS);

	gc->proto_data = g_new0(struct nap_data, 1);
	if (gaim_proxy_connect(account,
				gaim_account_get_string(account, "server", NAP_SERVER),
				gaim_account_get_int(account, "port", NAP_PORT),
				nap_login_connect, gc) != 0) {
		gaim_connection_error(gc, _("Unable to connect."));
	}
}

static void nap_close(GaimConnection *gc)
{
	struct nap_data *ndata = (struct nap_data *)gc->proto_data;

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	if (!ndata)
		return;

	close(ndata->fd);

	g_free(ndata->email);
	g_free(ndata);
}

static const char* nap_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "napster";
}

static void nap_list_emblems(GaimBuddy *b, const char **se, const char **sw,
							 const char **nw, const char **ne)
{
	if(!GAIM_BUDDY_IS_ONLINE(b))
		*se = "offline";
}

static GList *nap_status_types(GaimAccount *account)
{
	GList *types = NULL;
	GaimStatusType *type;

	g_return_val_if_fail(account != NULL, NULL);

	type = gaim_status_type_new_full(GAIM_STATUS_OFFLINE,
									 "offline",
									 _("Offline"), TRUE, TRUE, FALSE);
	types = g_list_append(types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_ONLINE,
									 "online",
									 _("Online"), TRUE, TRUE, FALSE);
	types = g_list_append(types, type);

	return types;
}

static GList *nap_chat_info(GaimConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Group:");
	pce->identifier = "group";
	m = g_list_append(m, pce);

	return m;
}

GHashTable *nap_chat_info_defaults(GaimConnection *gc, const char *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, "group", g_strdup(chat_name));

	return defaults;
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	nap_list_icon,			/* list_icon */
	nap_list_emblems,		/* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	nap_status_types,		/* status_types */
	NULL,					/* blist_node_menu */
	nap_chat_info,			/* chat_info */
	nap_chat_info_defaults, /* chat_info_defaults */
	nap_login,				/* login */
	nap_close,				/* close */
	nap_send_im,			/* send_im */
	NULL,					/* set_info */
	NULL,					/* send_typing */
	nap_get_info,			/* get_info */
	NULL,					/* set_away */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	nap_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	nap_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,					/* add_deny */
	NULL,					/* rem_permit */
	NULL,					/* rem_deny */
	NULL,					/* set_permit_deny */
	nap_join_chat,			/* join_chat */
	NULL,					/* reject chat invite */
	nap_get_chat_name,		/* get_chat_name */
	NULL,					/* chat_invite */
	nap_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	nap_chat_send,			/* chat_send */
	NULL,					/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	NULL,					/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	NULL					/* send_file */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	0,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-napster",                                   /**< id             */
	"Napster",                                        /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("NAPSTER Protocol Plugin"),
	                                                  /**  description    */
	N_("NAPSTER Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	NULL
};

static void init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Server"), "server",
											NAP_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_int_new(_("Port"), "port", 8888);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(napster, init_plugin, info);
