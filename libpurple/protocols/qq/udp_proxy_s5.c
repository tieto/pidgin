/**
 * @file udp_proxy_s5.c
 *
 * purple
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

#include "debug.h"

#include "udp_proxy_s5.h"

static void _qq_s5_canread_again(gpointer data, gint source, PurpleInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;
	struct sockaddr_in sin;
	int len, error;
	socklen_t errlen;
	int flags;

	purple_input_remove(phb->inpa);
	purple_debug(PURPLE_DEBUG_INFO, "socks5 proxy", "Able to read again.\n");

	len = read(source, buf, 10);
	if (len < 10) {
		purple_debug(PURPLE_DEBUG_WARNING, "socks5 proxy", "or not...\n");
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, source, NULL);
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}
	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		if ((buf[0] == 0x05) && (buf[1] < 0x09))
			purple_debug(PURPLE_DEBUG_ERROR, "socks5 proxy", "socks5 error: %x\n", buf[1]);
		else
			purple_debug(PURPLE_DEBUG_ERROR, "socks5 proxy", "Bad data.\n");
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	sin.sin_family = AF_INET;
	memcpy(&sin.sin_addr.s_addr, buf + 4, 4);
	memcpy(&sin.sin_port, buf + 8, 2);

	if (connect(phb->udpsock, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)) < 0) {
		purple_debug(PURPLE_DEBUG_INFO, "s5_canread_again", "connect failed: %s\n", g_strerror(errno));
		close(phb->udpsock);
		close(source);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	error = ETIMEDOUT;
	purple_debug(PURPLE_DEBUG_INFO, "QQ", "Connect didn't block\n");
	errlen = sizeof(error);
	if (getsockopt(phb->udpsock, SOL_SOCKET, SO_ERROR, &error, &errlen) < 0) {
		purple_debug(PURPLE_DEBUG_ERROR, "QQ", "getsockopt failed.\n");
		close(phb->udpsock);
		return;
	}
	flags = fcntl(phb->udpsock, F_GETFL);
	fcntl(phb->udpsock, F_SETFL, flags & ~O_NONBLOCK);

	if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {
		phb->func(phb->data, phb->udpsock, NULL);
	}

	g_free(phb->host);
	g_free(phb);
}

static void _qq_s5_sendconnect(gpointer data, gint source)
{
	unsigned char buf[512];
	struct PHB *phb = data;
	struct sockaddr_in sin, ctlsin;
	int port; 
	socklen_t ctllen;
	int flags;

	purple_debug(PURPLE_DEBUG_INFO, "s5_sendconnect", "remote host is %s:%d\n", phb->host, phb->port);

	buf[0] = 0x05;
	buf[1] = 0x03;		/* udp relay */
	buf[2] = 0x00;		/* reserved */
	buf[3] = 0x01;		/* address type -- ipv4 */
	memset(buf + 4, 0, 0x04);

	ctllen = sizeof(ctlsin);
	if (getsockname(source, (struct sockaddr *) &ctlsin, &ctllen) < 0) {
		purple_debug(PURPLE_DEBUG_INFO, "QQ", "getsockname: %s\n", g_strerror(errno));
		close(source);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->udpsock = socket(PF_INET, SOCK_DGRAM, 0);
	purple_debug(PURPLE_DEBUG_INFO, "s5_sendconnect", "UDP socket=%d\n", phb->udpsock);
	if (phb->udpsock < 0) {
		close(source);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	flags = fcntl(phb->udpsock, F_GETFL);
	fcntl(phb->udpsock, F_SETFL, flags | O_NONBLOCK);

	port = g_ntohs(ctlsin.sin_port) + 1;
	while (1) {
		inet_aton("0.0.0.0", &(sin.sin_addr));
		sin.sin_family = AF_INET;
		sin.sin_port = g_htons(port);
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
		purple_debug(PURPLE_DEBUG_INFO, "s5_sendconnect", "packet too small\n");

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {
			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = purple_input_add(source, PURPLE_INPUT_READ, _qq_s5_canread_again, phb);
}

static void _qq_s5_readauth(gpointer data, gint source, PurpleInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	purple_input_remove(phb->inpa);
	purple_debug(PURPLE_DEBUG_INFO, "socks5 proxy", "Got auth response.\n");

	if (read(source, buf, 2) < 2) {
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x01) || (buf[1] != 0x00)) {
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	_qq_s5_sendconnect(phb, source);
}

static void _qq_s5_canread(gpointer data, gint source, PurpleInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb;
	int ret;

	phb = data;

	purple_input_remove(phb->inpa);
	purple_debug(PURPLE_DEBUG_INFO, "socks5 proxy", "Able to read.\n");

	ret = read(source, buf, 2);
	if (ret < 2) {
		purple_debug(PURPLE_DEBUG_INFO, "s5_canread", "packet smaller than 2 octet\n");
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
		purple_debug(PURPLE_DEBUG_INFO, "s5_canread", "unsupport\n");
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	if (buf[1] == 0x02) {
		unsigned int i, j;

		i = strlen(purple_proxy_info_get_username(phb->gpi));
		j = strlen(purple_proxy_info_get_password(phb->gpi));

		buf[0] = 0x01;	/* version 1 */
		buf[1] = i;
		memcpy(buf + 2, purple_proxy_info_get_username(phb->gpi), i);
		buf[2 + i] = j;
		memcpy(buf + 2 + i + 1, purple_proxy_info_get_password(phb->gpi), j);

		if (write(source, buf, 3 + i + j) < 3 + i + j) {
			close(source);

			if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

				phb->func(phb->data, -1, _("Unable to connect"));
			}

			g_free(phb->host);
			g_free(phb);
			return;
		}

		phb->inpa = purple_input_add(source, PURPLE_INPUT_READ, _qq_s5_readauth, phb);
	} else {
		purple_debug(PURPLE_DEBUG_INFO, "s5_canread", "calling s5_sendconnect\n");
		_qq_s5_sendconnect(phb, source);
	}
}

static void _qq_s5_canwrite(gpointer data, gint source, PurpleInputCondition cond)
{
	unsigned char buf[512];
	int i;
	struct PHB *phb = data;
	socklen_t len;
	int error = ETIMEDOUT;
	int flags;

	purple_debug(PURPLE_DEBUG_INFO, "socks5 proxy", "Connected.\n");

	if (phb->inpa > 0)
		purple_input_remove(phb->inpa);

	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		purple_debug(PURPLE_DEBUG_INFO, "getsockopt", "%s\n", g_strerror(errno));
		close(source);
		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}
	flags = fcntl(source, F_GETFL);
	fcntl(source, F_SETFL, flags & ~O_NONBLOCK);

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */

	if (purple_proxy_info_get_username(phb->gpi) != NULL) {
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
		purple_debug(PURPLE_DEBUG_INFO, "write", "%s\n", g_strerror(errno));
		purple_debug(PURPLE_DEBUG_ERROR, "socks5 proxy", "Unable to write\n");
		close(source);

		if (phb->account == NULL || purple_account_get_connection(phb->account) != NULL) {

			phb->func(phb->data, -1, _("Unable to connect"));
		}

		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = purple_input_add(source, PURPLE_INPUT_READ, _qq_s5_canread, phb);
}

gint qq_proxy_socks5(struct PHB *phb, struct sockaddr *addr, socklen_t addrlen)
{
	gint fd;
	int flags;

	purple_debug(PURPLE_DEBUG_INFO, "QQ",
		   "Connecting to %s:%d via %s:%d using SOCKS5\n",
		   phb->host, phb->port, purple_proxy_info_get_host(phb->gpi), purple_proxy_info_get_port(phb->gpi));

	if ((fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0)
		return -1;

	purple_debug(PURPLE_DEBUG_INFO, "QQ", "proxy_sock5 return fd=%d\n", fd);

	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (connect(fd, addr, addrlen) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			purple_debug(PURPLE_DEBUG_WARNING, "QQ", "Connect in asynchronous mode.\n");
			phb->inpa = purple_input_add(fd, PURPLE_INPUT_WRITE, _qq_s5_canwrite, phb);
		} else {
			close(fd);
			return -1;
		}
	} else {
		purple_debug(PURPLE_DEBUG_MISC, "QQ", "Connect in blocking mode.\n");
		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		_qq_s5_canwrite(phb, fd, PURPLE_INPUT_WRITE);
	}

	return fd;
}
