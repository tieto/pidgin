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
#include <sys/time.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include "gaim.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include "proxy.h"

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

struct g_url parse_url(char *url)
{
	struct g_url test;
	char scan_info[255];
	char port[5];
	int f;

	if (strstr(url, "http://"))
		g_snprintf(scan_info, sizeof(scan_info),
			   "http://%%[A-Za-z0-9.]:%%[0-9]/%%[A-Za-z0-9.~_-/&%%?=+]");
	else
		g_snprintf(scan_info, sizeof(scan_info),
			   "%%[A-Za-z0-9.]:%%[0-9]/%%[A-Za-z0-9.~_-/&%%?=+^]");
	f = sscanf(url, scan_info, test.address, port, test.page);
	if (f == 1) {
		if (strstr(url, "http://"))
			g_snprintf(scan_info, sizeof(scan_info),
				   "http://%%[A-Za-z0-9.]/%%[A-Za-z0-9.~_-/&%%?=+^]");
		else
			g_snprintf(scan_info, sizeof(scan_info),
				   "%%[A-Za-z0-9.]/%%[A-Za-z0-9.~_-/&%%?=+^]");
		f = sscanf(url, scan_info, test.address, test.page);
		g_snprintf(port, sizeof(test.port), "80");
		port[2] = 0;
	}
	if (f == 1) {
		if (strstr(url, "http://"))
			g_snprintf(scan_info, sizeof(scan_info), "http://%%[A-Za-z0-9.]");
		else
			g_snprintf(scan_info, sizeof(scan_info), "%%[A-Za-z0-9.]");
		f = sscanf(url, scan_info, test.address);
		g_snprintf(test.page, sizeof(test.page), "%c", '\0');
	}

	sscanf(port, "%d", &test.port);
	return test;
}

struct grab_url_data {
	void (*callback)(gpointer, char *);
	gpointer data;
	struct g_url website;
	char *url;
};

static void grab_url_callback(gpointer dat, gint sock, GdkInputCondition cond)
{
	struct grab_url_data *gunk = dat;
	char *webdata = NULL;
	int len;
	int read_rv;
	int datalen = 0;
	char buf[256];
	char data;
	int startsaving = 0;
	GtkWidget *pw = NULL, *pbar = NULL, *label;

	if (sock == -1) {
		gunk->callback(gunk->data, NULL);
		g_free(gunk->url);
		g_free(gunk);
		return;
	}

	g_snprintf(buf, sizeof(buf), "GET /%s HTTP/1.0\r\n\r\n", gunk->website.page);
	debug_printf("Request: %s\n", buf);
	write(sock, buf, strlen(buf));
	fcntl(sock, F_SETFL, O_NONBLOCK);

	webdata = NULL;
	len = 0;

	/*
	 * avoid fgetc(), it causes problems on solaris
	 while ((data = fgetc(sockfile)) != EOF) {
	 */
	/* read_rv will be 0 on EOF and < 0 on error, so this should be fine */
	while ((read_rv = read(sock, &data, 1)) > 0 || errno == EWOULDBLOCK) {
		if (errno == EWOULDBLOCK) {
			errno = 0;
			continue;
		}

		if (!data)
			continue;

		if (!startsaving && data == '<') {
#ifdef HAVE_STRSTR
			char *cs = strstr(webdata, "Content-Length");
			if (cs) {
				char tmpbuf[1024];
				sscanf(cs, "Content-Length: %d", &datalen);

				g_snprintf(tmpbuf, 1024, _("Getting %d bytes from %s"),
						datalen, gunk->url);
				pw = gtk_dialog_new();

				label = gtk_label_new(tmpbuf);
				gtk_widget_show(label);
				gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pw)->vbox),
						   label, FALSE, FALSE, 5);

				pbar = gtk_progress_bar_new();
				gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pw)->action_area),
						   pbar, FALSE, FALSE, 5);
				gtk_widget_show(pbar);

				gtk_window_set_title(GTK_WINDOW(pw), _("Getting Data"));

				gtk_widget_realize(pw);
				aol_icon(pw->window);

				gtk_widget_show(pw);
			} else
				datalen = 0;
#else
			datalen = 0;
#endif
			g_free(webdata);
			webdata = NULL;
			len = 0;
			startsaving = 1;
		}

		len++;
		webdata = g_realloc(webdata, len);
		webdata[len - 1] = data;

		if (pbar)
			gtk_progress_bar_update(GTK_PROGRESS_BAR(pbar), ((100 * len) / datalen) / 100.0);

		while (gtk_events_pending())
			gtk_main_iteration();
	}

	webdata = g_realloc(webdata, len + 1);
	webdata[len] = 0;


	debug_printf(_("Receieved: '%s'\n"), webdata);

	if (pw)
		gtk_widget_destroy(pw);

	close(sock);
	gunk->callback(gunk->data, webdata);
	g_free(gunk->url);
	g_free(gunk);
}

void grab_url(char *url, void (*callback)(gpointer, char *), gpointer data)
{
	int sock;
	struct grab_url_data *gunk = g_new0(struct grab_url_data, 1);

	gunk->callback = callback;
	gunk->data = data;
	gunk->url = g_strdup(url);
	gunk->website = parse_url(url);

	if ((sock = proxy_connect(gunk->website.address, gunk->website.port,
					grab_url_callback, gunk)) < 0) {
		g_free(gunk->url);
		g_free(gunk);
		callback(data, g_strdup(_("g003: Error opening connection.\n")));
	}
}
