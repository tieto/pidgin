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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include "gaim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

gchar *strip_html(gchar *text)
{
	int i, j, k;
	int visible = 1;
	gchar *text2 = g_strdup(text);

	for (i = 0, j = 0; text2[i]; i++) {
		if (text2[i] == '<') {
			k = i + 1;
			while (text2[k]) {
				if (text2[k] == '<') {
					visible = 1;
					break;
				}
				if (text2[k] == '>') {
					visible = 0;
					break;
				}
				k++;
			}
		} else if (text2[i] == '>' && !visible) {
			visible = 1;
			continue;
		}
		if (visible) {
			text2[j++] = text2[i];
		}
	}
	text2[j] = '\0';
	return text2;
}

struct g_url *parse_url(char *url)
{
	struct g_url *test = g_new0(struct g_url, 1);
	char scan_info[255];
	char port[5];
	int f;

	if (strstr(url, "http://"))
		g_snprintf(scan_info, sizeof(scan_info),
			   "http://%%[A-Za-z0-9.]:%%[0-9]/%%[A-Za-z0-9.~_-/&%%?=+]");
	else
		g_snprintf(scan_info, sizeof(scan_info),
			   "%%[A-Za-z0-9.]:%%[0-9]/%%[A-Za-z0-9.~_-/&%%?=+^]");
	f = sscanf(url, scan_info, test->address, port, test->page);
	if (f == 1) {
		if (strstr(url, "http://"))
			g_snprintf(scan_info, sizeof(scan_info),
				   "http://%%[A-Za-z0-9.]/%%[A-Za-z0-9.~_-/&%%?=+^]");
		else
			g_snprintf(scan_info, sizeof(scan_info),
				   "%%[A-Za-z0-9.]/%%[A-Za-z0-9.~_-/&%%?=+^]");
		f = sscanf(url, scan_info, test->address, test->page);
		g_snprintf(port, sizeof(test->port), "80");
		port[2] = 0;
	}
	if (f == 1) {
		if (strstr(url, "http://"))
			g_snprintf(scan_info, sizeof(scan_info), "http://%%[A-Za-z0-9.]");
		else
			g_snprintf(scan_info, sizeof(scan_info), "%%[A-Za-z0-9.]");
		f = sscanf(url, scan_info, test->address);
		g_snprintf(test->page, sizeof(test->page), "%c", '\0');
	}

	sscanf(port, "%d", &test->port);
	return test;
}

struct grab_url_data {
	void (* callback)(gpointer, char *, unsigned long);
	gpointer data;
	struct g_url *website;
	char *url;
	gboolean full;

	int inpa;

	gboolean sentreq;
	gboolean newline;
	gboolean startsaving;
	char *webdata;
	unsigned long len;
};

static void grab_url_callback(gpointer dat, gint sock, GaimInputCondition cond)
{
	struct grab_url_data *gunk = dat;
	char data;

	if (sock == -1) {
		gunk->callback(gunk->data, NULL, 0);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
		return;
	}

	if (!gunk->sentreq) {
		char buf[256];

		g_snprintf(buf, sizeof(buf), "GET %s%s HTTP/1.0\r\n\r\n", gunk->full ? "" : "/",
			   gunk->full ? gunk->url : gunk->website->page);
		debug_printf("Request: %s\n", buf);

		write(sock, buf, strlen(buf));
		fcntl(sock, F_SETFL, O_NONBLOCK);
		gunk->sentreq = TRUE;
		gunk->inpa = gaim_input_add(sock, GAIM_INPUT_READ, grab_url_callback, dat);
		return;
	}

	if (read(sock, &data, 1) > 0 || errno == EWOULDBLOCK) {
		if (errno == EWOULDBLOCK) {
			errno = 0;
			return;
		}
		if (!gunk->startsaving) {
			if (data == '\r')
				return;
			if (data == '\n') {
				if (gunk->newline)
					gunk->startsaving = TRUE;
				else
					gunk->newline = TRUE;
				return;
			}
			gunk->newline = FALSE;
		} else {
			gunk->len++;
			gunk->webdata = g_realloc(gunk->webdata, gunk->len);
			gunk->webdata[gunk->len - 1] = data;
		}
	} else if (errno != ETIMEDOUT) {
		gunk->webdata = g_realloc(gunk->webdata, gunk->len + 1);
		gunk->webdata[gunk->len] = 0;

		debug_printf(_("Received: '%s'\n"), gunk->webdata);

		gaim_input_remove(gunk->inpa);
		close(sock);
		gunk->callback(gunk->data, gunk->webdata, gunk->len);
		if (gunk->webdata)
			g_free(gunk->webdata);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
	} else {
		gaim_input_remove(gunk->inpa);
		close(sock);
		gunk->callback(gunk->data, NULL, 0);
		if (gunk->webdata)
			g_free(gunk->webdata);
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
	}
}

void grab_url(char *url, gboolean full, void callback(gpointer, char *, unsigned long), gpointer data)
{
	int sock;
	struct grab_url_data *gunk = g_new0(struct grab_url_data, 1);

	gunk->callback = callback;
	gunk->data = data;
	gunk->url = g_strdup(url);
	gunk->website = parse_url(url);
	gunk->full = full;

	if ((sock = proxy_connect(gunk->website->address, gunk->website->port,
				  grab_url_callback, gunk)) < 0) {
		g_free(gunk->website);
		g_free(gunk->url);
		g_free(gunk);
		callback(data, g_strdup(_("g003: Error opening connection.\n")), 0);
	}
}
