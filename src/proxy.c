/*
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

/* this is a little piece of code to handle proxy connection */
/* it is intended to : 1st handle http proxy, using the CONNECT command
 , 2nd provide an easy way to add socks support */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#else
#include <winsock.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include "gaim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

#define GAIM_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

char proxyhost[128] = { 0 };
int proxyport = 0;
int proxytype = 0;
char proxyuser[128] = { 0 };
char proxypass[128] = { 0 };

struct PHB {
	GaimInputFunction func;
	gpointer data;
	char *host;
	int port;
	gint inpa;
};

typedef struct _GaimIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;
} GaimIOClosure;

static void gaim_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean gaim_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

	/*
	debug_printf("CLOSURE: callback for %d, fd is %d\n",
		     closure->result, g_io_channel_unix_get_fd(source));
	*/

	closure->function(closure->data, g_io_channel_unix_get_fd(source), gaim_cond);

	return TRUE;
}

gint gaim_input_add(gint source, GaimInputCondition condition, GaimInputFunction function, gpointer data)
{
	GaimIOClosure *closure = g_new0(GaimIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_WRITE_COND;

	channel = g_io_channel_unix_new(source);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      gaim_io_invoke, closure, gaim_io_destroy);

	/* debug_printf("CLOSURE: adding input watcher %d for fd %d\n", closure->result, source); */

	g_io_channel_unref(channel);
	return closure->result;
}

void gaim_input_remove(gint tag)
{
	/* debug_printf("CLOSURE: removing input watcher %d\n", tag); */
	if (tag > 0)
		g_source_remove(tag);
}

static struct sockaddr_in *gaim_gethostbyname(char *host, int port)
{
	static struct sockaddr_in sin;

	if (!inet_aton(host, &sin.sin_addr)) {
		struct hostent *hp;
		if(!(hp = gethostbyname(host))) {
			debug_printf("gaim_gethostbyname(\"%s\", %d) failed: %s",
				     host, port, hstrerror(h_errno));
			return NULL;
		}
		memset(&sin, 0, sizeof(struct sockaddr_in));
		memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
		sin.sin_family = hp->h_addrtype;
	} else
		sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	return &sin;
}

static void no_one_calls(gpointer data, gint source, GaimInputCondition cond)
{
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");

	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		gaim_input_remove(phb->inpa);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);
	gaim_input_remove(phb->inpa);
	phb->func(phb->data, source, GAIM_INPUT_READ);
	g_free(phb);
}

static gboolean clean_connect(gpointer data)
{
	struct PHB *phb = data;

	phb->func(phb->data, phb->port, GAIM_INPUT_READ);
	g_free(phb);

	return FALSE;
}


static int proxy_connect_none(char *host, unsigned short port, struct PHB *phb)
{
	struct sockaddr_in *sin;
	int fd = -1;

	debug_printf("connecting to %s:%d with no proxy\n", host, port);

	if (!(sin = gaim_gethostbyname(host, port))) {
		debug_printf("gethostbyname failed\n");
		g_free(phb);
		return -1;
	}

	if ((fd = socket(sin->sin_family, SOCK_STREAM, 0)) < 0) {
		debug_printf("unable to create socket\n");
		g_free(phb);
		return -1;
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, no_one_calls, phb);
		} else {
			debug_printf("connect failed (errno %d)\n", errno);
			close(fd);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;
		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			debug_printf("getsockopt failed\n");
			close(fd);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		phb->port = fd;	/* bleh */
		g_timeout_add(50, clean_connect, phb);	/* we do this because we never
							   want to call our callback
							   before we return. */
	}

	return fd;
}

#define HTTP_GOODSTRING "HTTP/1.0 200"
#define HTTP_GOODSTRING2 "HTTP/1.1 200"

static void http_canread(gpointer data, gint source, GaimInputCondition cond)
{
	int nlc = 0;
	int pos = 0;
	struct PHB *phb = data;
	char inputline[8192];

	gaim_input_remove(phb->inpa);

	while ((nlc != 2) && (read(source, &inputline[pos++], 1) == 1)) {
		if (inputline[pos - 1] == '\n')
			nlc++;
		else if (inputline[pos - 1] != '\r')
			nlc = 0;
	}
	inputline[pos] = '\0';

	debug_printf("Proxy says: %s\n", inputline);

	if ((memcmp(HTTP_GOODSTRING, inputline, strlen(HTTP_GOODSTRING)) == 0) ||
	    (memcmp(HTTP_GOODSTRING2, inputline, strlen(HTTP_GOODSTRING2)) == 0)) {
		phb->func(phb->data, source, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	close(source);
	phb->func(phb->data, -1, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
	return;
}

static void http_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	char cmd[384];
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");
	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);
	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);
	g_snprintf(cmd, sizeof(cmd), "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n", phb->host, phb->port,
		   phb->host, phb->port);
	if (send(source, cmd, strlen(cmd), 0) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if (proxyuser) {
		char *t1, *t2;
		t1 = g_strdup_printf("%s:%s", proxyuser, proxypass);
		t2 = tobase64(t1);
		g_free(t1);
		g_snprintf(cmd, sizeof(cmd), "Proxy-Authorization: Basic %s\r\n", t2);
		g_free(t2);
		if (send(source, cmd, strlen(cmd), 0) < 0) {
			close(source);
			phb->func(phb->data, -1, GAIM_INPUT_READ);
			g_free(phb->host);
			g_free(phb);
			return;
		}
	}

	g_snprintf(cmd, sizeof(cmd), "\r\n");
	if (send(source, cmd, strlen(cmd), 0) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, http_canread, phb);
}

static int proxy_connect_http(char *host, unsigned short port, struct PHB *phb)
{
	struct sockaddr_in *sin;
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using HTTP\n", host, port, proxyhost, proxyport);

	if (!(sin = gaim_gethostbyname(proxyhost, proxyport))) {
		g_free(phb);
		return -1;
	}

	if ((fd = socket(sin->sin_family, SOCK_STREAM, 0)) < 0) {
		g_free(phb);
		return -1;
	}

	phb->host = g_strdup(host);
	phb->port = port;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	if (connect(fd, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, http_canwrite, phb);
		} else {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		http_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void s4_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[12];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);

	memset(packet, 0, sizeof(packet));

	if (read(source, packet, 9) >= 4 && packet[1] == 90) {
		phb->func(phb->data, source, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	close(source);
	phb->func(phb->data, -1, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
}

static void s4_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char packet[12];
	struct hostent *hp;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");
	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);
	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	/* XXX does socks4 not support host name lookups by the proxy? */
	if (!(hp = gethostbyname(phb->host))) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	packet[0] = 4;
	packet[1] = 1;
	packet[2] = phb->port >> 8;
	packet[3] = phb->port & 0xff;
	packet[4] = (unsigned char)(hp->h_addr_list[0])[0];
	packet[5] = (unsigned char)(hp->h_addr_list[0])[1];
	packet[6] = (unsigned char)(hp->h_addr_list[0])[2];
	packet[7] = (unsigned char)(hp->h_addr_list[0])[3];
	packet[8] = 0;

	if (write(source, packet, 9) != 9) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s4_canread, phb);
}

static int proxy_connect_socks4(char *host, unsigned short port, struct PHB *phb)
{
	struct sockaddr_in *sin;
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using SOCKS4\n", host, port, proxyhost, proxyport);

	if (!(sin = gaim_gethostbyname(proxyhost, proxyport))) {
		g_free(phb);
		return -1;
	}

	if ((fd = socket(sin->sin_family, SOCK_STREAM, 0)) < 0) {
		g_free(phb);
		return -1;
	}

	phb->host = g_strdup(host);
	phb->port = port;

	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, s4_canwrite, phb);
		} else {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		s4_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

static void s5_canread_again(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	debug_printf("able to read again\n");

	if (read(source, buf, 10) < 10) {
		debug_printf("or not...\n");
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		debug_printf("bad data\n");
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->func(phb->data, source, GAIM_INPUT_READ);
	g_free(phb->host);
	g_free(phb);
	return;
}

static void s5_sendconnect(gpointer data, gint source)
{
	unsigned char buf[512];
	struct PHB *phb = data;
	int hlen = strlen(phb->host);

	buf[0] = 0x05;
	buf[1] = 0x01;		/* CONNECT */
	buf[2] = 0x00;		/* reserved */
	buf[3] = 0x03;		/* address type -- host name */
	buf[4] = hlen;
	memcpy(buf + 5, phb->host, hlen);
	buf[5 + strlen(phb->host)] = phb->port >> 8;
	buf[5 + strlen(phb->host) + 1] = phb->port & 0xff;

	if (write(source, buf, (5 + strlen(phb->host) + 2)) < (5 + strlen(phb->host) + 2)) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_canread_again, phb);
}

static void s5_readauth(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	debug_printf("got auth response\n");

	if (read(source, buf, 2) < 2) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x01) || (buf[1] != 0x00)) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	s5_sendconnect(phb, source);
}

static void s5_canread(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	struct PHB *phb = data;

	gaim_input_remove(phb->inpa);
	debug_printf("able to read\n");

	if (read(source, buf, 2) < 2) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	if (buf[1] == 0x02) {
		unsigned int i = strlen(proxyuser), j = strlen(proxypass);
		buf[0] = 0x01;	/* version 1 */
		buf[1] = i;
		memcpy(buf + 2, proxyuser, i);
		buf[2 + i] = j;
		memcpy(buf + 2 + i + 1, proxypass, j);

		if (write(source, buf, 3 + i + j) < 3 + i + j) {
			close(source);
			phb->func(phb->data, -1, GAIM_INPUT_READ);
			g_free(phb->host);
			g_free(phb);
			return;
		}

		phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_readauth, phb);
	} else {
		s5_sendconnect(phb, source);
	}
}

static void s5_canwrite(gpointer data, gint source, GaimInputCondition cond)
{
	unsigned char buf[512];
	int i;
	struct PHB *phb = data;
	unsigned int len;
	int error = ETIMEDOUT;

	debug_printf("Connected\n");
	if (phb->inpa > 0)
		gaim_input_remove(phb->inpa);
	len = sizeof(error);
	if (getsockopt(source, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}
	fcntl(source, F_SETFL, 0);

	i = 0;
	buf[0] = 0x05;		/* SOCKS version 5 */
	if (proxyuser[0]) {
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
		debug_printf("unable to write\n");
		close(source);
		phb->func(phb->data, -1, GAIM_INPUT_READ);
		g_free(phb->host);
		g_free(phb);
		return;
	}

	phb->inpa = gaim_input_add(source, GAIM_INPUT_READ, s5_canread, phb);
}

static int proxy_connect_socks5(char *host, unsigned short port, struct PHB *phb)
{
	struct sockaddr_in *sin;
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using SOCKS5\n", host, port, proxyhost, proxyport);

	if (!(sin = gaim_gethostbyname(proxyhost, proxyport))) {
		g_free(phb);
		return -1;
	}

	if ((fd = socket(sin->sin_family, SOCK_STREAM, 0)) < 0) {
		g_free(phb);
		return -1;
	}

	phb->host = g_strdup(host);
	phb->port = port;

	fcntl(fd, F_SETFL, O_NONBLOCK);
	if (connect(fd, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
		if ((errno == EINPROGRESS) || (errno == EINTR)) {
			debug_printf("Connect would have blocked\n");
			phb->inpa = gaim_input_add(fd, GAIM_INPUT_WRITE, s5_canwrite, phb);
		} else {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
	} else {
		unsigned int len;
		int error = ETIMEDOUT;

		debug_printf("Connect didn't block\n");
		len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
			close(fd);
			g_free(phb->host);
			g_free(phb);
			return -1;
		}
		fcntl(fd, F_SETFL, 0);
		s5_canwrite(phb, fd, GAIM_INPUT_WRITE);
	}

	return fd;
}

int proxy_connect(char *host, int port, GaimInputFunction func, gpointer data)
{
	struct PHB *phb = g_new0(struct PHB, 1);
	phb->func = func;
	phb->data = data;

	if (!host || !port || (port == -1) || !func) {
		g_free(phb);
		return -1;
	}
#ifndef _WIN32
	sethostent(1);
#endif
	if ((proxytype == PROXY_NONE) || !proxyhost || !proxyhost[0] || !proxyport || (proxyport == -1))
		return proxy_connect_none(host, port, phb);
	else if (proxytype == PROXY_HTTP)
		return proxy_connect_http(host, port, phb);
	else if (proxytype == PROXY_SOCKS4)
		return proxy_connect_socks4(host, port, phb);
	else if (proxytype == PROXY_SOCKS5)
		return proxy_connect_socks5(host, port, phb);

	g_free(phb);
	return -1;
}

