/*
 * @file yahoo_filexfer.c Yahoo Filetransfer
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

/* TODO: it needs further refactoring */

#include "internal.h"
#include "dnsquery.h"

#include "prpl.h"
#include "util.h"
#include "debug.h"
#include "http.h"
#include "network.h"
#include "notify.h"
#include "proxy.h"
#include "ft.h"
#include "libymsg.h"
#include "yahoo_packet.h"
#include "yahoo_filexfer.h"
#include "yahoo_doodle.h"
#include "yahoo_friend.h"

struct yahoo_xfer_data {
	gchar *url;
	gboolean is_relay;
	PurpleHttpConnection *hc;

	gchar *host;
	gchar *path;
	PurpleConnection *gc;
	gchar *xfer_peer_idstring;
	gchar *xfer_idstring_for_relay;
	int info_val_249;

	/* contains all filenames, in case of multiple transfers, with the first
	 * one in the list being the current file's name (ymsg15) */
	GSList *filename_list;
	GSList *size_list; /* corresponds to filename_list, with size as **STRING** */
	gboolean firstoflist;
};

static void
yahoo_xfer_data_free(struct yahoo_xfer_data *xd)
{
	PurpleConnection *gc;
	YahooData *yd;
	PurpleXfer *xfer;
	GSList *l;

	gc = xd->gc;
	yd = purple_connection_get_protocol_data(gc);

	/* remove entry from map */
	if(xd->xfer_peer_idstring) {
		xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map, xd->xfer_peer_idstring);
		if(xfer)
			g_hash_table_remove(yd->xfer_peer_idstring_map, xd->xfer_peer_idstring);
	}

	/* empty file & filesize list */
	for (l = xd->filename_list; l; l = l->next) {
		g_free(l->data);
		l->data=NULL;
	}
	for (l = xd->size_list; l; l = l->next) {
		g_free(l->data);
		l->data=NULL;
	}
	g_slist_free(xd->filename_list);
	g_slist_free(xd->size_list);

	g_free(xd->host);
	g_free(xd->path);
	g_free(xd->xfer_peer_idstring);
	g_free(xd->xfer_idstring_for_relay);
	g_free(xd);
}

static void
yahoo_xfer_start(PurpleXfer *xfer);

static PurpleXfer *
yahoo_ft_new_xfer_struct(PurpleConnection *gc, PurpleXferType type,
	const char *who);

static PurpleHttpRequest *
yahoo_ft_new_req(struct yahoo_xfer_data *xd)
{
	PurpleHttpRequest *req;
	YahooData *yd;

	g_return_val_if_fail(xd != NULL, NULL);

	yd = purple_connection_get_protocol_data(xd->gc);

	req = purple_http_request_new(xd->url);
	purple_http_request_header_set(req, "User-Agent",
		YAHOO_CLIENT_USERAGENT);
	purple_http_request_header_set(req, "Cache-Control", "no-cache");
	if (xd->is_relay) {
		PurpleHttpCookieJar *cjar;

		cjar = purple_http_request_get_cookie_jar(req);
		purple_http_cookie_jar_set(cjar, "Y", yd->cookie_y);
		purple_http_cookie_jar_set(cjar, "T", yd->cookie_t);
	}

	return req;
}

static gchar *
yahoo_ft_url_gen(PurpleXfer *xfer, const gchar *host)
{
	struct yahoo_xfer_data *xfer_data;
	PurpleAccount *account;

	g_return_val_if_fail(host != NULL, NULL);

	xfer_data = purple_xfer_get_protocol_data(xfer);
	account = purple_connection_get_account(xfer_data->gc);

	if (!xfer_data->is_relay) {
		purple_debug_fatal("yahoo", "Non-relay FT aren't tested yet\n");
		return NULL;
	}

	return g_strdup_printf("http://%s/relay?token=%s&sender=%s&recver=%s",
		host, purple_url_encode(xfer_data->xfer_idstring_for_relay),
		purple_normalize(account, purple_account_get_username(account)),
		purple_xfer_get_remote_user(xfer));
}

static void
yahoo_xfer_init_15(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;
	PurpleConnection *gc;
	PurpleAccount *account;
	YahooData *yd;
	struct yahoo_packet *pkt;

	xfer_data = purple_xfer_get_protocol_data(xfer);
	gc = xfer_data->gc;
	yd = purple_connection_get_protocol_data(gc);
	account = purple_connection_get_account(gc);

	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)	{
		gchar *filename;
		filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));
		pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
							   YAHOO_STATUS_AVAILABLE,
							   yd->session_id);
		yahoo_packet_hash(pkt, "sssiiiisiii",
			1, purple_normalize(account, purple_account_get_username(account)),
			5, purple_xfer_get_remote_user(xfer),
			265, xfer_data->xfer_peer_idstring,
			222, 1,
			266, 1,
			302, 268,
			300, 268,
			27,  filename,
			28,  (int)purple_xfer_get_size(xfer),
			301, 268,
			303, 268);
		g_free(filename);
	} else {
		if(xfer_data->firstoflist == TRUE) {
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
				YAHOO_STATUS_AVAILABLE, yd->session_id);

			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				222, 3);
		} else {
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_ACC_15,
				YAHOO_STATUS_AVAILABLE, yd->session_id);

			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				271, 1);
		}
	}
	yahoo_packet_send_and_free(pkt, yd);
}

static void
yahoo_xfer_cancel_send(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = purple_xfer_get_protocol_data(xfer);

	if(purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL)
	{
		PurpleConnection *gc;
		PurpleAccount *account;
		YahooData *yd;
		struct yahoo_packet *pkt;

		gc = xfer_data->gc;
		yd = purple_connection_get_protocol_data(gc);
		account = purple_connection_get_account(gc);
		if(xfer_data->xfer_idstring_for_relay) /* hack to see if file trans acc/info packet has been received */
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_INFO_15,
								   YAHOO_STATUS_DISCONNECTED,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				66, -1);
		}
		else
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
								   YAHOO_STATUS_AVAILABLE,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				222, 2);
		}
		yahoo_packet_send_and_free(pkt, yd);
	}

	if (xfer_data) {
		purple_http_conn_cancel(xfer_data->hc);
		yahoo_xfer_data_free(xfer_data);
	}
	purple_xfer_set_protocol_data(xfer, NULL);
}

static void
yahoo_xfer_cancel_recv(PurpleXfer *xfer)
{
	struct yahoo_xfer_data *xfer_data;

	xfer_data = purple_xfer_get_protocol_data(xfer);

	if(purple_xfer_get_status(xfer) == PURPLE_XFER_STATUS_CANCEL_LOCAL)
	{

		PurpleConnection *gc;
		PurpleAccount *account;
		YahooData *yd;
		struct yahoo_packet *pkt;

		gc = xfer_data->gc;
		yd = purple_connection_get_protocol_data(gc);
		account = purple_connection_get_account(gc);
		if(!xfer_data->xfer_idstring_for_relay) /* hack to see if file trans acc/info packet has been received */
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
								   YAHOO_STATUS_AVAILABLE,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				222, 4);
		}
		else
		{
			pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_15,
								   YAHOO_STATUS_DISCONNECTED,
								   yd->session_id);
			yahoo_packet_hash(pkt, "sssi",
				1, purple_normalize(account, purple_account_get_username(account)),
				5, purple_xfer_get_remote_user(xfer),
				265, xfer_data->xfer_peer_idstring,
				66, -1);
		}
		yahoo_packet_send_and_free(pkt, yd);
	}

	if (xfer_data) {
		purple_http_conn_cancel(xfer_data->hc);
		yahoo_xfer_data_free(xfer_data);
	}
	purple_xfer_set_protocol_data(xfer, NULL);
}

static void yahoo_xfer_end(PurpleXfer *xfer_old)
{
	struct yahoo_xfer_data *xfer_data;
	PurpleXfer *xfer = NULL;
	PurpleConnection *gc;
	YahooData *yd;

	xfer_data = purple_xfer_get_protocol_data(xfer_old);
	if(xfer_data
	   && purple_xfer_get_type(xfer_old) == PURPLE_XFER_RECEIVE
	   && xfer_data->filename_list) {
		/* removing top of filename & size list completely */
		g_free( xfer_data->filename_list->data );
		g_free( xfer_data->size_list->data );

		xfer_data->filename_list->data = NULL;
		xfer_data->size_list->data = NULL;

		xfer_data->filename_list = g_slist_delete_link(xfer_data->filename_list, xfer_data->filename_list);
		xfer_data->size_list = g_slist_delete_link(xfer_data->size_list, xfer_data->size_list);

		/* if there are still more files */
		if(xfer_data->filename_list)
		{
			gchar* filename;
			char *utf8_filename;

			filename = xfer_data->filename_list->data;

			gc = xfer_data->gc;
			yd = purple_connection_get_protocol_data(gc);

			/* setting up xfer_data for next file's tranfer */
			g_free(xfer_data->host);
			g_free(xfer_data->path);
			g_free(xfer_data->xfer_idstring_for_relay);
			xfer_data->host = NULL;
			xfer_data->host = NULL;
			xfer_data->xfer_idstring_for_relay = NULL;
			xfer_data->info_val_249 = 0;
			xfer_data->firstoflist = FALSE;

			/* Dereference xfer_data from old xfer */
			purple_xfer_set_protocol_data(xfer_old, NULL);

			/* Build the file transfer handle. */
			xfer = yahoo_ft_new_xfer_struct(gc, PURPLE_XFER_RECEIVE, purple_xfer_get_remote_user(xfer_old));

			g_return_if_fail(xfer != NULL);

			/* Set the info about the incoming file. */
			utf8_filename = yahoo_string_decode(gc, filename, TRUE);
			purple_xfer_set_filename(xfer, utf8_filename);
			g_free(utf8_filename);

			purple_xfer_set_protocol_data(xfer, xfer_data);

			/* update map to current xfer */
			g_hash_table_remove(yd->xfer_peer_idstring_map, xfer_data->xfer_peer_idstring);
			g_hash_table_insert(yd->xfer_peer_idstring_map, xfer_data->xfer_peer_idstring, xfer);

			/* Now perform the request */
			purple_xfer_request(xfer);

			return;
		}
	}
	if (xfer_data)
		yahoo_xfer_data_free(xfer_data);
	purple_xfer_set_protocol_data(xfer_old, NULL);
}

void yahoo_process_p2pfilexfer(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	GSList *l = pkt->hash;

	char *me      = NULL;
	char *from    = NULL;
	char *service = NULL;
	char *message = NULL;
	char *command = NULL;
	char *imv     = NULL;

	/* Get all the necessary values from this new packet */
	while(l != NULL)
	{
		struct yahoo_pair *pair = l->data;

		switch(pair->key) {
		case 5:         /* Get who the packet is for */
			me = pair->value;
			break;
		case 4:         /* Get who the packet is from */
			from = pair->value;
			break;
		case 49:        /* Get the type of service */
			service = pair->value;
			break;
		case 14:        /* Get the 'message' of the packet */
			message = pair->value;
			break;
		case 13:        /* Get the command associated with this packet */
			command = pair->value;
			break;
		case 63:        /* IMVironment name and version */
			imv = pair->value;
			break;
		case 64:        /* Not sure, but it does vary with initialization of Doodle */
			break;
		}

		l = l->next;
	}

	/* If this packet is an IMVIRONMENT, handle it accordingly */
	if(service != NULL && imv != NULL && !strcmp(service, "IMVIRONMENT"))
	{
		/* Check for a Doodle packet and handle it accordingly */
		if(strstr(imv, "doodle;") != NULL)
			yahoo_doodle_process(gc, me, from, command, message, imv);

		/* If an IMVIRONMENT packet comes without a specific imviroment name */
		if(!strcmp(imv, ";0"))
		{
			/* It is unfortunately time to close all IMVironments with the remote client */
			yahoo_doodle_command_got_shutdown(gc, from);
		}
	}
}

PurpleXfer *yahoo_new_xfer(PurpleConnection *gc, const char *who)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xfer_data;

	g_return_val_if_fail(who != NULL, NULL);

	xfer_data = g_new0(struct yahoo_xfer_data, 1);
	xfer_data->gc = gc;

	/* Build the file transfer handle. */
	xfer = yahoo_ft_new_xfer_struct(gc, PURPLE_XFER_SEND, who);
	if (xfer == NULL)
	{
		g_free(xfer_data);
		g_return_val_if_reached(NULL);
	}

	purple_xfer_set_protocol_data(xfer, xfer_data);

	return xfer;
}

static gchar* yahoo_xfer_new_xfer_id(void)
{
	gchar *ans;
	int i,j;
	ans = g_strnfill(24, ' ');
	ans[23] = '$';
	ans[22] = '$';
	for(i = 0; i < 22; i++)
	{
		j = g_random_int_range (0,61);
		if(j < 26)
			ans[i] = j + 'a';
		else if(j < 52)
			ans[i] = j - 26 + 'A';
		else
			ans[i] = j - 52 + '0';
	}
	return ans;
}

static void yahoo_xfer_dns_connected_15(GSList *hosts, gpointer data, const char *error_message)
{
	PurpleXfer *xfer;
	struct yahoo_xfer_data *xd;
	struct sockaddr_in *addr;
	struct yahoo_packet *pkt;
	unsigned long actaddr;
	unsigned char a,b,c,d;
	PurpleConnection *gc;
	PurpleAccount *account;
	YahooData *yd;
	gchar *filename;

	if (!(xfer = data))
		return;
	if (!(xd = purple_xfer_get_protocol_data(xfer)))
		return;
	gc = xd->gc;
	account = purple_connection_get_account(gc);
	yd = purple_connection_get_protocol_data(gc);

	if(!hosts)
	{
		purple_debug_error("yahoo", "Unable to find an IP address for relay.msg.yahoo.com\n");
		purple_xfer_cancel_remote(xfer);
		return;
	}

	/* Discard the length... */
	hosts = g_slist_remove(hosts, hosts->data);
	if(!hosts)
	{
		purple_debug_error("yahoo", "Unable to find an IP address for relay.msg.yahoo.com\n");
		purple_xfer_cancel_remote(xfer);
		return;
	}

	/* TODO:actually, u must try with addr no.1 , if its not working addr no.2 ..... */
	addr = hosts->data;
	actaddr = addr->sin_addr.s_addr;
	d = actaddr & 0xff;
	actaddr >>= 8;
	c = actaddr & 0xff;
	actaddr >>= 8;
	b = actaddr & 0xff;
	actaddr >>= 8;
	a = actaddr & 0xff;

	xd->host = g_strdup_printf("%u.%u.%u.%u", d, c, b, a);

	/* Free the address... */
	g_free(hosts->data);
	hosts = g_slist_remove(hosts, hosts->data);
	addr = NULL;
	while (hosts != NULL)
	{
		/* Discard the length... */
		hosts = g_slist_remove(hosts, hosts->data);
		/* Free the address... */
		g_free(hosts->data);
		hosts = g_slist_remove(hosts, hosts->data);
	}

	pkt = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_INFO_15, YAHOO_STATUS_AVAILABLE, yd->session_id);
	filename = g_path_get_basename(purple_xfer_get_local_filename(xfer));

	yahoo_packet_hash(pkt, "ssssis",
		1, purple_normalize(account, purple_account_get_username(account)),
		5, purple_xfer_get_remote_user(xfer),
		265, xd->xfer_peer_idstring,
		27,  filename,
		249, 3,
		250, xd->host);

	g_free(filename);
	yahoo_packet_send_and_free(pkt, yd);
}

gboolean yahoo_can_receive_file(PurpleConnection *gc, const char *who)
{
	if (!who || yahoo_get_federation_from_name(who) != YAHOO_FEDERATION_NONE)
		return FALSE;
	return TRUE;
}

void yahoo_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	struct yahoo_xfer_data *xfer_data;
	YahooData *yd = purple_connection_get_protocol_data(gc);
	PurpleXfer *xfer = yahoo_new_xfer(gc, who);

	g_return_if_fail(xfer != NULL);

#if 0
	/* if we don't have a p2p connection, try establishing it now */
	if( !g_hash_table_lookup(yd->peers, who) )
		yahoo_send_p2p_pkt(gc, who, 0);
#endif

	xfer_data = purple_xfer_get_protocol_data(xfer);
	xfer_data->xfer_peer_idstring = yahoo_xfer_new_xfer_id();
	g_hash_table_insert(yd->xfer_peer_idstring_map, xfer_data->xfer_peer_idstring, xfer);

	/* Now perform the request */
	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

void yahoo_process_filetrans_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *from = NULL;
	char *imv = NULL;
	long val_222 = 0L;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	char *service = NULL;
	char *filename = NULL;
	char *xfer_peer_idstring = NULL;
	char *utf8_filename;
	GSList *l;
	GSList *filename_list = NULL;
	GSList *size_list = NULL;
	int nooffiles = 0;

	yd = purple_connection_get_protocol_data(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4:
			from = pair->value;
			break;
		case 5: /* to */
			break;
		case 265:
			xfer_peer_idstring = pair->value;
			break;
		case 27:
			filename_list = g_slist_prepend(filename_list, g_strdup(pair->value));
			nooffiles++;
			break;
		case 28:
			size_list = g_slist_prepend(size_list, g_strdup(pair->value));
			break;
		case 222:
			val_222 = atol(pair->value);
			/* 1=send, 2=cancel, 3=accept, 4=reject */
			break;

		/* check for p2p and imviron .... not sure it comes by this service packet. Since it was bundled with filexfer in old ymsg version, still keeping it. */
		case 49:
			service = pair->value;
			break;
		case 63:
			imv = pair->value;
			break;
		/* end check */

		}
	}
	if(!xfer_peer_idstring)
		return;

	if(val_222 == 2 || val_222 == 4)
	{
		xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map,
								   xfer_peer_idstring);
		if(!xfer) return;
		purple_xfer_cancel_remote(xfer);
		return;
	}
	if(val_222 == 3)
	{
		PurpleAccount *account;
		struct yahoo_xfer_data *xd;

		xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map,
								   xfer_peer_idstring);
		if(!xfer)
			return;

		xd = purple_xfer_get_protocol_data(xfer);

		/*
		*	In the file trans info packet that we must reply with, we are
		*	supposed to mention the ip address...
		*	purple connect does not give me a way of finding the ip address...
		*	so, purple dnsquery is used... but retries, trying with next ip
		*	address etc. is not implemented..TODO
		*/

		/* To send through p2p */
		if( g_hash_table_lookup(yd->peers, from) )	{
			/* send p2p file transfer information */
			purple_debug_error("yahoo", "p2p file transfers are not supported yet\n");
			/*xd->is_relay = FALSE;*/
		}
		xd->is_relay = TRUE;

		account = purple_connection_get_account(gc);
		if (yd->jp)
		{
			purple_dnsquery_a(account, YAHOOJP_XFER_RELAY_HOST,
							YAHOOJP_XFER_RELAY_PORT,
							yahoo_xfer_dns_connected_15, xfer);
		}
		else
		{
			purple_dnsquery_a(account, YAHOO_XFER_RELAY_HOST,
							YAHOO_XFER_RELAY_PORT,
							yahoo_xfer_dns_connected_15, xfer);
		}
		return;
	}

	/* processing for p2p and imviron .... not sure it comes by this service packet. Since it was bundled with filexfer in old ymsg version, still keeping it. */
	/*
	* The remote user has changed their IMVironment.  We
	* record it for later use.
	*/
	if (from && imv && service && (strcmp("IMVIRONMENT", service) == 0)) {
		g_hash_table_replace(yd->imvironments, g_strdup(from), g_strdup(imv));
		return;
	}

	if (pkt->service == YAHOO_SERVICE_P2PFILEXFER) {
		if (service && (strcmp("FILEXFER", service) != 0)) {
			purple_debug_misc("yahoo", "unhandled service 0x%02x\n", pkt->service);
			return;
		}
	}
	/* end processing */

	if(!filename_list)
		return;
	/* have to change list into order in which client at other end sends */
	filename_list = g_slist_reverse(filename_list);
	size_list = g_slist_reverse(size_list);
	filename = filename_list->data;

	if(!from) return;
	xfer_data = g_new0(struct yahoo_xfer_data, 1);
	xfer_data->firstoflist = TRUE;
	xfer_data->gc = gc;
	xfer_data->xfer_peer_idstring = g_strdup(xfer_peer_idstring);
	xfer_data->filename_list = filename_list;
	xfer_data->size_list = size_list;

	/* Build the file transfer handle. */
	xfer = yahoo_ft_new_xfer_struct(gc, PURPLE_XFER_RECEIVE, from);

	/* Set the info about the incoming file. */
	utf8_filename = yahoo_string_decode(gc, filename, TRUE);
	purple_xfer_set_filename(xfer, utf8_filename);
	g_free(utf8_filename);

	purple_xfer_set_protocol_data(xfer, xfer_data);

	g_hash_table_insert(yd->xfer_peer_idstring_map,
						xfer_data->xfer_peer_idstring,
						xfer);

	if(nooffiles > 1) {
		gchar* message;
		message = g_strdup_printf(_("%s is trying to send you a group of %d files.\n"), purple_xfer_get_remote_user(xfer), nooffiles);
		purple_xfer_conversation_write(xfer, message, FALSE);
		g_free(message);
	}
	/* Now perform the request */
	purple_xfer_request(xfer);
}

static void
yahoo_process_filetrans_15_sent(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	struct yahoo_xfer_data *xd;

	xd = purple_xfer_get_protocol_data(xfer);

	xd->hc = NULL;

	if (purple_xfer_is_cancelled(xfer))
		return;

	if (!purple_http_response_is_successful(response) ||
		purple_xfer_get_bytes_remaining(xfer) > 0)
	{
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
		purple_xfer_end(xfer);
	} else {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	}
}

static void
yahoo_process_filetrans_15_downloaded(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	struct yahoo_xfer_data *xd;

	xd = purple_xfer_get_protocol_data(xfer);

	xd->hc = NULL;

	if (purple_xfer_is_cancelled(xfer))
		return;

	if (!purple_http_response_is_successful(response) ||
		purple_xfer_get_bytes_remaining(xfer) > 0)
	{
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
		purple_xfer_end(xfer);
	} else {
		purple_xfer_set_completed(xfer, TRUE);
		purple_xfer_end(xfer);
	}
}

static void
yahoo_process_filetrans_15_reader(PurpleHttpConnection *hc,
	gchar *buffer, size_t offset, size_t length, gpointer _xfer,
	PurpleHttpContentReaderCb cb)
{
	PurpleXfer *xfer = _xfer;
	gssize stored;

	if (offset != purple_xfer_get_bytes_sent(xfer)) {
		purple_debug_warning("yahoo",
			"offset != purple_xfer_get_bytes_sent(xfer)\n");
	}

	stored = purple_xfer_read_file(xfer, (guchar*)buffer, length);

	cb(hc, (stored >= 0), (purple_xfer_get_bytes_remaining(xfer) == 0),
		stored);
}

static gboolean
yahoo_process_filetrans_15_writer(PurpleHttpConnection *http_conn,
	PurpleHttpResponse *response, const gchar *buffer, size_t offset,
	size_t length, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;

	return purple_xfer_write_file(xfer, (const guchar*)buffer, length);
}

static void
yahoo_process_filetrans_15_watcher(PurpleHttpConnection *hc,
	gboolean reading_state, int processed, int total, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;

	if (reading_state !=
		(purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE))
	{
		return;
	}

	purple_xfer_set_size(xfer, total);
	purple_xfer_update_progress(xfer);
}

static void yahoo_xfer_start(PurpleXfer *xfer)
{
	PurpleHttpRequest *req;
	struct yahoo_xfer_data *xd;

	xd = purple_xfer_get_protocol_data(xfer);

	req = yahoo_ft_new_req(xd);
	purple_http_request_set_timeout(req, -1);
	if (purple_xfer_get_type(xfer) == PURPLE_XFER_RECEIVE) {
		purple_http_request_set_max_len(req, -1);
		purple_http_request_set_response_writer(req,
			yahoo_process_filetrans_15_writer, xfer);
		xd->hc = purple_http_request(xd->gc, req,
			yahoo_process_filetrans_15_downloaded, xfer);
	} else {
		purple_http_request_set_method(req, "POST");
		/* YHttpServer quirk: it sets content-length, but doesn't sends
		 * any data. */
		purple_http_request_set_max_len(req, 0);
		purple_http_request_set_contents_reader(req,
			yahoo_process_filetrans_15_reader,
			purple_xfer_get_size(xfer), xfer);
		xd->hc = purple_http_request(xd->gc, req,
			yahoo_process_filetrans_15_sent, xfer);
	}

	purple_http_conn_set_progress_watcher(xd->hc,
		yahoo_process_filetrans_15_watcher, xfer, -1);
	purple_http_request_unref(req);
}

static void
yahoo_process_filetrans_info_15_got(PurpleHttpConnection *hc,
	PurpleHttpResponse *response, gpointer _xfer)
{
	PurpleXfer *xfer = _xfer;
	struct yahoo_xfer_data *xd;
	YahooData *yd;

	xd = purple_xfer_get_protocol_data(xfer);
	yd = purple_connection_get_protocol_data(xd->gc);

	xd->hc = NULL;

	if (!purple_http_response_is_successful(response)) {
		purple_notify_error(yd->gc, NULL, _("File Transfer Failed"),
			_("Unable to get file header."));
		purple_xfer_cancel_remote(xfer);
		return;
	}

	purple_xfer_start(xfer, -1, NULL, 0);
}

void yahoo_process_filetrans_info_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	char *url = NULL;
	long val_249 = 0;
	long val_66 = 0;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	char *xfer_peer_idstring = NULL;
	char *xfer_idstring_for_relay = NULL;
	GSList *l;
	struct yahoo_packet *pkt_to_send;

	yd = purple_connection_get_protocol_data(gc);

	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 4: /* from */
			break;
		case 5: /* to */
			break;
		case 265:
			xfer_peer_idstring = pair->value;
			break;
		case 27: /* filename */
			break;
		case 66:
			val_66 = strtol(pair->value, NULL, 10);
			break;
		case 249:
			val_249 = strtol(pair->value, NULL, 10);
			/* 249 has value 1 or 2 when doing p2p transfer and value 3 when relaying through yahoo server */
			break;
		case 250:
			url = pair->value; /* TODO: rename to host? what about non-relay? */
			break;
		case 251:
			xfer_idstring_for_relay = pair->value;
			break;
		}
	}

	if(!xfer_peer_idstring)
		return;

	xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map, xfer_peer_idstring);

	if(!xfer) return;

	if(val_66==-1)
	{
		purple_xfer_cancel_remote(xfer);
		return;
	}

	xfer_data = purple_xfer_get_protocol_data(xfer);

	xfer_data->info_val_249 = val_249;
	xfer_data->xfer_idstring_for_relay = g_strdup(xfer_idstring_for_relay);
	if(val_249 == 1 || val_249 == 3) {
		PurpleHttpRequest *req;
		PurpleAccount *account;

		xfer_data->is_relay = (val_249 == 3);

		if (!xfer_data->is_relay) {
			purple_notify_error(gc, NULL, _("File Transfer Failed"),
				_("Non-relay FT aren't tested yet."));
			purple_xfer_cancel_remote(xfer);
		}

		account = purple_connection_get_account(xfer_data->gc);

		xfer_data->url = yahoo_ft_url_gen(xfer, url);

		pkt_to_send = yahoo_packet_new(YAHOO_SERVICE_FILETRANS_ACC_15,
			YAHOO_STATUS_AVAILABLE, yd->session_id);
		yahoo_packet_hash(pkt_to_send, "ssssis",
			1, purple_normalize(account, purple_account_get_username(account)),
			5, purple_xfer_get_remote_user(xfer),
			265, xfer_data->xfer_peer_idstring,
			27, purple_xfer_get_filename(xfer),
			249, xfer_data->info_val_249,
			251, xfer_data->xfer_idstring_for_relay);

		yahoo_packet_send_and_free(pkt_to_send, yd);

		req = yahoo_ft_new_req(xfer_data);
		purple_http_request_set_method(req, "HEAD");
		xfer_data->hc = purple_http_request(gc, req, yahoo_process_filetrans_info_15_got, xfer);
		purple_http_request_unref(req);
	}
	else if (val_249 == 2)
		purple_debug_error("yahoo", "p2p file transfers are not supported yet\n");
}

/* TODO: Check filename etc. No probs till some hacker comes in the way */
void yahoo_process_filetrans_acc_15(PurpleConnection *gc, struct yahoo_packet *pkt)
{
	gchar *xfer_peer_idstring = NULL;
	gchar *xfer_idstring_for_relay = NULL;
	PurpleXfer *xfer;
	YahooData *yd;
	struct yahoo_xfer_data *xfer_data;
	GSList *l;
	long val_66 = 0;
	gchar *url = NULL;
	int val_249 = 0;

	yd = purple_connection_get_protocol_data(gc);
	for (l = pkt->hash; l; l = l->next) {
		struct yahoo_pair *pair = l->data;

		switch (pair->key) {
		case 251:
			xfer_idstring_for_relay = pair->value;
			break;
		case 265:
			xfer_peer_idstring = pair->value;
			break;
		case 66:
			val_66 = atol(pair->value);
			break;
		case 249:
			val_249 = atol(pair->value);
			break;
		case 250:
			url = pair->value;	/* we get a p2p url here when sending file, connected as client */
			break;
		}
	}

	xfer = g_hash_table_lookup(yd->xfer_peer_idstring_map, xfer_peer_idstring);
	if(!xfer) return;

	if(val_66 == -1 || ( (!(xfer_idstring_for_relay)) && (val_249 != 2) ))
	{
		purple_xfer_cancel_remote(xfer);
		return;
	}

	if( (val_249 == 2) && (!(url)) )
	{
		purple_xfer_cancel_remote(xfer);
		return;
	}

	xfer_data = purple_xfer_get_protocol_data(xfer);
	if (url)
		xfer_data->host = g_strdup(url);

	xfer_data->xfer_idstring_for_relay = g_strdup(xfer_idstring_for_relay);

	xfer_data->url = yahoo_ft_url_gen(xfer, xfer_data->host);
	purple_xfer_start(xfer, -1, NULL, 0);
}

static PurpleXfer *
yahoo_ft_new_xfer_struct(PurpleConnection *gc, PurpleXferType type, const char *who)
{
	PurpleXfer *xfer;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(who != NULL, NULL);

	xfer = purple_xfer_new(purple_connection_get_account(gc), type, who);

	g_return_val_if_fail(xfer != NULL, NULL);

	purple_xfer_set_init_fnc(xfer, yahoo_xfer_init_15);
	purple_xfer_set_start_fnc(xfer, yahoo_xfer_start);
	purple_xfer_set_end_fnc(xfer, yahoo_xfer_end);
	purple_xfer_set_cancel_send_fnc(xfer, yahoo_xfer_cancel_send);
	purple_xfer_set_cancel_recv_fnc(xfer, yahoo_xfer_cancel_recv);
	purple_xfer_set_request_denied_fnc(xfer, yahoo_xfer_cancel_recv);

	return xfer;
}
