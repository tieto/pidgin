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
#include "cipher.h"
#include "debug.h"
#include "network.h"
#include "prpl.h"
#include "version.h"

#include "direct.h"
#include "mdns.h"
#include "rendezvous.h"
#include "util.h"

/****************************/
/* Utility Functions        */
/****************************/
static void
rendezvous_buddy_free(gpointer data)
{
	RendezvousBuddy *rb = data;

	if (rb->fd >= 0)
		close(rb->fd);
	if (rb->watcher >= 0)
		gaim_input_remove(rb->watcher);

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
static gchar *
rendezvous_extract_name(gchar *domain)
{
	gchar *ret, *suffix;

	if (!gaim_str_has_suffix(domain, "._presence._tcp.local"))
		return NULL;

	suffix = strstr(domain, "._presence._tcp.local");
	ret = g_strndup(domain, suffix - domain);

	return ret;
}

/****************************/
/* Buddy List Functions     */
/****************************/

static RendezvousBuddy *
rendezvous_find_or_create_rendezvousbuddy(GaimConnection *gc, const char *name)
{
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;

	rb = g_hash_table_lookup(rd->buddies, name);
	if (rb == NULL) {
		rb = g_malloc0(sizeof(RendezvousBuddy));
		rb->fd = -1;
		rb->watcher = -1;
		g_hash_table_insert(rd->buddies, g_strdup(name), rb);
	}

	return rb;
}

static void
rendezvous_addtolocal(GaimConnection *gc, const char *name, const char *domain)
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
	gaim_blist_node_set_flags((GaimBlistNode *)b, GAIM_BLIST_NODE_FLAG_NO_SAVE);
	/* gaim_blist_node_set_flag(b, GAIM_BLIST_NODE_FLAG_NO_SAVE); */
	gaim_blist_add_buddy(b, NULL, g, NULL);
	gaim_prpl_got_user_status(account, b->name, "online", NULL);

#if 0
	/* Remove the buddy if the TTL on their PTR record expires */
	RendezvousBuddy *rb;
	rb = rendezvous_find_or_create_rendezvousbuddy(gc, name);
	rb->ttltimer = gaim_timeout_add(time * 10000, rendezvous_buddy_timeout, gc);

	gaim_timeout_remove(rb->ttltimer);
	rb->ttltimer = 0;
#endif
}

static void
rendezvous_removefromlocal(GaimConnection *gc, const char *name, const char *domain)
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

	gaim_prpl_got_user_status(account, b->name, "offline", NULL);
	gaim_blist_remove_buddy(b);
	/* XXX - This results in incorrect group counts--needs to be fixed in the core */
	/* XXX - We also need to call remove_idle_buddy() in server.c for idle buddies */

	/*
	 * XXX - Instead of removing immediately, wait 10 seconds and THEN remove
	 * them.  If you do it immediately you don't see the door close icon.
	 */
}

static void
rendezvous_removeallfromlocal(GaimConnection *gc)
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
					gaim_prpl_got_user_status(account, b->name, "offline", NULL);
					gaim_blist_remove_buddy(b);
				}
			}
		}
	}
}

static gboolean
rendezvous_find_buddy_by_host(gpointer key, gpointer value, gpointer user_data)
{
	const gchar *domain;

	if (key == NULL)
		return FALSE;

	domain = strchr(key, '@');
	if (domain == NULL)
		return FALSE;
	domain++;

	return !strcasecmp(user_data, domain);
}

static void
rendezvous_handle_rr_a(GaimConnection *gc, ResourceRecord *rr, const gchar *name)
{
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;
	ResourceRecordRDataSRV *rdata;
	const gchar *end;
	gchar *domain;

	/*
	 * Remove the trailing ".local" from the domain.  If there is no
	 * trailing ".local" then do nothing and exit
	 */
	end = g_strrstr(name, ".local");
	if (end == NULL)
		return;

	domain = g_strndup(name, (gpointer)end - (gpointer)name);
	rb = g_hash_table_find(rd->buddies, rendezvous_find_buddy_by_host, domain);
	g_free(domain);

	if (rb == NULL)
		return;

	rdata = rr->rdata;

	memcpy(rb->ipv4, rdata, 4);
}

static void
rendezvous_handle_rr_txt(GaimConnection *gc, ResourceRecord *rr, const gchar *name)
{
	GaimAccount *account = gaim_connection_get_account(gc);
	RendezvousBuddy *rb;
	GSList *rdata;
	ResourceRecordRDataTXTNode *node1, *node2;

	rdata = rr->rdata;

	/* Don't do a damn thing if the version is greater than 1 */
	node1 = mdns_txt_find(rdata, "version");
	if ((node1 == NULL) || (node1->value == NULL) || (strcmp(node1->value, "1")))
		return;

	rb = rendezvous_find_or_create_rendezvousbuddy(gc, name);

	node1 = mdns_txt_find(rdata, "1st");
	node2 = mdns_txt_find(rdata, "last");
	g_free(rb->firstandlast);
	rb->firstandlast = g_strdup_printf("%s%s%s",
							(node1 && node1->value ? node1->value : ""),
							(node1 && node1->value && node2 && node2->value ? " " : ""),
							(node2 && node2->value ? node2->value : ""));
	serv_got_alias(gc, name, rb->firstandlast);

	node1 = mdns_txt_find(rdata, "aim");
	if ((node1 != NULL) && (node1->value != NULL)) {
		g_free(rb->aim);
		rb->aim = g_strdup(node1->value);
	}

	/*
	 * We only want to use this port as a back-up.  Ideally the port
	 * is specified in a separate, SRV resource record.
	 */
	if (rb->p2pjport == 0) {
		node1 = mdns_txt_find(rdata, "port.p2pj");
		if ((node1 != NULL) && (node1->value))
			rb->p2pjport = atoi(node1->value);
	}

	node1 = mdns_txt_find(rdata, "status");
	if ((node1 != NULL) && (node1->value != NULL)) {
		if (!strcmp(node1->value, "avail")) {
			/* Available */
			rb->status = 0;
		} else if (!strcmp(node1->value, "away")) {
			/* Idle */
			node2 = mdns_txt_find(rdata, "away");
			if ((node2 != NULL) && (node2->value)) {
				/* Time is seconds since January 1st 2001 GMT */
				rb->idle = atoi(node2->value);
				rb->idle += 978307200; /* convert to seconds-since-epoch */
			}
			rb->status = UC_IDLE;
			/* TODO: Do this when the user is added to the buddy list?  Definitely not here! */
			/* gaim_prpl_got_user_idle(account, name, TRUE, rb->idle); */
		} else if (!strcmp(node1->value, "dnd")) {
			/* Away */
			rb->status = UC_UNAVAILABLE;
		}
		gaim_prpl_got_user_status(account, name, "online", NULL);
		/* XXX - Idle time is rb->idle and status is rb->status */
	}

	node1 = mdns_txt_find(rdata, "msg");
	if ((node1 != NULL) && (node1->value != NULL)) {
		g_free(rb->msg);
		rb->msg = g_strdup(node1->value);
	}
}

static void
rendezvous_handle_rr_aaaa(GaimConnection *gc, ResourceRecord *rr, const gchar *name)
{
	RendezvousData *rd = gc->proto_data;
	RendezvousBuddy *rb;
	ResourceRecordRDataSRV *rdata;
	const gchar *end;
	gchar *domain;

	/*
	 * Remove the trailing ".local" from the domain.  If there is no
	 * trailing ".local" then do nothing and exit
	 */
	end = g_strrstr(name, ".local");
	if (end == NULL)
		return;

	domain = g_strndup(name, (gpointer)end - (gpointer)name);
	rb = g_hash_table_find(rd->buddies, rendezvous_find_buddy_by_host, domain);
	g_free(domain);

	if (rb == NULL)
		return;

	rdata = rr->rdata;

	memcpy(rb->ipv6, rdata, 16);
}

static void
rendezvous_handle_rr_srv(GaimConnection *gc, ResourceRecord *rr, const gchar *name)
{
	RendezvousBuddy *rb;
	ResourceRecordRDataSRV *rdata;

	rdata = rr->rdata;

	rb = rendezvous_find_or_create_rendezvousbuddy(gc, name);

	rb->p2pjport = rdata->port;
}

/*
 * Parse a resource record and do stuff if we need to.
 */
static void
rendezvous_handle_rr(GaimConnection *gc, ResourceRecord *rr)
{
	RendezvousData *rd = gc->proto_data;
	gchar *name;

	gaim_debug_misc("rendezvous", "Parsing resource record with domain name %s and type %d\n", rr->name, rr->type);

	switch (rr->type) {
		case RENDEZVOUS_RRTYPE_A: {
			rendezvous_handle_rr_a(gc, rr, rr->name);
		} break;

		case RENDEZVOUS_RRTYPE_NULL: {
			name = rendezvous_extract_name(rr->name);
			if (name != NULL) {
				if (rr->rdlength > 0) {
					/* Data is a buddy icon */
					gaim_buddy_icons_set_for_user(gaim_connection_get_account(gc), name, rr->rdata, rr->rdlength);
				}

				g_free(name);
			}
		} break;

		case RENDEZVOUS_RRTYPE_PTR: {
			gchar *rdata = rr->rdata;

			name = rendezvous_extract_name(rdata);
			if (name != NULL) {
				if (rr->ttl > 0) {
					/* Add them to our buddy list and request their icon */
					rendezvous_addtolocal(gc, name, "Rendezvous");
					mdns_query(rd->fd, rdata, RENDEZVOUS_RRTYPE_NULL);
				} else {
					/* Remove them from our buddy list */
					rendezvous_removefromlocal(gc, name, "Rendezvous");
				}
				g_free(name);
			}
		} break;

		case RENDEZVOUS_RRTYPE_TXT: {
			name = rendezvous_extract_name(rr->name);
			if (name != NULL) {
				rendezvous_handle_rr_txt(gc, rr, name);
				g_free(name);
			}
		} break;

		case RENDEZVOUS_RRTYPE_AAAA: {
			rendezvous_handle_rr_aaaa(gc, rr, rr->name);
		} break;

		case RENDEZVOUS_RRTYPE_SRV: {
			name = rendezvous_extract_name(rr->name);
			if (name != NULL) {
				rendezvous_handle_rr_srv(gc, rr, name);
				g_free(name);
			}
		} break;
	}
}

/****************************/
/* Connection Functions      */
/****************************/
static void
rendezvous_callback(gpointer data, gint source, GaimInputCondition condition)
{
	GaimConnection *gc = data;
	RendezvousData *rd = gc->proto_data;
	DNSPacket *dns;
	GSList *cur;

	gaim_debug_misc("rendezvous", "Received rendezvous datagram\n");

	dns = mdns_read(rd->fd);
	if (dns == NULL)
		return;

	/* Handle the DNS packet */
	for (cur = dns->answers; cur != NULL; cur = cur->next)
		rendezvous_handle_rr(gc, cur->data);
	for (cur = dns->authority; cur != NULL; cur = cur->next)
		rendezvous_handle_rr(gc, cur->data);
	for (cur = dns->additional; cur != NULL; cur = cur->next)
		rendezvous_handle_rr(gc, cur->data);

	mdns_free(dns);
}

static void
rendezvous_add_to_txt(RendezvousData *rd, const char *name, const char *value)
{
	ResourceRecordRDataTXTNode *node;
	node = g_malloc(sizeof(ResourceRecordRDataTXTNode));
	node->name = g_strdup(name);
	node->value = value != NULL ? g_strdup(value) : NULL;
	rd->mytxtdata = g_slist_append(rd->mytxtdata, node);
}

static guchar *
rendezvous_read_icon_data(const char *filename, unsigned short *length)
{
	struct stat st;
	FILE *file;
	guchar *data;

	*length = 0;

	g_return_val_if_fail(filename != NULL, NULL);

	if (g_stat(filename, &st))
		return NULL;

	if (!(file = g_fopen(filename, "rb")))
		return NULL;

	*length = st.st_size;
	data = g_malloc(*length);
	fread(data, 1, *length, file);
	fclose(file);

	return data;
}

static void
rendezvous_add_to_txt_iconhash(RendezvousData *rd, const char *iconfile)
{
	guchar *icondata;
	unsigned short iconlength;
	unsigned char hash[20];
	gchar *base16;

	g_return_if_fail(rd != NULL);

	if (iconfile == NULL)
		return;

	icondata = rendezvous_read_icon_data(iconfile, &iconlength);
	gaim_cipher_digest_region("sha1", (guint8 *)icondata, iconlength, sizeof(hash), hash, NULL);
	g_free(icondata);

	base16 = gaim_base16_encode(hash, 20);
	rendezvous_add_to_txt(rd, "phsh", base16);
	g_free(base16);
}

static void
rendezvous_send_icon(GaimConnection *gc)
{
	RendezvousData *rd = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	const char *iconfile = gaim_account_get_buddy_icon(account);
	unsigned char *rdata;
	unsigned short rdlength;
	gchar *myname;

	if (iconfile == NULL)
		return;

	rdata = rendezvous_read_icon_data(iconfile, &rdlength);

	myname = g_strdup_printf("%s._presence._tcp.local", gaim_account_get_username(account));
	mdns_advertise_null(rd->fd, myname, rdata, rdlength);
	g_free(myname);

	g_free(rdata);
}

static void
rendezvous_send_online(GaimConnection *gc)
{
	RendezvousData *rd = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	const gchar *me, *myip;
	gchar *myname, *mycomp, *myport;
	gchar **mysplitip;
	unsigned char myipv4[4];

	me = gaim_account_get_username(account);
	myname = g_strdup_printf("%s._presence._tcp.local", me);
	mycomp = g_strdup_printf("%s.local", strchr(me, '@') + 1);
	myport = g_strdup_printf("%d", rd->listener_port);

	myip = gaim_network_get_local_system_ip(-1);
	mysplitip = g_strsplit(myip, ".", 0);
	myipv4[0] = atoi(mysplitip[0]);
	myipv4[1] = atoi(mysplitip[1]);
	myipv4[2] = atoi(mysplitip[2]);
	myipv4[3] = atoi(mysplitip[3]);
	g_strfreev(mysplitip);

	mdns_advertise_a(rd->fd, mycomp, myipv4);
	mdns_advertise_ptr(rd->fd, "_presence._tcp.local", myname);
	mdns_advertise_srv(rd->fd, myname, rd->listener_port, mycomp);

	rendezvous_add_to_txt(rd, "txtvers", "1");
	rendezvous_add_to_txt(rd, "status", "avail");
	/* rendezvous_add_to_txt(rd, "vc", "A!"); */
	rendezvous_add_to_txt_iconhash(rd, gaim_account_get_buddy_icon(account));
	rendezvous_add_to_txt(rd, "1st", gaim_account_get_string(account, "first", "Gaim"));
	if (gaim_account_get_bool(account, "shareaim", FALSE)) {
		GList *l;
		GaimAccount *cur;
		for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
			cur = (GaimAccount *)l->data;
			if (!strcmp(gaim_account_get_protocol_id(cur), "prpl-oscar")) {
				rendezvous_add_to_txt(rd, "AIM", gaim_normalize(cur, gaim_account_get_username(cur)));
				break;
			}
		}
	}
	rendezvous_add_to_txt(rd, "version", "1");
	rendezvous_add_to_txt(rd, "msg", "Groovin'");
	rendezvous_add_to_txt(rd, "port.p2pj", myport);
	rendezvous_add_to_txt(rd, "last", gaim_account_get_string(account, "last", _("User")));
	mdns_advertise_txt(rd->fd, myname, rd->mytxtdata);

	rendezvous_send_icon(gc);

	g_free(myname);
	g_free(mycomp);
	g_free(myport);
}

static void
rendezvous_send_offline(GaimConnection *gc)
{
	RendezvousData *rd = gc->proto_data;
	GaimAccount *account = gaim_connection_get_account(gc);
	const gchar *me;
	gchar *myname;

	me = gaim_account_get_username(account);
	myname = g_strdup_printf("%s._presence._tcp.local", me);

	mdns_advertise_ptr_with_ttl(rd->fd, "_presence._tcp.local", myname, 0);
}

/***********************************/
/* All the PRPL Callback Functions */
/***********************************/
static const char *
rendezvous_prpl_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return "rendezvous";
}

static void
rendezvous_prpl_list_emblems(GaimBuddy *b, const char **se, const char **sw, const char **nw, const char **ne)
{
	if (GAIM_BUDDY_IS_ONLINE(b)) {
		if (b->uc & UC_UNAVAILABLE)
			*se = "away";
	} else {
		*se = "offline";
	}
}

static gchar *
rendezvous_prpl_status_text(GaimBuddy *b)
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

static gchar *
rendezvous_prpl_tooltip_text(GaimBuddy *b)
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

static GList *
rendezvous_prpl_status_types(GaimAccount *account)
{
	GList *status_types = NULL;
	GaimStatusType *type;

	type = gaim_status_type_new_full(GAIM_STATUS_OFFLINE, "offline", _("Offline"), FALSE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	type = gaim_status_type_new_full(GAIM_STATUS_ONLINE, "available", _("Available"), FALSE, TRUE, FALSE);
	status_types = g_list_append(status_types, type);

	return status_types;
}

static void
rendezvous_prpl_login(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc = gaim_account_get_connection(account);
	RendezvousData *rd;

	rd = g_new0(RendezvousData, 1);
	rd->buddies = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, rendezvous_buddy_free);
	gc->proto_data = rd;

	gaim_connection_update_progress(gc, _("Preparing Buddy List"), 0, RENDEZVOUS_CONNECT_STEPS);
	rendezvous_removeallfromlocal(gc);

	rd->listener = gaim_network_listen_range(5298, 5308);
	if (rd->listener == -1) {
		gaim_connection_error(gc, _("Unable to establish listening port."));
		return;
	}
	rd->listener_watcher = gaim_input_add(rd->listener, GAIM_INPUT_READ, rendezvous_direct_acceptconnection, gc);
	rd->listener_port = gaim_network_get_port_from_fd(rd->listener);

	gaim_connection_update_progress(gc, _("Connecting"), 1, RENDEZVOUS_CONNECT_STEPS);
	rd->fd = mdns_socket_establish();
	if (rd->fd == -1) {
		gaim_connection_error(gc, _("Unable to establish mDNS socket."));
		return;
	}

	/*
	 * Watch our listening multicast UDP socket for incoming DNS
	 * packets.
	 */
	gc->inpa = gaim_input_add(rd->fd, GAIM_INPUT_READ, rendezvous_callback, gc);

	gaim_connection_set_state(gc, GAIM_CONNECTED);

	mdns_query(rd->fd, "_presence._tcp.local", RENDEZVOUS_RRTYPE_ALL);
	rendezvous_send_online(gc);
}

static void
rendezvous_prpl_close(GaimConnection *gc)
{
	RendezvousData *rd = (RendezvousData *)gc->proto_data;
	ResourceRecordRDataTXTNode *node;

	rendezvous_send_offline(gc);

	if (gc->inpa)
		gaim_input_remove(gc->inpa);

	rendezvous_removeallfromlocal(gc);

	if (rd == NULL)
		return;

	mdns_socket_close(rd->fd);

	if (rd->listener >= 0)
		close(rd->listener);

	if (rd->listener_watcher != 0)
		gaim_input_remove(rd->listener_watcher);

	g_hash_table_destroy(rd->buddies);

	while (rd->mytxtdata != NULL) {
		node = rd->mytxtdata->data;
		rd->mytxtdata = g_slist_remove(rd->mytxtdata, node);
		g_free(node->name);
		g_free(node->value);
		g_free(node);
	}

	g_free(rd);
}

static int
rendezvous_prpl_send_im(GaimConnection *gc, const char *who, const char *message, GaimConvImFlags flags)
{
	gaim_debug_info("rendezvous", "Sending IM\n");

	/*
	 * TODO: Need to interpret any GaimConvImFlags here, before
	 * calling rendezvous_direct_send_message().
	 */
	rendezvous_direct_send_message(gc, who, message);

	return 1;
}

static void
rendezvous_prpl_set_status(GaimAccount *account, GaimStatus *status)
{
	gaim_debug_error("rendezvous", "Set status to %s\n", gaim_status_get_name(status));
}

static GaimPlugin *my_protocol = NULL;

static GaimPluginProtocolInfo prpl_info;

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
	&prpl_info,                                       /**< extra_info     */
	NULL,
	NULL
};

static void init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;
	char hostname[255];

	prpl_info.options				= OPT_PROTO_NO_PASSWORD;
	prpl_info.icon_spec.format		= "jpeg";
	prpl_info.icon_spec.min_width	= 0;
	prpl_info.icon_spec.min_height	= 0;
	prpl_info.icon_spec.max_width	= 0;
	prpl_info.icon_spec.max_height	= 0;
	prpl_info.icon_spec.scale_rules	= 0;
	prpl_info.list_icon				= rendezvous_prpl_list_icon;
	prpl_info.list_emblems			= rendezvous_prpl_list_emblems;
	prpl_info.status_text			= rendezvous_prpl_status_text;
	prpl_info.tooltip_text			= rendezvous_prpl_tooltip_text;
	prpl_info.status_types			= rendezvous_prpl_status_types;
	prpl_info.login					= rendezvous_prpl_login;
	prpl_info.close					= rendezvous_prpl_close;
	prpl_info.send_im				= rendezvous_prpl_send_im;
	prpl_info.set_status			= rendezvous_prpl_set_status;

	if (gethostname(hostname, 255) != 0) {
		gaim_debug_warning("rendezvous", "Error %d when getting host name.  Using \"localhost.\"\n", errno);
		strcpy(hostname, "localhost");
	}

	/* Try to avoid making this configurable... */
	split = gaim_account_user_split_new(_("Host name"), hostname, '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_string_new(_("First name"), "first", "Gaim");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_string_new(_("Last name"), "last", _("User"));
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = gaim_account_option_bool_new(_("Share AIM screen name"), "shareaim", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	my_protocol = plugin;
}

GAIM_INIT_PLUGIN(rendezvous, init_plugin, info);
