
/*
 * NOTES
 *
 * I don't like automatic updates of the buddylist stored on the server, so not
 * going to implement this. Maybe some kind of option to enable/disable this
 * feature.
 */

#include "internal.h"

#include "plugin.h"
#include "version.h"
#include "notify.h"
#include "status.h"
#include "blist.h"
#include "accountopt.h"
#include "debug.h"
#include "util.h"
#include "request.h"

#include "lib/libgadu.h"

static GaimPlugin *my_protocol = NULL;

typedef struct
{
	char *token_id;

} GGPToken;

typedef struct {

	char *uin;
	char *lastname;
	char *firstname;
	char *nickname;
	char *city;
	char *birthyear;
	char *gender;
	char *active;
	char *offset;

	char *last_uin;

} GGPSearchForm;

typedef struct {

	struct gg_session *session;
	GGPSearchForm *search_form;
	GGPToken *register_token;
	GGPToken *chpasswd_token;
	void *searchresults_window;

} GGPInfo;

/**
 * Convert enconding of a given string.
 *
 * @param locstr Input string.
 * @param encsrc Current encoding of the string.
 * @param encdst Target encoding of the string.
 *
 * @return Converted string (it must be g_free()ed when not used. Or NULL if
 * locstr is NULL.
 */
/* static gchar *charset_convert(const gchar *locstr, const char *encsrc, const char *encdst) {{{ */
static gchar *charset_convert(const gchar *locstr, const char *encsrc, const char *encdst)
{
	gchar *msg;
	GError *err = NULL;

	if (locstr == NULL)
		return NULL;

	msg = g_convert_with_fallback(locstr, strlen(locstr), encdst, encsrc, "?", NULL, NULL, &err);
	if (err != NULL) {
		gaim_debug_error("gg", "Error converting from %s to %s: %s\n",
						 encsrc, encdst, err->message);
		g_error_free(err);
	}

	/* Just in case? */
	if (msg == NULL)
		msg = g_strdup(locstr);

	return msg;
}
/* }}} */

/*
 * Convert string to number. Check wheter a given
 * string is a correct UIN.
 *
 * Return UIN or 0 if an error occurred.
 */
/* static uin_t ggp_str_to_uin(const char *text) {{{ */
static uin_t ggp_str_to_uin(const char *text)
{
	char *tmp;
	long num;

	if (!text)
		return 0;

	errno = 0;
	num = strtol(text, &tmp, 0);

	if (*text == '\0' || *tmp != '\0')
		return 0;

	if ((errno == ERANGE || (num == LONG_MAX || num == LONG_MIN)) || num > UINT_MAX || num < 0)
		return 0;

	return (uin_t) num;
}
/* }}} */

/**
 * Get UIN of a given account.
 *
 * @param account Current account.
 *
 * @return UIN of an account.
 */
/* static ggp_get_uin(GaimAccount *account) {{{ */
static uin_t ggp_get_uin(GaimAccount *account)
{
	return ggp_str_to_uin(gaim_account_get_username(account));
}
/* }}} */

/**
 * Create a new GGPSearchForm structure, and set the fields
 * to the sane defaults.
 *
 * @return Newly allocated GGPSearchForm.
 */
/* GGPSearchForm *ggp_searchform_new() {{{ */
GGPSearchForm *ggp_searchform_new()
{
	GGPSearchForm *form;

	form = g_new0(GGPSearchForm, 1);
	form->uin = NULL;
	form->lastname = NULL;
	form->firstname = NULL;
	form->nickname = NULL;
	form->city = NULL;
	form->birthyear = NULL;
	form->gender = NULL;
	form->active = NULL;
	form->offset = NULL;

	form->last_uin = NULL;

	return form;
}
/* }}} */

/* ---------------------------------------------------------------------- */
/* ----- BUDDYLIST STUFF ------------------------------------------------ */
/* ---------------------------------------------------------------------- */

/*
 * Adapted from the previous GG implementation in Gaim
 * by Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 */
/* static void ggp_buddylist_send(GaimConnection *gc) {{{ */
static void ggp_buddylist_send(GaimConnection *gc)
{
	GGPInfo *info = gc->proto_data;

	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *buddy;
	uin_t *userlist = NULL;
	gchar *types = NULL;
	int userlist_size = 0;

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

					if (buddy->account != gc->account)
						continue;

					userlist_size++;
					userlist = (uin_t *) g_renew(uin_t, userlist, userlist_size);
					types = (gchar *) g_renew(gchar, types, userlist_size);
					userlist[userlist_size - 1] = ggp_str_to_uin(buddy->name);
					types[userlist_size - 1] = GG_USER_NORMAL;
					gaim_debug_info("gg", "ggp_buddylist_send: adding %d\n", userlist[userlist_size - 1]);
				}
			}
		}
	}

	if (userlist) {
		int ret = gg_notify_ex(info->session, userlist, types, userlist_size);
		g_free(userlist);
		g_free(types);

		gaim_debug_info("gg", "send: ret=%d; size=%d\n", ret, userlist_size);
	}
}
/* }}} */

/**
 * Load buddylist from server into the rooster.
 *
 * @param gc GaimConnection
 * @param buddylist Pointer to the buddylist that will be loaded.
 */
/* static void ggp_buddylist_load(GaimConnection *gc, char *buddylist) {{{ */
static void ggp_buddylist_load(GaimConnection *gc, char *buddylist)
{
	GaimBuddy *buddy;
	GaimGroup *group;
	gchar **users_tbl;
	int i;

	users_tbl = g_strsplit(buddylist, "\r\n", 200);

	for (i = 0; users_tbl[i] != NULL; i++) {
		gchar **data_tbl;
		gchar *name, *show;

		if (strlen(users_tbl[i]) == 0)
			continue;

		data_tbl = g_strsplit(users_tbl[i], ";", 8);

		show = charset_convert(data_tbl[3], "CP1250", "UTF-8");
		name = data_tbl[6];

		gaim_debug_info("gg", "got buddy: name=%s show=%s\n", name, show);

		if (gaim_find_buddy(gaim_connection_get_account(gc), name)) {
			g_free(show);
			g_strfreev(data_tbl);
			continue;
		}

		gchar *g = g_strdup("Gadu-Gadu");

		if (strlen(data_tbl[5])) {
			/* Hard limit to at most 50 groups */
			gchar **group_tbl = g_strsplit(data_tbl[5], ",", 50);
			if (strlen(group_tbl[0]) > 0) {
				g_free(g);
				g = g_strdup(group_tbl[0]);
			}
			g_strfreev(group_tbl);
		}

		buddy = gaim_buddy_new(gaim_connection_get_account(gc), name, strlen(show) ? show : NULL);
		if (!(group = gaim_find_group(g))) {
			group = gaim_group_new(g);
			gaim_blist_add_group(group, NULL);
		}

		gaim_blist_add_buddy(buddy, NULL, group, NULL);
		g_free(g);

		g_free(show);
		g_strfreev(data_tbl);
	}
	g_strfreev(users_tbl);

	ggp_buddylist_send(gc);

}
/* }}} */

/*
 */
/* static void ggp_generic_status_handler(GaimConnection *gc, uin_t uin, int status, const char *descr) {{{ */
static void ggp_generic_status_handler(GaimConnection *gc, uin_t uin, int status, const char *descr)
{
	gchar *from;
	const char *st;
	gchar *msg;

	from = g_strdup_printf("%ld", (unsigned long int)uin);
	switch (status) {
		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
			st = "offline";
			break;
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			st = "online";
			break;
		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			st = "away";
			break;
		case GG_STATUS_BLOCKED:
			/* user is blocking us.... */
			st = "blocked";
			break;
		default:
			st = "online";
			gaim_debug_info("gg", "GG_EVENT_NOTIFY: Unknown status: %d\n", status);
			break;
	}

	gaim_debug_info("gg", "st = %s\n", st);
	msg = charset_convert(descr, "CP1250", "UTF-8");
	gaim_prpl_got_user_status(gaim_connection_get_account(gc), from, st, "message", msg, NULL);
	g_free(from);
	g_free(msg);
}
/* }}} */

/*
 */
/* static void ggp_pubdir_start_search(GaimConnection *gc, GGPSearchForm *form) {{{ */
static void ggp_pubdir_start_search(GaimConnection *gc, GGPSearchForm *form)
{
	GGPInfo *info = gc->proto_data;
	gg_pubdir50_t req;

	gaim_debug_info("gg", "It's time to perform a search...\n");

	if ((req = gg_pubdir50_new(GG_PUBDIR50_SEARCH)) == NULL) {
		gaim_debug_error("gg", "ggp_bmenu_show_details: Unable to create req variable.\n");
		return;
	}

	if (form->uin != NULL) {
		gaim_debug_info("gg", "    uin: %s\n", form->uin);
		gg_pubdir50_add(req, GG_PUBDIR50_UIN, form->uin);
	} else {
		if (form->lastname != NULL) {
			gaim_debug_info("gg", "    lastname: %s\n", form->lastname);
			gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, form->lastname);
		}

		if (form->firstname != NULL) {
			gaim_debug_info("gg", "    firstname: %s\n", form->firstname);
			gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, form->firstname);
		}

		if (form->nickname != NULL) {
			gaim_debug_info("gg", "    nickname: %s\n", form->nickname);
			gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, form->nickname);
		}

		if (form->city != NULL) {
			gaim_debug_info("gg", "    city: %s\n", form->city);
			gg_pubdir50_add(req, GG_PUBDIR50_CITY, form->city);
		}

		if (form->birthyear != NULL) {
			gaim_debug_info("gg", "    birthyear: %s\n", form->birthyear);
			gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, form->birthyear);
		}

		if (form->gender != NULL) {
			gaim_debug_info("gg", "    gender: %s\n", form->gender);
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, form->gender);
		}

		if (form->active != NULL) {
			gaim_debug_info("gg", "    active: %s\n", form->active);
			gg_pubdir50_add(req, GG_PUBDIR50_ACTIVE, form->active);
		}
	}

	gaim_debug_info("gg", "offset: %s\n", form->offset);
	gg_pubdir50_add(req, GG_PUBDIR50_START, g_strdup(form->offset));

	if (gg_pubdir50(info->session, req) == 0) {
		gaim_debug_warning("gg", "ggp_bmenu_show_details: Search failed.\n");
		return;
	}

	gg_pubdir50_free(req);
}
/* }}} */

/*
 * Return converted to the UTF-8 value of the specified field.
 *
 * @param res    Public directory look-up result
 * @param num    Id of the record
 * @param fileld Name of the field
 * 
 * @return UTF-8 encoded value of the field
 */
/* static char *ggp_get_pubdir_info(gg_pubdir50_t res, int num, const char *field) {{{ */
static char *ggp_get_pubdir_info(gg_pubdir50_t res, int num, const char *field)
{
	char *tmp;

	tmp = charset_convert(gg_pubdir50_get(res, num, field), "CP1250", "UTF-8");

	return (tmp == NULL) ? g_strdup("") : tmp;
}
/* }}} */

static void ggp_callback_show_next(GaimConnection *gc, GList *row)
{
	GGPInfo *info = gc->proto_data;

	g_free(info->search_form->offset);
	info->search_form->offset = g_strdup(info->search_form->last_uin);
	gaim_debug_info("gg", "london calling... offset = %s\n", info->search_form->offset);
	ggp_pubdir_start_search(gc, info->search_form);
}

static void ggp_callback_add_buddy(GaimConnection *gc, GList *row)
{
	gaim_blist_request_add_buddy(gaim_connection_get_account(gc),
			g_list_nth_data(row, 0), NULL, NULL);
}

/*
 */
/* static void ggp_callback_recv(gpointer _gc, gint fd, GaimInputCondition cond) {{{ */
static void ggp_callback_recv(gpointer _gc, gint fd, GaimInputCondition cond)
{
	GaimConnection *gc = _gc;
	GGPInfo *info = gc->proto_data;
	struct gg_event *ev;
	int i;

	if (!(ev = gg_watch_fd(info->session))) {
		gaim_debug_error("gg", "ggp_callback_recv: gg_watch_fd failed -- CRITICAL!\n");
		gaim_connection_error(gc, _("Unable to read socket"));
		return;
	}

	switch (ev->type) {
		case GG_EVENT_NONE:
			/* Nothing happened. */
			break;
		case GG_EVENT_MSG:
			{
				gchar *from;
				gchar *msg;
				gchar *tmp;

				from = g_strdup_printf("%lu", (unsigned long int)ev->event.msg.sender);

				msg = charset_convert((const char *)ev->event.msg.message,
									  "CP1250", "UTF-8");
				gaim_str_strip_cr(msg);
				tmp = g_markup_escape_text(msg, -1);
				gaim_debug_info("gg", "msg form (%s): %s (class = %d)\n", from, tmp, ev->event.msg.msgclass);
				serv_got_im(gc, from, tmp, 0, ev->event.msg.time);
				g_free(msg);
				g_free(tmp);
				g_free(from);
			}
			break;
		case GG_EVENT_ACK:
			gaim_debug_info("gg", "message sent to: %ld, delivery status=%d, seq=%d\n",
					ev->event.ack.recipient, ev->event.ack.status, ev->event.ack.seq);
			break;
		case GG_EVENT_NOTIFY:
		case GG_EVENT_NOTIFY_DESCR:
			{
				struct gg_notify_reply *n;
				char *descr;

				gaim_debug_info("gg", "notify_pre: (%d) status: %d\n",
						ev->event.notify->uin,
						ev->event.notify->status);

				n = (ev->type == GG_EVENT_NOTIFY) ? ev->event.notify
								  : ev->event.notify_descr.notify;

				for (; n->uin; n++) {
					descr = (ev->type == GG_EVENT_NOTIFY) ? NULL
					      				      : ev->event.notify_descr.descr;
					gaim_debug_info("gg", "notify: (%d) status: %d; descr: %s\n",
							n->uin, n->status, descr);

					ggp_generic_status_handler(gc,
							n->uin, n->status, descr);
				}
			}
			break;
		case GG_EVENT_NOTIFY60:
			gaim_debug_info("gg", "notify60_pre: (%d) status=%d; version=%d; descr=%s\n",
					ev->event.notify60->uin, ev->event.notify60->status,
					ev->event.notify60->version, ev->event.notify60->descr);

			for (i = 0; ev->event.notify60[i].uin; i++) {
				gaim_debug_info("gg", "notify60: (%d) status=%d; version=%d; descr=%s\n",
						ev->event.notify60[i].uin, ev->event.notify60[i].status,
						ev->event.notify60[i].version, ev->event.notify60[i].descr);

				ggp_generic_status_handler(gc,
						ev->event.notify60[i].uin,
						ev->event.notify60[i].status,
						ev->event.notify60[i].descr);
			}
			break;
		case GG_EVENT_STATUS:
			gaim_debug_info("gg", "status: (%d) status=%d; descr=%s\n",
					ev->event.status.uin, ev->event.status.status,
					ev->event.status.descr);

			ggp_generic_status_handler(gc,
						   ev->event.status.uin,
						   ev->event.status.status,
						   ev->event.status.descr);
			break;
		case GG_EVENT_STATUS60:
			gaim_debug_info("gg", "status60: (%d) status=%d; version=%d; descr=%s\n",
					ev->event.status60.uin,
					ev->event.status60.status,
					ev->event.status60.version,
					ev->event.status60.descr);

			ggp_generic_status_handler(gc,
						   ev->event.status60.uin,
						   ev->event.status60.status,
						   ev->event.status60.descr);
			break;
		case GG_EVENT_USERLIST:
	    		if (ev->event.userlist.type == GG_USERLIST_GET_REPLY) {
				gaim_debug_info("gg", "GG_USERLIST_GET_REPLY\n");
				if (ev->event.userlist.reply != NULL) {
				    ggp_buddylist_load(gc, ev->event.userlist.reply);
				}
			break;
			} else {
				gaim_debug_info("gg", "GG_USERLIST_PUT_REPLY. Userlist stored on the server.\n");
			}
			break;
		case GG_EVENT_PUBDIR50_SEARCH_REPLY:
			{
				GaimNotifySearchResults *results;
				GaimNotifySearchColumn *column;
				gg_pubdir50_t req = ev->event.pubdir50;
				int res_count = 0;
				int start;
				int i;

				res_count = gg_pubdir50_count(req);
				if (res_count < 1) {
					gaim_debug_info("gg", "GG_EVENT_PUBDIR50_SEARCH_REPLY: Nothing found\n");
					return;
				}
				res_count = (res_count > 20) ? 20 : res_count;

				results = gaim_notify_searchresults_new();

				column = gaim_notify_searchresults_column_new("UIN");
				gaim_notify_searchresults_column_add(results, column);

				column = gaim_notify_searchresults_column_new("First name");
				gaim_notify_searchresults_column_add(results, column);

				column = gaim_notify_searchresults_column_new("Nick name");
				gaim_notify_searchresults_column_add(results, column);

				column = gaim_notify_searchresults_column_new("City");
				gaim_notify_searchresults_column_add(results, column);

				column = gaim_notify_searchresults_column_new("Birth year");
				gaim_notify_searchresults_column_add(results, column);

				gaim_debug_info("gg", "Going with %d entries\n", res_count);

				start = (int)ggp_str_to_uin(gg_pubdir50_get(req, 0, GG_PUBDIR50_START));
				gaim_debug_info("gg", "start = %d\n", start);

				for (i = 0; i < res_count; i++) {
					GList *row = NULL;
					char *birth = ggp_get_pubdir_info(req, i, GG_PUBDIR50_BIRTHYEAR);
					
					/* TODO: Status will be displayed as an icon. */
					/* row = g_list_append(row, ggp_get_pubdir_info(req, i, GG_PUBDIR50_STATUS)); */
					row = g_list_append(row, ggp_get_pubdir_info(req, i, GG_PUBDIR50_UIN));
					row = g_list_append(row, ggp_get_pubdir_info(req, i, GG_PUBDIR50_FIRSTNAME));
					row = g_list_append(row, ggp_get_pubdir_info(req, i, GG_PUBDIR50_NICKNAME));
					row = g_list_append(row, ggp_get_pubdir_info(req, i, GG_PUBDIR50_CITY));
					row = g_list_append(row, (birth && strncmp(birth, "0", 1)) ? birth : g_strdup("-"));
					gaim_notify_searchresults_row_add(results, row);
					if (i == res_count - 1) {
						g_free(info->search_form->last_uin);
						info->search_form->last_uin = ggp_get_pubdir_info(req, i, GG_PUBDIR50_UIN);
					}
				}

				gaim_notify_searchresults_button_add(results, GAIM_NOTIFY_BUTTON_CONTINUE, ggp_callback_show_next);
				gaim_notify_searchresults_button_add(results, GAIM_NOTIFY_BUTTON_ADD_BUDDY, ggp_callback_add_buddy);
				if (info->searchresults_window == NULL) {
					void *h = gaim_notify_searchresults(gc, _("Gadu-Gadu Public Directory"),
											  _("Search results"), NULL, results, NULL, NULL);
					info->searchresults_window = h;
				} else {
					gaim_notify_searchresults_new_rows(gc, results, info->searchresults_window, NULL);
				}
			}
			break;
		default:
			gaim_debug_error("gg", "unsupported event type=%d\n", ev->type);
			break;
	}

	gg_free_event(ev);
}
/* }}} */

/**
 * Set offline status for all buddies.
 *
 * @param gc Connection handler
 */
/* static void ggp_buddylist_offline(GaimConnection *gc) {{{ */
static void ggp_buddylist_offline(GaimConnection *gc)
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

					if (buddy->account != gc->account)
						continue;

					gaim_prpl_got_user_status(
						gaim_connection_get_account(gc),
						buddy->name, "offline", NULL);
					gaim_debug_info("gg", "ggp_buddylist_offline: gone: %s\n", buddy->name);
				}
			}
		}
	}
}
/* }}} */

/**
 * Get all the buddies in the current account.
 *
 * @param account Current account.
 * 
 * @return List of buddies.
 */
/* static char *ggp_buddylist_dump(GaimAccount *account) {{{ */
static char *ggp_buddylist_dump(GaimAccount *account)
{
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimGroup *group;
	GaimBuddy *buddy;

	char *buddylist = g_strdup("");
	char *ptr;

	if ((blist = gaim_get_blist()) == NULL)
		return NULL;

	for (gnode = blist->root; gnode != NULL; gnode = gnode->next) {
		if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;

		group = (GaimGroup *)gnode;

		for (cnode = gnode->child; cnode != NULL; cnode = cnode->next) {
			if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
				continue;

			for (bnode = cnode->child; bnode != NULL; bnode = bnode->next) {
				if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;

				buddy = (GaimBuddy *)bnode;
				if (buddy->account != account)
					continue;

				gchar *newdata;
				/* GG Number */
				gchar *name = buddy->name;
				/* GG Pseudo */
				gchar *show = buddy->alias ? buddy->alias : buddy->name;
				/* Group Name */
				gchar *gname = group->name;

				newdata = g_strdup_printf("%s;%s;%s;%s;%s;%s;%s;%s%s\r\n",
						show, show, show, show, "", gname, name, "", "");

				ptr = buddylist;
				buddylist = g_strconcat(ptr, newdata, NULL);

				g_free(newdata);
				g_free(ptr);
			}
		}
	}

	return buddylist;
}
/* }}} */

/**
 * Request buddylist from the server.
 * Buddylist is received in the ggp_callback_recv().
 *
 * @param Current action handler.
 */
/* static void ggp_action_buddylist_get(GaimPluginAction *action) {{{ */
static void ggp_action_buddylist_get(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;
	GGPInfo *info = gc->proto_data;

	gaim_debug_info("gg", "Downloading...\n");

	gg_userlist_request(info->session, GG_USERLIST_GET, NULL);
}
/* }}} */

/**
 * Upload the buddylist to the server.
 *
 * @param action Current action handler.
 */
/* static void ggp_action_buddylist_put(GaimPluginAction *action) {{{ */
static void ggp_action_buddylist_put(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;
	GGPInfo *info = gc->proto_data;

	char *buddylist = ggp_buddylist_dump(gaim_connection_get_account(gc));

	gaim_debug_info("gg", "Uploading...\n");
	
	if (buddylist == NULL)
		return;

	gg_userlist_request(info->session, GG_USERLIST_PUT, buddylist);
	g_free(buddylist);
}
/* }}} */

/**
 * Delete buddylist from the server.
 *
 * @param action Current action handler.
 */
/* static void ggp_action_buddylist_delete(GaimPluginAction *action) {{{ */
static void ggp_action_buddylist_delete(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;
	GGPInfo *info = gc->proto_data;

	gaim_debug_info("gg", "Deleting...\n");

	gg_userlist_request(info->session, GG_USERLIST_PUT, NULL);
}
/* }}} */

/*
 */
/* static void ggp_callback_buddylist_save_ok(GaimConnection *gc, gchar *file) {{{ */
static void ggp_callback_buddylist_save_ok(GaimConnection *gc, gchar *file)
{
	GaimAccount *account = gaim_connection_get_account(gc);

	FILE *fh;
	char *buddylist = ggp_buddylist_dump(account);
	gchar *msg;

	gaim_debug_info("gg", "Saving...\n");
	gaim_debug_info("gg", "file = %s\n", file);

	if (buddylist == NULL) {
		gaim_notify_info(account, _("Save Buddylist..."),
				 _("Your buddylist is empty, nothing was written to the file."),
				 NULL);
		return;
	}

	if ((fh = g_fopen(file, "wb")) == NULL) {
		msg = g_strconcat(_("Couldn't open file"), ": ", file, "\n", NULL);
		gaim_debug_error("gg", "Could not open file: %s\n", file);
		gaim_notify_error(account, _("Couldn't open file"), msg, NULL);
		g_free(msg);
		g_free(file);
		return;
	}

	fwrite(buddylist, sizeof(char), g_utf8_strlen(buddylist, -1), fh);
	fclose(fh);
	g_free(buddylist);

	gaim_notify_info(account, _("Save Buddylist..."),
			 _("Buddylist saved successfully!"), NULL);
}
/* }}} */

/*
 */
/* static void ggp_callback_buddylist_load_ok(GaimConnection *gc, gchar *file) {{{ */
static void ggp_callback_buddylist_load_ok(GaimConnection *gc, gchar *file)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	char *buddylist, *tmp, *ptr;
	FILE *fh;
	gchar *msg;

	buddylist = g_strdup("");
	tmp = g_new0(gchar, 50);

	gaim_debug_info("gg", "file_name = %s\n", file);

	if ((fh = g_fopen(file, "rb")) == NULL) {
		msg = g_strconcat(_("Couldn't open file"), ": ", file, "\n", NULL);
		gaim_debug_error("gg", "Could not open file: %s\n", file);
		gaim_notify_error(account, _("Could't open file"), msg, NULL);
		g_free(msg);
		g_free(file);
		return;
	}

	while (fread(tmp, sizeof(gchar), 49, fh) == 49) {
		tmp[49] = '\0';
		/* gaim_debug_info("gg", "read: %s\n", tmp); */
		ptr = g_strconcat(buddylist, tmp, NULL);
		memset(tmp, '\0', 50); 
		g_free(buddylist);
		buddylist = ptr;
	}
	fclose(fh);
	g_free(tmp);

	ggp_buddylist_load(gc, buddylist);
	g_free(buddylist);

	gaim_notify_info(account,
			 _("Load Buddylist..."),
			 _("Buddylist loaded successfully!"), NULL);
}
/* }}} */

/*
 */
/* static void ggp_action_buddylist_save(GaimPluginAction *action) {{{ */
static void ggp_action_buddylist_save(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;

	gaim_request_file(action, _("Save buddylist..."), NULL, TRUE,
				G_CALLBACK(ggp_callback_buddylist_save_ok), NULL, gc);
}
/* }}} */

/*
 */
/* static void ggp_action_buddylist_load(GaimPluginAction *action) {{{ */
static void ggp_action_buddylist_load(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;

	gaim_request_file(action, "Load buddylist from file...", NULL, FALSE,
				G_CALLBACK(ggp_callback_buddylist_load_ok), NULL, gc);
}
/* }}} */

/*
 */
/* static void ggp_callback_change_passwd_ok(GaimConnection *gc, GaimRequestFields *fields) {{{ */
static void ggp_callback_change_passwd_ok(GaimConnection *gc, GaimRequestFields *fields)
{
	GaimAccount *account;
	GGPInfo *info = gc->proto_data;
	struct gg_http *h;
	gchar *cur, *p1, *p2, *t;

	cur = charset_convert(gaim_request_fields_get_string(fields, "password_cur"),
			     "UTF-8", "CP1250");
	p1  = charset_convert(gaim_request_fields_get_string(fields, "password1"),
			     "UTF-8", "CP1250");
	p2  = charset_convert(gaim_request_fields_get_string(fields, "password2"),
			     "UTF-8", "CP1250");
	t   = charset_convert(gaim_request_fields_get_string(fields, "token"),
			     "UTF-8", "CP1250");

	account = gaim_connection_get_account(gc);

	if (cur == NULL || p1 == NULL || p2 == NULL || t == NULL ||
	    *cur == '\0' || *p1 == '\0' || *p2 == '\0' || *t == '\0') {
		gaim_notify_error(account, NULL, _("Fill in the fields."), NULL);
		goto exit_err;
	}

	if (g_utf8_collate(p1, p2) != 0) {
		gaim_notify_error(account, NULL, _("New passwords do not match."), NULL);
		goto exit_err;
	}

	if (g_utf8_collate(cur, gaim_account_get_password(account)) != 0) {
		gaim_notify_error(account, NULL,
			_("Your current password is different from the one that you specified."),
			NULL);
		goto exit_err;
	}

	gaim_debug_info("gg", "change_passwd: old=%s; p1=%s; token=%s\n",
			cur, p1, info->chpasswd_token->token_id);

	/* XXX: this e-mail should be a pref... */
	h = gg_change_passwd4(ggp_get_uin(account),
			      "user@example.net", gaim_account_get_password(account),
			      p1, info->chpasswd_token->token_id, t, 0);

	if (h == NULL) {
		gaim_notify_error(account, NULL,
			_("Unable to change password. Error occured.\n"),
			NULL);
		goto exit_err;
	}

	gaim_account_set_password(account, p1);

	gg_change_passwd_free(h);

	gaim_notify_info(account, _("Change password for the Gadu-Gadu account"),
			 _("Password was changed successfully!"), NULL);

exit_err:
	g_free(cur);
	g_free(p1);
	g_free(p2);
	g_free(t);
	g_free(info->chpasswd_token->token_id);
	g_free(info->chpasswd_token);
}
/* }}} */

/*
 */
/* static void ggp_callback_register_account_ok(GaimConnection *gc, GaimRequestFields *fields) {{{ */
static void ggp_callback_register_account_ok(GaimConnection *gc, GaimRequestFields *fields)
{
	GaimAccount *account;
	GGPInfo *info = gc->proto_data;
	struct gg_http *h = NULL;
	struct gg_pubdir *s;
	uin_t uin;
	gchar *email, *p1, *p2, *t;

	email = charset_convert(gaim_request_fields_get_string(fields, "email"),
			     "UTF-8", "CP1250");
	p1  = charset_convert(gaim_request_fields_get_string(fields, "password1"),
			     "UTF-8", "CP1250");
	p2  = charset_convert(gaim_request_fields_get_string(fields, "password2"),
			     "UTF-8", "CP1250");
	t   = charset_convert(gaim_request_fields_get_string(fields, "token"),
			     "UTF-8", "CP1250");

	account = gaim_connection_get_account(gc);

	if (email == NULL || p1 == NULL || p2 == NULL || t == NULL ||
	    *email == '\0' || *p1 == '\0' || *p2 == '\0' || *t == '\0') {
		gaim_notify_error(account, NULL, _("Fill in the fields."), NULL);
		goto exit_err;
	}

	if (g_utf8_collate(p1, p2) != 0) {
		gaim_notify_error(account, NULL, _("Passwords do not match."), NULL);
		goto exit_err;
	}

	h = gg_register3(email, p1, info->register_token->token_id, t, 0);
	if (h == NULL || !(s = h->data) || !s->success) {
		gaim_notify_error(account, NULL,
			_("Unable to register new account. Error occured.\n"),
			NULL);
		goto exit_err;
	}

	uin = s->uin;
	gaim_debug_info("gg", "registered uin: %d\n", uin);

	gaim_notify_info(NULL, _("New Gadu-Gadu Account Registered"),
			 _("Registration completed successfully!"), NULL);

exit_err:
	gg_register_free(h);
	g_free(email);
	g_free(p1);
	g_free(p2);
	g_free(t);
	g_free(info->register_token->token_id);
	g_free(info->register_token);
}
/* }}} */

/*
 */
/* static void ggp_callback_find_buddies(GaimConnection *gc, GaimRequestFields *fields) {{{ */
static void ggp_callback_find_buddies(GaimConnection *gc, GaimRequestFields *fields)
{
	GGPInfo *info = gc->proto_data;
	GGPSearchForm *form;

	form = ggp_searchform_new();
	/*
	 * TODO: Fail if we have already a form attached. Only one search
	 * at a time will be allowed for now.
	 */
	info->search_form = form;

	form->lastname  = charset_convert(gaim_request_fields_get_string(fields, "lastname"),
									 "UTF-8", "CP1250");
	form->firstname = charset_convert(gaim_request_fields_get_string(fields, "firstname"),
									 "UTF-8", "CP1250");
	form->nickname  = charset_convert(gaim_request_fields_get_string(fields, "nickname"),
									 "UTF-8", "CP1250");
	form->city      = charset_convert(gaim_request_fields_get_string(fields, "city"),
									 "UTF-8", "CP1250");
	form->birthyear = charset_convert(gaim_request_fields_get_string(fields, "year"),
								     "UTF-8", "CP1250");

	switch (gaim_request_fields_get_choice(fields, "gender")) {
		case 1:
			form->gender = g_strdup(GG_PUBDIR50_GENDER_MALE);
			break;
		case 2:
			form->gender = g_strdup(GG_PUBDIR50_GENDER_FEMALE);
			break;
		default:
			form->gender = NULL;
			break;
	}

	form->active = gaim_request_fields_get_bool(fields, "active")
				   ? g_strdup(GG_PUBDIR50_ACTIVE_TRUE) : NULL;

	form->offset = g_strdup("0");

	ggp_pubdir_start_search(gc, form);
}
/* }}} */

/*
 */
/* static void ggp_find_buddies(GaimPluginAction *action) {{{ */
static void ggp_find_buddies(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;

	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("lastname", _("Last name"), NULL, FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("firstname", _("First name"), NULL, FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("nickname", _("Nickname"), NULL, FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("city", _("City"), NULL, FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("year", _("Year of birth"), NULL, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_choice_new("gender", "Gender", 0);
	gaim_request_field_choice_add(field, "Male or female");
	gaim_request_field_choice_add(field, "Male");
	gaim_request_field_choice_add(field, "Female");
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_bool_new("active", _("Only online"), FALSE);
	gaim_request_field_group_add_field(group, field);

	gaim_request_fields(gc,
		_("Find buddies"),
		_("Find buddies"),
		_("Please, enter your search criteria below"),
		fields,
		_("OK"), G_CALLBACK(ggp_callback_find_buddies),
		_("Cancel"), NULL,
		gc);
}
/* }}} */

/*
 */
/* static void ggp_change_passwd(GaimPluginAction *action) {{{ */
static void ggp_change_passwd(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *)action->context;
	GGPInfo *info = gc->proto_data;
	GGPToken *token;

	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;

	struct gg_http *req;
	struct gg_token *t;
	gchar *msg;

	gaim_debug_info("gg", "token: requested.\n");

	/* TODO: This should be async. */
	if ((req = gg_token(0)) == NULL) {
		gaim_notify_error(gaim_connection_get_account(gc),
						  _("Token Error"),
						  _("Unable to fetch the token.\n"), NULL);
		return;
	}

	t = req->data;

	token = g_new0(GGPToken, 1);
	token->token_id = g_strdup(t->tokenid);
	info->chpasswd_token = token;


	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("password_cur", _("Current password"), "", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("password1", _("Password"), "", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("password2", _("Password (retype)"), "", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("token", _("Enter current token"), "", FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	/* original size: 60x24 */
	field = gaim_request_field_image_new("token_img", _("Current token"), req->body, req->body_size);
	gaim_request_field_group_add_field(group, field);

	gg_token_free(req);

	msg = g_strdup_printf("%s %d",
			_("Please, enter your current password and your new password for UIN: "),
			ggp_get_uin(gaim_connection_get_account(gc)));

	gaim_request_fields(gc,
		_("Change Gadu-Gadu Password"),
		_("Change Gadu-Gadu Password"),
		msg,
		fields, _("OK"), G_CALLBACK(ggp_callback_change_passwd_ok),
		_("Cancel"), NULL, gc);

	g_free(msg);
}
/* }}} */

/* ---------------------------------------------------------------------- */
/* ----- GaimPluginProtocolInfo ----------------------------------------- */
/* ---------------------------------------------------------------------- */

/*
 */
/* static const char *ggp_list_icon(GaimAccount *account, GaimBuddy *buddy) {{{ */
static const char *ggp_list_icon(GaimAccount *account, GaimBuddy *buddy)
{
	return "gadu-gadu";
}
/* }}} */

/*
 */
/* static void ggp_list_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne) {{{ */
static void ggp_list_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne)
{
	GaimPresence *presence = gaim_buddy_get_presence(b);

	/* 
	 * Note to myself:
	 * 	The only valid status types are those defined
	 * 	in prpl_info->status_types.
	 *
	 * Usable icons: away, blocked, dnd, extendedaway,
	 * freeforchat, ignored, invisible, na, offline.
	 */

	if (!GAIM_BUDDY_IS_ONLINE(b)) {
		*se = "offline";
	} else if (gaim_presence_is_status_active(presence, "away")) {
		*se = "away";
	} else if (gaim_presence_is_status_active(presence, "online")) {
		*se = "online";
	} else if (gaim_presence_is_status_active(presence, "offline")) {
		*se = "offline";
	} else if (gaim_presence_is_status_active(presence, "blocked")) {
		*se = "blocked";
	} else {
		*se = "offline";
		gaim_debug_info("gg", "ggp_list_emblems: unknown status\n");
	}
}
/* }}} */

/*
 */
/* static char *ggp_status_text(GaimBuddy *b) {{{ */
static char *ggp_status_text(GaimBuddy *b)
{
	GaimStatus *status;
	const char *msg;
	char *text;
	char *tmp;

	status = gaim_presence_get_active_status(gaim_buddy_get_presence(b));

	msg = gaim_status_get_attr_string(status, "message");

	if (msg != NULL) {
		tmp = gaim_markup_strip_html(msg);
		text = g_markup_escape_text(tmp, -1);
		g_free(tmp);

		return text;
	} else {
		tmp = g_strdup(gaim_status_get_name(status));
		text = g_markup_escape_text(tmp, -1);
		g_free(tmp);

		return text;
	}
}
/* }}} */

/*
 */
/* static char *ggp_tooltip_text(GaimBuddy *b) {{{ */
static char *ggp_tooltip_text(GaimBuddy *b)
{
	GaimStatus *status;
	char *text;
	gchar *ret;
	const char *msg, *name;

	status = gaim_presence_get_active_status(gaim_buddy_get_presence(b));
	msg = gaim_status_get_attr_string(status, "message");
	name = gaim_status_get_name(status);

	if (msg != NULL) {
		char *tmp = gaim_markup_strip_html(msg);
		text = g_markup_escape_text(tmp, -1);
		g_free(tmp);

		ret = g_strdup_printf("\n<b>%s:</b> %s: %s",
							  _("Status"), name, text);

		g_free(text);
	} else {
		ret = g_strdup_printf("\n<b>%s:</b> %s",
							  _("Status"), name);
	}

	return ret;
}
/* }}} */

/*
 */
/* static GList *ggp_status_types(GaimAccount *account) {{{ */
static GList *ggp_status_types(GaimAccount *account)
{
	GaimStatusType *type;
	GList *types = NULL;

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_OFFLINE, "offline", _("Offline"),
	                                       TRUE, TRUE, FALSE, "message", _("Message"),
					       gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_ONLINE, "online", _("Online"),
	                                       TRUE, TRUE, FALSE, "message", _("Message"),
					       gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	/*
	 * Without this selecting Available or Invisible as own status doesn't
	 * work. It's not used and not needed to show status of buddies.
	 */
	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AVAILABLE, "available", _("Available"),
					       TRUE, TRUE, FALSE, "message", _("Message"),
					       gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_HIDDEN, "invisible", _("Invisible"),
					       TRUE, TRUE, FALSE, "message", _("Message"),
					       gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	/* type = gaim_status_type_new_with_attrs(GAIM_STATUS_UNAVAILABLE, "not-available", "Not Available", */
	/*                                        TRUE, TRUE, FALSE, "message", _("Message"),                */
	/*                                        gaim_value_new(GAIM_TYPE_STRING), NULL);                   */
	/* types = g_list_append(types, type);                                                               */

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_AWAY, "away", _("Busy"),
					       TRUE, TRUE, FALSE, "message", _("Message"),
					       gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_with_attrs(GAIM_STATUS_HIDDEN, "blocked", _("Blocked"),
					       TRUE, TRUE, FALSE, "message", _("Message"),
					       gaim_value_new(GAIM_TYPE_STRING), NULL);
	types = g_list_append(types, type);

	return types;
}
/* }}} */

/*
 */
/* static GList *ggp_blist_node_menu(GaimBlistNode *node) {{{ */
static GList *ggp_blist_node_menu(GaimBlistNode *node)
{
	GList *m = NULL;

	if (!GAIM_BLIST_NODE_IS_BUDDY(node))
		return NULL;

	/* act = gaim_blist_node_action_new("Change Password", ggp_bmenu_change_passwd, NULL, NULL); */
	/* m = g_list_append(m, act);                                                                */

	return m;
}
/* }}} */

/*
 */
/* static void ggp_login(GaimAccount *account, GaimStatus *status) {{{ */
static void ggp_login(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	struct gg_login_params *glp = g_new0(struct gg_login_params, 1);
	GGPInfo *info = g_new0(GGPInfo, 1);

	info->session = NULL;
	info->searchresults_window = NULL;

	gc->proto_data = info;

	glp->uin = ggp_get_uin(account);
	glp->password = (char *)gaim_account_get_password(account);

	glp->async = 0;
	glp->status = GG_STATUS_AVAIL;
	glp->tls = 0;

	info->session = gg_login(glp);
	if (info->session == NULL) {
		gaim_connection_error(gc, _("Connection failed."));
		g_free(glp);
		return;
	}
	gaim_debug_info("gg", "ggp_login: so far so good.\n");

	gc->inpa = gaim_input_add(info->session->fd, GAIM_INPUT_READ, ggp_callback_recv, gc);

	gg_change_status(info->session, GG_STATUS_AVAIL);
	gaim_connection_set_state(gc, GAIM_CONNECTED);
	ggp_buddylist_send(gc);
}
/* }}} */

/*
 */
/* static void ggp_close(GaimConnection *gc) {{{ */
static void ggp_close(GaimConnection *gc)
{
	GGPInfo *info;

	if (gc == NULL) {
		gaim_debug_info("gg", "gc == NULL\n");
		return;
	}

	info = gc->proto_data;

	/* XXX: Any way to pass description here? */
	if (info->session != NULL)
		gg_change_status(info->session, GG_STATUS_NOT_AVAIL);

	if (gc->inpa > 0)
		gaim_input_remove(gc->inpa);

	gg_logoff(info->session);
	gg_free_session(info->session);
	ggp_buddylist_offline(gc);

	gaim_debug_info("gg", "Connection closed.\n");
}
/* }}} */

/*
 */
/* static int ggp_send_im(GaimConnection *gc, const char *who, const char *msg, GaimConvImFlags flags) {{{ */
static int ggp_send_im(GaimConnection *gc, const char *who, const char *msg, GaimConvImFlags flags)
{
	GGPInfo *info = gc->proto_data;
	const char *tmp;

	if (strlen(msg) == 0)
		return 1;

	tmp = charset_convert(msg, "UTF-8", "CP1250");

	if (tmp != NULL && strlen(tmp) > 0) {
		if (gg_send_message(info->session, GG_CLASS_CHAT, ggp_str_to_uin(who), tmp) < 0) {
			return -1;
		}
	}

	return 1;
}
/* }}} */

/*
 */
/* static void ggp_get_info(GaimConnection *gc, const char *name) { {{{ */
static void ggp_get_info(GaimConnection *gc, const char *name)
{
	GGPInfo *info = gc->proto_data;
	GGPSearchForm *form;

	form = ggp_searchform_new();
	info->search_form = form;

	form->uin = g_strdup(name);
	form->offset = g_strdup("0");
	form->last_uin = g_strdup("0");

	ggp_pubdir_start_search(gc, form);
}
/* }}} */

/*
 */
/* static void ggp_set_status(GaimAccount *account, GaimStatus *status) {{{ */
static void ggp_set_status(GaimAccount *account, GaimStatus *status)
{
	GaimStatusPrimitive prim;
	GaimConnection *gc;
	GGPInfo *info;
	const char *status_id;
	int new_status, new_status_descr;

	prim = gaim_status_type_get_primitive(gaim_status_get_type(status));

	if (!gaim_status_is_active(status))
		return;

	if (prim == GAIM_STATUS_OFFLINE) {
		gaim_account_disconnect(account);
		return;
	}

	if (!gaim_account_is_connected(account)) {
		gaim_account_connect(account);
		return;
	}

	gc = gaim_account_get_connection(account);
	info = gc->proto_data;

	status_id = gaim_status_get_id(status);

	gaim_debug_info("gg", "ggp_set_status: Requested status = %s\n", status_id);

	if (strcmp(status_id, "available") == 0) {
		new_status = GG_STATUS_AVAIL;
		new_status_descr = GG_STATUS_AVAIL_DESCR;
	} else if (strcmp(status_id, "away") == 0) {
		new_status = GG_STATUS_BUSY;
		new_status_descr = GG_STATUS_BUSY_DESCR;
	} else if (strcmp(status_id, "invisible") == 0) {
		new_status = GG_STATUS_INVISIBLE;
		new_status_descr = GG_STATUS_INVISIBLE_DESCR;
	} else {
		new_status = GG_STATUS_AVAIL;
		new_status_descr = GG_STATUS_AVAIL_DESCR;
		gaim_debug_info("gg", "ggp_set_status: uknown status requested (status_id=%s)\n", status_id);
	}

	const char *msg = gaim_status_get_attr_string(status, "message");

	if (msg == NULL) {
		gaim_debug_info("gg", "ggp_set_status: msg == NULL\n");
		gg_change_status(info->session, new_status);
	} else {
		char *tmp = charset_convert(msg, "UTF-8", "CP1250");
		gaim_debug_info("gg", "ggp_set_status: msg != NULL. msg = %s\n", tmp);
		gaim_debug_info("gg", "ggp_set_status: gg_change_status_descr() = %d\n",
				gg_change_status_descr(info->session, new_status_descr, tmp));
		g_free(tmp);
	}

}
/* }}} */

/*
 */
/* static void ggp_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group) {{{ */
static void ggp_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	GGPInfo *info = gc->proto_data;

	gg_add_notify(info->session, ggp_str_to_uin(buddy->name));
}
/* }}} */

/*
 */
/* static void ggp_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group) {{{ */
static void ggp_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	GGPInfo *info = gc->proto_data;

	gg_remove_notify(info->session, ggp_str_to_uin(buddy->name));
}
/* }}} */

/*
 */
/* static void ggp_keepalive(GaimConnection *gc) {{{ */
static void ggp_keepalive(GaimConnection *gc)
{
	GGPInfo *info = gc->proto_data;

	/* gaim_debug_info("gg", "Keeping connection alive....\n"); */

	if (gg_ping(info->session) < 0) {
		gaim_debug_info("gg", "Not connected to the server "
				"or gg_session is not correct\n");
		gaim_connection_error(gc, _("Not connected to the server."));
	}
}
/* }}} */

/*
 */
/* static void ggp_register_user(GaimAccount *account) {{{ */
static void ggp_register_user(GaimAccount *account)
{
	GaimConnection *gc;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *group;
	GaimRequestField *field;
	GGPInfo *info;
	GGPToken *token;

	struct gg_http *req;
	struct gg_token *t;

	gaim_debug_info("gg", "token: requested.\n");

	if ((req = gg_token(0)) == NULL) {
		gaim_notify_error(account, _("Token Error"),
				_("Unable to fetch the token.\n"), NULL);
		return;
	}
	t = req->data;

	gc = gaim_account_get_connection(account);

	info = g_new0(GGPInfo, 1);
	gc->proto_data = info;

	token = g_new0(GGPToken, 1);
	token->token_id = g_strdup(t->tokenid);
	info->register_token = token;

	fields = gaim_request_fields_new();
	group = gaim_request_field_group_new(NULL);
	gaim_request_fields_add_group(fields, group);

	field = gaim_request_field_string_new("email", _("e-Mail"), "", FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("password1", _("Password"), "", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("password2", _("Password (retype)"), "", FALSE);
	gaim_request_field_string_set_masked(field, TRUE);
	gaim_request_field_group_add_field(group, field);

	field = gaim_request_field_string_new("token", _("Enter current token"), "", FALSE);
	gaim_request_field_string_set_masked(field, FALSE);
	gaim_request_field_group_add_field(group, field);

	/* original size: 60x24 */
	field = gaim_request_field_image_new("token_img", _("Current token"), req->body, req->body_size);
	gaim_request_field_group_add_field(group, field);

	gg_token_free(req);


	gaim_request_fields(account,
		_("Register New Gadu-Gadu Account"),
		_("Register New Gadu-Gadu Account"),
		_("Please, fill in the following fields"),
		fields, _("OK"), G_CALLBACK(ggp_callback_register_account_ok),
		_("Cancel"), NULL, gc);
}
/* }}} */

/*
 */
/* static GList *ggp_actions(GaimPlugin *plugin, gpointer context) {{{ */
static GList *ggp_actions(GaimPlugin *plugin, gpointer context)
{
	GList *m = NULL;
	GaimPluginAction *act;

	act = gaim_plugin_action_new(_("Find buddies"), ggp_find_buddies);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);

	act = gaim_plugin_action_new(_("Change password"), ggp_change_passwd);
	m = g_list_append(m, act);

	m = g_list_append(m, NULL);

	act = gaim_plugin_action_new(_("Upload buddylist to Server"), ggp_action_buddylist_put);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Download buddylist from Server"), ggp_action_buddylist_get);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Delete buddylist from Server"), ggp_action_buddylist_delete);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Save buddylist to file"), ggp_action_buddylist_save);
	m = g_list_append(m, act);

	act = gaim_plugin_action_new(_("Load buddylist from file"), ggp_action_buddylist_load);
	m = g_list_append(m, act);

	return m;
}
/* }}} */

/* prpl_info setup {{{ */
static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_REGISTER_NOSCREENNAME,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	ggp_list_icon,			/* list_icon */
	ggp_list_emblems,		/* list_emblems */
	ggp_status_text,		/* status_text */
	ggp_tooltip_text,		/* tooltip_text */
	ggp_status_types,		/* status_types */
	ggp_blist_node_menu,	/* blist_node_menu */
	NULL,					/* chat_info */
	NULL,					/* chat_info_defaults */
	ggp_login,				/* login */
	ggp_close,				/* close */
	ggp_send_im,			/* send_im */
	NULL,					/* set_info */
	NULL,					/* send_typing */
	ggp_get_info,			/* get_info */
	ggp_set_status,			/* set_away */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	ggp_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	ggp_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,					/* add_deny */
	NULL,					/* rem_permit */
	NULL,					/* rem_deny */
	NULL,					/* set_permit_deny */
	NULL,					/* join_chat */
	NULL,					/* reject_chat */
	NULL,					/* get_chat_name */
	NULL,					/* chat_invite */
	NULL,					/* chat_leave */
	NULL,					/* chat_whisper */
	NULL,					/* chat_send */
	ggp_keepalive,			/* keepalive */
	ggp_register_user,		/* register_user */
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
/* }}} */

/* GaimPluginInfo setup {{{ */
static GaimPluginInfo info = {
	GAIM_PLUGIN_MAGIC,		/* magic */
	GAIM_MAJOR_VERSION,		/* major_version */
	GAIM_MINOR_VERSION,		/* minor_version */
	GAIM_PLUGIN_PROTOCOL,		/* plugin type */
	NULL,				/* ui_requirement */
	0,				/* flags */
	NULL,				/* dependencies */
	GAIM_PRIORITY_DEFAULT,		/* priority */

	"prpl-gg",			/* id */
	"Gadu-Gadu",			/* name */
	VERSION,			/* version */

	N_("Gadu-Gadu Protocol Plugin"),	/* summary */
	N_("Polish popular IM"),		/* description */
	"boler@sourceforge.net",	/* author */
	GAIM_WEBSITE,			/* homepage */

	NULL,				/* load */
	NULL,				/* unload */
	NULL,				/* destroy */

	NULL,				/* ui_info */
	&prpl_info,			/* extra_info */
	NULL,				/* prefs_info */
	ggp_actions			/* actions */
};
/* }}} */

/*
 */
/* static void init_plugin(GaimPlugin *plugin) {{{ */
static void init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;

	option = gaim_account_option_string_new(_("Nickname"), "nick", _("Gadu-Gadu User"));
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	my_protocol = plugin;
}
/* }}} */

GAIM_INIT_PLUGIN(gadu-gadu, init_plugin, info);

/* vim: set ts=4 sts=0 sw=4 noet: */
