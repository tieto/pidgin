/**
 * @file msn.c The MSN protocol plugin
 *
 * gaim
 *
 * Copyright (C) 2003, Christian Hammond <chipx86@gnupdate.org>
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
#include "msn.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

G_MODULE_IMPORT GSList *connections;

static struct gaim_xfer *
find_xfer_by_cookie(GaimConnection *gc, unsigned long cookie)
{
	GSList *g;
	struct msn_data *md = (struct msn_data *)gc->proto_data;
	struct gaim_xfer *xfer = NULL;
	struct msn_xfer_data *xfer_data;

	for (g = md->file_transfers; g != NULL; g = g->next) {
		xfer = (struct gaim_xfer *)g->data;
		xfer_data = (struct msn_xfer_data *)xfer->data;

		if (xfer_data->cookie == cookie)
			break;

		xfer = NULL;
	}

	return xfer;
}

static void
msn_xfer_init(struct gaim_xfer *xfer)
{
	GaimAccount *account;
	struct msn_xfer_data *xfer_data;
	struct msn_switchboard *ms;
	char header[MSN_BUF_LEN];
	char buf[MSN_BUF_LEN];

	account = gaim_xfer_get_account(xfer);

	ms = msn_find_switch(account->gc, xfer->who);

	xfer_data = (struct msn_xfer_data *)xfer->data;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		/*
		 * NOTE: We actually have to wait for the next Invitation message
		 *       before the transfer starts. We handle that in
		 *       msn_xfer_start().
		 */

		g_snprintf(header, sizeof(header),
				   "MIME-Version: 1.0\r\n"
				   "Content-Type: text/x-msmsgsinvite; charset=UTF-8\r\n\r\n"
				   "Invitation-Command: ACCEPT\r\n"
				   "Invitation-Cookie: %lu\r\n"
				   "Launch-Application: FALSE\r\n"
				   "Request-Data: IP-Address:\r\n",
				   (unsigned long)xfer_data->cookie);

		g_snprintf(buf, sizeof(buf), "MSG %u N %d\r\n%s\r\n\r\n",
				   ++ms->trId, strlen(header) + strlen("\r\n\r\n"),
				   header);

		if (msn_write(ms->fd, buf, strlen(buf)) < 0) {
			msn_kill_switch(ms);
			gaim_xfer_destroy(xfer);

			return;
		}
	}
}

static void
msn_xfer_start(struct gaim_xfer *xfer)
{
	struct msn_xfer_data *xfer_data;

	xfer_data = (struct msn_xfer_data *)xfer->data;

	xfer_data->transferring = TRUE;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		char sendbuf[MSN_BUF_LEN];

		/* Send the TFR string to request the start of a transfer. */
		g_snprintf(sendbuf, sizeof(sendbuf), "TFR\r\n");

		if (msn_write(xfer->fd, sendbuf, strlen(sendbuf)) < 0) {
			gaim_xfer_cancel_remote(xfer);
		}
	}
}

static void
msn_xfer_end(struct gaim_xfer *xfer)
{
	GaimAccount *account;
	struct msn_xfer_data *xfer_data;
	struct msn_data *md;

	account   = gaim_xfer_get_account(xfer);
	xfer_data = (struct msn_xfer_data *)xfer->data;
	md        = (struct msn_data *)account->gc->proto_data;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		char sendbuf[MSN_BUF_LEN];

		g_snprintf(sendbuf, sizeof(sendbuf), "BYE 16777989\r\n");

		msn_write(xfer->fd, sendbuf, strlen(sendbuf));

		md->file_transfers = g_slist_remove(md->file_transfers, xfer);

		g_free(xfer_data);
		xfer->data = NULL;
	}
}

static void
msn_xfer_cancel_send(struct gaim_xfer *xfer)
{
	GaimAccount *account;
	struct msn_xfer_data *xfer_data;
	struct msn_data *md;

	xfer_data = (struct msn_xfer_data *)xfer->data;

	xfer_data->do_cancel = TRUE;

	account   = gaim_xfer_get_account(xfer);
	md        = (struct msn_data *)account->gc->proto_data;

	md->file_transfers = g_slist_remove(md->file_transfers, xfer);

	g_free(xfer_data);
	xfer->data = NULL;
}

static void
msn_xfer_cancel_recv(struct gaim_xfer *xfer)
{
	GaimAccount *account;
	struct msn_xfer_data *xfer_data;
	struct msn_data *md;

	account   = gaim_xfer_get_account(xfer);
	xfer_data = (struct msn_xfer_data *)xfer->data;
	md        = (struct msn_data *)account->gc->proto_data;

	md->file_transfers = g_slist_remove(md->file_transfers, xfer);

	g_free(xfer_data);
	xfer->data = NULL;
}

static size_t
msn_xfer_read(char **buffer, struct gaim_xfer *xfer)
{
	struct msn_xfer_data *xfer_data;
	unsigned char header[3];
	size_t len, size;

	xfer_data = (struct msn_xfer_data *)xfer->data;

	if (xfer_data->do_cancel)
	{
		write(xfer->fd, "CCL\n", 4);

		return 0;
	}

	if (read(xfer->fd, header, sizeof(header)) < 3) {
		gaim_xfer_set_completed(xfer, TRUE);
		return 0;
	}

	if (header[0] != 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "MSNFTP: Invalid header[0]: %d. Aborting.\n",
				   header[0]);
		return 0;
	}

	size = header[1] | (header[2] << 8);

	*buffer = g_new0(char, size);

	for (len = 0;
		 len < size;
		 len += read(xfer->fd, *buffer + len, size - len))
		;

	if (len == 0)
		gaim_xfer_set_completed(xfer, TRUE);

	return len;
}

static size_t
msn_xfer_write(const char *buffer, size_t size, struct gaim_xfer *xfer)
{
	GaimAccount *account;
	struct msn_xfer_data *xfer_data;
	struct msn_data *md;
	unsigned char header[3];

	xfer_data = (struct msn_xfer_data *)xfer->data;
	account   = gaim_xfer_get_account(xfer);
	md        = (struct msn_data *)account->gc->proto_data;

	if (xfer_data->do_cancel)
	{
		header[0] = 1;
		header[1] = 0;
		header[2] = 0;

		if (write(xfer->fd, header, sizeof(header)) < 3) {
			gaim_xfer_cancel_remote(xfer);
			return 0;
		}
	}
	else
	{
		/* Not implemented yet. */
	}

	return 0;
}

static int
msn_process_msnftp(struct gaim_xfer *xfer, gint source, const char *buf)
{
	struct msn_xfer_data *xfer_data;
	GaimAccount *account;
	char sendbuf[MSN_BUF_LEN];

	xfer_data = (struct msn_xfer_data *)xfer->data;
	account = gaim_xfer_get_account(xfer);

	if (!g_ascii_strncasecmp(buf, "VER MSNFTP", 10)) {
		/* Send the USR string */
		g_snprintf(sendbuf, sizeof(sendbuf), "USR %s %lu\r\n",
				   account->gc->username,
				   (unsigned long)xfer_data->authcookie);

		if (msn_write(source, sendbuf, strlen(sendbuf)) < 0) {
			gaim_xfer_cancel_remote(xfer); /* ? */

			return 0;
		}
	}
	else if (!g_ascii_strncasecmp(buf, "FIL", 3)) {
		gaim_input_remove(xfer_data->inpa);
		xfer_data->inpa = 0;

		gaim_xfer_start(xfer, source, NULL, 0);
	}
#if 0
		char *tmp = buf;

		/*
		 * This data is the size, but we already have
		 * the size, so who cares.
		 */
		GET_NEXT(tmp);

		/* Send the TFR string to request the start of a transfer. */
		g_snprintf(sendbuf, sizeof(sendbuf), "TFR\r\n");


		if (msn_write(source, sendbuf, strlen(sendbuf)) < 0) {
			gaim_xfer_cancel(xfer);

			return 0;
		}
#endif

	return 1;
}

static void
msn_msnftp_cb(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_xfer *xfer;
	struct msn_xfer_data *xfer_data;
	char buf[MSN_BUF_LEN];
	gboolean cont = TRUE;
	size_t len;

	xfer = (struct gaim_xfer *)data;
	xfer_data = (struct msn_xfer_data *)xfer->data;

	len = read(source, buf, sizeof(buf));

	if (len <= 0) {
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	xfer_data->rxqueue = g_realloc(xfer_data->rxqueue,
								   len + xfer_data->rxlen);
	memcpy(xfer_data->rxqueue + xfer_data->rxlen, buf, len);
	xfer_data->rxlen += len;

	while (cont) {
		char *end = xfer_data->rxqueue;
		char *cmd;
		int cmdlen;
		int i = 0;

		if (!xfer_data->rxlen)
			return;

		for (i = 0; i < xfer_data->rxlen - 1; end++, i++) {
			if (*end == '\r' && *(end + 1) == '\n')
				break;
		}

		if (i == xfer_data->rxlen - 1)
			return;

		cmdlen = end - xfer_data->rxqueue + 2;
		cmd = xfer_data->rxqueue;

		xfer_data->rxlen -= cmdlen;

		if (xfer_data->rxlen)
			xfer_data->rxqueue = g_memdup(cmd + cmdlen, xfer_data->rxlen);
		else {
			xfer_data->rxqueue = NULL;
			cmd = g_realloc(cmd, cmdlen + 1);
		}

		cmd[cmdlen] = '\0';

		g_strchomp(cmd);

		cont = msn_process_msnftp(xfer, source, cmd);

		g_free(cmd);
	}
}

static void
msn_msnftp_connect(gpointer data, gint source, GaimInputCondition cond)
{
	GaimAccount *account;
	struct gaim_xfer *xfer;
	struct msn_xfer_data *xfer_data;
	char buf[MSN_BUF_LEN];

	xfer      = (struct gaim_xfer *)data;
	account   = gaim_xfer_get_account(xfer);
	xfer_data = (struct msn_xfer_data *)xfer->data;

	if (source == -1 || !g_slist_find(connections, account->gc)) {
		gaim_debug(GAIM_DEBUG_ERROR, "msn",
				   "MSNFTP: Error establishing connection\n");
		close(source);

		gaim_xfer_cancel_remote(xfer);

		return;
	}

	g_snprintf(buf, sizeof(buf), "VER MSNFTP\r\n");

	if (msn_write(source, buf, strlen(buf)) < 0) {
		gaim_xfer_cancel_remote(xfer);
		return;
	}

	xfer_data->inpa = gaim_input_add(source, GAIM_INPUT_READ,
									 msn_msnftp_cb, xfer);
}

void
msn_process_ft_msg(struct msn_switchboard *ms, char *msg)
{
	struct gaim_xfer *xfer;
	struct msn_xfer_data *xfer_data;
	struct msn_data *md = ms->gc->proto_data;
	char *tmp = msg;

	if (strstr(msg, "Application-GUID: " MSN_FT_GUID) &&
		strstr(msg, "Invitation-Command: INVITE")) {

		/*
		 * First invitation message, requesting an ACCEPT or CANCEL from
		 * the recipient. Used in incoming file transfers.
		 */

		char *filename;
		char *cookie_s, *filesize_s;

		tmp = strstr(msg, "Invitation-Cookie");
		GET_NEXT(tmp);
		cookie_s = tmp;
		GET_NEXT(tmp);
		GET_NEXT(tmp);
		filename = tmp;

		/* Needed for filenames with spaces */
		tmp = strchr(tmp, '\r');
		*tmp = '\0';
		tmp += 2;

		GET_NEXT(tmp);
		filesize_s = tmp;
		GET_NEXT(tmp);

		/* Setup the MSN-specific file transfer data */
		xfer_data = g_new0(struct msn_xfer_data, 1);
		xfer_data->cookie = atoi(cookie_s);
		xfer_data->transferring = FALSE;

		/* Build the file transfer handle. */
		xfer = gaim_xfer_new(ms->gc->account, GAIM_XFER_RECEIVE, ms->msguser);
		xfer->data = xfer_data;

		/* Set the info about the incoming file. */
		gaim_xfer_set_filename(xfer, filename);
		gaim_xfer_set_size(xfer, atoi(filesize_s));

		/* Setup our I/O op functions */
		gaim_xfer_set_init_fnc(xfer,        msn_xfer_init);
		gaim_xfer_set_start_fnc(xfer,       msn_xfer_start);
		gaim_xfer_set_end_fnc(xfer,         msn_xfer_end);
		gaim_xfer_set_cancel_send_fnc(xfer, msn_xfer_cancel_send);
		gaim_xfer_set_cancel_recv_fnc(xfer, msn_xfer_cancel_recv);
		gaim_xfer_set_read_fnc(xfer,        msn_xfer_read);
		gaim_xfer_set_write_fnc(xfer,       msn_xfer_write);

		/* Keep track of this transfer for later. */
		md->file_transfers = g_slist_append(md->file_transfers, xfer);

		/* Now perform the request */
		gaim_xfer_request(xfer);
	}
	else if (strstr(msg, "Invitation-Command: ACCEPT")) {

		/*
		 * XXX I hope these checks don't return false positives, but they
		 *     seem like they should work. The only issue is alternative
		 *     protocols, *maybe*.
		 */

		if (strstr(msg, "AuthCookie:")) {

			/*
			 * Second invitation request, sent after the recipient accepts
			 * the request. Used in incoming file transfers.
			 */
			char *cookie_s, *ip, *port_s, *authcookie_s;
			char ip_s[16];

			tmp = strstr(msg, "Invitation-Cookie");
			GET_NEXT(tmp);
			cookie_s = tmp;
			GET_NEXT(tmp);
			GET_NEXT(tmp);
			ip = tmp;
			GET_NEXT(tmp);
			GET_NEXT(tmp);
			port_s = tmp;
			GET_NEXT(tmp);
			GET_NEXT(tmp);
			authcookie_s = tmp;
			GET_NEXT(tmp);

			xfer = find_xfer_by_cookie(ms->gc, atoi(cookie_s));

			if (xfer == NULL)
			{
				gaim_debug(GAIM_DEBUG_ERROR, "msn",
						   "MSNFTP : Cookie not found. "
						   "File transfer aborted.\n");
				return;
			}

			xfer_data = (struct msn_xfer_data *)xfer->data;
			xfer_data->authcookie = atol(authcookie_s);

			strncpy(ip_s, ip, sizeof(ip_s));

			if (proxy_connect(xfer->account, ip_s, atoi(port_s),
							  msn_msnftp_connect, xfer) != 0) {

				gaim_xfer_cancel_remote(xfer);

				return;
			}
		}
		else
		{
			/*
			 * An accept message from the recipient. Used in outgoing
			 * file transfers.
			 */
		}
	}
}

