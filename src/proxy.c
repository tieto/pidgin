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
#include "../config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "gaim.h"
#include "proxy.h"

/* this code is borrowed from cvs 1.10 */
static int proxy_recv_line (int sock, char **resultp)
{
    int c;
    char *result;
    size_t input_index = 0;
    size_t result_size = 80;

    result = g_malloc (result_size);

    while (1)
    {
	char ch;
	if (recv (sock, &ch, 1, 0) < 0)
	    fprintf (stderr, "recv() error from  proxy server\n");
	c = ch;

	if (c == EOF)
	{
	    g_free (result);

	    /* It's end of file.  */
	    fprintf(stderr, "end of file from  server\n");
	}

	if (c == '\012')
	    break;

	result[input_index++] = c;
	while (input_index + 1 >= result_size)
	{
	    result_size *= 2;
	    result = (char *) g_realloc (result, result_size);
	}
    }

    if (resultp)
	*resultp = result;

    /* Terminate it just for kicks, but we *can* deal with embedded NULs.  */
    result[input_index] = '\0';

    if (resultp == NULL)
	g_free (result);
    return input_index;
}

static int proxy_connect_none(char *host, unsigned short port)
{
	struct sockaddr_in sin;
	struct hostent *hp;
	int fd = -1;

	debug_printf("connecting to %s:%d with no proxy\n", host, port);

	if (!(hp = gethostbyname(host))) 
		return -1;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
	sin.sin_family = hp->h_addrtype;
	sin.sin_port = htons(port);

	if ((fd = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
		return -1;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

#define HTTP_GOODSTRING "HTTP/1.0 200 Connection established"
#define HTTP_GOODSTRING2 "HTTP/1.1 200 Connection established"

static int proxy_connect_http(char *host, unsigned short port,
                              char *proxyhost, unsigned short proxyport)
{
	struct hostent *hp;
	struct sockaddr_in sin;
	int fd = -1;
	char cmd[384];
	char *inputline;

	debug_printf("connecting to %s:%d via %s:%d using HTTP\n", host, port, proxyhost, proxyport);

	if (!(hp = gethostbyname(proxyhost))) 
		return -1;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
	sin.sin_family = hp->h_addrtype;
	sin.sin_port = htons(proxyport);

	if ((fd = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
		return -1;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		close(fd);
		return -1;
	}

	snprintf(cmd, sizeof(cmd), "CONNECT %s:%d HTTP/1.1\n\r\n\r", host, port);

	if (send(fd, cmd, strlen(cmd),0) < 0)
		return -1;
	if (proxy_recv_line(fd, &inputline) < 0)
		return -1;

	if ((memcmp(HTTP_GOODSTRING, inputline, strlen(HTTP_GOODSTRING) == 0) ||
			(memcmp(HTTP_GOODSTRING2, inputline, strlen(HTTP_GOODSTRING2) == 0)))) {
		while (strlen(inputline) > 1) {
			free(inputline);
			if (proxy_recv_line(fd, &inputline) < 0)
				return -1;
		}
		free(inputline);
		return fd;
	}

	free(inputline);

	close(fd);

	return -1;
}

static int proxy_connect_socks4(char *host, unsigned short port,
                                char *proxyhost, unsigned short proxyport)
{
	struct sockaddr_in sin;
	unsigned char packet[12];
	struct hostent *hp;
	int fd = -1;

	debug_printf("connecting to %s:%d via %s:%d using SOCKS4\n", host, port, proxyhost, proxyport);

	if (!(hp = gethostbyname(proxyhost))) 
		return -1;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
	sin.sin_family = hp->h_addrtype;
	sin.sin_port = htons(proxyport);

	if ((fd = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
		return -1;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		close(fd);
		return -1;
	}

	/* XXX does socks4 not support host name lookups by the proxy? */
	if (!(hp = gethostbyname(host)))
		return -1;

	packet[0] = 4;
	packet[1] = 1;
	packet[2] = (((unsigned short) htons(port)) >> 8);
	packet[3] = (((unsigned short) htons(port)) & 0xff);
	packet[4] = (unsigned char) (hp->h_addr_list[0])[0];
	packet[5] = (unsigned char) (hp->h_addr_list[0])[1];
	packet[6] = (unsigned char) (hp->h_addr_list[0])[2];
	packet[7] = (unsigned char) (hp->h_addr_list[0])[3];
	packet[8] = 0;
	if (write(fd, packet, 9) == 9) {
		memset(packet, 0, sizeof(packet));
		if (read(fd, packet, 9) >= 4 && packet[1] == 90)
			return fd;
	}
	close(fd);

	return -1;
}

static int proxy_connect_socks5(char *host, unsigned short port,
                                char *proxyhost, unsigned short proxyport)
{
	int i, fd = -1;
	unsigned char buf[512];
	struct sockaddr_in sin;
	struct hostent *hp;

	debug_printf("connecting to %s:%d via %s:%d using SOCKS5\n", host, port, proxyhost, proxyport);

	if (!(hp = gethostbyname(proxyhost))) 
		return -1;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	memcpy(&sin.sin_addr.s_addr, hp->h_addr, hp->h_length);
	sin.sin_family = hp->h_addrtype;
	sin.sin_port = htons(proxyport);

	if ((fd = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
		return -1;

	if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		close(fd);
		return -1;
	}

	i = 0;
	buf[0] = 0x05; /* SOCKS version 5 */
	buf[1] = 0x01;
	buf[2] = 0x00;
	i = 3;

	if (write(fd, buf, i) < i) {
		close(fd);
		return -1;
	}

	if (read(fd, buf, 2) < 2) {
		close(fd);
		return -1;
	}

	if ((buf[0] != 0x05) || (buf[1] == 0xff)) {
		close(fd);
		return -1;
	}

	buf[0] = 0x05;
	buf[1] = 0x01; /* CONNECT */
	buf[2] = 0x00; /* reserved */
	buf[3] = 0x03; /* address type -- host name */
	buf[4] = strlen(host);
	memcpy(buf+5, host, strlen(host));
	buf[5+strlen(host)] = htons(port) >> 8;
	buf[5+strlen(host)+1] = htons(port) & 0xff;

	if (write(fd, buf, (5+strlen(host)+2)) < (5+strlen(host)+2)) {
		close(fd);
		return -1;
	}
	if (read(fd, buf, 10) < 10) {
		close(fd);
		return -1;
	}
	if ((buf[0] != 0x05) || (buf[1] != 0x00)) {
		close(fd);
		return -1;
	}

	return fd;
}

int proxy_connect(char *host, int port, char *proxyhost, int proxyport, int proxytype)
{
	if (!host || !port || (port == -1))
		return -1;
	else if ((proxytype == PROXY_NONE) || 
			!proxyhost || !proxyhost[0] ||
			!proxyport || (proxyport == -1))
		return proxy_connect_none(host, port);
	else if (proxytype == PROXY_HTTP)
		return proxy_connect_http(host, port, proxyhost, proxyport);
	else if (proxytype == PROXY_SOCKS4)
		return proxy_connect_socks4(host, port, proxyhost, proxyport);
	else if (proxytype == PROXY_SOCKS5)
		return proxy_connect_socks5(host, port, proxyhost, proxyport);
	return -1;
}
