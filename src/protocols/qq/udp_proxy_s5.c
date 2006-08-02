/**
 * The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
 *                    Henry Ou  <henry@linux.net>
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

#include "debug.h"

#include "udp_proxy_s5.h"

extern gint			/* defined in qq_proxy.c */
 _qq_fill_host(struct sockaddr_in *addr, const gchar * host, guint16 port);

static void _qq_s5_canread_again(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;
	struct sockaddr_in sin;
	int len;

	gaim_input_remove(phb->inpa);
	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Able to read again.\n");

	len = read(source, buf, 10);
	if (len < 10) {
		gaim_debug(GAIM_DEBUG_WARNING, "socks5 proxy", "or not...\n");
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, source, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}
	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		if ((buf[0] == 0x05) && (buf[1] < 0x09))
			gaim_debug(GAIM_DEBUG_ERROR, "socks5 proxy", "socks5 error: %x\n", buf[1]);
		else
			gaim_debug(GAIM_DEBUG_ERROR, "socks5 proxy", "Bad data.\n");
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr.s_addr, buf + 4, 4);
	memcpy(&sin.sin_port, buf + 8, 2);

	if (connect(phb->udpsock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) < 0) {
		gaim_debug(GAIM_DEBUG_INFO, "s5_canread_again", "connect failed: %s\n", strerror(errno));
		close(phb->udpsock);
		close(source);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	int error = ETIMEDOUT;
	gaim_debug(GAIM_DEBUG_INFO, "QQ", "Connect didn't block\n");
	len = sizeof(error);
	if (getsockopt(phb->udpsock, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		gaim_debug(GAIM_DEBUG_ERROR, "QQ", "getsockopt failed.\n");
		close(phb->udpsock);
		return;
	}
	fcntl(phb->udpsock, F_SETFL, 0);

	if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {
		phb->func(phb->data, phb->udpsock, GAIM_INPUT_READ);
	}

	g_free(phb->host);
	g_free(phb);
}

static void _qq_s5_sendconnect(gpointer data, gint source)
{
	unsigned char buf[512];
	struct PHB *phb = data;
	struct sockaddr_in sin, ctlsin;
	int port, ctllen;

	gaim_debug(GAIM_DEBUG_INFO, "s5_sendconnect", "remote host is %s:%d\n", phb->host, phb->port);

	buf[0] = 0x05;
	buf[1] = 0x03;		/* udp relay */
	buf[2] = 0x00;		/* reserved */
	buf[3] = 0x01;		/* address type -- ipv4 */
	memset(buf + 4, 0, 0x04);

	ctllen = sizeof(ctlsin);
	if (getsockname(source, (struct sockaddr *) &ctlsin, &ctllen) < 0) {
		gaim_debug(GAIM_DEBUG_INFO, "QQ", "getsockname: %s\n", strerror(errno));
		close(source);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->udpsock = socket(PF_INET, SOCK_DGRAM, 0);
	gaim_debug(GAIM_DEBUG_INFO, "s5_sendconnect", "UDP socket=%d\n", phb->udpsock);
	if (phb->udpsock < 0) {
		close(source);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	fcntl(phb->udpsock, F_SETFL, O_NONBLOCK);

	port = ntohs(ctlsin.sin_port) + 1;
	while (1) {
		_qq_fill_host(&sin, "0.0.0.0", port);
		if (bind(phb->udpsock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
			port++;
			if (port > 65500) {
				close(source);
				g_free(phb->host);
				g_free(phb);
				return;
			}
		} else
			break;
	}

	memset(buf + 4, 0, 0x04);
	memcpy(buf + 8, &(sin.sin_port), 0x02);

	if (write(source, buf, 10) < 10) {
		close(source);
		gaim_debug(GAIM_DEBUG_INFO, "s5_sendconnect", "packet too small\n");

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {
			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, _qq_s5_canread_again, phb);
}

static void _qq_s5_readauth(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Got auth response.\n");

	if (read(source, buf, 2) < 2) {
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x01) || (buf[1] != 0x00)) {
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	_qq_s5_sendconnect(phb, source);
}

static void _qq_s5_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Able to read.\n");

	int ret = read(source, buf, 2);
	if (ret < 2) {
		gaim_debug(GAIM_DEBUG_INFO, "s5_canread", "packet smaller than 2 octet\n");
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, source, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
		gaim_debug(GAIM_DEBUG_INFO, "s5_canread", "unsupport\n");
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	if (buf[1] == 0x02) {
		unsigned int i, j;

		i = strlen(gaim_proxy_info_get_username(phb->gpi));
		j = strlen(gaim_proxy_info_get_password(phb->gpi));

		buf[0] = 0x01;	/* version 1 */
		buf[1] = i;
		memcpy(buf + 2, gaim_proxy_info_get_username(phb->gpi), i);
		buf[2 + i] = j;
		memcpy(buf + 2 + i + 1, gaim_proxy_info_get_password(phb->gpi), j);

		if (write(source, buf, 3 + i + j) < 3 + i + j) {
			close(source);

			if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

				phb->func(phb->data, -1, GAIM_INPUT_READ);
			}

			g_free(phb->host);
			g_free(phb);
			return;
		}

		phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, _qq_s5_readauth, phb);
	} else {
		gaim_debug(GAIM_DEBUG_INFO, "s5_canread", "calling s5_sendconnect\n");
		_qq_s5_sendconnect(phb, source);
	}
}

void _qq_s5_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	int i;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	gaim_debug(GAIM_DEBUG_INFO, "socks5 proxy", "Connected.\n");

	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);

	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		gaim_debug(GAIM_DEBUG_INFO, "getsockopt", "%s\n", strerror(errno));
		close(source);
		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */

	if (gaim_proxy_info_get_username(phb->gpi) != NULL) {
		buf[1] = 0x02;	/* two methods */
		buf[2] = 0x00;	/* no authentication */
		buf[3] = 0x02;	/* username/password authentication */
		i = 4;
	} else {
		buf[1] = 0x01;
		buf[2] = 0x00;
		i = 3;
	}

	if (write(source, buf, i) < i) {
		gaim_debug(GAIM_DEBUG_INFO, "write", "%s\n", strerror(errno));
		gaim_debug(GAIM_DEBUG_ERROR, "socks5 proxy", "Unable to write\n");
		close(source);

		if (phb->account == NULL || gaim_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, GAIM_INPUT_READ);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, _qq_s5_canread, phb);
}

gint qq_proxy_socks5(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	gint fd;
	gaim_debug(GAIM_DEBUG_INFO, "QQ",
		   "Connecting to %s:%d via %s:%d using SOCKS5\n",
		   phb->host, phb->port, gaim_proxy_info_get_host(phb->gpi), gaim_proxy_info_get_port(phb->gpi));

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0)
		return -1;

	gaim_debug(GAIM_DEBUG_INFO, "QQ", "proxy_sock5 return fd=%d\n", fd);

	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			gaim_debug(GAIM_DEBUG_WARNING, "QQ", "Connect in asynchronous mode.\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, _qq_s5_canwrite, phb);
		} else {
			close(fd);
			return -1;
		}
	} else {
		gaim_debug(GAIM_DEBUG_MISC, "QQ", "Connect in blocking mode.\n");
		fcntl(fd, F_SETFL, 0);
		_qq_s5_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}
