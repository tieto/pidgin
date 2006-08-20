/**
 * @file simple.c
 *
 * gaim
 *
 * Copyright (C) 2005 Thomas Butter <butter@uni-mannheim.de>
 *
 * ***
 * Thanks to Google's Summer of Code Program and the helpful mentors
 * ***
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

#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "dnsquery.h"
#include "debug.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "network.h"
#include "xmlnode.h"

#include "simple.h"
#include "sipmsg.h"
#include "dnssrv.h"
#include "ntlm.h"

static char *gentag() {
	return g_strdup_printf("%04d%04d", rand() & 0xFFFF, rand() & 0xFFFF);
}

static char *genbranch() {
	return g_strdup_printf("z9hG4bK%04X%04X%04X%04X%04X",
		rand() & 0xFFFF, rand() & 0xFFFF, rand() & 0xFFFF,
		rand() & 0xFFFF, rand() & 0xFFFF);
}

static char *gencallid() {
	return g_strdup_printf("%04Xg%04Xa%04Xi%04Xm%04Xt%04Xb%04Xx%04Xx",
		rand() & 0xFFFF, rand() & 0xFFFF, rand() & 0xFFFF,
		rand() & 0xFFFF, rand() & 0xFFFF, rand() & 0xFFFF,
		rand() & 0xFFFF, rand() & 0xFFFF);
}

static const char *simple_list_icon(GaimAccount *a, GaimBuddy *b) {
	return "simple";
}

static void simple_keep_alive(GaimConnection *gc) {
	struct simple_account_data *sip = gc->proto_data;
	if(sip->udp) { /* in case of UDP send a packet only with a 0 byte to
			 remain in the NAT table */
		gchar buf[2] = {0, 0};
		gaim_debug_info("simple", "sending keep alive\n");
		sendto(sip->fd, buf, 1, 0, (struct sockaddr*)&sip->serveraddr, sizeof(struct sockaddr_in));
	}
	return;
}

static gboolean process_register_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc);
static void send_notify(struct simple_account_data *sip, struct simple_watcher *);

static void send_publish(struct simple_account_data *sip);

static void do_notifies(struct simple_account_data *sip) {
	GSList *tmp = sip->watcher;
	gaim_debug_info("simple", "do_notifies()\n");
	if((sip->republish != -1) || sip->republish < time(NULL)) {
		if(gaim_account_get_bool(sip->account, "dopublish", TRUE)) {
			send_publish(sip);
		}
	}

	while(tmp) {
		gaim_debug_info("simple", "notifying %s\n", ((struct simple_watcher*)tmp->data)->name);
		send_notify(sip, tmp->data);
		tmp = tmp->next;
	}
}

static void simple_set_status(GaimAccount *account, GaimStatus *status) {
	GaimStatusPrimitive primitive = gaim_status_type_get_primitive(gaim_status_get_type(status));
	struct simple_account_data *sip = NULL;

	if (!gaim_status_is_active(status))
		return;

	if (account->gc)
		sip = account->gc->proto_data;

	if (sip)
	{
		g_free(sip->status);
		if (primitive == GAIM_STATUS_AVAILABLE)
			sip->status = g_strdup("available");
		else
			sip->status = g_strdup("busy");

		do_notifies(sip);
	}
}

static struct sip_connection *connection_find(struct simple_account_data *sip, int fd) {
	struct sip_connection *ret = NULL;
	GSList *entry = sip->openconns;
	while(entry) {
		ret = entry->data;
		if(ret->fd == fd) return ret;
		entry = entry->next;
	}
	return NULL;
}

static struct simple_watcher *watcher_find(struct simple_account_data *sip,
		const gchar *name) {
	struct simple_watcher *watcher;
	GSList *entry = sip->watcher;
	while(entry) {
		watcher = entry->data;
		if(!strcmp(name, watcher->name)) return watcher;
		entry = entry->next;
	}
	return NULL;
}

static struct simple_watcher *watcher_create(struct simple_account_data *sip,
		const gchar *name, const gchar *callid, const gchar *ourtag,
		const gchar *theirtag, gboolean needsxpidf) {
	struct simple_watcher *watcher = g_new0(struct simple_watcher, 1);
	watcher->name = g_strdup(name);
	watcher->dialog.callid = g_strdup(callid);
	watcher->dialog.ourtag = g_strdup(ourtag);
	watcher->dialog.theirtag = g_strdup(theirtag);
	watcher->needsxpidf = needsxpidf;
	sip->watcher = g_slist_append(sip->watcher, watcher);
	return watcher;
}

static void watcher_remove(struct simple_account_data *sip, const gchar *name) {
	struct simple_watcher *watcher = watcher_find(sip, name);
	sip->watcher = g_slist_remove(sip->watcher, watcher);
	g_free(watcher->name);
	g_free(watcher->dialog.callid);
	g_free(watcher->dialog.ourtag);
	g_free(watcher->dialog.theirtag);
	g_free(watcher);
}

static struct sip_connection *connection_create(struct simple_account_data *sip, int fd) {
	struct sip_connection *ret = g_new0(struct sip_connection, 1);
	ret->fd = fd;
	sip->openconns = g_slist_append(sip->openconns, ret);
	return ret;
}

static void connection_remove(struct simple_account_data *sip, int fd) {
	struct sip_connection *conn = connection_find(sip, fd);
	sip->openconns = g_slist_remove(sip->openconns, conn);
	if(conn->inputhandler) gaim_input_remove(conn->inputhandler);
	g_free(conn->inbuf);
	g_free(conn);
}

static void connection_free_all(struct simple_account_data *sip) {
	struct sip_connection *ret = NULL;
	GSList *entry = sip->openconns;
	while(entry) {
		ret = entry->data;
		connection_remove(sip, ret->fd);
		entry = sip->openconns;
	}
}

static void simple_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct simple_account_data *sip = (struct simple_account_data *)gc->proto_data;
	struct simple_buddy *b;
	if(strncmp("sip:", buddy->name, 4)) {
		gchar *buf = g_strdup_printf("sip:%s", buddy->name);
		gaim_blist_rename_buddy(buddy, buf);
		g_free(buf);
	}
	if(!g_hash_table_lookup(sip->buddies, buddy->name)) {
		b = g_new0(struct simple_buddy, 1);
		gaim_debug_info("simple", "simple_add_buddy %s\n", buddy->name);
		b->name = g_strdup(buddy->name);
		g_hash_table_insert(sip->buddies, b->name, b);
	} else {
		gaim_debug_info("simple", "buddy %s already in internal list\n", buddy->name);
	}
}

static void simple_get_buddies(GaimConnection *gc) {
	GaimBlistNode *gnode, *cnode, *bnode;

	gaim_debug_info("simple", "simple_get_buddies\n");

	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode)) continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode)) continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode)) continue;
				if(((GaimBuddy*)bnode)->account == gc->account)
					simple_add_buddy(gc, (GaimBuddy*)bnode, (GaimGroup *)gnode);
			}
		}
	}
}

static void simple_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct simple_account_data *sip = (struct simple_account_data *)gc->proto_data;
	struct simple_buddy *b = g_hash_table_lookup(sip->buddies, buddy->name);
	g_hash_table_remove(sip->buddies, buddy->name);
	g_free(b->name);
	g_free(b);
}

static GList *simple_status_types(GaimAccount *acc) {
	GaimStatusType *type;
	GList *types = NULL;

	type = gaim_status_type_new_with_attrs(
		GAIM_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), gaim_value_new(GAIM_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	type = gaim_status_type_new_full(
		GAIM_STATUS_OFFLINE, NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, type);

	return types;
}

static gchar *auth_header(struct simple_account_data *sip,
		struct sip_auth *auth, const gchar *method, const gchar *target) {
	gchar noncecount[9];
	gchar *response;
	gchar *ret;
	gchar *tmp;
	const char *authdomain;
	const char *authuser;

	authdomain = gaim_account_get_string(sip->account, "authdomain", "");
	authuser = gaim_account_get_string(sip->account, "authuser", sip->username);

	if(!authuser || strlen(authuser) < 1) {
		authuser = sip->username;
	}

	if(auth->type == 1) { /* Digest */
		sprintf(noncecount, "%08d", auth->nc++);
		response = gaim_cipher_http_digest_calculate_response(
							"md5", method, target, NULL, NULL,
							auth->nonce, noncecount, NULL, auth->digest_session_key);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "response %s\n", response);

		ret = g_strdup_printf("Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", nc=\"%s\", response=\"%s\"\r\n", authuser, auth->realm, auth->nonce, target, noncecount, response);
		g_free(response);
		return ret;
	} else if(auth->type == 2) { /* NTLM */
		if(auth->nc == 3 && auth->nonce) {
			/* TODO: Don't hardcode "gaim" as the hostname */
			ret = gaim_ntlm_gen_type3(authuser, sip->password, "gaim", authdomain, (const guint8 *)auth->nonce, &auth->flags);
			tmp = g_strdup_printf("NTLM qop=\"auth\", opaque=\"%s\", realm=\"%s\", targetname=\"%s\", gssapi-data=\"%s\"\r\n", auth->opaque, auth->realm, auth->target, ret);
			g_free(ret);
			return tmp;
		}
		tmp = g_strdup_printf("NTLM qop=\"auth\", realm=\"%s\", targetname=\"%s\", gssapi-data=\"\"\r\n", auth->realm, auth->target);
		return tmp;
	}

	sprintf(noncecount, "%08d", auth->nc++);
	response = gaim_cipher_http_digest_calculate_response(
						"md5", method, target, NULL, NULL,
						auth->nonce, noncecount, NULL, auth->digest_session_key);
	gaim_debug(GAIM_DEBUG_MISC, "simple", "response %s\n", response);

	ret = g_strdup_printf("Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", nc=\"%s\", response=\"%s\"\r\n", authuser, auth->realm, auth->nonce, target, noncecount, response);
	g_free(response);
	return ret;
}

static char *parse_attribute(const char *attrname, const char *source) {
	const char *tmp, *tmp2;
	char *retval = NULL;
	int len = strlen(attrname);

	if(!strncmp(source, attrname, len)) {
		tmp = source + len;
		tmp2 = g_strstr_len(tmp, strlen(tmp), "\"");
		if(tmp2)
			retval = g_strndup(tmp, tmp2 - tmp);
		else
			retval = g_strdup(tmp);
	}

	return retval;
}

static void fill_auth(struct simple_account_data *sip, gchar *hdr, struct sip_auth *auth) {
	int i = 0;
	const char *authuser;
	char *tmp;
	gchar **parts;

	authuser = gaim_account_get_string(sip->account, "authuser", sip->username);

	if(!authuser || strlen(authuser) < 1) {
		authuser = sip->username;
	}

	if(!hdr) {
		gaim_debug_error("simple", "fill_auth: hdr==NULL\n");
		return;
	}

	if(!g_strncasecmp(hdr, "NTLM", 4)) {
		gaim_debug_info("simple", "found NTLM\n");
		auth->type = 2;
		parts = g_strsplit(hdr+5, "\", ", 0);
		i = 0;
		while(parts[i]) {
			gaim_debug_info("simple", "parts[i] %s\n", parts[i]);
			if((tmp = parse_attribute("gssapi-data=\"", parts[i]))) {
				auth->nonce = g_memdup(gaim_ntlm_parse_type2(tmp, &auth->flags), 8);
				g_free(tmp);
			}
			if((tmp = parse_attribute("targetname=\"",
					parts[i]))) {
				auth->target = tmp;
			}
			else if((tmp = parse_attribute("realm=\"",
					parts[i]))) {
				auth->realm = tmp;
			}
			else if((tmp = parse_attribute("opaque=\"", parts[i]))) {
				auth->opaque = tmp;
			}
			i++;
		}
		g_strfreev(parts);
		auth->nc = 1;
		if(!strstr(hdr, "gssapi-data")) {
			auth->nc = 1;
		} else {
			auth->nc = 3;
                }
		return;
	}

	auth->type = 1;
	parts = g_strsplit(hdr, " ", 0);
	while(parts[i]) {
		if((tmp = parse_attribute("nonce=\"", parts[i]))) {
			auth->nonce = tmp;
		}
		else if((tmp = parse_attribute("realm=\"", parts[i]))) {
			auth->realm = tmp;
		}
		i++;
	}
	g_strfreev(parts);

	gaim_debug(GAIM_DEBUG_MISC, "simple", "nonce: %s realm: %s\n", auth->nonce ? auth->nonce : "(null)", auth->realm ? auth->realm : "(null)");
	if(auth->realm) {
		auth->digest_session_key = gaim_cipher_http_digest_calculate_session_key(
				"md5", authuser, auth->realm, sip->password, auth->nonce, NULL);

		auth->nc = 1;
	}
}

static void simple_canwrite_cb(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	gsize max_write;
	gssize written;

	max_write = gaim_circ_buffer_get_max_read(sip->txbuf);

	if(max_write == 0) {
		gaim_input_remove(sip->tx_handler);
		sip->tx_handler = 0;
		return;
	}

	written = write(sip->fd, sip->txbuf->outptr, max_write);

	if(written < 0 && errno == EAGAIN)
		written = 0;
	else if(written <= 0) {
		/*TODO: do we really want to disconnect on a failure to write?*/
		gaim_connection_error(gc, _("Could not write"));
		return;
	}

	gaim_circ_buffer_mark_read(sip->txbuf, written);
}

static void simple_input_cb(gpointer data, gint source, GaimInputCondition cond);

static void send_later_cb(gpointer data, gint source, const gchar *error) {
	GaimConnection *gc = data;
	struct simple_account_data *sip;
	struct sip_connection *conn;

	if (!GAIM_CONNECTION_IS_VALID(gc))
	{
		if (source >= 0)
			close(source);
		return;
	}

	if(source < 0) {
		gaim_connection_error(gc, _("Could not connect"));
		return;
	}

	sip = gc->proto_data;
	sip->fd = source;
	sip->connecting = FALSE;

	simple_canwrite_cb(gc, sip->fd, GAIM_INPUT_WRITE);

	/* If there is more to write now, we need to register a handler */
	if(sip->txbuf->bufused > 0)
		sip->tx_handler = gaim_input_add(sip->fd, GAIM_INPUT_WRITE,
			simple_canwrite_cb, gc);

	conn = connection_create(sip, source);
	conn->inputhandler = gaim_input_add(sip->fd, GAIM_INPUT_READ, simple_input_cb, gc);
}


static void sendlater(GaimConnection *gc, const char *buf) {
	struct simple_account_data *sip = gc->proto_data;
	GaimProxyConnectInfo *connect_info;

	if(!sip->connecting) {
		gaim_debug_info("simple", "connecting to %s port %d\n", sip->realhostname ? sip->realhostname : "{NULL}", sip->realport);
		connect_info = gaim_proxy_connect(sip->account, sip->realhostname, sip->realport, send_later_cb, gc);
		if(connect_info == NULL) {
			gaim_connection_error(gc, _("Couldn't create socket"));
		}
		sip->connecting = TRUE;
	}

	if(gaim_circ_buffer_get_max_read(sip->txbuf) > 0)
		gaim_circ_buffer_append(sip->txbuf, "\r\n", 2);

	gaim_circ_buffer_append(sip->txbuf, buf, strlen(buf));
}

static void sendout_pkt(GaimConnection *gc, const char *buf) {
	struct simple_account_data *sip = gc->proto_data;
	time_t currtime = time(NULL);
	int writelen = strlen(buf);

	gaim_debug(GAIM_DEBUG_MISC, "simple", "\n\nsending - %s\n######\n%s\n######\n\n", ctime(&currtime), buf);
	if(sip->udp) {
		if(sendto(sip->fd, buf, writelen, 0, (struct sockaddr*)&sip->serveraddr, sizeof(struct sockaddr_in)) < writelen) {
			gaim_debug_info("simple", "could not send packet\n");
		}
	} else {
		int ret;
		if(sip->fd < 0) {
			sendlater(gc, buf);
			return;
		}

		if(sip->tx_handler) {
			ret = -1;
			errno = EAGAIN;
		} else
			ret = write(sip->fd, buf, writelen);

		if (ret < 0 && errno == EAGAIN)
			ret = 0;
		else if(ret <= 0) { /* XXX: When does this happen legitimately? */
			sendlater(gc, buf);
			return;
		}

		if (ret < writelen) {
			if(!sip->tx_handler)
				sip->tx_handler = gaim_input_add(sip->fd,
					GAIM_INPUT_WRITE, simple_canwrite_cb,
					gc);

			/* XXX: is it OK to do this? You might get part of a request sent
			   with part of another. */
			if(sip->txbuf->bufused > 0)
				gaim_circ_buffer_append(sip->txbuf, "\r\n", 2);

			gaim_circ_buffer_append(sip->txbuf, buf + ret,
				writelen - ret);
		}
	}
}

static void sendout_sipmsg(struct simple_account_data *sip, struct sipmsg *msg) {
	GSList *tmp = msg->headers;
	gchar *name;
	gchar *value;
	GString *outstr = g_string_new("");
	g_string_append_printf(outstr, "%s %s SIP/2.0\r\n", msg->method, msg->target);
	while(tmp) {
		name = ((struct siphdrelement*) (tmp->data))->name;
		value = ((struct siphdrelement*) (tmp->data))->value;
		g_string_append_printf(outstr, "%s: %s\r\n", name, value);
		tmp = g_slist_next(tmp);
	}
	g_string_append_printf(outstr, "\r\n%s", msg->body ? msg->body : "");
	sendout_pkt(sip->gc, outstr->str);
	g_string_free(outstr, TRUE);
}

static void send_sip_response(GaimConnection *gc, struct sipmsg *msg, int code,
		const char *text, const char *body) {
	GSList *tmp = msg->headers;
	gchar *name;
	gchar *value;
	GString *outstr = g_string_new("");

	/* When sending the acknowlegements and errors, the content length from the original
	   message is still here, but there is no body; we need to make sure we're sending the
	   correct content length */
	sipmsg_remove_header(msg, "Content-Length");
	if(body) {
		gchar len[12];
		sprintf(len, "%" G_GSIZE_FORMAT , strlen(body));
		sipmsg_add_header(msg, "Content-Length", len);
	}
	else
		sipmsg_add_header(msg, "Content-Length", "0");
	g_string_append_printf(outstr, "SIP/2.0 %d %s\r\n", code, text);
	while(tmp) {
		name = ((struct siphdrelement*) (tmp->data))->name;
		value = ((struct siphdrelement*) (tmp->data))->value;

		g_string_append_printf(outstr, "%s: %s\r\n", name, value);
		tmp = g_slist_next(tmp);
	}
	g_string_append_printf(outstr, "\r\n%s", body ? body : "");
	sendout_pkt(gc, outstr->str);
	g_string_free(outstr, TRUE);
}

static void transactions_remove(struct simple_account_data *sip, struct transaction *trans) {
	if(trans->msg) sipmsg_free(trans->msg);
	sip->transactions = g_slist_remove(sip->transactions, trans);
	g_free(trans);
}

static void transactions_add_buf(struct simple_account_data *sip, const gchar *buf, void *callback) {
	struct transaction *trans = g_new0(struct transaction, 1);
	trans->time = time(NULL);
	trans->msg = sipmsg_parse_msg(buf);
	trans->cseq = sipmsg_find_header(trans->msg, "CSeq");
	trans->callback = callback;
	sip->transactions = g_slist_append(sip->transactions, trans);
}

static struct transaction *transactions_find(struct simple_account_data *sip, struct sipmsg *msg) {
	struct transaction *trans;
	GSList *transactions = sip->transactions;
	gchar *cseq = sipmsg_find_header(msg, "CSeq");

	while(transactions) {
		trans = transactions->data;
		if(!strcmp(trans->cseq, cseq)) {
			return trans;
		}
		transactions = transactions->next;
	}

	return NULL;
}

static void send_sip_request(GaimConnection *gc, const gchar *method,
		const gchar *url, const gchar *to, const gchar *addheaders,
		const gchar *body, struct sip_dialog *dialog, TransCallback tc) {
	struct simple_account_data *sip = gc->proto_data;
	char *callid = dialog ? g_strdup(dialog->callid) : gencallid();
	char *auth = NULL;
	const char *addh = "";
	gchar *branch = genbranch();
	gchar *tag = NULL;
	char *buf;

	if(!strcmp(method, "REGISTER")) {
		if(sip->regcallid) {
			g_free(callid);
			callid = g_strdup(sip->regcallid);
		}
		else sip->regcallid = g_strdup(callid);
	}

	if(addheaders) addh = addheaders;
	if(sip->registrar.type && !strcmp(method, "REGISTER")) {
		buf = auth_header(sip, &sip->registrar, method, url);
		auth = g_strdup_printf("Authorization: %s", buf);
		g_free(buf);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "header %s", auth);
	}

	if(sip->proxy.type && strcmp(method, "REGISTER")) {
		buf = auth_header(sip, &sip->proxy, method, url);
		auth = g_strdup_printf("Proxy-Authorization: %s", buf);
		g_free(buf);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "header %s", auth);
	}

	if (!dialog)
		tag = gentag();

	buf = g_strdup_printf("%s %s SIP/2.0\r\n"
			"Via: SIP/2.0/%s %s:%d;branch=%s\r\n"
			/* Don't know what epid is, but LCS wants it */
			"From: <sip:%s@%s>;tag=%s;epid=1234567890\r\n"
			"To: <%s>%s%s\r\n"
			"Max-Forwards: 10\r\n"
			"CSeq: %d %s\r\n"
			"User-Agent: Gaim/" VERSION "\r\n"
			"Call-ID: %s\r\n"
			"%s%s"
			"Content-Length: %" G_GSIZE_FORMAT "\r\n\r\n%s",
			method,
			url,
			sip->udp ? "UDP" : "TCP",
			gaim_network_get_my_ip(-1),
			sip->listenport,
			branch,
			sip->username,
			sip->servername,
			dialog ? dialog->ourtag : tag,
			to,
			dialog ? ";tag=" : "",
			dialog ? dialog->theirtag : "",
			++sip->cseq,
			method,
			callid,
			auth ? auth : "",
			addh,
			strlen(body),
			body);

	g_free(tag);
	g_free(auth);
	g_free(branch);
	g_free(callid);

	/* add to ongoing transactions */

	transactions_add_buf(sip, buf, tc);

	sendout_pkt(gc, buf);

	g_free(buf);
}

static char *get_contact(struct simple_account_data  *sip) {
	return g_strdup_printf("<sip:%s@%s:%d;transport=%s>;methods=\"MESSAGE, SUBSCRIBE, NOTIFY\"", sip->username, gaim_network_get_my_ip(-1), sip->listenport, sip->udp ? "udp" : "tcp");
}

static void do_register_exp(struct simple_account_data *sip, int expire) {
	char *uri = g_strdup_printf("sip:%s", sip->servername);
	char *to = g_strdup_printf("sip:%s@%s", sip->username, sip->servername);
	char *contact = get_contact(sip);
	char *hdr = g_strdup_printf("Contact: %s\r\nExpires: %d\r\n", contact, expire);
	g_free(contact);

	sip->registerstatus = 1;

	if(expire) {
		sip->reregister = time(NULL) + expire - 50;
	} else {
		sip->reregister = time(NULL) + 600;
	}

	send_sip_request(sip->gc, "REGISTER", uri, to, hdr, "", NULL,
		process_register_response);

	g_free(hdr);
	g_free(uri);
	g_free(to);
}

static void do_register(struct simple_account_data *sip) {
	do_register_exp(sip, sip->registerexpire);
}

static gchar *parse_from(const gchar *hdr) {
	gchar *from;
	const gchar *tmp, *tmp2 = hdr;

	if(!hdr) return NULL;
	gaim_debug_info("simple", "parsing address out of %s\n", hdr);
	tmp = strchr(hdr, '<');

	/* i hate the different SIP UA behaviours... */
	if(tmp) { /* sip address in <...> */
		tmp2 = tmp + 1;
		tmp = strchr(tmp2, '>');
		if(tmp) {
			from = g_strndup(tmp2, tmp - tmp2);
		} else {
			gaim_debug_info("simple", "found < without > in From\n");
			return NULL;
		}
	} else {
		tmp = strchr(tmp2, ';');
		if(tmp) {
			from = g_strndup(tmp2, tmp - tmp2);
		} else {
			from = g_strdup(tmp2);
		}
	}
	gaim_debug_info("simple", "got %s\n", from);
	return from;
}

static gboolean process_subscribe_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	gchar *to;

	if(msg->response == 200 || msg->response == 202) {
		return TRUE;
	}

	to = parse_from(sipmsg_find_header(tc->msg, "To")); /* cant be NULL since it is our own msg */

	/* we can not subscribe -> user is offline (TODO unknown status?) */

	gaim_prpl_got_user_status(sip->account, to, "offline", NULL);
	g_free(to);
	return TRUE;
}

static void simple_subscribe(struct simple_account_data *sip, struct simple_buddy *buddy) {
	gchar *contact = "Expires: 1200\r\nAccept: application/pidf+xml, application/xpidf+xml\r\nEvent: presence\r\n";
	gchar *to;
	gchar *tmp;

	if(strstr(buddy->name, "sip:"))
		to = g_strdup(buddy->name);
	else
		to = g_strdup_printf("sip:%s", buddy->name);

	tmp = get_contact(sip);
	contact = g_strdup_printf("%sContact: %s\r\n", contact, tmp);
	g_free(tmp);

	/* subscribe to buddy presence
	 * we dont need to know the status so we do not need a callback */

	send_sip_request(sip->gc, "SUBSCRIBE", to, to, contact, "", NULL,
		process_subscribe_response);

	g_free(to);
	g_free(contact);

	/* resubscribe before subscription expires */
	/* add some jitter */
	buddy->resubscribe = time(NULL)+1140+(rand()%50);
}

static gboolean simple_add_lcs_contacts(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	gchar *tmp;
	xmlnode *item, *group, *isc;
	const char *name_group;
	GaimBuddy *b;
	GaimGroup *g = NULL;
	struct simple_buddy *bs;
	int len = msg->bodylen;


	tmp = sipmsg_find_header(msg, "Event");
	if(tmp && !strncmp(tmp, "vnd-microsoft-roaming-contacts", 30)){

		gaim_debug_info("simple", "simple_add_lcs_contacts->%s-%d\n", msg->body, len);
		/*Convert the contact from XML to Gaim Buddies*/
		isc = xmlnode_from_str(msg->body, len);

		/* ToDo. Find for all groups */
		if ((group = xmlnode_get_child(isc, "group"))) {
			name_group = xmlnode_get_attrib(group, "name");
			gaim_debug_info("simple", "name_group->%s\n", name_group);
			g = gaim_find_group(name_group);
			if(!g)
				g = gaim_group_new(name_group);
		}

		if (!g) {
			g = gaim_find_group("Buddies");
			if(!g)
				g = gaim_group_new("Buddies");
		}

		for(item = xmlnode_get_child(isc, "contact"); item; item = xmlnode_get_next_twin(item))
		{
			const char *uri, *name, *groups;
			char *buddy_name;
			uri = xmlnode_get_attrib(item, "uri");
			name = xmlnode_get_attrib(item, "name");
			groups = xmlnode_get_attrib(item, "groups");
			gaim_debug_info("simple", "URI->%s\n", uri);

			buddy_name = g_strdup_printf("sip:%s", uri);

			b = gaim_find_buddy(sip->account, buddy_name);
			if(!b){
				b = gaim_buddy_new(sip->account, buddy_name, uri);
			}
			g_free(buddy_name);

			gaim_blist_add_buddy(b, NULL, g, NULL);
			gaim_blist_alias_buddy(b, uri);
			bs = g_new0(struct simple_buddy, 1);
			bs->name = g_strdup(b->name);
			g_hash_table_insert(sip->buddies, bs->name, bs);
		}
		xmlnode_free(isc);
	}
	return 0;
}

static void simple_subscribe_buddylist(struct simple_account_data *sip) {
	gchar *contact = "Event: vnd-microsoft-roaming-contacts\r\nAccept: application/vnd-microsoft-roaming-contacts+xml\r\nSupported: com.microsoft.autoextend\r\nSupported: ms-benotify\r\nProxy-Require: ms-benotify\r\nSupported: ms-piggyback-first-notify\r\n";
	gchar *to;
	gchar *tmp;
	to = g_strdup_printf("sip:%s@%s", sip->username, sip->servername);

	tmp = get_contact(sip);

	contact = g_strdup_printf("%sContact: %s\r\n", contact, tmp);
	g_free(tmp);

	send_sip_request(sip->gc, "SUBSCRIBE", to, to, contact, "", NULL, simple_add_lcs_contacts);

	g_free(to);
	g_free(contact);
}


static void simple_buddy_resub(char *name, struct simple_buddy *buddy, struct simple_account_data *sip) {
	time_t curtime = time(NULL);
	gaim_debug_info("simple", "buddy resub\n");
	if(buddy->resubscribe < curtime) {
		gaim_debug(GAIM_DEBUG_MISC, "simple", "simple_buddy_resub %s\n", name);
		simple_subscribe(sip, buddy);
	}
}

static gboolean resend_timeout(struct simple_account_data *sip) {
	GSList *tmp = sip->transactions;
	time_t currtime = time(NULL);
	while(tmp) {
		struct transaction *trans = tmp->data;
		tmp = tmp->next;
		gaim_debug_info("simple", "have open transaction age: %d\n", currtime- trans->time);
		if((currtime - trans->time > 5) && trans->retries >= 1) {
			/* TODO 408 */
		} else {
			if((currtime - trans->time > 2) && trans->retries == 0) {
				trans->retries++;
				sendout_sipmsg(sip, trans->msg);
			}
		}
	}
	return TRUE;
}

static gboolean subscribe_timeout(struct simple_account_data *sip) {
	GSList *tmp;
	time_t curtime = time(NULL);
	/* register again if first registration expires */
	if(sip->reregister < curtime) {
		do_register(sip);
	}
	/* check for every subscription if we need to resubscribe */
	g_hash_table_foreach(sip->buddies, (GHFunc)simple_buddy_resub, (gpointer)sip);

	/* remove a timed out suscriber */

	tmp = sip->watcher;
	while(tmp) {
		struct simple_watcher *watcher = tmp->data;
		if(watcher->expire < curtime) {
			watcher_remove(sip, watcher->name);
			tmp = sip->watcher;
		}
		if(tmp) tmp = tmp->next;
	}

	return TRUE;
}

static void simple_send_message(struct simple_account_data *sip, const char *to, const char *msg, const char *type) {
	gchar *hdr;
	gchar *fullto;
	if(strncmp("sip:", to, 4)) {
		fullto = g_strdup_printf("sip:%s", to);
	} else {
		fullto = g_strdup(to);
	}
	if(type) {
		hdr = g_strdup_printf("Content-Type: %s\r\n", type);
	} else {
		hdr = g_strdup("Content-Type: text/plain\r\n");
	}
	send_sip_request(sip->gc, "MESSAGE", fullto, fullto, hdr, msg, NULL, NULL);
	g_free(hdr);
	g_free(fullto);
}

static int simple_im_send(GaimConnection *gc, const char *who, const char *what, GaimMessageFlags flags) {
	struct simple_account_data *sip = gc->proto_data;
	char *to = g_strdup(who);
	char *text = gaim_unescape_html(what);
	simple_send_message(sip, to, text, NULL);
	g_free(to);
	g_free(text);
	return 1;
}

static void process_incoming_message(struct simple_account_data *sip, struct sipmsg *msg) {
	gchar *from;
	gchar *contenttype;
	gboolean found = FALSE;

	from = parse_from(sipmsg_find_header(msg, "From"));

	if(!from) return;

	gaim_debug(GAIM_DEBUG_MISC, "simple", "got message from %s: %s\n", from, msg->body);

	contenttype = sipmsg_find_header(msg, "Content-Type");
	if(!contenttype || !strncmp(contenttype, "text/plain", 10) || !strncmp(contenttype, "text/html", 9)) {
		serv_got_im(sip->gc, from, msg->body, 0, time(NULL));
		send_sip_response(sip->gc, msg, 200, "OK", NULL);
		found = TRUE;
	}
	if(!strncmp(contenttype, "application/im-iscomposing+xml", 30)) {
		xmlnode *isc = xmlnode_from_str(msg->body, msg->bodylen);
		xmlnode *state;
		gchar *statedata;

		if(!isc) {
			gaim_debug_info("simple", "process_incoming_message: can not parse iscomposing\n");
			return;
		}

		state = xmlnode_get_child(isc, "state");

		if(!state) {
			gaim_debug_info("simple", "process_incoming_message: no state found\n");
			xmlnode_free(isc);
			return;
		}

		statedata = xmlnode_get_data(state);
		if(statedata) {
			if(strstr(statedata, "active")) serv_got_typing(sip->gc, from, 0, GAIM_TYPING);
			else serv_got_typing_stopped(sip->gc, from);

			g_free(statedata);
		}
		xmlnode_free(isc);
		send_sip_response(sip->gc, msg, 200, "OK", NULL);
		found = TRUE;
	}
	if(!found) {
		gaim_debug_info("simple", "got unknown mime-type");
		send_sip_response(sip->gc, msg, 415, "Unsupported media type", NULL);
	}
	g_free(from);
}


gboolean process_register_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	gchar *tmp;
	gaim_debug(GAIM_DEBUG_MISC, "simple", "in process register response response: %d\n", msg->response);
	switch (msg->response) {
		case 200:
			if(sip->registerstatus < 3) { /* registered */
				if(gaim_account_get_bool(sip->account, "dopublish", TRUE)) {
					send_publish(sip);
				}
			}
			sip->registerstatus = 3;
			gaim_connection_set_state(sip->gc, GAIM_CONNECTED);

			/* get buddies from blist */
			simple_get_buddies(sip->gc);

			subscribe_timeout(sip);
			tmp = sipmsg_find_header(msg, "Allow-Events");
		        if(tmp && strstr(tmp, "vnd-microsoft-provisioning")){
				simple_subscribe_buddylist(sip);
			}

			break;
		case 401:
			if(sip->registerstatus != 2) {
				gaim_debug_info("simple", "REGISTER retries %d\n", sip->registrar.retries);
				if(sip->registrar.retries > 3) {
					sip->gc->wants_to_die = TRUE;
					gaim_connection_error(sip->gc, _("Wrong Password"));
					return TRUE;
				}
				tmp = sipmsg_find_header(msg, "WWW-Authenticate");
				fill_auth(sip, tmp, &sip->registrar);
				sip->registerstatus = 2;
				do_register(sip);
			}
			break;
		}
	return TRUE;
}

static void process_incoming_notify(struct simple_account_data *sip, struct sipmsg *msg) {
	gchar *from;
	gchar *fromhdr;
	gchar *tmp2;
	xmlnode *pidf;
	xmlnode *basicstatus = NULL, *tuple, *status;
	gboolean isonline = FALSE;

	fromhdr = sipmsg_find_header(msg, "From");
	from = parse_from(fromhdr);
	if(!from) return;

	pidf = xmlnode_from_str(msg->body, msg->bodylen);

	if(!pidf) {
		gaim_debug_info("simple", "process_incoming_notify: no parseable pidf\n");
		return;
	}

	if ((tuple = xmlnode_get_child(pidf, "tuple")))
		if ((status = xmlnode_get_child(tuple, "status")))
			basicstatus = xmlnode_get_child(status, "basic");

	if(!basicstatus) {
		gaim_debug_info("simple", "process_incoming_notify: no basic found\n");
		xmlnode_free(pidf);
		return;
	}

	tmp2 = xmlnode_get_data(basicstatus);

	if(!tmp2) {
		gaim_debug_info("simple", "process_incoming_notify: no basic data found\n");
		xmlnode_free(pidf);
		return;
	}

	if(strstr(tmp2, "open")) {
		isonline = TRUE;
	}

	g_free(tmp2);

	if(isonline) gaim_prpl_got_user_status(sip->account, from, "available", NULL);
	else gaim_prpl_got_user_status(sip->account, from, "offline", NULL);

	xmlnode_free(pidf);

	g_free(from);
	send_sip_response(sip->gc, msg, 200, "OK", NULL);
}

static unsigned int simple_typing(GaimConnection *gc, const char *name, GaimTypingState state) {
	struct simple_account_data *sip = gc->proto_data;

	gchar *xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\"\n"
			"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
			"xsi:schemaLocation=\"urn:ietf:params:xml:ns:im-composing iscomposing.xsd\">\n"
			"<state>%s</state>\n"
			"<contenttype>text/plain</contenttype>\n"
			"<refresh>60</refresh>\n"
			"</isComposing>";
	gchar *recv = g_strdup(name);
	if(state == GAIM_TYPING) {
		gchar *msg = g_strdup_printf(xml, "active");
		simple_send_message(sip, recv, msg, "application/im-iscomposing+xml");
		g_free(msg);
	} else /* TODO: Only if (state == GAIM_TYPED) ? */ {
		gchar *msg = g_strdup_printf(xml, "idle");
		simple_send_message(sip, recv, msg, "application/im-iscomposing+xml");
		g_free(msg);
	}
	g_free(recv);
	/*
	 * TODO: Is this right?  It will cause the core to call
	 *       serv_send_typing(gc, who, GAIM_TYPING) once every second
	 *       until the user stops typing.  If that's not desired,
	 *       then return 0 instead.
	 */
	return 1;
}

static gchar *find_tag(const gchar *hdr) {
	const gchar *tmp = strstr(hdr, ";tag="), *tmp2;

	if(!tmp) return NULL;
	tmp += 5;
	if((tmp2 = strchr(tmp, ';'))) {
		return g_strndup(tmp, tmp2 - tmp);
	}
	return g_strdup(tmp);
}

static gchar* gen_xpidf(struct simple_account_data *sip) {
	gchar *doc = g_strdup_printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<presence>\n"
			"<presentity uri=\"sip:%s@%s;method=SUBSCRIBE\"/>\n"
			"<display name=\"sip:%s@%s\"/>\n"
			"<atom id=\"1234\">\n"
			"<address uri=\"sip:%s@%s\">\n"
			"<status status=\"%s\"/>\n"
			"</address>\n"
			"</atom>\n"
			"</presence>\n",
			sip->username,
			sip->servername,
			sip->username,
			sip->servername,
			sip->username,
			sip->servername,
			sip->status);
	return doc;
}



static gchar* gen_pidf(struct simple_account_data *sip) {
	gchar *doc = g_strdup_printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<presence xmlns=\"urn:ietf:params:xml:ns:pidf\"\n"
			"xmlns:im=\"urn:ietf:params:xml:ns:pidf:im\"\n"
			"entity=\"sip:%s@%s\">\n"
			"<tuple id=\"bs35r9f\">\n"
			"<status>\n"
			"<basic>open</basic>\n"
			"</status>\n"
			"<note>%s</note>\n"
			"</tuple>\n"
			"</presence>",
			sip->username,
			sip->servername,
			sip->status);
	return doc;
}

static void send_notify(struct simple_account_data *sip, struct simple_watcher *watcher) {
	gchar *doc = watcher->needsxpidf ? gen_xpidf(sip) : gen_pidf(sip);
	gchar *hdr = watcher->needsxpidf ? "Event: presence\r\nContent-Type: application/xpidf+xml\r\n" : "Event: presence\r\nContent-Type: application/pidf+xml\r\n";
	send_sip_request(sip->gc, "NOTIFY", watcher->name, watcher->name, hdr, doc, &watcher->dialog, NULL);
	g_free(doc);
}

static gboolean process_publish_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	if(msg->response != 200 && msg->response != 408) {
		/* never send again */
		sip->republish = -1;
	}
	return TRUE;
}

static void send_publish(struct simple_account_data *sip) {
	gchar *uri = g_strdup_printf("sip:%s@%s", sip->username, sip->servername);
	gchar *doc = gen_pidf(sip);
	send_sip_request(sip->gc, "PUBLISH", uri, uri,
		"Expires: 600\r\nEvent: presence\r\n"
		"Content-Type: application/pidf+xml\r\n",
		doc, NULL, process_publish_response);
	sip->republish = time(NULL) + 500;
	g_free(uri);
	g_free(doc);
}

static void process_incoming_subscribe(struct simple_account_data *sip, struct sipmsg *msg) {
	const char *from_hdr = sipmsg_find_header(msg, "From");
	gchar *from = parse_from(from_hdr);
	gchar *theirtag = find_tag(from_hdr);
	gchar *ourtag = find_tag(sipmsg_find_header(msg, "To"));
	gboolean tagadded = FALSE;
	gchar *callid = sipmsg_find_header(msg, "Call-ID");
	gchar *expire = sipmsg_find_header(msg, "Expire");
	gchar *tmp;
	struct simple_watcher *watcher = watcher_find(sip, from);
	if(!ourtag) {
		tagadded = TRUE;
		ourtag = gentag();
	}
	if(!watcher) { /* new subscription */
		gchar *acceptheader = sipmsg_find_header(msg, "Accept");
		gboolean needsxpidf = FALSE;
		if(!gaim_privacy_check(sip->account, from)) {
			send_sip_response(sip->gc, msg, 202, "Ok", NULL);
			goto privend;
		}
		if(acceptheader) {
			gchar *tmp = acceptheader;
			gboolean foundpidf = FALSE;
			gboolean foundxpidf = FALSE;
			while(tmp && tmp < acceptheader + strlen(acceptheader)) {
				gchar *tmp2 = strchr(tmp, ',');
				if(tmp2) *tmp2 = '\0';
				if(!strcmp("application/pidf+xml", tmp))
					foundpidf = TRUE;
				if(!strcmp("application/xpidf+xml", tmp))
					foundxpidf = TRUE;
				if(tmp2) {
					*tmp2 = ',';
					tmp = tmp2;
					while(*tmp == ' ') tmp++;
				} else
					tmp = 0;
			}
			if(!foundpidf && foundxpidf) needsxpidf = TRUE;
			g_free(acceptheader);
		}
		watcher = watcher_create(sip, from, callid, ourtag, theirtag, needsxpidf);
	}
	if(tagadded) {
		gchar *to = g_strdup_printf("%s;tag=%s", sipmsg_find_header(msg, "To"), ourtag);
		sipmsg_remove_header(msg, "To");
		sipmsg_add_header(msg, "To", to);
		g_free(to);
	}
	if(expire)
		watcher->expire = time(NULL) + strtol(expire, NULL, 10);
	else
		watcher->expire = time(NULL) + 600;
	sipmsg_remove_header(msg, "Contact");
	tmp = get_contact(sip);
	sipmsg_add_header(msg, "Contact", tmp);
	g_free(tmp);
	gaim_debug_info("simple", "got subscribe: name %s ourtag %s theirtag %s callid %s\n", watcher->name, watcher->dialog.ourtag, watcher->dialog.theirtag, watcher->dialog.callid);
	send_sip_response(sip->gc, msg, 200, "Ok", NULL);
	send_notify(sip, watcher);
privend:
	g_free(from);
	g_free(theirtag);
	g_free(ourtag);
	g_free(callid);
	g_free(expire);
}

static void process_input_message(struct simple_account_data *sip, struct sipmsg *msg) {
	gboolean found = FALSE;
	if(msg->response == 0) { /* request */
		if(!strcmp(msg->method, "MESSAGE")) {
			process_incoming_message(sip, msg);
			found = TRUE;
		} else if(!strcmp(msg->method, "NOTIFY")) {
			process_incoming_notify(sip, msg);
			found = TRUE;
		} else if(!strcmp(msg->method, "SUBSCRIBE")) {
			process_incoming_subscribe(sip, msg);
			found = TRUE;
		} else {
			send_sip_response(sip->gc, msg, 501, "Not implemented", NULL);
		}
	} else { /* response */
		struct transaction *trans = transactions_find(sip, msg);
		if(trans) {
			if(msg->response == 407) {
				gchar *resend, *auth, *ptmp;

				if(sip->proxy.retries > 3) return;
				sip->proxy.retries++;
				/* do proxy authentication */

				ptmp = sipmsg_find_header(msg, "Proxy-Authenticate");

				fill_auth(sip, ptmp, &sip->proxy);
				auth = auth_header(sip, &sip->proxy, trans->msg->method, trans->msg->target);
				sipmsg_remove_header(trans->msg, "Proxy-Authorization");
				sipmsg_add_header(trans->msg, "Proxy-Authorization", auth);
				g_free(auth);
				resend = sipmsg_to_string(trans->msg);
				/* resend request */
				sendout_pkt(sip->gc, resend);
				g_free(resend);
			} else {
				if(msg->response == 100) {
					/* ignore provisional response */
					gaim_debug_info("simple", "got trying response\n");
				} else {
					sip->proxy.retries = 0;
					if(!strcmp(trans->msg->method, "REGISTER")) {
						if(msg->response == 401) sip->registrar.retries++;
						else sip->registrar.retries = 0;
					} else {
						if(msg->response == 401) {
							gchar *resend, *auth, *ptmp;

							if(sip->registrar.retries > 4) return;
							sip->registrar.retries++;

							ptmp = sipmsg_find_header(msg, "WWW-Authenticate");

							fill_auth(sip, ptmp, &sip->registrar);
							auth = auth_header(sip, &sip->registrar, trans->msg->method, trans->msg->target);
							sipmsg_remove_header(trans->msg, "Authorization");
							sipmsg_add_header(trans->msg, "Authorization", auth);
							g_free(auth);
							resend = sipmsg_to_string(trans->msg);
							/* resend request */
							sendout_pkt(sip->gc, resend);
							g_free(resend);
						}
					}
					if(trans->callback) {
						/* call the callback to process response*/
						(trans->callback)(sip, msg, trans);
					}
					transactions_remove(sip, trans);
				}
			}
			found = TRUE;
		} else {
			gaim_debug(GAIM_DEBUG_MISC, "simple", "received response to unknown transaction");
		}
	}
	if(!found) {
		gaim_debug(GAIM_DEBUG_MISC, "simple", "received a unknown sip message with method %s and response %d\n", msg->method, msg->response);
	}
}

static void process_input(struct simple_account_data *sip, struct sip_connection *conn)
{
	char *cur;
	char *dummy;
	struct sipmsg *msg;
	int restlen;
	cur = conn->inbuf;

	/* according to the RFC remove CRLF at the beginning */
	while(*cur == '\r' || *cur == '\n') {
		cur++;
	}
	if(cur != conn->inbuf) {
		memmove(conn->inbuf, cur, conn->inbufused - (cur - conn->inbuf));
		conn->inbufused = strlen(conn->inbuf);
	}

	/* Received a full Header? */
	if((cur = strstr(conn->inbuf, "\r\n\r\n")) != NULL) {
		time_t currtime = time(NULL);
		cur += 2;
		cur[0] = '\0';
		gaim_debug_info("simple", "\n\nreceived - %s\n######\n%s\n#######\n\n", ctime(&currtime), conn->inbuf);
		msg = sipmsg_parse_header(conn->inbuf);
		cur[0] = '\r';
		cur += 2;
		restlen = conn->inbufused - (cur - conn->inbuf);
		if(restlen >= msg->bodylen) {
			dummy = g_malloc(msg->bodylen + 1);
			memcpy(dummy, cur, msg->bodylen);
			dummy[msg->bodylen] = '\0';
			msg->body = dummy;
			cur += msg->bodylen;
			memmove(conn->inbuf, cur, conn->inbuflen - (cur - conn->inbuf));
			conn->inbufused = strlen(conn->inbuf);
		} else {
			sipmsg_free(msg);
			return;
		}
		gaim_debug(GAIM_DEBUG_MISC, "simple", "in process response response: %d\n", msg->response);
		process_input_message(sip, msg);
	} else {
		gaim_debug(GAIM_DEBUG_MISC, "simple", "received a incomplete sip msg: %s\n", conn->inbuf);
	}
}

static void simple_udp_process(gpointer data, gint source, GaimInputCondition con) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	struct sipmsg *msg;
	int len;
	time_t currtime;

	static char buffer[65536];
	if((len = recv(source, buffer, sizeof(buffer) - 1, 0)) > 0) {
		buffer[len] = '\0';
		gaim_debug_info("simple", "\n\nreceived - %s\n######\n%s\n#######\n\n", ctime(&currtime), buffer);
		msg = sipmsg_parse_msg(buffer);
		if(msg) process_input_message(sip, msg);
	}
}

static void simple_input_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	int len;
	struct sip_connection *conn = connection_find(sip, source);
	if(!conn) {
		gaim_debug_error("simple", "Connection not found!\n");
		return;
	}

	if(conn->inbuflen < conn->inbufused + SIMPLE_BUF_INC) {
		conn->inbuflen += SIMPLE_BUF_INC;
		conn->inbuf = g_realloc(conn->inbuf, conn->inbuflen);
	}

	len = read(source, conn->inbuf + conn->inbufused, SIMPLE_BUF_INC - 1);

	if(len < 0 && errno == EAGAIN)
		return;
	else if(len <= 0) {
		gaim_debug_info("simple", "simple_input_cb: read error\n");
		connection_remove(sip, source);
		if(sip->fd == source) sip->fd = -1;
		return;
	}

	conn->inbufused += len;
	conn->inbuf[conn->inbufused] = '\0';

	process_input(sip, conn);
}

/* Callback for new connections on incoming TCP port */
static void simple_newconn_cb(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	struct sip_connection *conn;

	int newfd = accept(source, NULL, NULL);

	conn = connection_create(sip, newfd);

	conn->inputhandler = gaim_input_add(newfd, GAIM_INPUT_READ, simple_input_cb, gc);
}

static void login_cb(gpointer data, gint source, const gchar *error_message) {
	GaimConnection *gc = data;
	struct simple_account_data *sip;
	struct sip_connection *conn;

	if (!GAIM_CONNECTION_IS_VALID(gc))
	{
		if (source >= 0)
			close(source);
		return;
	}

	if(source < 0) {
		gaim_connection_error(gc, _("Could not connect"));
		return;
	}

	sip = gc->proto_data;
	sip->fd = source;

	conn = connection_create(sip, source);

	sip->registertimeout = gaim_timeout_add((rand()%100)+10*1000, (GSourceFunc)subscribe_timeout, sip);

	do_register(sip);

	conn->inputhandler = gaim_input_add(sip->fd, GAIM_INPUT_READ, simple_input_cb, gc);
}

static guint simple_ht_hash_nick(const char *nick) {
	char *lc = g_utf8_strdown(nick, -1);
	guint bucket = g_str_hash(lc);
	g_free(lc);

	return bucket;
}

static gboolean simple_ht_equals_nick(const char *nick1, const char *nick2) {
	return (gaim_utf8_strcasecmp(nick1, nick2) == 0);
}

static void simple_udp_host_resolved_listen_cb(int listenfd, gpointer data) {
	struct simple_account_data *sip = (struct simple_account_data*) data;

	if(listenfd == -1) {
		gaim_connection_error(sip->gc, _("Could not create listen socket"));
		return;
	}

	sip->fd = listenfd;

	sip->listenport = gaim_network_get_port_from_fd(sip->fd);
	sip->listenfd = sip->fd;

	sip->listenpa = gaim_input_add(sip->fd, GAIM_INPUT_READ, simple_udp_process, sip->gc);

	sip->resendtimeout = gaim_timeout_add(2500, (GSourceFunc) resend_timeout, sip);
	sip->registertimeout = gaim_timeout_add((rand()%100)+10*1000, (GSourceFunc)subscribe_timeout, sip);
	do_register(sip);
}

static void simple_udp_host_resolved(GSList *hosts, gpointer data, const char *error_message) {
	struct simple_account_data *sip = (struct simple_account_data*) data;
	int addr_size;

	sip->query_data = NULL;

	if (!hosts || !hosts->data) {
		gaim_connection_error(sip->gc, _("Couldn't resolve host"));
		return;
	}

	addr_size = GPOINTER_TO_INT(hosts->data);
	hosts = g_slist_remove(hosts, hosts->data);
	memcpy(&(sip->serveraddr), hosts->data, addr_size);
	g_free(hosts->data);
	hosts = g_slist_remove(hosts, hosts->data);
	while(hosts) {
		hosts = g_slist_remove(hosts, hosts->data);
		g_free(hosts->data);
		hosts = g_slist_remove(hosts, hosts->data);
	}

	/* create socket for incoming connections */
	if(!gaim_network_listen_range(5060, 5160, SOCK_DGRAM,
				simple_udp_host_resolved_listen_cb, sip)) {
		gaim_connection_error(sip->gc, _("Could not create listen socket"));
		return;
	}
}

static void
simple_tcp_connect_listen_cb(int listenfd, gpointer data) {
	struct simple_account_data *sip = (struct simple_account_data*) data;
	GaimProxyConnectInfo *connect_info;

	sip->listenfd = listenfd;
	if(sip->listenfd == -1) {
		gaim_connection_error(sip->gc, _("Could not create listen socket"));
		return;
	}

	gaim_debug_info("simple", "listenfd: %d\n", sip->listenfd);
	sip->listenport = gaim_network_get_port_from_fd(sip->listenfd);
	sip->listenpa = gaim_input_add(sip->listenfd, GAIM_INPUT_READ,
			simple_newconn_cb, sip->gc);
	gaim_debug_info("simple", "connecting to %s port %d\n",
			sip->realhostname, sip->realport);
	/* open tcp connection to the server */
	connect_info = gaim_proxy_connect(sip->account, sip->realhostname,
			sip->realport, login_cb, sip->gc);
	if(connect_info == NULL) {
		gaim_connection_error(sip->gc, _("Couldn't create socket"));
	}
}

static void srvresolved(GaimSrvResponse *resp, int results, gpointer data) {
	GaimConnection *gc;
	struct simple_account_data *sip;
	gchar *hostname;
	int port;

	gc = data;
	if (!GAIM_CONNECTION_IS_VALID(gc))
	{
		/* This connection has been closed */
		g_free(resp);
		return;
	}

	sip = gc->proto_data;
	port = gaim_account_get_int(sip->account, "port", 0);

	/* find the host to connect to */
	if(results) {
		hostname = g_strdup(resp->hostname);
		if(!port)
			port = resp->port;
		g_free(resp);
	} else {
		if(!gaim_account_get_bool(sip->account, "useproxy", FALSE)) {
			hostname = g_strdup(sip->servername);
		} else {
			hostname = g_strdup(gaim_account_get_string(sip->account, "proxy", sip->servername));
		}
	}

	sip->realhostname = hostname;
	sip->realport = port;
	if(!sip->realport) sip->realport = 5060;

	/* TCP case */
	if(!sip->udp) {
		/* create socket for incoming connections */
		if(!gaim_network_listen_range(5060, 5160, SOCK_STREAM,
					simple_tcp_connect_listen_cb, sip)) {
			gaim_connection_error(sip->gc, _("Could not create listen socket"));
			return;
		}
	} else { /* UDP */
		gaim_debug_info("simple", "using udp with server %s and port %d\n", hostname, port);

		sip->query_data = gaim_dnsquery_a(hostname, port, simple_udp_host_resolved, sip);
		if (sip->query_data == NULL) {
			gaim_connection_error(sip->gc, _("Could not resolve hostname"));
		}
	}
}

static void simple_login(GaimAccount *account)
{
	GaimConnection *gc;
	struct simple_account_data *sip;
	gchar **userserver;
	gchar *hosttoconnect;

	const char *username = gaim_account_get_username(account);
	gc = gaim_account_get_connection(account);

	if (strpbrk(username, " \t\v\r\n") != NULL) {
		gc->wants_to_die = TRUE;
		gaim_connection_error(gc, _("SIP usernames may not contain whitespaces or @ symbols"));
		return;
	}

	gc->proto_data = sip = g_new0(struct simple_account_data, 1);
	sip->gc = gc;
	sip->account = account;
	sip->registerexpire = 900;
	sip->udp = gaim_account_get_bool(account, "udp", FALSE);
	/* TODO: is there a good default grow size? */
	if(!sip->udp)
		sip->txbuf = gaim_circ_buffer_new(0);

	userserver = g_strsplit(username, "@", 2);
	gaim_connection_set_display_name(gc, userserver[0]);
	sip->username = g_strdup(userserver[0]);
	sip->servername = g_strdup(userserver[1]);
	sip->password = g_strdup(gaim_connection_get_password(gc));
	g_strfreev(userserver);

	sip->buddies = g_hash_table_new((GHashFunc)simple_ht_hash_nick, (GEqualFunc)simple_ht_equals_nick);

	gaim_connection_update_progress(gc, _("Connecting"), 1, 2);

	/* TODO: Set the status correctly. */
	sip->status = g_strdup("available");

	if(!gaim_account_get_bool(account, "useproxy", FALSE)) {
		hosttoconnect = g_strdup(sip->servername);
	} else {
		hosttoconnect = g_strdup(gaim_account_get_string(account, "proxy", sip->servername));
	}

	/* TCP case */
	if(!sip->udp) {
		gaim_srv_resolve("sip", "tcp", hosttoconnect, srvresolved, gc);
	} else { /* UDP */
		gaim_srv_resolve("sip", "udp", hosttoconnect, srvresolved, gc);
	}
	g_free(hosttoconnect);
}

static void simple_close(GaimConnection *gc)
{
	struct simple_account_data *sip = gc->proto_data;

	if(sip) {
		/* unregister */
		do_register_exp(sip, 0);
		connection_free_all(sip);

		if (sip->query_data != NULL)
			gaim_dnsquery_destroy(sip->query_data);

		g_free(sip->servername);
		g_free(sip->username);
		g_free(sip->password);
		g_free(sip->registrar.nonce);
		g_free(sip->registrar.opaque);
		g_free(sip->registrar.target);
		g_free(sip->registrar.realm);
		g_free(sip->registrar.digest_session_key);
		g_free(sip->proxy.nonce);
		g_free(sip->proxy.opaque);
		g_free(sip->proxy.target);
		g_free(sip->proxy.realm);
		g_free(sip->proxy.digest_session_key);
		if(sip->txbuf)
			gaim_circ_buffer_destroy(sip->txbuf);
		g_free(sip->realhostname);
		if(sip->listenpa) gaim_input_remove(sip->listenpa);
		if(sip->tx_handler) gaim_input_remove(sip->tx_handler);
		if(sip->resendtimeout) gaim_timeout_remove(sip->resendtimeout);
		if(sip->registertimeout) gaim_timeout_remove(sip->registertimeout);
	}
	g_free(gc->proto_data);
	gc->proto_data = NULL;
}

/* not needed since privacy is checked for every subscribe */
static void dummy_add_deny(GaimConnection *gc, const char *name) {
}

static void dummy_permit_deny(GaimConnection *gc) {
}

static GaimPluginProtocolInfo prpl_info =
{
	0,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	simple_list_icon,		/* list_icon */
	NULL,					/* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	simple_status_types,	/* away_states */
	NULL,					/* blist_node_menu */
	NULL,					/* chat_info */
	NULL,					/* chat_info_defaults */
	simple_login,			/* login */
	simple_close,			/* close */
	simple_im_send,			/* send_im */
	NULL,					/* set_info */
	simple_typing,			/* send_typing */
	NULL,					/* get_info */
	simple_set_status,		/* set_status */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	simple_add_buddy,		/* add_buddy */
	NULL,					/* add_buddies */
	simple_remove_buddy,	/* remove_buddy */
	NULL,					/* remove_buddies */
	dummy_add_deny,			/* add_permit */
	dummy_add_deny,			/* add_deny */
	dummy_add_deny,			/* rem_permit */
	dummy_add_deny,			/* rem_deny */
	dummy_permit_deny,		/* set_permit_deny */
	NULL,					/* join_chat */
	NULL,					/* reject_chat */
	NULL,					/* get_chat_name */
	NULL,					/* chat_invite */
	NULL,					/* chat_leave */
	NULL,					/* chat_whisper */
	NULL,					/* chat_send */
	simple_keep_alive,		/* keepalive */
	NULL,					/* register_user */
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
	NULL,					/* send_file */
	NULL,					/* new_xfer */
	NULL,					/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
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

	"prpl-simple",                                    /**< id             */
	"SIMPLE",                                         /**< name           */
	VERSION,                                          /**< version        */
	N_("SIP/SIMPLE Protocol Plugin"),                 /**  summary        */
	N_("The SIP/SIMPLE Protocol Plugin"),             /**  description    */
	"Thomas Butter <butter@uni-mannheim.de>",         /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	NULL
};

static void _init_plugin(GaimPlugin *plugin)
{
	GaimAccountUserSplit *split;
	GaimAccountOption *option;

	split = gaim_account_user_split_new(_("Server"), "", '@');
	prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_bool_new(_("Publish status (note: everyone may watch you)"), "dopublish", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_int_new(_("Connect port"), "port", 0);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_bool_new(_("Use UDP"), "udp", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_bool_new(_("Use proxy"), "useproxy", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_string_new(_("Proxy"), "proxy", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_string_new(_("Auth User"), "authuser", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_string_new(_("Auth Domain"), "authdomain", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

GAIM_INIT_PLUGIN(simple, _init_plugin, info);
