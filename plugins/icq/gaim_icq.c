#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include "icq.h"   /* well, we're doing ICQ, right? */
#include "multi.h" /* needed for gaim_connection */
#include "prpl.h"  /* needed for prpl */
#include "gaim.h"  /* needed for every other damn thing */

#include "pixmaps/gnomeicu-online.xpm"
#include "pixmaps/gnomeicu-away.xpm"
#include "pixmaps/gnomeicu-dnd.xpm"
#include "pixmaps/gnomeicu-na.xpm"
#include "pixmaps/gnomeicu-occ.xpm"
#include "pixmaps/gnomeicu-ffc.xpm"

#define USEROPT_NICK 0

struct icq_data {
	ICQLINK *link;
	int cur_status;
	int ack_timer;
};

static struct gaim_connection *find_gaim_conn_by_icq_link(ICQLINK *link) {
	GSList *c = connections;
	struct gaim_connection *gc = NULL;
	struct icq_data *id;

	while (c) {
		gc = (struct gaim_connection *)c->data;
		if (gc->protocol == PROTO_ICQ) {
			id = (struct icq_data *)gc->proto_data;
			if (id->link == link)
				break;
		}
		gc = NULL;
		c = c->next;
	}

	return gc;
}

static char *icq_name() {
	return "ICQ";
}

static void icq_do_log(ICQLINK *link, time_t time, unsigned char level, const char *log) {
	debug_printf("ICQ debug %d: %s", level, log);
}

GList *sockets = NULL;
struct gaim_sock {
	int socket;
	gint inpa;
};

static void gaim_icq_handler(gpointer data, gint source, GdkInputCondition cond) {
	if ((cond & GDK_INPUT_READ) || (cond & GDK_INPUT_EXCEPTION))
		icq_HandleReadySocket(source, ICQ_SOCKET_READ);
	if (cond & GDK_INPUT_WRITE)
		icq_HandleReadySocket(source, ICQ_SOCKET_WRITE);
}

static void icq_sock_notify(int socket, int type, int status) {
	struct gaim_sock *gs;
	if (status) {
		GdkInputCondition cond;
		if (type == ICQ_SOCKET_READ)
			cond = GDK_INPUT_READ | GDK_INPUT_EXCEPTION;
		else
			cond = GDK_INPUT_WRITE | GDK_INPUT_EXCEPTION;
		gs = g_new0(struct gaim_sock, 1);
		gs->socket = socket;
		gs->inpa = gdk_input_add(socket, cond, gaim_icq_handler, NULL);
		sockets = g_list_append(sockets, gs);
	} else {
		GList *m = sockets;
		while (m) {
			gs = m->data;
			if (gs->socket == socket)
				break;
			m = g_list_next(m);
		}
		if (m) {
			gdk_input_remove(gs->inpa);
			sockets = g_list_remove(sockets, gs);
			g_free(gs);
		}
	}
}

static void icq_online(ICQLINK *link) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	debug_printf("%s is now online.\n", gc->username);
	account_online(gc);
	gc->options |= OPT_USR_KEEPALV;
	serv_finish_login(gc);

	icq_ContactClear(id->link);
	if (bud_list_cache_exists(gc))
		do_import(NULL, gc);

	icq_ChangeStatus(id->link, STATUS_ONLINE);
}

static void icq_logged_off(ICQLINK *link) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	if (icq_Connect(link, "icq.mirabilis.com", 4000) < 1) {
		hide_login_progress(gc, "Unable to connect");
		signoff(gc);
		return;
	}

	icq_Login(link, STATUS_ONLINE);
	id->cur_status = STATUS_ONLINE;
}

static void icq_msg_incoming(ICQLINK *link, unsigned long uin, unsigned char hour, unsigned char minute,
			unsigned char day, unsigned char month, unsigned short year, const char *data) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	char buf[256], *tmp = g_malloc(BUF_LONG);
	g_snprintf(tmp, BUF_LONG, "%s", data);
	g_snprintf(buf, sizeof buf, "%lu", uin);
	serv_got_im(gc, buf, tmp, 0);
	g_free(tmp);
}

static void icq_user_online(ICQLINK *link, unsigned long uin, unsigned long st,
				unsigned long ip, unsigned short port, unsigned long real_ip,
				unsigned char tcp_flags) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	guint status;
	char buf[256];
	time_t t;

	g_snprintf(buf, sizeof buf, "%lu", uin);
	status = (st == STATUS_ONLINE) ? UC_NORMAL : UC_UNAVAILABLE | (st << 5);
	time(&t);
	serv_got_update(gc, buf, 1, 0, t, 0, status, 0);
}

static void icq_user_offline(ICQLINK *link, unsigned long uin) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	char buf[256]; g_snprintf(buf, sizeof buf, "%lu", uin);
	serv_got_update(gc, buf, 0, 0, 0, 0, 0, 0);
}

static void icq_user_status(ICQLINK *link, unsigned long uin, unsigned long st) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	guint status;
	char buf[256];

	g_snprintf(buf, sizeof buf, "%lu", uin);
	status = (st == STATUS_ONLINE) ? UC_NORMAL : UC_UNAVAILABLE | (st << 5);
	serv_got_update(gc, buf, 1, 0, 0, 0, status, 0);
}

static gint icq_set_timeout_cb(struct icq_data *id) {
	icq_HandleTimeout(id->link);
	id->ack_timer = -1;
	return FALSE;
}

static void icq_set_timeout(ICQLINK *link, long interval) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	struct icq_data *id = (struct icq_data *)gc->proto_data;

	debug_printf("icq_SetTimeout: %ld\n", interval);
	if (interval > 0 && id->ack_timer < 1)
		id->ack_timer = gtk_timeout_add(interval * 1000, (GtkFunction)icq_set_timeout_cb, id);
	else if (id->ack_timer > 0) {
		gtk_timeout_remove(id->ack_timer);
		id->ack_timer = -1;
	}
}

static void icq_url_incoming(struct icq_link *link, unsigned long uin, unsigned char hour,
				unsigned char minute, unsigned char day, unsigned char month,
				unsigned short year, const char *url, const char *descr) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	char *msg = g_malloc(BUF_LONG), buf[256];
	g_snprintf(msg, BUF_LONG, "<A HREF=\"%s\">%s</A>", url, descr);
	g_snprintf(buf, 256, "%lu", uin);
	serv_got_im(gc, buf, msg, 0);
	g_free(msg);
}

static void icq_wrong_passwd(struct icq_link *link) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	hide_login_progress(gc, "Invalid password.");
	signoff(gc);
}

static void icq_invalid_uin(struct icq_link *link) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	hide_login_progress(gc, "Invalid UIN.");
	signoff(gc);
}

static void icq_info_reply(struct icq_link *link, unsigned long uin, const char *nick,
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
	g_show_info_text(buf);
}

static void icq_web_pager(struct icq_link *link, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *email, const char *msg) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	char *who = g_strdup_printf("ICQ Web Pager: %s (%s)", nick, email);
	char *what = g_malloc(BUF_LONG);
	g_snprintf(what, BUF_LONG, "%s", msg);
	serv_got_im(gc, who, what, 0);
	g_free(who);
	g_free(what);
}

static void icq_mail_express(struct icq_link *link, unsigned char hour, unsigned char minute,
		unsigned char day, unsigned char month, unsigned short year, const char *nick,
		const char *email, const char *msg) {
	struct gaim_connection *gc = find_gaim_conn_by_icq_link(link);
	char *who = g_strdup_printf("ICQ Mail Express: %s (%s)", nick, email);
	char *what = g_malloc(BUF_LONG);
	g_snprintf(what, BUF_LONG, "%s", msg);
	serv_got_im(gc, who, what, 0);
	g_free(who);
	g_free(what);
}

static void icq_login(struct aim_user *user) {
	struct gaim_connection *gc = new_gaim_conn(user);
	struct icq_data *id = gc->proto_data = g_new0(struct icq_data, 1);
	ICQLINK *link;

	if (!icq_SocketNotify)
		icq_SocketNotify = icq_sock_notify;

	icq_LogLevel = ICQ_LOG_MESSAGE;

	link = id->link = icq_ICQLINKNew(atol(user->username), user->password,
			  user->proto_opt[USEROPT_NICK][0] ? user->proto_opt[USEROPT_NICK] : "gaim user",
			  TRUE);

	link->icq_Logged = icq_online;
	link->icq_Disconnected = icq_logged_off;
	link->icq_RecvMessage = icq_msg_incoming;
	link->icq_RecvURL = icq_url_incoming;
	link->icq_RecvWebPager = icq_web_pager;
	link->icq_RecvMailExpress = icq_mail_express;
	link->icq_UserOnline = icq_user_online;
	link->icq_UserOffline = icq_user_offline;
	link->icq_UserStatusUpdate = icq_user_status;
	link->icq_InfoReply = icq_info_reply;
	link->icq_WrongPassword = icq_wrong_passwd;
	link->icq_InvalidUIN = icq_invalid_uin;
	link->icq_Log = icq_do_log;
	link->icq_SetTimeout = icq_set_timeout;

	icq_UnsetProxy(link);

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

static struct prpl *my_protocol = NULL;

static void icq_send_msg(struct gaim_connection *gc, char *who, char *msg, int away) {
	struct icq_data *id = (struct icq_data *)gc->proto_data;
	if (!away && (strlen(msg) > 0))
		icq_SendMessage(id->link, atol(who), msg, ICQ_SEND_BESTWAY);
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
	if (uc == UC_NORMAL)
		return icon_online_xpm;
	status = uc >> 5;
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

static void icq_info(GtkObject *obj, char *who) {
	serv_get_info(gtk_object_get_user_data(obj), who);
}

static void icq_action_menu(GtkWidget *menu, struct gaim_connection *gc, char *who) {
	GtkWidget *button;

	button = gtk_menu_item_new_with_label(_("Get Info"));
	gtk_signal_connect(GTK_OBJECT(button), "activate",
			   GTK_SIGNAL_FUNC(icq_info), who);
	gtk_object_set_user_data(GTK_OBJECT(button), gc);
	gtk_menu_append(GTK_MENU(menu), button);
	gtk_widget_show(button);
}

static void icq_print_option(GtkEntry *entry, struct aim_user *user) {
	int entrynum;

	entrynum = (int) gtk_object_get_user_data(GTK_OBJECT(entry));

	if (entrynum == USEROPT_NICK)
		g_snprintf(user->proto_opt[USEROPT_NICK],
				sizeof(user->proto_opt[USEROPT_NICK]),
				"%s", gtk_entry_get_text(entry));
}

static void icq_user_opts(GtkWidget *book, struct aim_user *user) {
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox,
			gtk_label_new("ICQ Options"));
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("Nick");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	entry = gtk_entry_new();
	gtk_box_pack_end(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
	gtk_object_set_user_data(GTK_OBJECT(entry), (void *)USEROPT_NICK);
	gtk_signal_connect(GTK_OBJECT(entry), "changed",
			GTK_SIGNAL_FUNC(icq_print_option), user);
	if (user->proto_opt[USEROPT_NICK][0])
		gtk_entry_set_text(GTK_ENTRY(entry), user->proto_opt[USEROPT_NICK]);
	else
		gtk_entry_set_text(GTK_ENTRY(entry), "gaim user");
	gtk_widget_show(entry);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
}

static GList *icq_away_states() {
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

static void icq_init(struct prpl *ret) {
	ret->protocol = PROTO_ICQ;
	ret->name = icq_name;
	ret->list_icon = icq_list_icon;
	ret->away_states = icq_away_states;
	ret->action_menu = icq_action_menu;
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
}

char *gaim_plugin_init(GModule *handle) {
	load_protocol(icq_init, sizeof(struct prpl));
	return NULL;
}

void gaim_plugin_remove() {
	struct prpl *p = find_prpl(PROTO_ICQ);
	if (p == my_protocol)
		unload_protocol(p);
}

char *name() {
	return "ICQ";
}

char *description() {
	return "Allows gaim to use the ICQ protocol";
}
