/*
 * gaim - Gadu-Gadu Protocol Plugin
 * $Id: gg.c 11638 2004-12-21 01:48:30Z nosnilmot $
 *
 * Copyright (C) 2001 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
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
 *
 */
#include "internal.h"

/* Library from EKG (Eksperymentalny Klient Gadu-Gadu) */
#include "libgg.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "debug.h"
#include "notify.h"
#include "proxy.h"
#include "prpl.h"
#include "server.h"
#include "util.h"
#include "version.h"

#define GG_CONNECT_STEPS 5

#define AGG_BUF_LEN 1024

#define AGG_GENDER_NONE -1

#define AGG_PUBDIR_USERLIST_EXPORT_FORM "/appsvc/fmcontactsput.asp"
#define AGG_PUBDIR_USERLIST_IMPORT_FORM "/appsvc/fmcontactsget.asp"
#define AGG_PUBDIR_SEARCH_FORM "/appsvc/fmpubquery2.asp"
#define AGG_REGISTER_DATA_FORM "/appsvc/fmregister.asp"
#define AGG_PUBDIR_MAX_ENTRIES 200

#define AGG_STATUS_AVAIL              _("Available")
#define AGG_STATUS_AVAIL_FRIENDS      _("Available for friends only")
#define AGG_STATUS_BUSY               _("Away")
#define AGG_STATUS_BUSY_FRIENDS       _("Away for friends only")
#define AGG_STATUS_INVISIBLE          _("Invisible")
#define AGG_STATUS_INVISIBLE_FRIENDS  _("Invisible for friends only")
#define AGG_STATUS_NOT_AVAIL          _("Unavailable")

#define AGG_HTTP_NONE			0
#define AGG_HTTP_SEARCH			1
#define AGG_HTTP_USERLIST_IMPORT	2
#define AGG_HTTP_USERLIST_EXPORT	3
#define AGG_HTTP_USERLIST_DELETE	4
#define AGG_HTTP_PASSWORD_CHANGE	5

#define UC_NORMAL 2

struct agg_data {
	struct gg_session *sess;
	int own_status;
};

struct agg_http {
	GaimConnection *gc;
	gchar *request;
	gchar *form;
	gchar *host;
	int inpa;
	int type;
};


static gchar *charset_convert(const gchar *locstr, const char *encsrc, const char *encdst)
{
	return (g_convert (locstr, strlen (locstr), encdst, encsrc, NULL, NULL, NULL));
}

static gboolean invalid_uin(const char *uin)
{
	unsigned long int res = strtol(uin, (char **)NULL, 10);
	if (res == ULONG_MAX || res == 0)
		return TRUE;
	return FALSE;
}

static gint args_compare(gconstpointer a, gconstpointer b)
{
	return g_ascii_strcasecmp((const gchar *)a,(const gchar *)b);
}

static gboolean allowed_uin(GaimConnection *gc, char *uin)
{
	GaimAccount *account = gaim_connection_get_account(gc);

	switch (account->perm_deny) {
	case 1:
		/* permit all, deny none */
		return TRUE;
		break;
	case 2:
		/* deny all, permit none. */
		return FALSE;
		break;
	case 3:
		/* permit some. */
		if (g_slist_find_custom(gc->account->permit, uin, args_compare))
			return TRUE;
		return FALSE;
		break;
	case 4:
		/* deny some. */
		if (g_slist_find_custom(gc->account->deny, uin, args_compare))
			return FALSE;
		return TRUE;
		break;
	default:
		return TRUE;
		break;
	}
}

static char *handle_errcode(GaimConnection *gc, int errcode)
{
	static char msg[AGG_BUF_LEN];

	switch (errcode) {
	case GG_FAILURE_RESOLVING:
		g_snprintf(msg, sizeof(msg), _("Unable to resolve hostname."));
		break;
	case GG_FAILURE_CONNECTING:
		g_snprintf(msg, sizeof(msg), _("Unable to connect to server."));
		break;
	case GG_FAILURE_INVALID:
		g_snprintf(msg, sizeof(msg), _("Invalid response from server."));
		break;
	case GG_FAILURE_READING:
		g_snprintf(msg, sizeof(msg), _("Error while reading from socket."));
		break;
	case GG_FAILURE_WRITING:
		g_snprintf(msg, sizeof(msg), _("Error while writing to socket."));
		break;
	case GG_FAILURE_PASSWORD:
		g_snprintf(msg, sizeof(msg), _("Authentication failed."));
		break;
	default:
		g_snprintf(msg, sizeof(msg), _("Unknown Error Code."));
		break;
	}

	gaim_connection_error(gc, msg);

	return msg;
}

static void agg_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	int status_num = gd->own_status;
	const char *status_id;
	char *msg = NULL;

	status_id = gaim_status_get_id(status);

	if (!strcmp(status_id, "available"))
		status_num = GG_STATUS_AVAIL;
	else if (!strcmp(status_id, "available-friends"))
		status_num = GG_STATUS_AVAIL | GG_STATUS_FRIENDS_MASK;
	else if (!strcmp(status_id, "away"))
		status_num = GG_STATUS_BUSY;
	else if (!strcmp(status_id, "away-friends"))
		status_num = GG_STATUS_BUSY | GG_STATUS_FRIENDS_MASK;
	else if (!strcmp(status_id, "invisible"))
		status_num = GG_STATUS_INVISIBLE;
	else if (!strcmp(status_id, "invisible-friends"))
		status_num = GG_STATUS_INVISIBLE | GG_STATUS_FRIENDS_MASK;
	else if (!strcmp(status_id, "unavailable"))
		status_num = GG_STATUS_NOT_AVAIL;
	else
		/* Don't need to do anything */
		return;

	/* XXX: this was added between the status_rewrite and now and needs to be fixed */
	if (msg != NULL) {
	    switch (status_num) {
		case GG_STATUS_AVAIL:
		    status_num = GG_STATUS_AVAIL_DESCR;
		    break;
		case GG_STATUS_BUSY:
		    status_num = GG_STATUS_BUSY_DESCR;
		    break;
		case GG_STATUS_INVISIBLE:
		    status_num = GG_STATUS_INVISIBLE_DESCR;
		    break;
		case GG_STATUS_NOT_AVAIL:
		    status_num = GG_STATUS_NOT_AVAIL_DESCR;
		    break;
	    }
	}

	gd->own_status = status_num;

	if (msg)
	    gg_change_status_descr(gd->sess, status_num, msg);
	else
	    gg_change_status(gd->sess, status_num);
}


#if 0
static void agg_get_away(GaimConnection *gc, const char *who)
{
    GaimBuddy *buddy;
    char *dialog_msg, **splitmsg;

    if (invalid_uin(who))
		return;
    
    buddy = gaim_find_buddy(gaim_connection_get_account(gc), who);
    if (buddy->proto_data) {
		/* Split at (carriage return/newline)'s, then rejoin later with BRs between. */
		splitmsg = g_strsplit(buddy->proto_data, "\r\n", 0);
    
		dialog_msg = g_strdup_printf(_("<B>UIN:</B> %s<BR><B>Status:</B> %s<HR>%s"), who, (char *)buddy->proto_data, g_strjoinv("<BR>", splitmsg));
		gaim_notify_userinfo(gc, who, NULL, _("Buddy Information"), buddy->proto_data, dialog_msg, NULL, NULL);
    }
}
#endif

static gchar *get_away_text(GaimBuddy *buddy)
{
	GaimPresence *presence = gaim_buddy_get_presence(buddy);

	if (gaim_presence_is_status_active(presence, "available"))
		return AGG_STATUS_AVAIL;
	else if (gaim_presence_is_status_active(presence, "available-friends"))
		return AGG_STATUS_AVAIL_FRIENDS;
	else if (gaim_presence_is_status_active(presence, "away"))
		return AGG_STATUS_BUSY;
	else if (gaim_presence_is_status_active(presence, "away-friends"))
		return AGG_STATUS_BUSY_FRIENDS;
	else if (gaim_presence_is_status_active(presence, "invisible"))
  		return AGG_STATUS_INVISIBLE;
	else if (gaim_presence_is_status_active(presence, "invisible-friends"))
  		return AGG_STATUS_INVISIBLE_FRIENDS;
	else if (gaim_presence_is_status_active(presence, "unavailable"))
		return AGG_STATUS_NOT_AVAIL;

	return AGG_STATUS_AVAIL;
}


static GList *agg_status_types(GaimAccount *account)
{
	GaimStatusType *type;
	GList *types = NULL;

	type = gaim_status_type_new(GAIM_STATUS_OFFLINE, "offline",
								_("Offline"), FALSE);
	types = g_list_append(types, type);

	type = gaim_status_type_new(GAIM_STATUS_ONLINE, "online",
								_("Online"), FALSE);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_AVAILABLE, "available", AGG_STATUS_AVAIL,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_AWAY, "away", AGG_STATUS_BUSY,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_HIDDEN, "invisible", AGG_STATUS_INVISIBLE,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_AVAILABLE, "available-friends", AGG_STATUS_AVAIL_FRIENDS,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_AWAY, "away-friends", AGG_STATUS_BUSY_FRIENDS,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_HIDDEN, "invisible-friends", AGG_STATUS_INVISIBLE_FRIENDS,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_UNAVAILABLE, "unavailable", AGG_STATUS_NOT_AVAIL,
		TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	return types;
}

/* Enhance these functions, more options and such stuff */
static GList *agg_buddy_menu(GaimBuddy *buddy)
{
	GList *m = NULL;
	GaimBlistNodeAction *act;

	static char buf[AGG_BUF_LEN];
	g_snprintf(buf, sizeof(buf), _("Status: %s"), get_away_text(buddy));

	/* um... this seems silly. since in this pass, I'm only converting
	   over the menu building, I'm not going to mess with it though */
	/* XXX: shouldn't this be in the tooltip instead? */
	act = gaim_blist_node_action_new(buf, NULL, NULL);
	m = g_list_append(m, act);

	return m;
}


static GList *
agg_blist_node_menu(GaimBlistNode *node)
{
	if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		return agg_buddy_menu((GaimBuddy *) node);
	} else {
		return NULL;
	}
}


static void agg_load_buddy_list(GaimConnection *gc, char *buddylist)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	gchar *ptr = buddylist;
	gchar **users_tbl;
	int i;
	uin_t *userlist = NULL;
	int userlist_size = 0;

	users_tbl = g_strsplit(ptr, "\r\n", AGG_PUBDIR_MAX_ENTRIES);

	/* Parse array of Buddies List */
	for (i = 0; users_tbl[i] != NULL; i++) {
		gchar **data_tbl;
		gchar *name, *show;

		if (strlen(users_tbl[i])==0) {
			gaim_debug(GAIM_DEBUG_MISC, "gg",
					   "import_buddies_server_results: users_tbl[i] is empty\n");
			continue;
		}

		data_tbl = g_strsplit(users_tbl[i], ";", 8);

		show = charset_convert(data_tbl[3], "CP1250", "UTF-8");
		name = data_tbl[6];

		if (invalid_uin(name)) {
			continue;
		}

		gaim_debug(GAIM_DEBUG_MISC, "gg",
				   "import_buddies_server_results: uin: %s\n", name);
		if (!gaim_find_buddy(gc->account, name)) {
			GaimBuddy *b;
			GaimGroup *g;
			/* Default group if none specified on server */
			gchar *group = g_strdup("Gadu-Gadu");
			if (strlen(data_tbl[5])) {
				gchar **group_tbl = g_strsplit(data_tbl[5], ",", 2);
				if (strlen(group_tbl[0])) {
					g_free(group);
					group = g_strdup(group_tbl[0]);
				}
				g_strfreev(group_tbl);
			}
			/* Add Buddy to our userlist */
			if (!(g = gaim_find_group(group))) {
				g = gaim_group_new(group);
				gaim_blist_add_group(g, NULL);
			}
			b = gaim_buddy_new(gc->account, name, strlen(show) ? show : NULL);
			gaim_blist_add_buddy(b,NULL,g,NULL);

			userlist_size++;
			userlist = g_renew(uin_t, userlist, userlist_size);
			userlist[userlist_size - 1] =
			    (uin_t) strtol((char *)name, (char **)NULL, 10);

			g_free(group);
		}
		g_free(show);
		g_strfreev(data_tbl);
	}
	g_strfreev(users_tbl);

	if (userlist) {
	    gg_notify(gd->sess, userlist, userlist_size);
	    g_free(userlist);
	}
}

static void agg_save_buddy_list(GaimConnection *gc, char *existlist)
{
    struct agg_data *gd = (struct agg_data *)gc->proto_data;
    GaimBlistNode *gnode, *cnode, *bnode;
    char *buddylist = g_strdup(existlist ? existlist : "");
    char *ptr;

    for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		GaimGroup *g = (GaimGroup *)gnode;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *b = (GaimBuddy *)bnode;

				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				if(b->account == gc->account) {
					gchar *newdata;
					/* GG Number */
					gchar *name = b->name;
					/* GG Pseudo */
					gchar *show = b->alias ? b->alias : b->name;
					/* Group Name */
					gchar *gname = g->name;

					newdata = g_strdup_printf("%s;%s;%s;%s;%s;%s;%s;%s%s\r\n",
							show, show, show, show, "", gname, name, "", "");

					ptr = buddylist;
					buddylist = g_strconcat(ptr, newdata, NULL);

					g_free(newdata);
					g_free(ptr);
				}
			}
		}
	}

	/* save the list to the gadu gadu server */
	gg_userlist_request(gd->sess, GG_USERLIST_PUT, buddylist);
}

static void main_callback(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	GaimAccount *account = gaim_connection_get_account(gc);
	struct agg_data *gd = gc->proto_data;
	struct gg_event *e;

	gaim_debug(GAIM_DEBUG_INFO, "gg", "main_callback enter: begin\n");

	if (gd->sess->fd != source)
		gd->sess->fd = source;

	if (source == 0) {
		gaim_connection_error(gc, _("Could not connect"));
		return;
	}

	if (!(e = gg_watch_fd(gd->sess))) {
		gaim_debug(GAIM_DEBUG_ERROR, "gg",
				   "main_callback: gg_watch_fd failed - CRITICAL!\n");
		gaim_connection_error(gc, _("Unable to read socket"));
		return;
	}

	switch (e->type) {
	case GG_EVENT_NONE:
		/* do nothing */
		break;
	case GG_EVENT_CONN_SUCCESS:
		gaim_debug(GAIM_DEBUG_WARNING, "gg",
				   "main_callback: CONNECTED AGAIN!?\n");
		break;
	case GG_EVENT_CONN_FAILED:
		if (gc->inpa)
			gaim_input_remove(gc->inpa);
		handle_errcode(gc, e->event.failure);
		break;
	case GG_EVENT_MSG:
		{
			gchar *imsg;
			gchar *jmsg;
			gchar user[20];

			g_snprintf(user, sizeof(user), "%lu", e->event.msg.sender);
			if (!allowed_uin(gc, user))
				break;
			imsg = charset_convert(e->event.msg.message, "CP1250", "UTF-8");
			gaim_str_strip_cr(imsg);
			jmsg = gaim_escape_html(imsg);
			/* e->event.msg.time - we don't know what this time is for */
			serv_got_im(gc, user, jmsg, 0, time(NULL));
			g_free(imsg);
			g_free(jmsg);
		}
		break;
	case GG_EVENT_NOTIFY:
		{
			gchar user[20];
			struct gg_notify_reply *n = e->event.notify;
			const char *status_id;

			while (n->uin) {
				switch (n->status) {
					case GG_STATUS_NOT_AVAIL:
						status_id = "unavailable";
						break;

					case GG_STATUS_AVAIL:
						status_id = "available";
						break;

					case GG_STATUS_BUSY:
						status_id = "away";
						break;

					case GG_STATUS_INVISIBLE:
						status_id = "invisible";
						break;

					default:
						status_id = "available";
						break;

				}

				g_snprintf(user, sizeof(user), "%lu", n->uin);
				gaim_prpl_got_user_status(account, user, status_id, NULL);
				n++;
			}
		}
		break;
	case GG_EVENT_NOTIFY60:
		{
			gchar user[20];
			const char *status_id;
			guint i = 0;

			for (i = 0; e->event.notify60[i].uin; i++) {
				GaimBuddy *buddy;
				
				g_snprintf(user, sizeof(user), "%lu", (long unsigned int)e->event.notify60[i].uin);

				buddy = gaim_find_buddy(gaim_connection_get_account(gc), user);
				if (buddy && buddy->proto_data != NULL) {
					g_free(buddy->proto_data);
					buddy->proto_data = NULL;
				}
			
				switch (e->event.notify60[i].status) {
					case GG_STATUS_NOT_AVAIL:
					case GG_STATUS_NOT_AVAIL_DESCR:
						status_id = "unavailable";
						break;

					case GG_STATUS_AVAIL:
					case GG_STATUS_AVAIL_DESCR:
						status_id = "available";
						break;

					case GG_STATUS_BUSY:
					case GG_STATUS_BUSY_DESCR:
						status_id = "away";
						break;

					case GG_STATUS_INVISIBLE:
					case GG_STATUS_INVISIBLE_DESCR:
						status_id = "invisible";
						break;

					default:
						status_id = "available";
						break;
				}
				
				if (buddy && e->event.notify60[i].descr != NULL) {
					buddy->proto_data = g_strdup(e->event.notify60[i].descr);
				}

				gaim_prpl_got_user_status(account, user, status_id, NULL);
				i++;
			}
		}
		break;
	case GG_EVENT_STATUS:
		{
			gchar user[20];
			const char *status_id;

			switch (e->event.status.status) {
 				case GG_STATUS_NOT_AVAIL:
					status_id = "unavailable";
					break;

				case GG_STATUS_AVAIL:
					status_id = "available";
					break;

				case GG_STATUS_BUSY:
					status_id = "away";
					break;

				case GG_STATUS_INVISIBLE:
					status_id = "invisible";
					break;

				default:
					status_id = "available";
					break;
			}

			g_snprintf(user, sizeof(user), "%lu", e->event.status.uin);
			gaim_prpl_got_user_status(account, user, status_id, NULL);
		}
		break;
	case GG_EVENT_STATUS60:
		{
			gchar user[20];
			const char *status_id;

			GaimBuddy *buddy;
				
			g_snprintf(user, sizeof(user), "%lu", e->event.status60.uin);

			buddy = gaim_find_buddy(gaim_connection_get_account(gc), user);
			if (buddy && buddy->proto_data != NULL) {
				g_free(buddy->proto_data);
				buddy->proto_data = NULL;
			}

			switch (e->event.status60.status) {
				case GG_STATUS_NOT_AVAIL_DESCR:
				case GG_STATUS_NOT_AVAIL:
					status_id = "unavailable";
					break;

				case GG_STATUS_AVAIL:
				case GG_STATUS_AVAIL_DESCR:
					status_id = "available";
					break;

				case GG_STATUS_BUSY:
				case GG_STATUS_BUSY_DESCR:
					status_id = "away";
					break;

				case GG_STATUS_INVISIBLE:
				case GG_STATUS_INVISIBLE_DESCR:
					status_id = "invisible";
					break;

				default:
					status_id = "available";
					break;
			}
 
			if (buddy && e->event.status60.descr != NULL) {
				buddy->proto_data = g_strdup(e->event.status60.descr);
			}

			gaim_prpl_got_user_status(account, user, status_id, NULL);
		}
		break;
	case GG_EVENT_ACK:
		gaim_debug(GAIM_DEBUG_MISC, "gg",
				   "main_callback: message %d to %lu sent with status %d\n",
			     e->event.ack.seq, e->event.ack.recipient, e->event.ack.status);
		break;
	case GG_EVENT_USERLIST:
	{
	    gaim_debug(GAIM_DEBUG_MISC, "gg", "main_callback: received userlist reply\n");
	    switch (e->event.userlist.type) {
		case GG_USERLIST_GET_REPLY:
		{

			if (e->event.userlist.reply) {
			    agg_load_buddy_list(gc, e->event.userlist.reply);
			}
			break;
		}

		case GG_USERLIST_PUT_REPLY:
		{
		    /* ignored */
		}
	    }
	}

	default:
		gaim_debug(GAIM_DEBUG_ERROR, "gg",
				   "main_callback: unsupported event %d\n", e->type);
		break;
	}
	gg_free_event(e);
}

void login_callback(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct agg_data *gd = gc->proto_data;
	struct gg_event *e;

	gaim_debug(GAIM_DEBUG_INFO, "gg", "login_callback...\n");
	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		return;
	}

	gaim_debug(GAIM_DEBUG_INFO, "gg", "Found GG connection\n");

	if (source == 0) {
		gaim_connection_error(gc, _("Unable to connect."));
		return;
	}

	gd->sess->fd = source;

	gaim_debug(GAIM_DEBUG_MISC, "gg", "Source is valid.\n");
	if (gc->inpa == 0) {
		gaim_debug(GAIM_DEBUG_MISC, "gg",
				   "login_callback.. checking gc->inpa .. is 0.. setting fd watch\n");
		gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, login_callback, gc);
		gaim_debug(GAIM_DEBUG_INFO, "gg", "Adding watch on fd\n"); 
	}
	gaim_debug(GAIM_DEBUG_INFO, "gg", "Checking State.\n");
	switch (gd->sess->state) {
	case GG_STATE_READING_DATA:
		gaim_connection_update_progress(gc, _("Reading data"), 1, GG_CONNECT_STEPS);
		break;
	case GG_STATE_CONNECTING_GG:
		gaim_connection_update_progress(gc, _("Balancer handshake"), 2, GG_CONNECT_STEPS);
		break;
	case GG_STATE_READING_KEY:
		gaim_connection_update_progress(gc, _("Reading server key"), 3, GG_CONNECT_STEPS);
		break;
	case GG_STATE_READING_REPLY:
		gaim_connection_update_progress(gc, _("Exchanging key hash"), 4, GG_CONNECT_STEPS);
		break;
	default:
		gaim_debug(GAIM_DEBUG_INFO, "gg", "No State found\n");
		break;
	}
	gaim_debug(GAIM_DEBUG_MISC, "gg", "gg_watch_fd\n");
	if (!(e = gg_watch_fd(gd->sess))) {
		gaim_debug(GAIM_DEBUG_ERROR, "gg",
				   "login_callback: gg_watch_fd failed - CRITICAL!\n");
		gaim_connection_error(gc, _("Critical error in GG library\n"));
		return;
	}

	/* If we are GG_STATE_CONNECTING_GG then we still need to connect, as
	   we could not use gaim_proxy_connect in libgg.c */
	switch( gd->sess->state ) {
	case GG_STATE_CONNECTING_GG:
		{
			struct in_addr ip;
			char buf[256];

			/* Remove watch on initial socket - now that we have ip and port of login server */
			gaim_input_remove(gc->inpa);

			ip.s_addr = gd->sess->server_ip;

			if (gaim_proxy_connect(gc->account, inet_ntoa(ip), gd->sess->port, login_callback, gc) < 0) {
				g_snprintf(buf, sizeof(buf), _("Connect to %s failed"), inet_ntoa(ip));
				gaim_connection_error(gc, buf);
				return;
			}
			break;
		}
	case GG_STATE_READING_KEY:
		/* Set new watch on login server ip */
		if(gc->inpa)
			gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, login_callback, gc);
		gaim_debug(GAIM_DEBUG_INFO, "gg",
				   "Setting watch on connection with login server.\n"); 
		break;
	}/* end switch() */

	gaim_debug(GAIM_DEBUG_MISC, "gg", "checking gg_event\n");
	switch (e->type) {
	case GG_EVENT_NONE:
		/* nothing */
		break;
	case GG_EVENT_CONN_SUCCESS:
		/* Setup new input handler */
		if (gc->inpa)
			gaim_input_remove(gc->inpa);
		gc->inpa = gaim_input_add(gd->sess->fd, GAIM_INPUT_READ, main_callback, gc);

		/* Our signon is complete */
		gaim_connection_set_state(gc, GAIM_CONNECTED);
		serv_finish_login(gc);

		break;
	case GG_EVENT_CONN_FAILED:
		gaim_input_remove(gc->inpa);
		gc->inpa = 0;
		handle_errcode(gc, e->event.failure);
		break;
	default:
		gaim_debug(GAIM_DEBUG_MISC, "gg", "no gg_event\n");
		break;
	}
	gaim_debug(GAIM_DEBUG_INFO, "gg", "Returning from login_callback\n");
	gg_free_event(e);
}

static void agg_keepalive(GaimConnection *gc)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (gg_ping(gd->sess) < 0) {
		gaim_connection_error(gc, _("Unable to ping server"));
		return;
	}
}

static void agg_login(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	struct agg_data *gd = gc->proto_data = g_new0(struct agg_data, 1);
	char buf[80];

#if 0
	gc->checkbox = _("Send as message");
#endif

	gd->sess = g_new0(struct gg_session, 1);

	gaim_connection_update_progress(gc, _("Looking up GG server"), 0, GG_CONNECT_STEPS);

	if (invalid_uin(account->username)) {
		gaim_connection_error(gc, _("Invalid Gadu-Gadu UIN specified"));
		return;
	}

	gc->inpa = 0;

	/*
	   if (gg_login(gd->sess, strtol(user->username, (char **)NULL, 10), user->password, 1) < 0) {
	   gaim_debug(GAIM_DEBUG_MISC, "gg", "uin=%u, pass=%s", strtol(user->username, (char **)NULL, 10), user->password); 
	   gaim_connection_error(gc, _("Unable to connect."));
	   signoff(gc);
	   return;
	   }

	   gg_login() sucks for me, so I'm using gaim_proxy_connect()
	 */

	gd->sess->uin = (uin_t) strtol(account->username, (char **)NULL, 10);
	gd->sess->password = g_strdup(account->password);
	gd->sess->state = GG_STATE_CONNECTING;
	gd->sess->check = GG_CHECK_WRITE;
	gd->sess->async = 1;
	if (gaim_proxy_connect(account, GG_APPMSG_HOST, GG_APPMSG_PORT, login_callback, gc) < 0) {
		g_snprintf(buf, sizeof(buf), _("Connect to %s failed"), GG_APPMSG_HOST);
		gaim_connection_error(gc, buf);
		return;
	}
}

static void agg_close(GaimConnection *gc)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (gc->inpa)
		gaim_input_remove(gc->inpa);
	gg_logoff(gd->sess);
	gd->own_status = GG_STATUS_NOT_AVAIL;
	gg_free_session(gd->sess);
	g_free(gc->proto_data);
}

static int agg_send_im(GaimConnection *gc, const char *who, const char *msg, GaimConvImFlags flags)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	gchar *imsg;

	if (invalid_uin(who)) {
		gaim_notify_error(gc, NULL,
						  _("You are trying to send a message to an "
							"invalid Gadu-Gadu UIN."), NULL);
		return -1;
	}

	if (strlen(msg) > 0) {
		imsg = charset_convert(msg, "UTF-8", "CP1250");
		if (gg_send_message(gd->sess, GG_CLASS_CHAT,
				    strtol(who, (char **)NULL, 10), imsg) < 0)
			return -1;
		g_free(imsg);
	}
	return 1;
}

static void agg_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (invalid_uin(buddy->name))
		return;
	gg_add_notify(gd->sess, strtol(buddy->name, (char **)NULL, 10));
	agg_save_buddy_list(gc, NULL);
}

static void agg_rem_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	if (invalid_uin(buddy->name))
		return;
	gg_remove_notify(gd->sess, strtol(buddy->name, (char **)NULL, 10));
	agg_save_buddy_list(gc, NULL);
}

static void agg_add_buddies(GaimConnection *gc, GList *buddies, GList *groups)
{
	struct agg_data *gd = (struct agg_data *)gc->proto_data;
	uin_t *userlist = NULL;
	int userlist_size = 0;

	while (buddies) {
		GaimBuddy *buddy = buddies->data;
		if (!invalid_uin(buddy->name)) {
			userlist_size++;
			userlist = g_renew(uin_t, userlist, userlist_size);
			userlist[userlist_size - 1] =
			    (uin_t) strtol(buddy->name, (char **)NULL, 10);
		}
		buddies = g_list_next(buddies);
	}

	if (userlist) {
		gg_notify(gd->sess, userlist, userlist_size);
		g_free(userlist);
	}

	agg_save_buddy_list(gc, NULL);
}

static void agg_buddy_free (GaimBuddy *buddy)
{
    if (buddy->proto_data) {
	g_free(buddy->proto_data);
	buddy->proto_data = NULL;
    }
}

static void search_results(GaimConnection *gc, gchar *webdata)
{
	gchar **webdata_tbl;
	gchar *buf;
	char *ptr;
	int i, j;

	if ((ptr = strstr(webdata, "query_results:")) == NULL || (ptr = strchr(ptr, '\n')) == NULL) {
		gaim_debug(GAIM_DEBUG_MISC, "gg", "search_callback: pubdir result [%s]\n", webdata);
		gaim_notify_error(gc, NULL, _("Couldn't get search results"), NULL);
		return;
	}
	ptr++;

	buf = g_strconcat("<B>", _("Gadu-Gadu Search Engine"), "</B><BR>\n", NULL);

	webdata_tbl = g_strsplit(ptr, "\n", AGG_PUBDIR_MAX_ENTRIES);

	j = 0;

	/* Parse array */
	/* XXX - Make this use a GString */
	for (i = 0; webdata_tbl[i] != NULL; i++) {
		gchar *p, *oldibuf;
		static gchar *ibuf;

		g_strdelimit(webdata_tbl[i], "\t\n", ' ');

		/* GG_PUBDIR_HOST service returns 7 lines of data per directory entry */
		if (i % 8 == 0)
			j = 0;

		p = charset_convert(g_strstrip(webdata_tbl[i]), "CP1250", "UTF-8");

		oldibuf = ibuf;

		switch (j) {
		case 0:
			ibuf = g_strconcat("---------------------------------<BR>\n", NULL);
			oldibuf = ibuf;
			ibuf = g_strconcat(oldibuf, "<B>", _("Active"), ":</B> ",
					   (atoi(p) == 2) ? _("Yes") : _("No"), "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 1:
			ibuf = g_strconcat(oldibuf, "<B>", _("UIN"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 2:
			ibuf = g_strconcat(oldibuf, "<B>", _("First Name"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 3:
			ibuf =
			    g_strconcat(oldibuf, "<B>", _("Last Name"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 4:
			ibuf = g_strconcat(oldibuf, "<B>", _("Nick"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 5:
			/* Hack, invalid_uin does what we really want here but may change in future */
			if (invalid_uin(p))
				ibuf =
				    g_strconcat(oldibuf, "<B>", _("Birth Year"), ":</B> <BR>\n", NULL);
			else
				ibuf =
				    g_strconcat(oldibuf, "<B>", _("Birth Year"), ":</B> ", p, "<BR>\n",
						NULL);
			g_free(oldibuf);
			break;
		case 6:
			if (atoi(p) == GG_GENDER_FEMALE)
				ibuf = g_strconcat(oldibuf, "<B>", _("Sex"), ":</B> woman<BR>\n", NULL);
			else if (atoi(p) == GG_GENDER_MALE)
				ibuf = g_strconcat(oldibuf, "<B>", _("Sex"), ":</B> man<BR>\n", NULL);
			else
				ibuf = g_strconcat(oldibuf, "<B>", _("Sex"), ":</B> <BR>\n", NULL);
			g_free(oldibuf);
			break;
		case 7:
			ibuf = g_strconcat(oldibuf, "<B>", _("City"), ":</B> ", p, "<BR>\n", NULL);
			g_free(oldibuf);

			/* We have all lines, so add them to buffer */
			{
				gchar *oldbuf = buf;
				buf = g_strconcat(oldbuf, ibuf, NULL);
				g_free(oldbuf);
			}

			g_free(ibuf);
			break;
		}

		g_free(p);

		j++;
	}

	g_strfreev(webdata_tbl);

	gaim_notify_formatted(gc, NULL, _("Buddy Information"), NULL,
						  buf, NULL, NULL);

	g_free(buf);
}

static void
change_pass(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	GaimAccount *account = gaim_connection_get_account(gc);
	gaim_account_request_change_password(account);
}

#if 0
static void import_buddies_server_results(GaimConnection *gc, gchar *webdata)
{
	gchar *ptr;
	gchar **users_tbl;
	int i;
	if (strstr(webdata, "no_data:")) {
		gaim_notify_error(gc, NULL,
						  _("There is no Buddy List stored on the "
							"Gadu-Gadu server."), NULL);
		return;
	}

	if ((ptr = strstr(webdata, "get_results:")) == NULL || (ptr = strchr(ptr, ':')) == NULL) {
		gaim_debug(GAIM_DEBUG_MISC, "gg", "import_buddies_server_results: import buddies result [%s]\n", webdata);
		gaim_notify_error(gc, NULL,
						  _("Couldn't Import Buddy List from Server"), NULL);
		return;
	}
	ptr++;

	users_tbl = g_strsplit(ptr, "\n", AGG_PUBDIR_MAX_ENTRIES);

	/* Parse array of Buddies List */
	for (i = 0; users_tbl[i] != NULL; i++) {
		gchar **data_tbl;
		gchar *name, *show;

		if (strlen(users_tbl[i])==0) {
			gaim_debug(GAIM_DEBUG_MISC, "gg",
					   "import_buddies_server_results: users_tbl[i] is empty\n");
			continue;
		}

		g_strdelimit(users_tbl[i], "\r\t\n\015", ' ');
		data_tbl = g_strsplit(users_tbl[i], ";", 8);

		show = charset_convert(data_tbl[3], "CP1250", "UTF-8");
		name = data_tbl[6];

		if (invalid_uin(name)) {
			continue;
		}

		gaim_debug(GAIM_DEBUG_MISC, "gg",
				   "import_buddies_server_results: uin: %s\n", name);
		if (!gaim_find_buddy(gc->account, name)) {
			GaimBuddy *b;
			GaimGroup *g;
			/* Default group if none specified on server */
			gchar *group = g_strdup("Gadu-Gadu");
			if (strlen(data_tbl[5])) {
				gchar **group_tbl = g_strsplit(data_tbl[5], ",", 2);
				if (strlen(group_tbl[0])) {
					g_free(group);
					group = g_strdup(group_tbl[0]);
				}
				g_strfreev(group_tbl);
			}
			/* Add Buddy to our userlist */
			if (!(g = gaim_find_group(group))) {
				g = gaim_group_new(group);
				gaim_blist_add_group(g, NULL);
			}
			b = gaim_buddy_new(gc->account, name, strlen(show) ? show : NULL);
			gaim_blist_add_buddy(b, NULL, g, NULL);
			g_free(group);
		}
		g_free(show);
		g_strfreev(data_tbl);
	}
	g_strfreev(users_tbl);
}

static void export_buddies_server_results(GaimConnection *gc, gchar *webdata)
{
	if (strstr(webdata, "put_success:")) {
		gaim_notify_info(gc, NULL,
						 _("Buddy List successfully transferred to "
						   "Gadu-Gadu server"), NULL);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "gg",
			   "export_buddies_server_results: webdata [%s]\n", webdata);
	gaim_notify_error(gc, NULL,
					  _("Couldn't transfer Buddy List to Gadu-Gadu server"),
					  NULL);
}

static void delete_buddies_server_results(GaimConnection *gc, gchar *webdata)
{
	if (strstr(webdata, "put_success:")) {
		gaim_notify_info(gc, NULL,
						 _("Buddy List successfully deleted from "
						   "Gadu-Gadu server"), NULL);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "gg",
			   "delete_buddies_server_results: webdata [%s]\n", webdata);
	gaim_notify_error(gc, NULL,
					  _("Couldn't delete Buddy List from Gadu-Gadu server"),
					  NULL);
}
#endif

static void password_change_server_results(GaimConnection *gc, gchar *webdata)
{
	if (strstr(webdata, "reg_success:")) {
		gaim_notify_info(gc, NULL,
						 _("Password changed successfully"), NULL);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "gg",
			   "password_change_server_results: webdata [%s]\n", webdata);
	gaim_notify_error(gc, NULL,
					  _("Password couldn't be changed"), NULL);
}

static void http_results(gpointer data, gint source, GaimInputCondition cond)
{
	struct agg_http *hdata = data;
	GaimConnection *gc = hdata->gc;
	char *webdata;
	int len;
	char read_data;

	gaim_debug(GAIM_DEBUG_INFO, "gg", "http_results: begin\n");

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_debug(GAIM_DEBUG_ERROR, "gg",
				   "search_callback: g_slist_find error\n");
		gaim_input_remove(hdata->inpa);
		g_free(hdata);
		close(source);
		return;
	}

	webdata = NULL;
	len = 0;

	while (read(source, &read_data, 1) > 0 || errno == EWOULDBLOCK) {
		if (errno == EWOULDBLOCK) {
			errno = 0;
			continue;
		}

		if (!read_data)
			continue;

		len++;
		webdata = g_realloc(webdata, len);
		webdata[len - 1] = read_data;
	}

	webdata = g_realloc(webdata, len + 1);
	webdata[len] = 0;

	gaim_input_remove(hdata->inpa);
	close(source);

	gaim_debug(GAIM_DEBUG_MISC, "gg",
			   "http_results: type %d, webdata [%s]\n", hdata->type, webdata);

	switch (hdata->type) {
	case AGG_HTTP_SEARCH:
		search_results(gc, webdata);
		break;
#if 0
	case AGG_HTTP_USERLIST_IMPORT:
		import_buddies_server_results(gc, webdata);
		break;
	case AGG_HTTP_USERLIST_EXPORT:
		export_buddies_server_results(gc, webdata);
		break;
	case AGG_HTTP_USERLIST_DELETE:
	        delete_buddies_server_results(gc, webdata);
		break;
#endif
	case AGG_HTTP_PASSWORD_CHANGE:
		password_change_server_results(gc, webdata);
		break;
	case AGG_HTTP_NONE:
	default:
		gaim_debug(GAIM_DEBUG_ERROR, "gg",
				   "http_results: unsupported type %d\n", hdata->type);
		break;
	}

	g_free(webdata);
	g_free(hdata);
}

static void http_req_callback(gpointer data, gint source, GaimInputCondition cond)
{
	struct agg_http *hdata = data;
	GaimConnection *gc = hdata->gc;
	gchar *request = hdata->request;
	gchar *buf;

	gaim_debug(GAIM_DEBUG_INFO, "gg", "http_req_callback: begin\n");

	if (!g_list_find(gaim_connections_get_all(), gc)) {
		gaim_debug(GAIM_DEBUG_ERROR, "gg",
				   "http_req_callback: g_slist_find error\n");
		g_free(request);
		g_free(hdata);
		close(source);
		return;
	}

	if (source == 0) {
		g_free(request);
		g_free(hdata);
		return;
	}

	gaim_debug(GAIM_DEBUG_MISC, "gg",
			   "http_req_callback: http request [%s]\n", request);

	buf = g_strdup_printf("POST %s HTTP/1.0\r\n"
			      "Host: %s\r\n"
			      "Content-Type: application/x-www-form-urlencoded\r\n"
			      "User-Agent: " GG_HTTP_USERAGENT "\r\n"
			      "Content-Length: %d\r\n"
			      "Pragma: no-cache\r\n" "\r\n" "%s\r\n",
			      hdata->form, hdata->host, (int)strlen(request), request);

	g_free(request);

	if (write(source, buf, strlen(buf)) < strlen(buf)) {
		g_free(buf);
		g_free(hdata);
		close(source);
		gaim_notify_error(gc, NULL,
						  _("Error communicating with Gadu-Gadu server"),
						  _("Gaim was unable to complete your request due "
							"to a problem communicating with the Gadu-Gadu "
							"HTTP server.  Please try again later."));
		return;
	}

	g_free(buf);

	hdata->inpa = gaim_input_add(source, GAIM_INPUT_READ, http_results, hdata);
}

#if 0
static void import_buddies_server(GaimConnection *gc)
{
	struct agg_http *hi = g_new0(struct agg_http, 1);
	gchar *u = gg_urlencode(gaim_account_get_username(gc->account));
	gchar *p = gg_urlencode(gaim_account_get_password(gc->account));

	hi->gc = gc;
	hi->type = AGG_HTTP_USERLIST_IMPORT;
	hi->form = AGG_PUBDIR_USERLIST_IMPORT_FORM;
	hi->host = GG_PUBDIR_HOST;
	hi->request = g_strdup_printf("FmNum=%s&Pass=%s", u, p);

	g_free(u);
	g_free(p);

	if (gaim_proxy_connect(gc->account, GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, hi) < 0) {
		gaim_notify_error(gc, NULL,
						  _("Unable to import Gadu-Gadu buddy list"),
						  _("Gaim was unable to connect to the Gadu-Gadu "
							"buddy list server.  Please try again later."));
		g_free(hi->request);
		g_free(hi);
		return;
	}
}

static void export_buddies_server(GaimConnection *gc)
{
	struct agg_http *he = g_new0(struct agg_http, 1);
	gchar *ptr;
	gchar *u = gg_urlencode(gaim_account_get_username(gc->account));
	gchar *p = gg_urlencode(gaim_account_get_password(gc->account));

	GaimBlistNode *gnode, *cnode, *bnode;

	he->gc = gc;
	he->type = AGG_HTTP_USERLIST_EXPORT;
	he->form = AGG_PUBDIR_USERLIST_EXPORT_FORM;
	he->host = GG_PUBDIR_HOST;
	he->request = g_strdup_printf("FmNum=%s&Pass=%s&Contacts=", u, p);

	g_free(u);
	g_free(p);

	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		GaimGroup *g = (GaimGroup *)gnode;
		int num_added=0;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				GaimBuddy *b = (GaimBuddy *)bnode;

				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				if(b->account == gc->account) {
					gchar *newdata;
					/* GG Number */
					gchar *name = gg_urlencode(b->name);
					/* GG Pseudo */
					gchar *show = gg_urlencode(b->alias ? b->alias : b->name);
					/* Group Name */
					gchar *gname = gg_urlencode(g->name);

					ptr = he->request;
					newdata = g_strdup_printf("%s;%s;%s;%s;%s;%s;%s",
							show, show, show, show, "", gname, name);

					if(num_added > 0)
						he->request = g_strconcat(ptr, "%0d%0a", newdata, NULL);
					else
						he->request = g_strconcat(ptr, newdata, NULL);

					num_added++;

					g_free(newdata);
					g_free(ptr);

					g_free(gname);
					g_free(show);
					g_free(name);
				}
			}
		}
	}

	if (gaim_proxy_connect(gc->account, GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, he) < 0) {
		gaim_notify_error(gc, NULL,
						  _("Couldn't export buddy list"),
						  _("Gaim was unable to connect to the buddy "
							"list server.  Please try again later."));
		g_free(he->request);
		g_free(he);
		return;
	}
}

static void delete_buddies_server(GaimConnection *gc)
{
	struct agg_http *he = g_new0(struct agg_http, 1);
	gchar *u = gg_urlencode(gaim_account_get_username(gc->account));
	gchar *p = gg_urlencode(gaim_account_get_password(gc->account));

	he->gc = gc;
	he->type = AGG_HTTP_USERLIST_DELETE;
	he->form = AGG_PUBDIR_USERLIST_EXPORT_FORM;
	he->host = GG_PUBDIR_HOST;
	he->request = g_strdup_printf("FmNum=%s&Pass=%s&Delete=1", u, p);

	if (gaim_proxy_connect(gc->account, GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, he) < 0) {
		gaim_notify_error(gc, NULL,
						  _("Unable to delete Gadu-Gadu buddy list"),
						  _("Gaim was unable to connect to the buddy "
							"list server.  Please try again later."));
		g_free(he->request);
		g_free(he);
		return;
	}
}
#endif

#if 0
static void agg_dir_search(GaimConnection *gc, const char *first, const char *middle,
			   const char *last, const char *maiden, const char *city, const char *state,
			   const char *country, const char *email)
{
	struct agg_http *srch = g_new0(struct agg_http, 1);
	srch->gc = gc;
	srch->type = AGG_HTTP_SEARCH;
	srch->form = AGG_PUBDIR_SEARCH_FORM;
	srch->host = GG_PUBDIR_HOST;

	if (email && strlen(email)) {
		gchar *eemail = gg_urlencode(email);
		srch->request = g_strdup_printf("Mode=1&Email=%s", eemail);
		g_free(eemail);
	} else {
		gchar *new_first = charset_convert(first, "UTF-8", "CP1250");
		gchar *new_last = charset_convert(last, "UTF-8", "CP1250");
		gchar *new_city = charset_convert(city, "UTF-8", "CP1250");

		gchar *enew_first = gg_urlencode(new_first);
		gchar *enew_last = gg_urlencode(new_last);
		gchar *enew_city = gg_urlencode(new_city);

		g_free(new_first);
		g_free(new_last);
		g_free(new_city);

		/* For active only add &ActiveOnly= */
		srch->request = g_strdup_printf("Mode=0&FirstName=%s&LastName=%s&Gender=%d"
						"&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d",
						enew_first, enew_last, AGG_GENDER_NONE,
						"", enew_city, 0, 0);

		g_free(enew_first);
		g_free(enew_last);
		g_free(enew_city);
	}

	if (gaim_proxy_connect(gc->account, GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, srch) < 0) {
		gaim_notify_error(gc, NULL,
						  _("Unable to access directory"),
						  _("Gaim was unable to search the Directory "
							"because it was unable to connect to the "
							"directory server.  Please try again later."));
		g_free(srch->request);
		g_free(srch);
		return;
	}
}
#endif

static void agg_change_passwd(GaimConnection *gc, const char *old, const char *new)
{
	struct agg_http *hpass = g_new0(struct agg_http, 1);
	gchar *u = gg_urlencode(gaim_account_get_username(gc->account));
	gchar *p = gg_urlencode(gaim_account_get_password(gc->account));
	gchar *enew = gg_urlencode(new);
	gchar *eold = gg_urlencode(old);

	hpass->gc = gc;
	hpass->type = AGG_HTTP_PASSWORD_CHANGE;
	hpass->form = AGG_REGISTER_DATA_FORM;
	hpass->host = GG_REGISTER_HOST;

	/* We are using old password as place for email - it's ugly */
	hpass->request = g_strdup_printf("fmnumber=%s&fmpwd=%s&pwd=%s&email=%s&code=%u",
					 u, p, enew, eold, gg_http_hash(old, new));

	g_free(u);
	g_free(p);
	g_free(enew);
	g_free(eold);

	if (gaim_proxy_connect(gc->account, GG_REGISTER_HOST, GG_REGISTER_PORT, http_req_callback, hpass) < 0) {
		gaim_notify_error(gc, NULL,
							  _("Unable to change Gadu-Gadu password"),
							  _("Gaim was unable to change your password "
								"due to an error connecting to the "
								"Gadu-Gadu server.  Please try again "
								"later."));
		g_free(hpass->request);
		g_free(hpass);
		return;
	}
}

static GList *agg_actions(GaimPlugin *plugin, gpointer context)
{
	GList *m = NULL;
	GaimPluginAction *act = NULL;

#if 0
	act = gaim_plugin_action_new(_("Directory Search"), show_find_info);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);
#endif

	act = gaim_plugin_action_new(_("Change Password"), change_pass);
	m = g_list_append(m, act);

#if 0
	act = gaim_plugin_action_new(_("Import Buddy List from Server"),
			import_buddies_server);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Export Buddy List to Server"),
			export_buddies_server);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Delete Buddy List from Server"),
			delete_buddies_server);
	m = g_list_append(m, act);
#endif

	return m;
}

static void agg_get_info(GaimConnection *gc, const char *who)
{
	struct agg_http *srch = g_new0(struct agg_http, 1);
	srch->gc = gc;
	srch->type = AGG_HTTP_SEARCH;
	srch->form = AGG_PUBDIR_SEARCH_FORM;
	srch->host = GG_PUBDIR_HOST;

	/* If it's invalid uin then maybe it's nickname? */
	if (invalid_uin(who)) {
		gchar *new_who = charset_convert(who, "UTF-8", "CP1250");
		gchar *enew_who = gg_urlencode(new_who);

		g_free(new_who);

		srch->request = g_strdup_printf("Mode=0&FirstName=%s&LastName=%s&Gender=%d"
						"&NickName=%s&City=%s&MinBirth=%d&MaxBirth=%d",
						"", "", AGG_GENDER_NONE, enew_who, "", 0, 0);

		g_free(enew_who);
	} else
		srch->request = g_strdup_printf("Mode=3&UserId=%s", who);

	if (gaim_proxy_connect(gc->account, GG_PUBDIR_HOST, GG_PUBDIR_PORT, http_req_callback, srch) < 0) {
		gaim_notify_error(gc, NULL,
						  _("Unable to access user profile."),
						  _("Gaim was unable to access this user's "
							"profile due to an error connecting to the "
							"directory server.  Please try again later."));
		g_free(srch->request);
		g_free(srch);
		return;
	}
}

static const char *agg_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "gadu-gadu";
}

static void agg_list_emblems(GaimBuddy *b, const char **se, const char **sw,
							 const char **nw, const char **ne)
{
	GaimPresence *presence = gaim_buddy_get_presence(b);

	if (!GAIM_BUDDY_IS_ONLINE(b))
		*se = "offline";
	else if (gaim_presence_is_status_active(presence, "away") ||
			 gaim_presence_is_status_active(presence, "away-friends"))
	{
		*se = "away";
	}
	else if (gaim_presence_is_status_active(presence, "invisible") ||
			 gaim_presence_is_status_active(presence, "invisible-friends"))
	{
		*se = "invisible";
	}
}


static void agg_set_permit_deny_dummy(GaimConnection *gc)
{
	/* It's implemented on client side because GG server doesn't support this */
}

static void agg_permit_deny_dummy(GaimConnection *gc, const char *who)
{
	/* It's implemented on client side because GG server doesn't support this */
}

static void agg_group_buddy (GaimConnection *gc, const char *who,
			const char *old_group, const char *new_group)
{
    GaimBuddy *buddy = gaim_find_buddy(gaim_connection_get_account(gc), who);
    gchar *newdata;
    /* GG Number */
    gchar *name = buddy->name;
    /* GG Pseudo */
    gchar *show = buddy->alias ? buddy->alias : buddy->name;
    /* Group Name */
    const gchar *gname = new_group;

    newdata = g_strdup_printf("%s;%s;%s;%s;%s;%s;%s;%s%s\r\n",
		    show, show, show, show, "", gname, name, "", "");
    agg_save_buddy_list(gc, newdata);
    g_free(newdata);
}

static void agg_rename_group (GaimConnection *gc, const char *old_name,
		     GaimGroup *group, GList *moved_buddies)
{
    agg_save_buddy_list(gc, NULL);
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	0,
	NULL,						/* user_splits */
	NULL,						/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	agg_list_icon,			/* list_icon */
	agg_list_emblems,		/* list_emblems */
	NULL,						/* status_text */
	NULL,						/* tooltip_text */
	agg_status_types,		/* status_types */
	agg_blist_node_menu,		/* blist_node_menu */
	NULL,						/* chat_info */
	NULL,						/* chat_info_defaults */
	agg_login,			/* login */
	agg_close,			/* close */
	agg_send_im,			/* send_im */
	NULL,						/* set_info */
	NULL,						/* send_typing */
	agg_get_info,			/* get_info */
	agg_set_status,			/* set_away */
	NULL,						/* set_idle */
	agg_change_passwd,		/* change_passwd */
	agg_add_buddy,			/* add_buddy */
	agg_add_buddies,		/* add_buddies */
	agg_rem_buddy,			/* remove_buddy */
	NULL,						/* remove_buddies */
	agg_permit_deny_dummy,		/* add_permit */
	agg_permit_deny_dummy,		/* add_deny */
	agg_permit_deny_dummy,		/* rem_permit */
	agg_permit_deny_dummy,		/* rem_deny */
	agg_set_permit_deny_dummy,	/* set_permit_deny */
	NULL,						/* warn */
	NULL,						/* join_chat */
	NULL,						/* reject_chat */
	NULL,						/* get_chat_name */
	NULL,						/* chat_invite */
	NULL,						/* chat_leave */
	NULL,						/* chat_whisper */
	NULL,						/* chat_send */
	agg_keepalive,			/* keepalive */
	NULL,						/* register_user */
	NULL,						/* get_cb_info */
	NULL,						/* get_cb_away */
	NULL,						/* alias_buddy */
	agg_group_buddy,		/* group_buddy */
	agg_rename_group,		/* rename_group */
	agg_buddy_free,			/* buddy_free */
	NULL,						/* convo_closed */
	NULL,						/* normalize */
	NULL,						/* set_buddy_icon */
	NULL,						/* remove_group */
	NULL,						/* get_cb_real_name */
	NULL,						/* set_chat_topic */
	NULL,						/* find_blist_chat */
	NULL,						/* roomlist_get_list */
	NULL,						/* roomlist_cancel */
	NULL,						/* roomlist_expand_category */
	NULL,						/* can_receive_file */
	NULL						/* send_file */
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-gg",		                          /**< id             */
	"Gadu-Gadu",                                      /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Gadu-Gadu Protocol Plugin"),
	                                                  /**  description    */
	N_("Gadu-Gadu Protocol Plugin"),
	"Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>",       /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	agg_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Nick"), "nick",
			"Gadu-Gadu User");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
			option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(gg, init_plugin, info)
