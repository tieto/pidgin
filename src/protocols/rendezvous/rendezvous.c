/*
 * gaim - Rendezvous Protocol Plugin
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
#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "prpl.h"

#include "mdns.h"
#include "util.h"

#define RENDEZVOUS_CONNECT_STEPS 2

typedef struct _RendezvousData {
	int fd;
	GHashTable *buddies;
} RendezvousData;

typedef struct _RendezvousBuddy {
#if 0
	guint ttltimer;
#endif
	gchar *firstandlast;
	gchar *aim;
	int p2pjport;
	int status;
	int idle;
	gchar *msg;
} RendezvousBuddy;

#define UC_IDLE 2

/****************************/
/* Utility Functions        */
/****************************/
static void rendezvous_buddy_free(gpointer data)
{
	RendezvousBuddy *rb = data;

	g_free(rb->firstandlast);
	g_free(rb->msg);
	g_free(rb);
}

/**
 * Extract the "user@host" name from a full presence domain
 * of the form "user@host._presence._tcp.local"
 *
 * @return If the domain is NOT a _presence._tcp.local domain
 *         then return NULL.  Otherwise return a newly allocated
 *         null-terminated string containing the "user@host" for
 *         the given domain.  This string should be g_free'd
 *         when no longer needed.
 */
static gchar *rendezvous_extract_name(gchar *domain)
{
	gchar *ret, *suffix;

	if (!g_str_has_suffix(domain, "._presence._tcp.local"))
		return NULL;

	suffix = strstr(domain, "._presence._tcp.local");
	ret = g_strndup(domain, suffix - domain);

	return ret;
}

/****************************/
/* Buddy List Functions     */
/****************************/

static void rendezvous_addtolocal(GaimConnection *gc, const char *name, const char *domain)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimBuddy *b;
	GaimGroup *g;

	g = gaim_find_group(domain);
	if (g == NULL) {
		g = gaim_group_new(domain);
		gaim_blist_add_group(g, NULL);
	}

	b = gaim_find_buddy_in_group(account, name, g);
	if (b != NULL)
		return;

	b = gaim_buddy_new(account, name, NULL);
	gaim_blist_add_buddy(b, NULL, g, NULL);
	serv_got_update(gc, b->name, 1, 0, 0, 0, 0);

#if 0
	RendezvousBuddy *rb;
	rb = g_hash_table_lookup(rd->buddies, name);
	if (rb == NULL) {
		rb = g_malloc0(sizeof(RendezvousBuddy));
		g_hash_table_insert(rd->buddies, g_strdup(name), rb);
	}
	rb->ttltimer = gaim_timeout_add(time * 10000, rendezvous_buddy_timeout, gc);

	gaim_timeout_remove(rb->ttltimer);
	rb->ttltimer = 0;
#endif
}

static void rendezvous_removefromlocal(GaimConnection *gc, const char *name, const char *domain)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimBuddy *b;
	GaimGroup *g;

	g = gaim_find_group(domain);
	if (g == NULL)
		return;

	b = gaim_find_buddy_in_group(account, name, g);
	if (b == NULL)
		return;

	serv_got_update(gc, b->name, 0, 0, 0, 0, 0);
	gaim_blist_remove_buddy(b);
	/* XXX - This results in incorrect group counts--needs to be fixed in the core */

	/*
	 * XXX - Instead of removing immediately, wait 10 seconds and THEN remove
	 * them.  If you do it immediately you don't see the door close icon.
	 */
}

static void rendezvous_removeallfromlocal(GaimConnection *gc)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	GaimBuddyList *blist;
	GaimBlistNode *gnode, *cnode, *bnode;
	GaimBuddy *b;

	/* Go through and remove all buddies that belong to this account */
	if ((blist = gaim_get_blist()) != NULL) {
		for (gnode = blist->root; gnode; gnode = gnode->next) {
			if (!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for (cnode = gnode->child; cnode; cnode = cnode->next) {
				if (!GAIM_BLIST_NODE_IS_CONTACT(cnode))
					continue;
				for (bnode = cnode->child; bnode; bnode = bnode->next) {
					if (!GAIM_BLIST_NODE_IS_BUDDY(bnode))
						continue;
					b = (GaimBuddy *)bnode;
					if (b->account != account)
						continue;
					serv_got_update(gc, b->name, 0, 0, 0, 0, 0);
					gaim_blist_remove_buddy(b);
				}
			}
		}
	}
}

static void rendezvous_handle_rr_txt(GaimConnection *gc, ResourceRecord *rr, const gchar *name)
{
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;
	GHashTable *rdata;
	gchar *tmp1, *tmp2;

	rdata = rr->rdata;

	/* Don't do a damn thing if the version is greater than 1 */
	tmp1 = g_hash_table_lookup(rdata, "version");
	if ((tmp1 == NULL) || (strcmp(tmp1, "1")))
		return;

	rb = g_hash_table_lookup(rd->buddies, name);
	if (rb == NULL) {
		rb = g_malloc0(sizeof(RendezvousBuddy));
		g_hash_table_insert(rd->buddies, g_strdup(name), rb);
	}

	tmp1 = g_hash_table_lookup(rdata, "1st");
	tmp2 = g_hash_table_lookup(rdata, "last");
	g_free(rb->firstandlast);
	rb->firstandlast = g_strdup_printf("%s%s%s",
							(tmp1 ? tmp1 : ""),
							(tmp1 || tmp2 ? " " : ""),
							(tmp2 ? tmp2 : ""));
	serv_got_alias(gc, name, rb->firstandlast);

	tmp1 = g_hash_table_lookup(rdata, "aim");
	if (tmp1 != NULL) {
		g_free(rb->aim);
		rb->aim = g_strdup(tmp1);
	}

	/*
	 * We only want to use this port as a back-up.  Ideally the port
	 * is specified in a separate, SRV resource record.
	 */
	if (rb->p2pjport == 0) {
		tmp1 = g_hash_table_lookup(rdata, "port.p2pj");
		rb->p2pjport = atoi(tmp1);
	}

	tmp1 = g_hash_table_lookup(rdata, "status");
	if (tmp1 != NULL) {
		if (!strcmp(tmp1, "dnd")) {
			/* Available */
			rb->status = 0;
		} else if (!strcmp(tmp1, "away")) {
			/* Idle */
			tmp2 = g_hash_table_lookup(rdata, "away");
			rb->idle = atoi(tmp2);
			gaim_debug_error("XXX", "User has been idle since %d\n", rb->idle);
			rb->status = UC_IDLE;
		} else if (!strcmp(tmp1, "avail")) {
			/* Away */
			rb->status = UC_UNAVAILABLE;
		}
		serv_got_update(gc, name, 1, 0, 0, 0, rb->status);
	}

	tmp1 = g_hash_table_lookup(rdata, "msg");
	if (tmp1 != NULL) {
		g_free(rb->msg);
		rb->msg = g_strdup(tmp1);
	}
}

static void rendezvous_handle_rr_srv(GaimConnection *gc, ResourceRecord *rr, const gchar *name)
{
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;
	ResourceRecordSRV *rdata;

	rdata = rr->rdata;

	rb = g_hash_table_lookup(rd->buddies, name);
	if (rb == NULL) {
		rb = g_malloc0(sizeof(RendezvousBuddy));
		g_hash_table_insert(rd->buddies, g_strdup(name), rb);
	}

	rb->p2pjport = rdata->port;
}

/*
 * Parse a resource record and do stuff if we need to.
 */
static void rendezvous_handle_rr(GaimConnection *gc, ResourceRecord *rr)
{
	gchar *name;

	gaim_debug_misc("rendezvous", "Parsing resource record with domain name %s\n", rr->name);

	/*
	 * XXX - Cache resource records from this function, if needed.
	 * Use the TTL value of the rr to cause this data to expire, but let
	 * the mdns_cache stuff handle that as much as possible.
	 */

	switch (rr->type) {
		case RENDEZVOUS_RRTYPE_NULL: {
			if ((name = rendezvous_extract_name(rr->name)) != NULL) {
				if (rr->rdlength > 0) {
					/* Data is a buddy icon */
					gaim_buddy_icons_set_for_user(gaim_connection_get_account(gc), name, rr->rdata, rr->rdlength);
				}

				g_free(name);
			}
		} break;

		case RENDEZVOUS_RRTYPE_PTR: {
			gchar *rdata = rr->rdata;
			if ((name = rendezvous_extract_name(rdata)) != NULL) {
				if (rr->ttl > 0)
					rendezvous_addtolocal(gc, name, "Rendezvous");
				else
					rendezvous_removefromlocal(gc, name, "Rendezvous");
				g_free(name);
			}
		} break;

		case RENDEZVOUS_RRTYPE_TXT: {
			if ((name = rendezvous_extract_name(rr->name)) != NULL) {
				rendezvous_handle_rr_txt(gc, rr, name);
				g_free(name);
			}
		} break;

		case RENDEZVOUS_RRTYPE_SRV: {
			if ((name = rendezvous_extract_name(rr->name)) != NULL) {
				rendezvous_handle_rr_srv(gc, rr, name);
				g_free(name);
			}
		} break;
	}
}

/****************************/
/* Icon and Emblem Funtions */
/****************************/
static const char* rendezvous_prpl_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "rendezvous";
}

static void rendezvous_prpl_list_emblems(GaimBuddy *b, char **se, char **sw, char **nw, char **ne)
{
	if (GAIM_BUDDY_IS_ONLINE(b)) {
		if (b->uc & UC_UNAVAILABLE)
			*se = "away";
	} else {
		*se = "offline";
	}
}

static gchar *rendezvous_prpl_status_text(GaimBuddy *b)
{
	GaimConnection *gc = b->account->gc;
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;
	gchar *ret;

	rb = g_hash_table_lookup(rd->buddies, b->name);
	if ((rb == NULL) || (rb->msg == NULL))
		return NULL;

	ret = g_strdup(rb->msg);

	return ret;
}

static gchar *rendezvous_prpl_tooltip_text(GaimBuddy *b)
{
	GaimConnection *gc = b->account->gc;
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;
	GString *ret;

	rb = g_hash_table_lookup(rd->buddies, b->name);
	if (rb == NULL)
		return NULL;

	ret = g_string_new("");

	if (rb->aim != NULL)
		g_string_append_printf(ret, "\n<b>%s</b>: %s", _("AIM Screen name"), rb->aim);

	if (rb->msg != NULL) {
		if (rb->status == UC_UNAVAILABLE)
			g_string_append_printf(ret, "\n<b>%s</b>: %s", _("Away"), rb->msg);
		else
			g_string_append_printf(ret, "\n<b>%s</b>: %s", _("Available"), rb->msg);
	}

	return g_string_free(ret, FALSE);
}

/****************************/
/* Connection Funtions      */
/****************************/
static void rendezvous_callback(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	RendezvousData *rd = gc->proto_data;
	DNSPacket *dns;
	int i;

	gaim_debug_misc("rendezvous", "Received rendezvous datagram\n");

	dns = mdns_read(rd->fd);
	if (dns == NULL)
		return;

	/* Handle the DNS packet */
	for (i = 0; i < dns->header.numanswers; i++)
		rendezvous_handle_rr(gc, &dns->answers[i]);
	for (i = 0; i < dns->header.numauthority; i++)
		rendezvous_handle_rr(gc, &dns->authority[i]);
	for (i = 0; i < dns->header.numadditional; i++)
		rendezvous_handle_rr(gc, &dns->additional[i]);

	mdns_free(dns);
}

static void rendezvous_prpl_login(GaimAccount *account)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	RendezvousData *rd;

	rd = g_new0(RendezvousData, 1);
	rd->buddies = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, rendezvous_buddy_free);
	gc->proto_data = rd;

	gaim_connection_update_progress(gc, _("Preparing Buddy List"), 0, RENDEZVOUS_CONNECT_STEPS);
	rendezvous_removeallfromlocal(gc);

	gaim_connection_update_progress(gc, _("Connecting"), 1, RENDEZVOUS_CONNECT_STEPS);
	rd->fd = mdns_establish_socket();
	if (rd->fd == -1) {
		gaim_connection_error(gc, _("Unable to login to rendezvous"));
		return;
	}

	gc->inpa = gaim_input_add(rd->fd, GAIM_INPUT_READ, rendezvous_callback, gc);
	gaim_connection_set_state(gc, GAIM_CONNECTED);

	mdns_query(rd->fd, "_presence._tcp.local");
	/* mdns_advertise_ptr(rd->fd, "_presence._tcp.local", "mark@diverge._presence._tcp.local"); */

#if 0
	text_record_add("txtvers", "1");
	text_record_add("status", "avail");
	text_record_add("1st", gaim_account_get_string(account, "first", "Gaim"));
	text_record_add("AIM", "markdoliner");
	text_record_add("version", "1");
	text_record_add("port.p2pj", "5298");
	text_record_add("last", gaim_account_get_string(account, "last", _("User")));

	publish(account->username, "_presence._tcp", 5298);
#endif
}

static void rendezvous_prpl_close(GaimConnection *gc)
{
	RendezvousData *rd = (RendezvousData *)gc->proto_data;

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	rendezvous_removeallfromlocal(gc);

	if (!rd)
		return;

	if (rd->fd >= 0)
		close(rd->fd);

	g_hash_table_destroy(rd->buddies);

	g_free(rd);
}

static int rendezvous_prpl_send_im(GaimConnection *gc, const char *who, const char *message, GaimConvImFlags flags)
{
	gaim_debug_info("rendezvous", "Sending IM\n");

	return 1;
}

static void rendezvous_prpl_set_away(GaimConnection *gc, const char *state, const char *text)
{
	gaim_debug_error("rendezvous", "Set away, state=%s,  text=%s\n", state, text);
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info =
{
	OPT_PROTO_NO_PASSWORD,
	NULL,
	NULL,
	NULL,
	rendezvous_prpl_list_icon,
	rendezvous_prpl_list_emblems,
	rendezvous_prpl_status_text,
	rendezvous_prpl_tooltip_text,
	NULL,
	NULL,
	NULL,
	NULL,
	rendezvous_prpl_login,
	rendezvous_prpl_close,
	rendezvous_prpl_send_im,
	NULL,
	NULL,
	NULL,
	rendezvous_prpl_set_away,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-rendezvous",                                /**< id             */
	"Rendezvous",                                     /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Rendezvous Protocol Plugin"),
	                                                  /**  description    */
	N_("Rendezvous Protocol Plugin"),
	NULL,                                             /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info                                        /**< extra_info     */
};

static void init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	/* Try to avoid making this configurable... */
	split = gaim_account_user_split_new(_("Host Name"), "localhost", '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_string_new(_("First Name"), "first", "Gaim");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_string_new(_("Last Name"), "last", _("User"));
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_bool_new(_("Share AIM screen name"), "shareaim", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(rendezvous, init_plugin, info);
