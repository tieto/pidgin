#include <string.h>
#include <stdlib.h>
#include "icq.h"   /* well, we're doing ICQ, right? */
#include "gaim.h"  /* needed for every other damn thing */
#include "multi.h" /* needed for gaim_connection */
#include "prpl.h"  /* needed for prpl */
#include "proxy.h"

#include "pixmaps/protocols/icq/gnomeicu-online.xpm"
#include "pixmaps/protocols/icq/gnomeicu-away.xpm"
#include "pixmaps/protocols/icq/gnomeicu-dnd.xpm"
#include "pixmaps/protocols/icq/gnomeicu-na.xpm"
#include "pixmaps/protocols/icq/gnomeicu-occ.xpm"
#include "pixmaps/protocols/icq/gnomeicu-ffc.xpm"

#define USEROPT_NICK 0

static struct prpl *my_protocol = NULL;

struct icq_data {
	icq_Link *link;
	int cur_status;
	gboolean connected;
};

static guint ack_timer = 0;

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
	if (!id->connected) {
		account_online(gc);
		serv_finish_login(gc);
		icq_ChangeStatus(id->link, STATUS_ONLINE);
		id->connected = TRUE;
	}
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

	icq_Login(link, id->cur_status);
}

static void icq_msg_incoming(icq_Link *link, unsigned long uin, unsigned char hour, unsigned char minute,
			unsigned char day, unsigned char month, unsigned short year, const char *data) {
	struct gaim_connection *gc = link->icq_UserData;
	char buf[256], *tmp = g_malloc(BUF_LONG);
	g_snprintf(tmp, BUF_LONG, "%s", data);
	g_snprintf(buf, sizeof buf, "%lu", uin);
	strip_linefeed(tmp);
	serv_got_im(gc, buf, tmp, 0, time(NULL), -1);
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
	serv_got_im(gc, buf, msg, 0, time(NULL), -1);
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
	struct gaim_connection *gc = link->icq_UserData;
	char buf[16 * 1024];
	char who[16];

	g_snprintf(who, sizeof who, "%lu", uin);
	g_snprintf(buf, sizeof buf,
		   "<B>UIN:</B> %lu<BR>"
		   "<B>Nick:</B> %s<BR>"
		   "<B>Name:</B> %s %s<BR>"
		   "<B>Email:</B> %s\n",
		   uin,
		   nick,
		   first, last,
		   email);
	g_show_info_text(gc, who, 2, buf, NULL);
}

static void icq_web_pager(icq_Link *link, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *email, const char *msg) {
	struct gaim_connection *gc = link->icq_UserData;
	char *who = g_strdup_printf("ICQ Web Pager: %s (%s)", nick, email);
	char *what = g_malloc(BUF_LONG);
	g_snprintf(what, BUF_LONG, "%s", msg);
	serv_got_im(gc, who, what, 0, time(NULL), -1);
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
	serv_got_im(gc, who, what, 0, time(NULL), -1);
	g_free(who);
	g_free(what);
}

static void icq_req_not(icq_Link *link, unsigned long id, int type, int arg, void *data) {
	if (type == ICQ_NOTIFY_FAILED)
		do_error_dialog(_("Gaim encountered an error communicating with the ICQ server."), NULL, GAIM_ERROR);
	return;
}

static void icq_recv_add(icq_Link *link, unsigned long id, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *first, const char *last, const char *email)
{
	char uin[16];
	g_snprintf(uin, sizeof(uin), "%ld", id);
	show_got_added(link->icq_UserData, NULL, uin, nick, NULL);
}

struct icq_auth {
	icq_Link *link;
	char *nick;
	unsigned long uin;
	struct gaim_connection *gc;
};

static void icq_den_auth(struct icq_auth *iq)
{
	g_free(iq->nick);
	g_free(iq);
}

static void icq_add_after_auth(struct icq_auth *iq)
{
	if (g_slist_find(connections, iq->gc)) {
		char uin[16];
		g_snprintf(uin, sizeof(uin), "%ld", iq->uin);
		show_add_buddy(iq->gc, uin, NULL, iq->nick);
	}
	icq_den_auth(iq);
}

static void icq_acc_auth(struct icq_auth *iq)
{
	if (g_slist_find(connections, iq->gc)) {
		char msg[1024];
		char uin[16];
		struct icq_auth *iqnew;

		icq_SendAuthMsg(iq->link, iq->uin);

		g_snprintf(uin, sizeof(uin), "%ld", iq->uin);
		if (find_buddy(iq->gc->account, uin))
			return;

		iqnew = g_memdup(iq, sizeof(struct icq_auth));
		iqnew->nick = g_strdup(iq->nick);

		g_snprintf(msg, sizeof(msg), "Add %ld to your buddy list?", iq->uin);
		do_ask_dialog(msg, NULL, iqnew, _("Add"), icq_add_after_auth, _("Cancel"), icq_den_auth, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
	}

	icq_den_auth(iq);
}

static void icq_auth_req(icq_Link *link, unsigned long uin, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *first, const char *last, const char *email, const char *reason)
{
	char msg[8192];
	struct icq_auth *iq = g_new0(struct icq_auth, 1);
	iq->link = link;
	iq->nick = g_strdup(nick);
	iq->uin = uin;
	iq->gc = link->icq_UserData;

	g_snprintf(msg, sizeof(msg), "The user %s (%s%s%s%s%s) wants you to authorize them.",
			nick, first ? first : "", first && last ? " " : "", last ? last : "",
			(first || last) && email ? ", " : "", email ? email : "");
	do_ask_dialog(msg, NULL, iq, _("Authorize"), icq_acc_auth, _("Deny"), icq_den_auth, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
}

static void icq_login(struct gaim_account *account) {
	struct gaim_connection *gc = new_gaim_conn(account);
	struct icq_data *id = gc->proto_data = g_new0(struct icq_data, 1);
	icq_Link *link;
	char ps[9];

	gc->checkbox = _("Send message through server");

	icq_LogLevel = ICQ_LOG_MESSAGE;

	g_snprintf(ps, sizeof(ps), "%s", account->password);
	link = id->link = icq_ICQLINKNew(atol(account->username), ps,
			  account->proto_opt[USEROPT_NICK][0] ? account->proto_opt[USEROPT_NICK] : "gaim user",
			  TRUE);
	g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", account->proto_opt[USEROPT_NICK]);

	link->icq_Logged = icq_online;
	link->icq_Disconnected = icq_logged_off;
	link->icq_RecvMessage = icq_msg_incoming;
	link->icq_RecvURL = icq_url_incoming;
	link->icq_RecvWebPager = icq_web_pager;
	link->icq_RecvMailExpress = icq_mail_express;
	link->icq_RecvAdded = icq_recv_add;
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

	if (icq_Connect(link, "icq.mirabilis.com", 4000) < 1) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	id->cur_status = STATUS_ONLINE;
	icq_Login(link, STATUS_ONLINE);

	set_login_progress(gc, 0, "Connecting...");
}

static void icq_close(struct gaim_connection *gc) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	icq_Logout(id->link);
	icq_Disconnect(id->link);
	icq_ICQLINKDelete(id->link);
	g_free(id);
}

static int icq_send_msg(struct gaim_connection *gc, char *who, char *msg, int len, int flags) {
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

static void icq_add_buddy(struct gaim_connection *gc, const char *who) {
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

static void icq_rem_buddy(struct gaim_connection *gc, char *who, char *group) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	icq_ContactRemove(id->link, atol(who));
}

static void icq_set_away(struct gaim_connection *gc, char *state, char *msg) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (!strcmp(state, "Online"))
		icq_ChangeStatus(id->link, STATUS_ONLINE);
	else if (!strcmp(state, "Away")) {
		icq_ChangeStatus(id->link, STATUS_AWAY);
		gc->away = g_strdup("");
	} else if (!strcmp(state, "Do Not Disturb")) {
		icq_ChangeStatus(id->link, STATUS_DND);
		gc->away = g_strdup("");
	} else if (!strcmp(state, "Not Available")) {
		icq_ChangeStatus(id->link, STATUS_NA);
		gc->away = g_strdup("");
	} else if (!strcmp(state, "Occupied")) {
		icq_ChangeStatus(id->link, STATUS_OCCUPIED);
		gc->away = g_strdup("");
	} else if (!strcmp(state, "Free For Chat")) {
		icq_ChangeStatus(id->link, STATUS_FREE_CHAT);
		gc->away = g_strdup("");
	} else if (!strcmp(state, "Invisible")) {
		icq_ChangeStatus(id->link, STATUS_INVISIBLE);
		gc->away = g_strdup("");
	} else if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
		if (msg) {
			icq_ChangeStatus(id->link, STATUS_NA);
			gc->away = g_strdup("");
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

void icq_init(struct prpl *ret) {
	struct proto_user_opt *puo;
	ret->protocol = PROTO_ICQ;
	ret->name = g_strdup("ICQ");
	ret->list_icon = icq_list_icon;
	ret->away_states = icq_away_states;
	ret->buddy_menu = icq_buddy_menu;
	ret->login = icq_login;
	ret->close = icq_close;
	ret->send_im = icq_send_msg;
	ret->add_buddy = icq_add_buddy;
	ret->add_buddies = icq_add_buddies;
	ret->remove_buddy = icq_rem_buddy;
	ret->get_info = icq_get_info;
	ret->set_away = icq_set_away;
	ret->keepalive = icq_keepalive;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup(_("Nick:"));
	puo->def = g_strdup(_("Gaim User"));
	puo->pos = USEROPT_NICK;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	my_protocol = ret;

	icq_SocketNotify = icq_sock_notify;
	icq_SetTimeout = icq_set_timeout;
}

#ifndef STATIC

G_MODULE_EXPORT void gaim_prpl_init(struct prpl *prpl)
{
	icq_init(prpl);
	prpl->plug->desc.api_version = PLUGIN_API_VERSION;
}
#endif
