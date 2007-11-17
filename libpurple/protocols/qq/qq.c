/**
 * @file qq.c
 *
 * purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"

#ifdef _WIN32
#define random rand
#endif

#include "accountopt.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "prpl.h"
#include "request.h"
#include "roomlist.h"
#include "server.h"
#include "util.h"

#include "buddy_info.h"
#include "buddy_opt.h"
#include "buddy_status.h"
#include "char_conv.h"
#include "crypt.h"
#include "group.h"
#include "group_find.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "header_info.h"
#include "im.h"
#include "keep_alive.h"
#include "login_logout.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_proxy.h"
#include "send_core.h"
#include "send_file.h"
#include "utils.h"
#include "version.h"

#define OPENQ_AUTHOR            "Puzzlebird"
#define OPENQ_WEBSITE            "http://openq.sourceforge.net"
#define QQ_TCP_QUERY_PORT       "8000"
#define QQ_UDP_PORT             "8000"

const gchar *udp_server_list[] = {
	"sz.tencent.com",
	"sz2.tencent.com",
	"sz3.tencent.com",
	"sz4.tencent.com",
	"sz5.tencent.com",
	"sz6.tencent.com",
	"sz7.tencent.com",
	"sz8.tencent.com",
	"sz9.tencent.com"
};
const gint udp_server_amount = (sizeof(udp_server_list) / sizeof(udp_server_list[0]));


const gchar *tcp_server_list[] = {
	"tcpconn.tencent.com",
	"tcpconn2.tencent.com",
	"tcpconn3.tencent.com",
	"tcpconn4.tencent.com",
	"tcpconn5.tencent.com",
	"tcpconn6.tencent.com"
};
const gint tcp_server_amount = (sizeof(tcp_server_list) / sizeof(tcp_server_list[0]));

static void _qq_login(PurpleAccount *account)
{
	const gchar *qq_server, *qq_port;
	qq_data *qd;
	PurpleConnection *gc;
	PurplePresence *presence;
	gboolean use_tcp;

	g_return_if_fail(account != NULL);

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_BGCOLOR | PURPLE_CONNECTION_AUTO_RESP;

	qd = g_new0(qq_data, 1);
	qd->gc = gc;
	gc->proto_data = qd;

	qq_server = purple_account_get_string(account, "server", NULL);
	qq_port = purple_account_get_string(account, "port", NULL);
	use_tcp = purple_account_get_bool(account, "use_tcp", FALSE);
	presence = purple_account_get_presence(account);

	qd->use_tcp = use_tcp;

	if(purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		qd->login_mode = QQ_LOGIN_MODE_HIDDEN;
	} else if(purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
				|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)) {
		qd->login_mode = QQ_LOGIN_MODE_AWAY;
	} else {
		qd->login_mode = QQ_LOGIN_MODE_NORMAL;
	}

	if (qq_server == NULL || strlen(qq_server) == 0)
		qq_server = use_tcp ?
		    tcp_server_list[random() % tcp_server_amount] :
		    udp_server_list[random() % udp_server_amount];

	if (qq_port == NULL || strtol(qq_port, NULL, 10) == 0)
		qq_port = use_tcp ? QQ_TCP_QUERY_PORT : QQ_UDP_PORT;

	purple_connection_update_progress(gc, _("Connecting"), 0, QQ_CONNECT_STEPS);

	if (qq_connect(account, qq_server, strtol(qq_port, NULL, 10), use_tcp, FALSE) < 0)
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to connect."));
}

/* directly goes for qq_disconnect */
static void _qq_close(PurpleConnection *gc)
{
	g_return_if_fail(gc != NULL);
	qq_disconnect(gc);
}

/* returns the icon name for a buddy or protocol */
static const gchar *_qq_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "qq";
}


/* a short status text beside buddy icon*/
static gchar *_qq_status_text(PurpleBuddy *b)
{
	qq_buddy *q_bud;
	GString *status;

	q_bud = (qq_buddy *) b->proto_data;
	if (q_bud == NULL)
		return NULL;

	status = g_string_new("");

	switch(q_bud->status) {
	case QQ_BUDDY_OFFLINE:
		g_string_append(status, _("Offline"));
		break;
	case QQ_BUDDY_ONLINE_NORMAL:
		return NULL;
		break;
	/* TODO What does this status mean? Labelling it as offline... */
	case QQ_BUDDY_ONLINE_OFFLINE:
		g_string_append(status, _("Offline"));
		break;
	case QQ_BUDDY_ONLINE_AWAY:
		g_string_append(status, _("Away"));
		break;
	case QQ_BUDDY_ONLINE_INVISIBLE:
		g_string_append(status, _("Invisible"));
		break;
	default:
		g_string_printf(status, _("Unknown-%d"), q_bud->status);
	}

	return g_string_free(status, FALSE);
}


/* a floating text when mouse is on the icon, show connection status here */
static void _qq_tooltip_text(PurpleBuddy *b, PurpleNotifyUserInfo *user_info, gboolean full)
{
	qq_buddy *q_bud;
	gchar *ip_str;
	char *tmp;
	const char *tmp2;

	g_return_if_fail(b != NULL);

	q_bud = (qq_buddy *) b->proto_data;
	g_return_if_fail(q_bud != NULL);

	if (PURPLE_BUDDY_IS_ONLINE(b) && q_bud != NULL)
	{
		ip_str = gen_ip_str(q_bud->ip);
		if (strlen(ip_str) != 0) {
			if (q_bud->comm_flag & QQ_COMM_FLAG_TCP_MODE)
				tmp2 = _("TCP Address");
			else
				tmp2 = _("UDP Address");
			tmp = g_strdup_printf("%s:%d", ip_str, q_bud->port);
			purple_notify_user_info_add_pair(user_info, tmp2, tmp);
			g_free(tmp);
		}
		g_free(ip_str);

		tmp = g_strdup_printf("%d", q_bud->age);
		purple_notify_user_info_add_pair(user_info, _("Age"), tmp);
		g_free(tmp);

		switch (q_bud->gender) {
		case QQ_BUDDY_GENDER_GG:
			purple_notify_user_info_add_pair(user_info, _("Gender"), _("Male"));
			break;
		case QQ_BUDDY_GENDER_MM:
			purple_notify_user_info_add_pair(user_info, _("Gender"), _("Female"));
			break;
		case QQ_BUDDY_GENDER_UNKNOWN:
			purple_notify_user_info_add_pair(user_info, _("Gender"), _("Unknown"));
			break;
		default:
			tmp = g_strdup_printf("Error (%d)", q_bud->gender);
			purple_notify_user_info_add_pair(user_info, _("Gender"), tmp);
			g_free(tmp);
		}

		if (q_bud->level) {
			tmp = g_strdup_printf("%d", q_bud->level);
			purple_notify_user_info_add_pair(user_info, _("Level"), tmp);
			g_free(tmp);
		}
		/* For debugging */
		/*
		g_string_append_printf(tooltip, "\n<b>Flag:</b> %01x", q_bud->flag1);
		g_string_append_printf(tooltip, "\n<b>CommFlag:</b> %01x", q_bud->comm_flag);
		g_string_append_printf(tooltip, "\n<b>Client:</b> %04x", q_bud->client_version);
		*/
	}
}

/* we can show tiny icons on the four corners of buddy icon, */
static const char *_qq_list_emblem(PurpleBuddy *b)
{
	/* each char** are refering to a filename in pixmaps/purple/status/default/ */

	qq_buddy *q_bud = b->proto_data;

	if (q_bud) {
		if (q_bud->comm_flag & QQ_COMM_FLAG_QQ_MEMBER)
			return "qq_member";
		/*
		if (q_bud->comm_flag & QQ_COMM_FLAG_VIDEO)
			return "video";
		*/
	}

	return NULL;
}

/* QQ away status (used to initiate QQ away packet) */
static GList *_qq_away_states(PurpleAccount *ga)
{
	PurpleStatusType *status;
	GList *types = NULL;

	status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE,
			"available", _("Available"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_AWAY,
			"away", _("Away"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_INVISIBLE,
			"invisible", _("Invisible"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
			"offline", _("Offline"), FALSE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_MOBILE,
			"mobile", NULL, FALSE, FALSE, TRUE);
	types = g_list_append(types, status);

	return types;
}

/* initiate QQ away with proper change_status packet */
static void _qq_set_away(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc = purple_account_get_connection(account);

	qq_send_packet_change_status(gc);
}

/* IMPORTANT: PurpleConvImFlags -> PurpleMessageFlags */
/* send an instant msg to a buddy */
static gint _qq_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, PurpleMessageFlags flags)
{
	gint type, to_uid;
	gchar *msg, *msg_with_qq_smiley;
	qq_data *qd;

	g_return_val_if_fail(who != NULL, -1);

	qd = (qq_data *) gc->proto_data;

	g_return_val_if_fail(strlen(message) <= QQ_MSG_IM_MAX, -E2BIG);

	type = (flags == PURPLE_MESSAGE_AUTO_RESP ? QQ_IM_AUTO_REPLY : QQ_IM_TEXT);
	to_uid = purple_name_to_uid(who);

	/* if msg is to myself, bypass the network */
	if (to_uid == qd->uid) {
		serv_got_im(gc, who, message, flags, time(NULL));
	} else {
		msg = utf8_to_qq(message, QQ_CHARSET_DEFAULT);
		msg_with_qq_smiley = purple_smiley_to_qq(msg);
		qq_send_packet_im(gc, to_uid, msg_with_qq_smiley, type);
		g_free(msg);
		g_free(msg_with_qq_smiley);
	}

	return 1;
}

/* send a chat msg to a QQ Qun */
static int _qq_chat_send(PurpleConnection *gc, int channel, const char *message, PurpleMessageFlags flags)
{
	gchar *msg, *msg_with_qq_smiley;
	qq_group *group;

	g_return_val_if_fail(message != NULL, -1);
	g_return_val_if_fail(strlen(message) <= QQ_MSG_IM_MAX, -E2BIG);

	group = qq_group_find_by_channel(gc, channel);
	g_return_val_if_fail(group != NULL, -1);

	msg = utf8_to_qq(message, QQ_CHARSET_DEFAULT);
	msg_with_qq_smiley = purple_smiley_to_qq(msg);
	qq_send_packet_group_im(gc, group, msg_with_qq_smiley);
	g_free(msg);
	g_free(msg_with_qq_smiley);

	return 1;
}

/* send packet to get who's detailed information */
static void _qq_get_info(PurpleConnection *gc, const gchar *who)
{
	guint32 uid;
	qq_data *qd;

	qd = gc->proto_data;
	uid = purple_name_to_uid(who);

	if (uid <= 0) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "Not valid QQid: %s\n", who);
		purple_notify_error(gc, NULL, _("Invalid name"), NULL);
		return;
	}

	qq_send_packet_get_level(gc, uid);
	qq_send_packet_get_info(gc, uid, TRUE);
}

/* get my own information */
static void _qq_menu_modify_my_info(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;
	qq_prepare_modify_info(gc);
}

static void _qq_menu_change_password(PurplePluginAction *action)
{
	purple_notify_uri(NULL, "https://password.qq.com");
}

/* remove a buddy from my list and remove myself from his list */
/* TODO: re-enable this
static void _qq_menu_block_buddy(PurpleBlistNode * node)
{
	guint32 uid;
	gc_and_uid *g;
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	const gchar *who;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	gc = purple_account_get_connection(buddy->account);
	who = buddy->name;
	g_return_if_fail(who != NULL);

	uid = purple_name_to_uid(who);
	g_return_if_fail(uid > 0);

	g = g_new0(gc_and_uid, 1);
	g->gc = gc;
	g->uid = uid;

	purple_request_action(gc, _("Block Buddy"),
			    _("Are you sure you want to block this buddy?"), NULL,
			    1, g, 2,
			    _("Cancel"),
			    G_CALLBACK(qq_do_nothing_with_gc_and_uid),
			    _("Block"), G_CALLBACK(qq_block_buddy_with_gc_and_uid));
}
*/

/* show a brief summary of what we get from login packet */
static void _qq_menu_show_login_info(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	qq_data *qd;
	GString *info;

	qd = (qq_data *) gc->proto_data;
	info = g_string_new("<html><body>\n");

	g_string_append_printf(info, _("<b>Current Online</b>: %d<br>\n"), qd->all_online);
	g_string_append_printf(info, _("<b>Last Refresh</b>: %s<br>\n"), ctime(&qd->last_get_online));

	g_string_append(info, "<hr>\n");

	g_string_append_printf(info, _("<b>Connection Mode</b>: %s<br>\n"), qd->use_tcp ? "TCP" : "UDP");
	g_string_append_printf(info, _("<b>Server IP</b>: %s: %d<br>\n"), qd->server_ip, qd->server_port);
	g_string_append_printf(info, _("<b>My Public IP</b>: %s<br>\n"), qd->my_ip);

	g_string_append(info, "<hr>\n");
	g_string_append(info, "<i>Information below may not be accurate</i><br>\n");

	g_string_append_printf(info, _("<b>Login Time</b>: %s<br>\n"), ctime(&qd->login_time));
	g_string_append_printf(info, _("<b>Last Login IP</b>: %s<br>\n"), qd->last_login_ip);
	g_string_append_printf(info, _("<b>Last Login Time</b>: %s\n"), ctime(&qd->last_login_time));

	g_string_append(info, "</body></html>");

	purple_notify_formatted(gc, NULL, _("Login Information"), NULL, info->str, NULL, NULL);

	g_string_free(info, TRUE);
}

/*
static void _qq_menu_search_or_add_permanent_group(PurplePluginAction *action)
{
	purple_roomlist_show_with_account(NULL);
}
*/

/*
static void _qq_menu_create_permanent_group(PurplePluginAction * action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	purple_request_input(gc, _("Create QQ Qun"),
			   _("Input Qun name here"),
			   _("Only QQ members can create permanent Qun"),
			   "OpenQ", FALSE, FALSE, NULL,
			   _("Create"), G_CALLBACK(qq_group_create_with_name), _("Cancel"), NULL, gc);
}
*/

static void _qq_menu_unsubscribe_group(PurpleBlistNode * node)
{
	PurpleChat *chat = (PurpleChat *)node;
	PurpleConnection *gc = purple_account_get_connection(chat->account);
	GHashTable *components = chat -> components;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_CHAT(node));

	g_return_if_fail(components != NULL);
	qq_group_exit(gc, components);
}

/*
static void _qq_menu_manage_group(PurpleBlistNode * node)
{
	PurpleChat *chat = (PurpleChat *)node;
	PurpleConnection *gc = purple_account_get_connection(chat->account);
	GHashTable *components = chat -> components;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_CHAT(node));

	g_return_if_fail(components != NULL);
	qq_group_manage_group(gc, components);
}
*/

#if 0
/* TODO: re-enable this */
static void _qq_menu_send_file(PurpleBlistNode * node, gpointer ignored)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	qq_buddy *q_bud;

	g_return_if_fail (PURPLE_BLIST_NODE_IS_BUDDY (node));
	buddy = (PurpleBuddy *) node;
	q_bud = (qq_buddy *) buddy->proto_data;
/*	if (is_online (q_bud->status)) { */
	gc = purple_account_get_connection (buddy->account);
	g_return_if_fail (gc != NULL && gc->proto_data != NULL);
	qq_send_file(gc, buddy->name, NULL);
/*	} */
}
#endif

/* protocol related menus */
static GList *_qq_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m;
	PurplePluginAction *act;

	m = NULL;
	act = purple_plugin_action_new(_("Set My Information"), _qq_menu_modify_my_info);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Change Password"), _qq_menu_change_password);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Show Login Information"), _qq_menu_show_login_info);
	m = g_list_append(m, act);

	/*
	act = purple_plugin_action_new(_("Qun: Search a permanent Qun"), _qq_menu_search_or_add_permanent_group);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Qun: Create a permanent Qun"), _qq_menu_create_permanent_group);
	m = g_list_append(m, act);
	*/

	return m;
}

/* chat-related (QQ Qun) menu shown up with right-click */
static GList *_qq_chat_menu(PurpleBlistNode *node)
{
	GList *m;
	PurpleMenuAction *act;

	m = NULL;
	act = purple_menu_action_new(_("Leave this QQ Qun"), PURPLE_CALLBACK(_qq_menu_unsubscribe_group), NULL, NULL);
	m = g_list_append(m, act);

	/* TODO: enable this
	act = purple_menu_action_new(_("Show Details"), PURPLE_CALLBACK(_qq_menu_manage_group), NULL, NULL);
	m = g_list_append(m, act);
	*/

	return m;
}

/* buddy-related menu shown up with right-click */
static GList *_qq_buddy_menu(PurpleBlistNode * node)
{
	GList *m;

	if(PURPLE_BLIST_NODE_IS_CHAT(node))
		return _qq_chat_menu(node);

	m = NULL;

/* TODO : not working, temp commented out by gfhuang */
#if 0

	act = purple_menu_action_new(_("Block this buddy"), PURPLE_CALLBACK(_qq_menu_block_buddy), NULL, NULL); /* add NULL by gfhuang */
	m = g_list_append(m, act);
/*	if (q_bud && is_online(q_bud->status)) { */
		act = purple_menu_action_new(_("Send File"), PURPLE_CALLBACK(_qq_menu_send_file), NULL, NULL); /* add NULL by gfhuang */
		m = g_list_append(m, act);
/*	} */
#endif

	return m;
}


static void _qq_keep_alive(PurpleConnection *gc)
{
	qq_group *group;
	qq_data *qd;
	GList *list;

	if (NULL == (qd = (qq_data *) gc->proto_data))
		return;

	list = qd->groups;
	while (list != NULL) {
		group = (qq_group *) list->data;
		if (group->my_status == QQ_GROUP_MEMBER_STATUS_IS_MEMBER ||
		    group->my_status == QQ_GROUP_MEMBER_STATUS_IS_ADMIN)
			/* no need to get info time and time again, online members enough */
			qq_send_cmd_group_get_online_members(gc, group);

		list = list->next;
	}

	qq_send_packet_keep_alive(gc);
}

/* convert chat nickname to qq-uid to get this buddy info */
/* who is the nickname of buddy in QQ chat-room (Qun) */
static void _qq_get_chat_buddy_info(PurpleConnection *gc, gint channel, const gchar *who)
{
	gchar *purple_name;
	g_return_if_fail(who != NULL);

	purple_name = chat_name_to_purple_name(who);
	if (purple_name != NULL)
		_qq_get_info(gc, purple_name);
}

/* convert chat nickname to qq-uid to invite individual IM to buddy */
/* who is the nickname of buddy in QQ chat-room (Qun) */
static gchar *_qq_get_chat_buddy_real_name(PurpleConnection *gc, gint channel, const gchar *who)
{
	g_return_val_if_fail(who != NULL, NULL);
	return chat_name_to_purple_name(who);
}

PurplePlugin *my_protocol = NULL;
static PurplePluginProtocolInfo prpl_info	= {
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_USE_POINTSIZE,
	NULL,							/* user_splits	*/
	NULL,							/* protocol_options */
	{"png", 96, 96, 96, 96, 0, PURPLE_ICON_SCALE_SEND}, /* icon_spec */
	_qq_list_icon,						/* list_icon */
	_qq_list_emblem,					/* list_emblems */
	_qq_status_text,					/* status_text	*/
	_qq_tooltip_text,					/* tooltip_text */
	_qq_away_states,					/* away_states	*/
	_qq_buddy_menu,						/* blist_node_menu */
	qq_chat_info,						/* chat_info */
	qq_chat_info_defaults,					/* chat_info_defaults */
	_qq_login,						/* login */
	_qq_close,						/* close */
	_qq_send_im,						/* send_im */
	NULL,							/* set_info */
	NULL,							/* send_typing	*/
	_qq_get_info,						/* get_info */
	_qq_set_away,						/* set_away */
	NULL,							/* set_idle */
	NULL,							/* change_passwd */
	qq_add_buddy,						/* add_buddy */
	NULL,							/* add_buddies	*/
	qq_remove_buddy,					/* remove_buddy */
	NULL,							/* remove_buddies */
	NULL,							/* add_permit */
	NULL,							/* add_deny */
	NULL,							/* rem_permit */
	NULL,							/* rem_deny */
	NULL,							/* set_permit_deny */
	qq_group_join,						/* join_chat */
	NULL,							/* reject chat	invite */
	NULL,							/* get_chat_name */
	NULL,							/* chat_invite	*/
	NULL,							/* chat_leave */
	NULL,							/* chat_whisper */
	_qq_chat_send,						/* chat_send */
	_qq_keep_alive,						/* keepalive */
	NULL,							/* register_user */
	_qq_get_chat_buddy_info,				/* get_cb_info	*/
	NULL,							/* get_cb_away	*/
	NULL,							/* alias_buddy	*/
	NULL,							/* group_buddy	*/
	NULL,							/* rename_group */
	NULL,							/* buddy_free */
	NULL,							/* convo_closed */
	NULL,							/* normalize */
	qq_set_my_buddy_icon,					/* set_buddy_icon */
	NULL,							/* remove_group */
	_qq_get_chat_buddy_real_name,				/* get_cb_real_name */
	NULL,							/* set_chat_topic */
	NULL,							/* find_blist_chat */
	qq_roomlist_get_list,					/* roomlist_get_list */
	qq_roomlist_cancel,					/* roomlist_cancel */
	NULL,							/* roomlist_expand_category */
	NULL,							/* can_receive_file */
	qq_send_file,						/* send_file */
	NULL,							/* new xfer */
	NULL,							/* offline_message */
	NULL,							/* PurpleWhiteboardPrplOps */
	NULL,							/* send_raw */
	NULL,							/* roomlist_room_serialize */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,		/**< type		*/
	NULL,				/**< ui_requirement	*/
	0,				/**< flags		*/
	NULL,				/**< dependencies	*/
	PURPLE_PRIORITY_DEFAULT,		/**< priority		*/

	"prpl-qq",			/**< id			*/
	"QQ",				/**< name		*/
	DISPLAY_VERSION,		/**< version		*/
					/**  summary		*/
	N_("QQ Protocol	Plugin"),
					/**  description	*/
	N_("QQ Protocol	Plugin"),
	OPENQ_AUTHOR,			/**< author		*/
	OPENQ_WEBSITE,			/**< homepage		*/

	NULL,				/**< load		*/
	NULL,				/**< unload		*/
	NULL,				/**< destroy		*/

	NULL,				/**< ui_info		*/
	&prpl_info,			/**< extra_info		*/
	NULL,				/**< prefs_info		*/
	_qq_actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};


static void init_plugin(PurplePlugin *plugin)
{
	PurpleAccountOption *option;

	option = purple_account_option_bool_new(_("Connect using TCP"), "use_tcp", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Server"), "server", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Port"), "port", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;

	purple_prefs_add_none("/plugins/prpl/qq");
	purple_prefs_add_bool("/plugins/prpl/qq/show_status_by_icon", TRUE);
	purple_prefs_add_bool("/plugins/prpl/qq/show_fake_video", FALSE);
	purple_prefs_add_bool("/plugins/prpl/qq/prompt_group_msg_on_recv", TRUE);
}

PURPLE_INIT_PLUGIN(qq, init_plugin, info);
