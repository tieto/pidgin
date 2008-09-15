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
#include "buddy_list.h"
#include "char_conv.h"
#include "group.h"
#include "group_find.h"
#include "group_im.h"
#include "group_info.h"
#include "group_join.h"
#include "group_opt.h"
#include "header_info.h"
#include "im.h"
#include "qq_process.h"
#include "qq_base.h"
#include "packet_parse.h"
#include "qq.h"
#include "qq_network.h"
#include "send_file.h"
#include "utils.h"
#include "version.h"

#define OPENQ_AUTHOR            "Puzzlebird"
#define OPENQ_WEBSITE            "http://openq.sourceforge.net"

static GList *server_list_build(gchar select)
{
	GList *list = NULL;

	if ( select == 'T' || select == 'A') {
		list = g_list_append(list, "tcpconn.tencent.com:8000");
		list = g_list_append(list, "tcpconn2.tencent.com:8000");
		list = g_list_append(list, "tcpconn3.tencent.com:8000");
		list = g_list_append(list, "tcpconn4.tencent.com:8000");
		list = g_list_append(list, "tcpconn5.tencent.com:8000");
		list = g_list_append(list, "tcpconn6.tencent.com:8000");
	}
	if ( select == 'U' || select == 'A') {
		list = g_list_append(list, "sz.tencent.com:8000");
		list = g_list_append(list, "sz2.tencent.com:8000");
		list = g_list_append(list, "sz3.tencent.com:8000");
		list = g_list_append(list, "sz4.tencent.com:8000");
		list = g_list_append(list, "sz5.tencent.com:8000");
		list = g_list_append(list, "sz6.tencent.com:8000");
		list = g_list_append(list, "sz7.tencent.com:8000");
		list = g_list_append(list, "sz8.tencent.com:8000");
		list = g_list_append(list, "sz9.tencent.com:8000");
	}
	return list;
}

static void server_list_create(PurpleAccount *account)
{
	PurpleConnection *gc;
	qq_data *qd;
	PurpleProxyInfo *gpi;
	const gchar *user_server;

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL  && gc->proto_data != NULL);
	qd = gc->proto_data;

	gpi = purple_proxy_get_setup(account);

	qd->use_tcp  = TRUE;
	if (purple_proxy_info_get_type(gpi) == PURPLE_PROXY_UDP) {
		qd->use_tcp  = FALSE;
	}

	user_server = purple_account_get_string(account, "server", NULL);
	purple_debug_info("QQ", "Select server '%s'\n", user_server);
	if ( (user_server != NULL && strlen(user_server) > 0) && strcasecmp(user_server, "auto") != 0) {
		qd->servers = g_list_append(qd->servers, g_strdup(user_server));
		return;
	}

	if (qd->use_tcp) {
		qd->servers =	server_list_build('T');
		return;
    }

	qd->servers =	server_list_build('U');
}

static void server_list_remove_all(qq_data *qd)
{
 	g_return_if_fail(qd != NULL);

	purple_debug_info("QQ", "free server list\n");
 	g_list_free(qd->servers);
	qd->curr_server = NULL;
}

static void qq_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	qq_data *qd;
	PurplePresence *presence;

	g_return_if_fail(account != NULL);

	gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_BGCOLOR | PURPLE_CONNECTION_AUTO_RESP;

	qd = g_new0(qq_data, 1);
	memset(qd, 0, sizeof(qq_data));
	qd->gc = gc;
	gc->proto_data = qd;

	presence = purple_account_get_presence(account);
	if(purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_INVISIBLE)) {
		qd->login_mode = QQ_LOGIN_MODE_HIDDEN;
	} else if(purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_AWAY)
				|| purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_EXTENDED_AWAY)) {
		qd->login_mode = QQ_LOGIN_MODE_AWAY;
	} else {
		qd->login_mode = QQ_LOGIN_MODE_NORMAL;
	}

	server_list_create(account);
	purple_debug_info("QQ", "Server list has %d\n", g_list_length(qd->servers));

	qd->is_show_notice = purple_account_get_bool(account, "show_notice", TRUE);
	qd->is_show_news = purple_account_get_bool(account, "show_news", TRUE);

	qd->itv_config.resend = purple_account_get_int(account, "resend_interval", 10);
	if (qd->itv_config.resend <= 0) qd->itv_config.resend = 10;

	qd->itv_config.keep_alive = purple_account_get_int(account, "keep_alive_interval", 60);
	if (qd->itv_config.keep_alive < 30) qd->itv_config.keep_alive = 30;
	qd->itv_config.keep_alive /= qd->itv_config.resend;
	qd->itv_count.keep_alive = qd->itv_config.keep_alive;

	qd->itv_config.update = purple_account_get_int(account, "update_interval", 300);
	if (qd->itv_config.update > 0) {
		if (qd->itv_config.update < qd->itv_config.keep_alive) {
			qd->itv_config.update = qd->itv_config.keep_alive;
		}
		qd->itv_config.update /= qd->itv_config.resend;
		qd->itv_count.update = qd->itv_config.update;
	} else {
		qd->itv_config.update = 0;
	}

	qd->connect_watcher = purple_timeout_add_seconds(0, qq_connect_later, gc);
}

/* clean up the given QQ connection and free all resources */
static void qq_close(PurpleConnection *gc)
{
	qq_data *qd;

	g_return_if_fail(gc != NULL  && gc->proto_data);
	qd = gc->proto_data;

	if (qd->check_watcher > 0) {
		purple_timeout_remove(qd->check_watcher);
		qd->check_watcher = 0;
	}

	if (qd->connect_watcher > 0) {
		purple_timeout_remove(qd->connect_watcher);
		qd->connect_watcher = 0;
	}

	qq_disconnect(gc);
	server_list_remove_all(qd);

	g_free(qd);
	gc->proto_data = NULL;
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
		g_string_append(status, _("Online"));
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
	gchar *tmp;
	GString *str;

	g_return_if_fail(b != NULL);

	q_bud = (qq_buddy *) b->proto_data;
	if (q_bud == NULL)
		return;

	/* if (PURPLE_BUDDY_IS_ONLINE(b) && q_bud != NULL) */
	if (q_bud->ip.s_addr != 0) {
		str = g_string_new(NULL);
		g_string_printf(str, "%s:%d", inet_ntoa(q_bud->ip), q_bud->port);
		if (q_bud->comm_flag & QQ_COMM_FLAG_TCP_MODE) {
			g_string_append(str, " TCP");
		} else {
			g_string_append(str, " UDP");
		}
		g_string_free(str, TRUE);
	}

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

	str = g_string_new(NULL);
	if (q_bud->comm_flag & QQ_COMM_FLAG_QQ_MEMBER) {
		g_string_append( str, _("Member") );
	}
	if (q_bud->comm_flag & QQ_COMM_FLAG_QQ_VIP) {
		g_string_append( str, _(" VIP") );
	}
	if (q_bud->comm_flag & QQ_COMM_FLAG_TCP_MODE) {
		g_string_append( str, _(" TCP") );
	}
	if (q_bud->comm_flag & QQ_COMM_FLAG_MOBILE) {
		g_string_append( str, _(" FromMobile") );
	}
	if (q_bud->comm_flag & QQ_COMM_FLAG_BIND_MOBILE) {
		g_string_append( str, _(" BindMobile") );
	}
	if (q_bud->comm_flag & QQ_COMM_FLAG_VIDEO) {
		g_string_append( str, _(" Video") );
	}

	if (q_bud->ext_flag & QQ_EXT_FLAG_ZONE) {
		g_string_append( str, _(" Zone") );
	}
	purple_notify_user_info_add_pair(user_info, _("Flag"), str->str);

	g_string_free(str, TRUE);

#ifdef DEBUG
	tmp = g_strdup_printf( "%s (%04X)",
										qq_get_ver_desc(q_bud->client_version),
										q_bud->client_version );
	purple_notify_user_info_add_pair(user_info, _("Ver"), tmp);
	g_free(tmp);

	tmp = g_strdup_printf( "Ext 0x%X, Comm 0x%X",
												q_bud->ext_flag, q_bud->comm_flag );
	purple_notify_user_info_add_pair(user_info, _("Flag"), tmp);
	g_free(tmp);
#endif
}

/* we can show tiny icons on the four corners of buddy icon, */
static const char *_qq_list_emblem(PurpleBuddy *b)
{
	/* each char** are refering to a filename in pixmaps/purple/status/default/ */
	qq_buddy *q_bud;

	if (!b || !(q_bud = b->proto_data)) {
		return NULL;
	}

	if (q_bud->comm_flag & QQ_COMM_FLAG_MOBILE)
		return "mobile";
	if (q_bud->comm_flag & QQ_COMM_FLAG_VIDEO)
		return "video";
	if (q_bud->comm_flag & QQ_COMM_FLAG_QQ_MEMBER)
		return "qq_member";

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
static void _qq_change_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc = purple_account_get_connection(account);

	qq_request_change_status(gc, 0);
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

	purple_debug_info("QQ_MESG", "Send qun mesg in utf8: %s\n", message);
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
		purple_debug_error("QQ", "Not valid QQid: %s\n", who);
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

	g_string_append_printf(info, _("<b>Current Online</b>: %d<br>\n"), qd->total_online);
	g_string_append_printf(info, _("<b>Last Refresh</b>: %s<br>\n"), ctime(&qd->last_get_online));

	g_string_append(info, "<hr>\n");

	g_string_append_printf(info, _("<b>Server</b>: %s<br>\n"), qd->curr_server);
	g_string_append_printf(info, _("<b>Connection Mode</b>: %s<br>\n"), qd->use_tcp ? "TCP" : "UDP");
	g_string_append_printf(info, _("<b>My Public IP</b>: %s<br>\n"), inet_ntoa(qd->my_ip));

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
			   _("Create"), G_CALLBACK(qq_room_create_new), _("Cancel"), NULL, gc);
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
	act = purple_menu_action_new(_("Leave the QQ Qun"), PURPLE_CALLBACK(_qq_menu_unsubscribe_group), NULL, NULL);
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
	qq_login,							/* open */
	qq_close,						/* close */
	_qq_send_im,						/* send_im */
	NULL,							/* set_info */
	NULL,							/* send_typing	*/
	_qq_get_info,						/* get_info */
	_qq_change_status,						/* change status */
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
	_qq_chat_send,			/* chat_send */
	NULL,							/* keepalive */
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
	NULL,							/* qq_send_file send_file */
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
	PurpleKeyValuePair *kvp;
	GList *list = NULL;
	GList *kvlist = NULL;
	GList *entry;

	list = server_list_build('A');

	purple_prefs_add_string_list("/plugins/prpl/qq/serverlist", list);
	list = purple_prefs_get_string_list("/plugins/prpl/qq/serverlist");

	kvlist = NULL;
	kvp = g_new0(PurpleKeyValuePair, 1);
	kvp->key = g_strdup(_("Auto"));
	kvp->value = g_strdup("auto");
	kvlist = g_list_append(kvlist, kvp);

	entry = list;
	while(entry) {
		if (entry->data != NULL && strlen(entry->data) > 0) {
			kvp = g_new0(PurpleKeyValuePair, 1);
			kvp->key = g_strdup(entry->data);
			kvp->value = g_strdup(entry->data);
			kvlist = g_list_append(kvlist, kvp);
		}
		entry = entry->next;
	}

	/*
	option = purple_account_option_string_new(_("Server"), "server", NULL);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Port"), "port", 0);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	*/
	option = purple_account_option_list_new(_("Server"), "server", kvlist);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Show server notice"), "show_notice", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Show server news"), "show_news", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Resend interval(s)"), "resend_interval", 10);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Keep alive interval(s)"), "keep_alive_interval", 60);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Update interval(s)"), "update_interval", 300);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;

	purple_prefs_add_none("/plugins/prpl/qq");
	purple_prefs_add_bool("/plugins/prpl/qq/show_status_by_icon", TRUE);
	purple_prefs_add_bool("/plugins/prpl/qq/show_fake_video", FALSE);
	purple_prefs_add_bool("/plugins/prpl/qq/prompt_group_msg_on_recv", TRUE);

}

PURPLE_INIT_PLUGIN(qq, init_plugin, info);
