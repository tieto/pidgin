#include <string.h>
#include <stdlib.h>
#include "icq.h"   /* well, we're doing ICQ, right? */
#include "multi.h" /* needed for gaim_connection */
#include "prpl.h"  /* needed for prpl */
#include "gaim.h"  /* needed for every other damn thing */
#include "proxy.h"

#include "pixmaps/gnomeicu-online.xpm"
#include "pixmaps/gnomeicu-away.xpm"
#include "pixmaps/gnomeicu-dnd.xpm"
#include "pixmaps/gnomeicu-na.xpm"
#include "pixmaps/gnomeicu-occ.xpm"
#include "pixmaps/gnomeicu-ffc.xpm"

#define USEROPT_NICK 0

struct icq_data {
	icq_Link *link;
	int cur_status;
	gboolean connected;
};

static guint ack_timer = 0;

static char *icq_name() {
	return "ICQ";
}

static void icq_do_log(icq_Link *link, time_t time, unsigned char level, const char *log) {
	debug_printf("ICQ debug %d: %s", level, log);
}

GList *sockets = NULL;
struct gaim_sock {
	int socket;
	int type;
	gint inpa;
};

static void gaim_icq_handler(gpointer data, gint source, GaimInputCondition cond) {
	if (cond & GAIM_INPUT_READ)
		icq_HandleReadySocket(source, ICQ_SOCKET_READ);
	if (cond & GAIM_INPUT_WRITE)
		icq_HandleReadySocket(source, ICQ_SOCKET_WRITE);
}

static void icq_sock_notify(int socket, int type, int status) {
	struct gaim_sock *gs = NULL;
	if (status) {
		GaimInputCondition cond;
		if (type == ICQ_SOCKET_READ)
			cond = GAIM_INPUT_READ;
		else
			cond = GAIM_INPUT_WRITE;
		gs = g_new0(struct gaim_sock, 1);
		gs->socket = socket;
		gs->type = type;
		gs->inpa = gaim_input_add(socket, cond, gaim_icq_handler, NULL);
		sockets = g_list_append(sockets, gs);
		debug_printf("Adding socket notifier: %d %d (%d)\n", socket, type, gs->inpa);
	} else {
		GList *m = sockets;
		while (m) {
			gs = m->data;
			if ((gs->socket == socket) && (gs->type == type))
				break;
			m = g_list_next(m);
		}
		if (m) {
			gaim_input_remove(gs->inpa);
			sockets = g_list_remove(sockets, gs);
			debug_printf("Removing socket notifier: %d %d (%d)\n", socket, type, gs->inpa);
			g_free(gs);
		}
	}
}

static void icq_online(icq_Link *link) {
	struct gaim_connection *gc = link->icq_UserData;
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	debug_printf("%s is now online.\n", gc->username);
	id->connected = TRUE;
	account_online(gc);
	serv_finish_login(gc);

	icq_ChangeStatus(id->link, STATUS_ONLINE);
}

static void icq_logged_off(icq_Link *link) {
	struct gaim_connection *gc = link->icq_UserData;
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	if (!id->connected) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	if (icq_Connect(link, "icq.mirabilis.com", 4000) < 1) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	icq_Login(link, STATUS_ONLINE);
	id->cur_status = STATUS_ONLINE;
}

void strip_linefeed(gchar *text)
{
	int i, j;
	gchar *text2 = g_malloc(strlen(text) + 1);

	for (i = 0, j = 0; text[i]; i++)
		if (text[i] != '\r')
			text2[j++] = text[i];
	text2[j] = '\0';

	strcpy(text, text2);
	g_free(text2);
}

static void icq_msg_incoming(icq_Link *link, unsigned long uin, unsigned char hour, unsigned char minute,
			unsigned char day, unsigned char month, unsigned short year, const char *data) {
	struct gaim_connection *gc = link->icq_UserData;
	char buf[256], *tmp = g_malloc(BUF_LONG);
	g_snprintf(tmp, BUF_LONG, "%s", data);
	g_snprintf(buf, sizeof buf, "%lu", uin);
	strip_linefeed(tmp);
	serv_got_im(gc, buf, tmp, 0, time((time_t)NULL));
	g_free(tmp);
}

static void icq_user_online(icq_Link *link, unsigned long uin, unsigned long st,
				unsigned long ip, unsigned short port, unsigned long real_ip,
				unsigned char tcp_flags) {
	struct gaim_connection *gc = link->icq_UserData;
	guint status;
	char buf[256];

	g_snprintf(buf, sizeof buf, "%lu", uin);
	status = (st == STATUS_ONLINE) ? 0 : UC_UNAVAILABLE | (st << 1);
	serv_got_update(gc, buf, 1, 0, 0, 0, status, 0);
}

static void icq_user_offline(icq_Link *link, unsigned long uin) {
	struct gaim_connection *gc = link->icq_UserData;
	char buf[256]; g_snprintf(buf, sizeof buf, "%lu", uin);
	serv_got_update(gc, buf, 0, 0, 0, 0, 0, 0);
}

static void icq_user_status(icq_Link *link, unsigned long uin, unsigned long st) {
	struct gaim_connection *gc = link->icq_UserData;
	guint status;
	char buf[256];

	g_snprintf(buf, sizeof buf, "%lu", uin);
	status = (st == STATUS_ONLINE) ? 0 : UC_UNAVAILABLE | (st << 1);
	serv_got_update(gc, buf, 1, 0, 0, 0, status, 0);
}

static gboolean icq_set_timeout_cb(gpointer data) {
	icq_HandleTimeout();
	ack_timer = 0;
	return FALSE;
}

static void icq_set_timeout(long interval) {
	debug_printf("icq_SetTimeout: %ld\n", interval);
	if (interval > 0 && ack_timer == 0)
		ack_timer = g_timeout_add(interval * 1000, icq_set_timeout_cb, NULL);
	else if (ack_timer > 0) {
		g_source_remove(ack_timer);
		ack_timer = 0;
	}
}

static void icq_url_incoming(icq_Link *link, unsigned long uin, unsigned char hour,
				unsigned char minute, unsigned char day, unsigned char month,
				unsigned short year, const char *url, const char *descr) {
	struct gaim_connection *gc = link->icq_UserData;
	char *msg = g_malloc(BUF_LONG), buf[256];
	g_snprintf(msg, BUF_LONG, "<A HREF=\"%s\">%s</A>", url, descr);
	g_snprintf(buf, 256, "%lu", uin);
	serv_got_im(gc, buf, msg, 0, time((time_t)NULL));
	g_free(msg);
}

static void icq_wrong_passwd(icq_Link *link) {
	struct gaim_connection *gc = link->icq_UserData;
	hide_login_progress(gc, "Invalid password.");
	signoff(gc);
}

static void icq_invalid_uin(icq_Link *link) {
	struct gaim_connection *gc = link->icq_UserData;
	hide_login_progress(gc, "Invalid UIN.");
	signoff(gc);
}

static void icq_info_reply(icq_Link *link, unsigned long uin, const char *nick,
			const char *first, const char *last, const char *email, char auth) {
	char buf[16 * 1024];

	g_snprintf(buf, sizeof buf,
		   "<B>UIN:</B> %lu<BR>"
		   "<B>Nick:</B> %s<BR>"
		   "<B>Name:</B> %s %s<BR>"
		   "<B>Email:</B> %s\n",
		   uin,
		   nick,
		   first, last,
		   email);
	g_show_info_text(buf, NULL);
}

static void icq_web_pager(icq_Link *link, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *email, const char *msg) {
	struct gaim_connection *gc = link->icq_UserData;
	char *who = g_strdup_printf("ICQ Web Pager: %s (%s)", nick, email);
	char *what = g_malloc(BUF_LONG);
	g_snprintf(what, BUF_LONG, "%s", msg);
	serv_got_im(gc, who, what, 0, time((time_t)NULL));
	g_free(who);
	g_free(what);
}

static void icq_mail_express(icq_Link *link, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *email, const char *msg) {
	struct gaim_connection *gc = link->icq_UserData;
	char *who = g_strdup_printf("ICQ Mail Express: %s (%s)", nick, email);
	char *what = g_malloc(BUF_LONG);
	g_snprintf(what, BUF_LONG, "%s", msg);
	serv_got_im(gc, who, what, 0, time((time_t)NULL));
	g_free(who);
	g_free(what);
}

static void icq_req_not(icq_Link *link, unsigned long id, int type, int arg, void *data) {
	if (type == ICQ_NOTIFY_FAILED)
		do_error_dialog("Failure in sending packet", "ICQ error");
	return;
}

struct icq_auth {
	icq_Link *link;
	unsigned long uin;
};

static void icq_den_auth(gpointer x, struct icq_auth *iq)
{
	g_free(iq);
}

static void icq_acc_auth(gpointer x, struct icq_auth *iq)
{
	icq_SendAuthMsg(iq->link, iq->uin);
}

static void icq_auth_req(icq_Link *link, unsigned long uin, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *first, const char *last, const char *email, const char *reason)
{
	char msg[8192];
	struct icq_auth *iq = g_new0(struct icq_auth, 1);
	iq->link = link;
	iq->uin = uin;

	g_snprintf(msg, sizeof(msg), "The user %s (%s%s%s%s%s) wants you to authorize them.",
			nick, first ? first : "", first && last ? " " : "", last ? last : "",
			(first || last) && email ? ", " : "", email ? email : "");
	do_ask_dialog(msg, iq, icq_acc_auth, icq_den_auth);
}

static void icq_login(struct aim_user *user) {
	struct gaim_connection *gc = new_gaim_conn(user);
	struct icq_data *id = gc->proto_data = g_new0(struct icq_data, 1);
	icq_Link *link;
	char ps[9];

	icq_LogLevel = ICQ_LOG_MESSAGE;

	g_snprintf(ps, sizeof(ps), "%s", user->password);
	link = id->link = icq_ICQLINKNew(atol(user->username), ps,
			  user->proto_opt[USEROPT_NICK][0] ? user->proto_opt[USEROPT_NICK] : "gaim user",
			  TRUE);
	g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", user->proto_opt[USEROPT_NICK]);

	link->icq_Logged = icq_online;
	link->icq_Disconnected = icq_logged_off;
	link->icq_RecvMessage = icq_msg_incoming;
	link->icq_RecvURL = icq_url_incoming;
	link->icq_RecvWebPager = icq_web_pager;
	link->icq_RecvMailExpress = icq_mail_express;
	link->icq_RecvAuthReq = icq_auth_req;
	link->icq_UserOnline = icq_user_online;
	link->icq_UserOffline = icq_user_offline;
	link->icq_UserStatusUpdate = icq_user_status;
	link->icq_InfoReply = icq_info_reply;
	link->icq_WrongPassword = icq_wrong_passwd;
	link->icq_InvalidUIN = icq_invalid_uin;
	link->icq_Log = icq_do_log;
	link->icq_RequestNotify = icq_req_not;
	link->icq_UserData = gc;

	if (proxytype == PROXY_SOCKS5)
		icq_SetProxy(link, proxyhost, proxyport, proxyuser[0], proxyuser, proxypass);

	icq_ContactClear(id->link);
	if (bud_list_cache_exists(gc))
		do_import(gc, NULL);

	if (icq_Connect(link, "icq.mirabilis.com", 4000) < 1) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	icq_Login(link, STATUS_ONLINE);
	id->cur_status = STATUS_ONLINE;

	set_login_progress(gc, 0, "Connecting...");
}

static void icq_close(struct gaim_connection *gc) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	icq_Logout(id->link);
	icq_Disconnect(id->link);
	icq_ICQLINKDelete(id->link);
	g_free(id);
}

static int icq_send_msg(struct gaim_connection *gc, char *who, char *msg, int flags) {
	if (!(flags & IM_FLAG_AWAY) && (strlen(msg) > 0)) {
		struct icq_data *id = (struct icq_data *)gc->proto_data;
		long w = atol(who);
		icq_SendMessage(id->link, w, msg,
				(flags & IM_FLAG_CHECKBOX) ? ICQ_SEND_THRUSERVER : ICQ_SEND_BESTWAY);
	}
	return 1;
}

static void icq_keepalive(struct gaim_connection *gc) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	icq_KeepAlive(id->link);
}

static void icq_add_buddy(struct gaim_connection *gc, char *who) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	icq_ContactAdd(id->link, atol(who));
	icq_ContactSetVis(id->link, atol(who), TRUE);
}

static void icq_add_buddies(struct gaim_connection *gc, GList *whos) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	while (whos) {
		icq_ContactAdd(id->link, atol(whos->data));
		icq_ContactSetVis(id->link, atol(whos->data), TRUE);
		whos = whos->next;
	}
}

static void icq_rem_buddy(struct gaim_connection *gc, char *who) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	icq_ContactRemove(id->link, atol(who));
}

static void icq_set_away(struct gaim_connection *gc, char *state, char *msg) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	if (gc->away)
		gc->away = NULL;

	if (!strcmp(state, "Online"))
		icq_ChangeStatus(id->link, STATUS_ONLINE);
	else if (!strcmp(state, "Away")) {
		icq_ChangeStatus(id->link, STATUS_AWAY);
		gc->away = "";
	} else if (!strcmp(state, "Do Not Disturb")) {
		icq_ChangeStatus(id->link, STATUS_DND);
		gc->away = "";
	} else if (!strcmp(state, "Not Available")) {
		icq_ChangeStatus(id->link, STATUS_NA);
		gc->away = "";
	} else if (!strcmp(state, "Occupied")) {
		icq_ChangeStatus(id->link, STATUS_OCCUPIED);
		gc->away = "";
	} else if (!strcmp(state, "Free For Chat")) {
		icq_ChangeStatus(id->link, STATUS_FREE_CHAT);
		gc->away = "";
	} else if (!strcmp(state, "Invisible")) {
		icq_ChangeStatus(id->link, STATUS_INVISIBLE);
		gc->away = "";
	} else if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
		if (msg) {
			icq_ChangeStatus(id->link, STATUS_NA);
			gc->away = "";
		} else {
			icq_ChangeStatus(id->link, STATUS_ONLINE);
		}
	}
}

static char **icq_list_icon(int uc) {
	guint status;
	if (uc == 0)
		return icon_online_xpm;
	status = uc >> 1;
	if (status & STATUS_NA)
		return icon_na_xpm;
	if (status & STATUS_DND)
		return icon_dnd_xpm;
	if (status & STATUS_OCCUPIED)
		return icon_occ_xpm;
	if (status & STATUS_AWAY)
		return icon_away_xpm;
	if (status & STATUS_FREE_CHAT)
		return icon_ffc_xpm;
	if (status & STATUS_INVISIBLE)
		return NULL;
	return icon_online_xpm;
}

static void icq_get_info(struct gaim_connection *gc, char *who) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	icq_SendInfoReq(id->link, atol(who));
}

static void icq_info(struct gaim_connection *gc, char *who) {
	serv_get_info(gc, who);
}

static GList *icq_buddy_menu(struct gaim_connection *gc, char *who) {
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Info");
	pbm->callback = icq_info;
	pbm->gc = gc;
	m = g_list_append(m, pbm);

	return m;
}

static GList *icq_user_opts() {
	GList *m = NULL;
	struct proto_user_opt *puo;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = "Nick:";
	puo->def = "Gaim User";
	puo->pos = USEROPT_NICK;
	m = g_list_append(m, puo);

	return m;
}

static GList *icq_away_states(struct gaim_connection *gc) {
	GList *m = NULL;

	m = g_list_append(m, "Online");
	m = g_list_append(m, "Away");
	m = g_list_append(m, "Do Not Disturb");
	m = g_list_append(m, "Not Available");
	m = g_list_append(m, "Occupied");
	m = g_list_append(m, "Free For Chat");
	m = g_list_append(m, "Invisible");

	return m;
}

static struct prpl *my_protocol = NULL;

void icq_init(struct prpl *ret) {
	ret->protocol = PROTO_ICQ;
	ret->checkbox = "Send message through server";
	ret->name = icq_name;
	ret->list_icon = icq_list_icon;
	ret->away_states = icq_away_states;
	ret->buddy_menu = icq_buddy_menu;
	ret->user_opts = icq_user_opts;
	ret->login = icq_login;
	ret->close = icq_close;
	ret->send_im = icq_send_msg;
	ret->add_buddy = icq_add_buddy;
	ret->add_buddies = icq_add_buddies;
	ret->remove_buddy = icq_rem_buddy;
	ret->get_info = icq_get_info;
	ret->set_away = icq_set_away;
	ret->keepalive = icq_keepalive;

	my_protocol = ret;

	icq_SocketNotify = icq_sock_notify;
	icq_SetTimeout = icq_set_timeout;
}

#ifndef STATIC

char *gaim_plugin_init(GModule *handle)
{
	load_protocol(icq_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove()
{
	struct prpl *p = find_prpl(PROTO_ICQ);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name()
{
	return "ICQ";
}

char *description()
{
	return PRPL_DESC("ICQ");
}

#endif
