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
#include "../config.h"
#endif
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/wait.h>
#include <gtk/gtk.h>
#include <ctype.h>
#include <math.h>
#include <pixmaps/aimicon.xpm>
#include "gaim.h"
#include "prpl.h"

static GdkPixmap *icon_pm = NULL;
static GdkBitmap *icon_bm = NULL;

char *full_date()
{
	char *date;
	time_t tme;

	time(&tme);
	date = ctime(&tme);
	date[strlen(date) - 1] = '\0';
	return date;
}

gint badchar(char c)
{
	switch (c) {
	case ' ':
	case ',':
	case '(':
	case ')':
	case '\0':
	case '\n':
	case '<':
	case '>':
	case '"':
		return 1;
	default:
		return 0;
	}
}


gchar *sec_to_text(guint sec)
{
	int daze, hrs, min;
	char *ret = g_malloc(256);

	daze = sec / (60 * 60 * 24);
	hrs = (sec % (60 * 60 * 24)) / (60 * 60);
	min = (sec % (60 * 60)) / 60;
	sec = min % 60;

	if (daze) {
		if (hrs || min) {
			if (hrs) {
				if (min) {
					g_snprintf(ret, 256,
							"%d day%s, %d hour%s, %d minute%s.",
							daze, daze == 1 ? "" : "s",
							hrs, hrs == 1 ? "" : "s",
							min, min == 1 ? "" : "s");
				} else {
					g_snprintf(ret, 256,
							"%d day%s, %d hour%s.",
							daze, daze == 1 ? "" : "s",
							hrs, hrs == 1 ? "" : "s");
				}
			} else {
				g_snprintf(ret, 256,
						"%d day%s, %d minute%s.",
						daze, daze == 1 ? "" : "s",
						min, min == 1 ? "" : "s");
			}
		} else
			g_snprintf(ret, 256, "%d day%s.", daze, daze == 1 ? "" : "s");
	} else {
		if (hrs) {
			if (min) {
				g_snprintf(ret, 256,
						"%d hour%s, %d minute%s.",
						hrs, hrs == 1 ? "" : "s",
						min, min == 1 ? "" : "s");
			} else {
				g_snprintf(ret, 256, "%d hour%s.", hrs, hrs == 1 ? "" : "s");
			}
		} else {
			g_snprintf(ret, 256, "%d minute%s.", min, min == 1 ? "" : "s");
		}
	}

	return ret;
}

gint linkify_text(char *text)
{
	char *c, *t;
	char *cpy = g_malloc(strlen(text) * 3 + 1);
	char url_buf[BUF_LEN * 4];
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */

	strncpy(cpy, text, strlen(text));
	cpy[strlen(text)] = 0;
	c = cpy;
	while (*c) {
		if (!g_strncasecmp(c, "<A", 2)) {
			while (1) {
				if (!strncasecmp(c, "/A>", 3)) {
					break;
				}
				text[cnt++] = *c;
				c++;
				if (!(*c))
					break;
			}
		} else if ((!g_strncasecmp(c, "http://", 7) || (!g_strncasecmp(c, "https://", 8)))) {
			t = c;
			while (1) {
				if (badchar(*t)) {

					if (*(t) == ',' && (*(t + 1) != ' ')) {
						t++;
						continue;
					}

					if (*(t - 1) == '.')
						t--;
					strncpy(url_buf, c, t - c);
					url_buf[t - c] = 0;
					cnt +=
					    g_snprintf(&text[cnt++], 1024, "<A HREF=\"%s\">%s</A>",
						       url_buf, url_buf);
					cnt--;
					c = t;
					break;
				}
				if (!t)
					break;
				t++;

			}
		} else if (!g_strncasecmp(c, "www.", 4)) {
			if (g_strncasecmp(c, "www..", 5)) {
				t = c;
				while (1) {
					if (badchar(*t)) {
						if (t - c == 4) {
							break;
						}

						if (*(t) == ',' && (*(t + 1) != ' ')) {
							t++;
							continue;
						}

						if (*(t - 1) == '.')
							t--;
						strncpy(url_buf, c, t - c);
						url_buf[t - c] = 0;
						cnt +=
						    g_snprintf(&text[cnt++], 1024,
							       "<A HREF=\"http://%s\">%s</A>", url_buf,
							       url_buf);
						cnt--;
						c = t;
						break;
					}
					if (!t)
						break;
					t++;
				}
			}
		} else if (!g_strncasecmp(c, "ftp://", 6)) {
			t = c;
			while (1) {
				if (badchar(*t)) {
					if (*(t - 1) == '.')
						t--;
					strncpy(url_buf, c, t - c);
					url_buf[t - c] = 0;
					cnt +=
					    g_snprintf(&text[cnt++], 1024, "<A HREF=\"%s\">%s</A>",
						       url_buf, url_buf);
					cnt--;
					c = t;
					break;
				}
				if (!t)
					break;
				t++;

			}
		} else if (!g_strncasecmp(c, "ftp.", 4)) {
			if (g_strncasecmp(c, "ftp..", 5)) {
				t = c;
				while (1) {
					if (badchar(*t)) {
						if (t - c == 4) {
							break;
						}
						if (*(t - 1) == '.')
							t--;
						strncpy(url_buf, c, t - c);
						url_buf[t - c] = 0;
						cnt +=
						    g_snprintf(&text[cnt++], 1024,
							       "<A HREF=\"ftp://%s\">%s</A>", url_buf,
							       url_buf);
						cnt--;
						c = t;
						break;
					}
					if (!t)
						break;
					t++;
				}
			}
		} else if (!g_strncasecmp(c, "mailto:", 7)) {
			t = c;
			while (1) {
				if (badchar(*t)) {
					if (*(t - 1) == '.')
						t--;
					strncpy(url_buf, c, t - c);
					url_buf[t - c] = 0;
					cnt +=
					    g_snprintf(&text[cnt++], 1024, "<A HREF=\"%s\">%s</A>",
						       url_buf, url_buf);
					cnt--;
					c = t;
					break;
				}
				if (!t)
					break;
				t++;

			}
		} else if (c != cpy && !g_strncasecmp(c, "@", 1)) {
			char *tmp;
			int flag;
			int len = 0;
			char illegal_chars[] = "!@#$%^&*()[]{}/\\<>\":;\0";
			url_buf[0] = 0;

			if (*(c - 1) == ' ' || *(c + 1) == ' ' || rindex(illegal_chars, *(c + 1))
			    || *(c + 1) == 13 || *(c + 1) == 10)
				flag = 0;
			else
				flag = 1;

			t = c;
			while (flag) {
				if (badchar(*t)) {
					cnt -= (len - 1);
					break;
				} else {
					len++;
					tmp = g_malloc(len + 1);
					tmp[len] = 0;
					tmp[0] = *t;
					strncpy(tmp + 1, url_buf, len - 1);
					strcpy(url_buf, tmp);
					url_buf[len] = 0;
					g_free(tmp);
					t--;
					if (t < cpy) {
						cnt = 0;
						break;
					}
				}
			}


			t = c + 1;

			while (flag) {
				if (badchar(*t)) {
					if (*(t - 1) == '.')
						t--;
					cnt +=
					    g_snprintf(&text[cnt++], 1024,
						       "<A HREF=\"mailto:%s\">%s</A>", url_buf, url_buf);
					text[cnt] = 0;


					cnt--;
					c = t;

					break;
				} else {
					strncat(url_buf, t, 1);
					len++;
					url_buf[len] = 0;
				}

				t++;

			}


		}

		if (*c == 0)
			break;

		text[cnt++] = *c;
		c++;

	}
	text[cnt] = 0;
	g_free(cpy);
	return cnt;
}


FILE *open_gaim_log_file(char *name, int *flag)
{
	char *buf;
	char *buf2;
	char log_all_file[256];
	struct stat st;
	FILE *fd;
	int res;
	gchar *gaim_dir;

	buf = g_malloc(BUF_LONG);
	buf2 = g_malloc(BUF_LONG);
	gaim_dir = gaim_user_dir();

	/*  Dont log yourself */
	strncpy(log_all_file, gaim_dir, 256);

	stat(log_all_file, &st);
	if (!S_ISDIR(st.st_mode))
		unlink(log_all_file);

	fd = fopen(log_all_file, "r");

	if (!fd) {
		res = mkdir(log_all_file, S_IRUSR | S_IWUSR | S_IXUSR);
		if (res < 0) {
			g_snprintf(buf, BUF_LONG, "Unable to make directory %s for logging",
				   log_all_file);
			do_error_dialog(buf, "Error!");
			g_free(buf);
			g_free(buf2);
			g_free(gaim_dir);
			return NULL;
		}
	} else
		fclose(fd);

	g_snprintf(log_all_file, 256, "%s/logs", gaim_dir);

	if (stat(log_all_file, &st) < 0)
		*flag = 1;
	if (!S_ISDIR(st.st_mode))
		unlink(log_all_file);

	fd = fopen(log_all_file, "r");
	if (!fd) {
		res = mkdir(log_all_file, S_IRUSR | S_IWUSR | S_IXUSR);
		if (res < 0) {
			g_snprintf(buf, BUF_LONG, "Unable to make directory %s for logging",
				   log_all_file);
			do_error_dialog(buf, "Error!");
			g_free(buf);
			g_free(buf2);
			g_free(gaim_dir);
			return NULL;
		}
	} else
		fclose(fd);



	g_snprintf(log_all_file, 256, "%s/logs/%s", gaim_dir, name);
	if (stat(log_all_file, &st) < 0)
		*flag = 1;

	debug_printf("Logging to: \"%s\"\n", log_all_file);

	fd = fopen(log_all_file, "a");

	g_free(buf);
	g_free(buf2);
	g_free(gaim_dir);
	return fd;
}

FILE *open_log_file(char *name)
{
	struct stat st;
	char realname[256];
	struct log_conversation *l;
	FILE *fd;
	int flag = 0;

	if (!(logging_options & OPT_LOG_ALL)) {

		l = find_log_info(name);
		if (!l)
			return NULL;

		if (stat(l->filename, &st) < 0)
			flag = 1;

		fd = fopen(l->filename, "a");

		if (flag) {	/* is a new file */
			if (logging_options & OPT_LOG_STRIP_HTML) {
			   fprintf(fd, "IM Sessions with %s\n", name);
			}else {
			   fprintf(fd, "<HTML><HEAD><TITLE>");
			   fprintf(fd, "IM Sessions with %s", name);
			   fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"ffffff\">\n");
			}
		}

		return fd;
	}

	g_snprintf(realname, sizeof(realname), "%s.log", normalize(name));
	fd = open_gaim_log_file(realname, &flag);

	if (fd && flag) {	/* is a new file */
		if (logging_options & OPT_LOG_STRIP_HTML) {
		   fprintf(fd, "IM Sessions with %s\n", name);
		}else {
		   fprintf(fd, "<HTML><HEAD><TITLE>");
		   fprintf(fd, "IM Sessions with %s", name);
		   fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"ffffff\">\n");
		}
	}

	return fd;
}

FILE *open_system_log_file(char *name)
{
	int x;

	if (name)
		return open_log_file(name);
	else
		return open_gaim_log_file("system", &x);
}

/* we only need this for TOC, because messages must be escaped */
int escape_message(char *msg)
{
	char *c, *cpy;
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */
	if (strlen(msg) > BUF_LEN) {
		debug_printf("Warning:  truncating message to 2048 bytes\n");
		msg[2047] = '\0';
	}

	cpy = g_strdup(msg);
	c = cpy;
	while (*c) {
		switch (*c) {
		case '$':
		case '[':
		case ']':
		case '(':
		case ')':
		case '#':
			msg[cnt++] = '\\';
			/* Fall through */
		default:
			msg[cnt++] = *c;
		}
		c++;
	}
	msg[cnt] = '\0';
	g_free(cpy);
	return cnt;
}

/* we don't need this for oscar either */
int escape_text(char *msg)
{
	char *c, *cpy;
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 4 bytes */
	if (strlen(msg) > BUF_LEN) {
		fprintf(stderr, "Warning:  truncating message to 2048 bytes\n");
		msg[2047] = '\0';
	}

	cpy = g_strdup(msg);
	c = cpy;
	while (*c) {
		switch (*c) {
		case '\n':
			msg[cnt++] = '<';
			msg[cnt++] = 'B';
			msg[cnt++] = 'R';
			msg[cnt++] = '>';
			break;
		case '{':
		case '}':
		case '\\':
		case '"':
			msg[cnt++] = '\\';
			/* Fall through */
		default:
			msg[cnt++] = *c;
		}
		c++;
	}
	msg[cnt] = '\0';
	g_free(cpy);
	return cnt;
}

char *escape_text2(const char *msg)
{
	char *c, *cpy;
	char *woo;
	int cnt = 0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */
	if (strlen(msg) > BUF_LEN) {
		fprintf(stderr, "Warning:  truncating message to 2048 bytes\n");
	}

	woo = malloc(strlen(msg) * 4 + 1);
	cpy = g_strndup(msg, 2048);
	c = cpy;
	while (*c) {
		switch (*c) {
		case '\n':
			woo[cnt++] = '<';
			woo[cnt++] = 'B';
			woo[cnt++] = 'R';
			woo[cnt++] = '>';
			break;
		case '{':
		case '}':
		case '\\':
		case '"':
			woo[cnt++] = '\\';
			/* Fall through */
		default:
			woo[cnt++] = *c;
		}
		c++;
	}
	woo[cnt] = '\0';

	g_free(cpy);
	return woo;
}


char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" "0123456789+/";


char *tobase64(const char *text)
{
	char *out = NULL;
	const char *c;
	unsigned int tmp = 0;
	int len = 0, n = 0;

	c = text;

	while (*c) {
		tmp = tmp << 8;
		tmp += *c;
		n++;

		if (n == 3) {
			out = g_realloc(out, len + 4);
			out[len] = alphabet[(tmp >> 18) & 0x3f];
			out[len + 1] = alphabet[(tmp >> 12) & 0x3f];
			out[len + 2] = alphabet[(tmp >> 6) & 0x3f];
			out[len + 3] = alphabet[tmp & 0x3f];
			len += 4;
			tmp = 0;
			n = 0;
		}
		c++;
	}
	switch (n) {

	case 2:
		tmp <<= 8;
		out = g_realloc(out, len + 5);
		out[len] = alphabet[(tmp >> 18) & 0x3f];
		out[len + 1] = alphabet[(tmp >> 12) & 0x3f];
		out[len + 2] = alphabet[(tmp >> 6) & 0x3f];
		out[len + 3] = '=';
		out[len + 4] = 0;
		break;
	case 1:
		tmp <<= 16;
		out = g_realloc(out, len + 5);
		out[len] = alphabet[(tmp >> 18) & 0x3f];
		out[len + 1] = alphabet[(tmp >> 12) & 0x3f];
		out[len + 2] = '=';
		out[len + 3] = '=';
		out[len + 4] = 0;
		break;
	case 0:
		out = g_realloc(out, len + 1);
		out[len] = 0;
		break;
	}
	return out;
}


void frombase64(const char *text, char **data, int *size)
{
	char *out = NULL;
	char tmp = 0;
	const char *c;
	gint32 tmp2 = 0;
	int len = 0, n = 0;

	if (!text || !data)
		return;

	c = text;

	while (*c) {
		if (*c >= 'A' && *c <= 'Z') {
			tmp = *c - 'A';
		} else if (*c >= 'a' && *c <= 'z') {
			tmp = 26 + (*c - 'a');
		} else if (*c >= '0' && *c <= 57) {
			tmp = 52 + (*c - '0');
		} else if (*c == '+') {
			tmp = 62;
		} else if (*c == '/') {
			tmp = 63;
		} else if (*c == '=') {
			if (n == 3) {
				out = g_realloc(out, len + 2);
				out[len] = (char)(tmp2 >> 10) & 0xff;
				len++;
				out[len] = (char)(tmp2 >> 2) & 0xff;
				len++;
			} else if (n == 2) {
				out = g_realloc(out, len + 1);
				out[len] = (char)(tmp2 >> 4) & 0xff;
				len++;
			}
			break;
		}
		tmp2 = ((tmp2 << 6) | (tmp & 0xff));
		n++;
		if (n == 4) {
			out = g_realloc(out, len + 3);
			out[len] = (char)((tmp2 >> 16) & 0xff);
			len++;
			out[len] = (char)((tmp2 >> 8) & 0xff);
			len++;
			out[len] = (char)(tmp2 & 0xff);
			len++;
			tmp2 = 0;
			n = 0;
		}
		c++;
	}

	out = g_realloc(out, len + 1);
	out[len] = 0;

	*data = out;
	if (size)
		*size = len;
}


char *normalize(const char *s)
{
	static char buf[BUF_LEN];
	char *t, *u;
	int x = 0;

	g_return_val_if_fail((s != NULL), NULL);

	u = t = g_strdup(s);

	strcpy(t, s);
	g_strdown(t);

	while (*t && (x < BUF_LEN - 1)) {
		if (*t != ' ') {
			buf[x] = *t;
			x++;
		}
		t++;
	}
	buf[x] = '\0';
	g_free(u);
	return buf;
}

char *date()
{
	static char date[80];
	time_t tme;
	time(&tme);
	strftime(date, sizeof(date), "%H:%M:%S", localtime(&tme));
	return date;
}


gint clean_pid(void *dummy)
{
	int status;
	pid_t pid;

	pid = waitpid(-1, &status, WNOHANG);

	if (pid == 0)
		return TRUE;

	return FALSE;
}

void aol_icon(GdkWindow *w)
{
#ifndef _WIN32
	if (icon_pm == NULL) {
		icon_pm = gdk_pixmap_create_from_xpm_d(w, &icon_bm, NULL, (gchar **)aimicon_xpm);
	}
	gdk_window_set_icon(w, NULL, icon_pm, icon_bm);
	if (mainwindow)
		gdk_window_set_group(w, mainwindow->window);
#endif
}

struct aim_user *find_user(const char *name, int protocol)
{
	char *who = g_strdup(normalize(name));
	GList *usr = aim_users;
	struct aim_user *u;

	while (usr) {
		u = (struct aim_user *)usr->data;
		if (!strcmp(normalize(u->username), who)) {
			if (protocol != -1) {
				if (u->protocol == protocol) {
					g_free(who);
					return u;
				}
			} else {
				g_free(who);
				return u;
			}

		}
		usr = usr->next;
	}
	g_free(who);
	return NULL;
}


/* Look for %n, %d, or %t in msg, and replace with the sender's name, date,
   or time */
char *away_subs(char *msg, char *name)
{
	char *c;
	static char cpy[BUF_LONG];
	int cnt = 0;
	time_t t = time(0);
	struct tm *tme = localtime(&t);
	char tmp[20];

	cpy[0] = '\0';
	c = msg;
	while (*c) {
		switch (*c) {
		case '%':
			if (*(c + 1)) {
				switch (*(c + 1)) {
				case 'n':
					/* append name */
					strcpy(cpy + cnt, name);
					cnt += strlen(name);
					c++;
					break;
				case 'd':
					/* append date */
					strftime(tmp, 20, "%D", tme);
					strcpy(cpy + cnt, tmp);
					cnt += strlen(tmp);
					c++;
					break;
				case 't':
					/* append time */
					strftime(tmp, 20, "%r", tme);
					strcpy(cpy + cnt, tmp);
					cnt += strlen(tmp);
					c++;
					break;
				default:
					cpy[cnt++] = *c;
				}
			}
			break;
		default:
			cpy[cnt++] = *c;
		}
		c++;
	}
	cpy[cnt] = '\0';
	return (cpy);
}

GtkWidget *picture_button(GtkWidget *window, char *text, char **xpm)
{
	GtkWidget *button;
	GtkWidget *button_box, *button_box_2, *button_box_3;
	GtkWidget *label;
	GdkBitmap *mask;
	GdkPixmap *pm;
	GtkWidget *pixmap;

	button = gtk_button_new();
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	button_box = gtk_hbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(button), button_box);

	button_box_2 = gtk_hbox_new(FALSE, 0);
	button_box_3 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), button_box_2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(button_box), button_box_3, TRUE, TRUE, 0);
	pm = gdk_pixmap_create_from_xpm_d(window->window, &mask, NULL, xpm);
	pixmap = gtk_pixmap_new(pm, mask);
	gtk_box_pack_end(GTK_BOX(button_box_2), pixmap, FALSE, FALSE, 0);

	if (text) {
		label = gtk_label_new(text);
		gtk_box_pack_start(GTK_BOX(button_box_3), label, FALSE, FALSE, 2);
		gtk_widget_show(label);
	}

	gtk_widget_show(pixmap);
	gtk_widget_show(button_box_2);
	gtk_widget_show(button_box_3);
	gtk_widget_show(button_box);

/* this causes clipping on lots of buttons with long text */
/*  gtk_widget_set_usize(button, 75, 30);*/
	gtk_widget_show(button);
	gdk_pixmap_unref(pm);
	gdk_bitmap_unref(mask);

	return button;
}

static GtkTooltips *button_tips = NULL;
GtkWidget *picture_button2(GtkWidget *window, char *text, char **xpm, short dispstyle)
{
	GtkWidget *button;
	GtkWidget *button_box, *button_box_2;
	GdkBitmap *mask;
	GdkPixmap *pm;
	GtkWidget *pixmap;
	GtkWidget *label;

	if (!button_tips)
		button_tips = gtk_tooltips_new();
	button = gtk_button_new();
	if (display_options & OPT_DISP_COOL_LOOK)
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	button_box = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(button), button_box);

	button_box_2 = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(button_box), button_box_2, TRUE, TRUE, 0);
	gtk_widget_show(button_box_2);
	gtk_widget_show(button_box);
	if (dispstyle == 2 || dispstyle == 0) {
		pm = gdk_pixmap_create_from_xpm_d(window->window, &mask, NULL, xpm);
		pixmap = gtk_pixmap_new(pm, mask);
		gtk_box_pack_start(GTK_BOX(button_box_2), pixmap, FALSE, FALSE, 0);

		gtk_widget_show(pixmap);

		gdk_pixmap_unref(pm);
		gdk_bitmap_unref(mask);
	}

	if (dispstyle == 2 || dispstyle == 1) {
		label = gtk_label_new(text);
		gtk_widget_show(label);
		gtk_box_pack_end(GTK_BOX(button_box_2), label, FALSE, FALSE, 0);
	}

	gtk_tooltips_set_tip(button_tips, button, text, "Gaim");
	gtk_widget_show(button);
	return button;
}


/* remove leading whitespace from a string */
char *remove_spaces(char *str)
{
	int i;
	char *new;

	if (str == NULL)
		return NULL;

	i = strspn(str, " \t\n\r\f");
	new = &str[i];

	return new;
}


/* translate an AIM 3 buddylist (*.lst) to a GAIM buddylist */
void translate_lst(FILE *src_fp, char *dest)
{
	char line[BUF_LEN], *line2;
	char *name;
	int i;

	sprintf(dest, "m 1\n");

	while (fgets(line, BUF_LEN, src_fp)) {
		line2 = remove_spaces(line);
		if (strstr(line2, "group") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			strcat(dest, "g ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					strncat(dest, &name[i], 1);
			strcat(dest, "\n");
		}
		if (strstr(line2, "buddy") == line2) {
			name = strpbrk(line2, " \t\n\r\f") + 1;
			strcat(dest, "b ");
			for (i = 0; i < strcspn(name, "\n\r"); i++)
				if (name[i] != '\"')
					strncat(dest, &name[i], 1);
			strcat(dest, "\n");
		}
	}

	return;
}


/* translate an AIM 4 buddylist (*.blt) to GAIM format */
void translate_blt(FILE *src_fp, char *dest)
{
	int i;
	char line[BUF_LEN];
	char *buddy;

	sprintf(dest, "m 1\n");

	while (strstr(fgets(line, BUF_LEN, src_fp), "Buddy") == NULL);
	while (strstr(fgets(line, BUF_LEN, src_fp), "list") == NULL);

	while (1) {
		fgets(line, BUF_LEN, src_fp);
		if (strchr(line, '}') != NULL)
			break;

		/* Syntax starting with "<group> {" */
		if (strchr(line, '{') != NULL) {
			strcat(dest, "g ");
			buddy = remove_spaces(strtok(line, "{"));
			for (i = 0; i < strlen(buddy); i++) {
				if (buddy[i] != '\"')
					strncat(dest, &buddy[i], 1);
			}
			strcat(dest, "\n");
			while (strchr(fgets(line, BUF_LEN, src_fp), '}') == NULL) {
				buddy = remove_spaces(line);
				strcat(dest, "b ");
				if (strchr(buddy, '\"') != NULL) {
					strncat(dest, &buddy[1], strlen(buddy) - 3);
					strcat(dest, "\n");
				} else
					strcat(dest, buddy);
			}
		}
		/* Syntax "group buddy buddy ..." */
		else {
			buddy = remove_spaces(strtok(line, " \n"));
			strcat(dest, "g ");
			if (strchr(buddy, '\"') != NULL) {
				strcat(dest, &buddy[1]);
				strcat(dest, " ");
				buddy = remove_spaces(strtok(NULL, " \n"));
				while (strchr(buddy, '\"') == NULL) {
					strcat(dest, buddy);
					strcat(dest, " ");
					buddy = remove_spaces(strtok(NULL, " \n"));
				}
				strncat(dest, buddy, strlen(buddy) - 1);
			} else {
				strcat(dest, buddy);
			}
			strcat(dest, "\n");
			while ((buddy = remove_spaces(strtok(NULL, " \n"))) != NULL) {
				strcat(dest, "b ");
				if (strchr(buddy, '\"') != NULL) {
					strcat(dest, &buddy[1]);
					strcat(dest, " ");
					buddy = remove_spaces(strtok(NULL, " \n"));
					while (strchr(buddy, '\"') == NULL) {
						strcat(dest, buddy);
						strcat(dest, " ");
						buddy = remove_spaces(strtok(NULL, " \n"));
					}
					strncat(dest, buddy, strlen(buddy) - 1);
				} else {
					strcat(dest, buddy);
				}
				strcat(dest, "\n");
			}
		}
	}

	return;
}

char *stylize(gchar *text, int length)
{
	gchar *buf;
	char *tmp = g_malloc(length);

	buf = g_malloc(length);
	g_snprintf(buf, length, "%s", text);

	if (font_options & OPT_FONT_BOLD) {
		g_snprintf(tmp, length, "<B>%s</B>", buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_ITALIC) {
		g_snprintf(tmp, length, "<I>%s</I>", buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_UNDERLINE) {
		g_snprintf(tmp, length, "<U>%s</U>", buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_STRIKE) {
		g_snprintf(tmp, length, "<S>%s</S>", buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_FACE) {
		g_snprintf(tmp, length, "<FONT FACE=\"%s\">%s</FONT>", fontface, buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_SIZE) {
		g_snprintf(tmp, length, "<FONT SIZE=\"%d\">%s</FONT>", fontsize, buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_FGCOL) {
		g_snprintf(tmp, length, "<FONT COLOR=\"#%02X%02X%02X\">%s</FONT>", fgcolor.red,
			   fgcolor.green, fgcolor.blue, buf);
		strcpy(buf, tmp);
	}

	if (font_options & OPT_FONT_BGCOL) {
		g_snprintf(tmp, length, "<BODY BGCOLOR=\"#%02X%02X%02X\">%s</BODY>", bgcolor.red,
			   bgcolor.green, bgcolor.blue, buf);
		strcpy(buf, tmp);
	}

	g_free(tmp);
	return buf;
}

int set_dispstyle(int chat)
{
	int dispstyle;

	if (chat) {
		switch (display_options & (OPT_DISP_CHAT_BUTTON_TEXT | OPT_DISP_CHAT_BUTTON_XPM)) {
		case OPT_DISP_CHAT_BUTTON_TEXT:
			dispstyle = 1;
			break;
		case OPT_DISP_CHAT_BUTTON_XPM:
			dispstyle = 0;
			break;
		default:	/* both or neither */
			dispstyle = 2;
			break;
		}
	} else {
		switch (display_options & (OPT_DISP_CONV_BUTTON_TEXT | OPT_DISP_CONV_BUTTON_XPM)) {
		case OPT_DISP_CONV_BUTTON_TEXT:
			dispstyle = 1;
			break;
		case OPT_DISP_CONV_BUTTON_XPM:
			dispstyle = 0;
			break;
		default:	/* both or neither */
			dispstyle = 2;
			break;
		}
	}
	return dispstyle;
}


void show_usage(int mode, char *name)
{
	switch (mode) {
	case 0:		/* full help text */
		printf("Usage: %s [OPTION]...\n\n"
		       "  -a, --acct          display account editor window\n"
		       "  -w, --away[=MESG]   make away on signon (optional argument MESG specifies\n"
		       "                      name of away message to use)\n"
		       "  -l, --login[=NAME]  automatically login (optional argument NAME specifies\n"
		       "                      account(s) to use)\n"
		       "  -u, --user=NAME     use account NAME\n"
		       "  -f, --file=FILE     use FILE as config\n"
		       "  -v, --version       display version information window\n"
		       "  -h, --help          display this help and exit\n", name);
		break;
	case 1:		/* short message */
		printf("Try `%s -h' for more information.\n", name);
		break;
	}
}


void set_first_user(char *name)
{
	struct aim_user *u;

	u = find_user(name, -1);

	if (!u) {		/* new user */
		u = g_new0(struct aim_user, 1);
		g_snprintf(u->username, sizeof(u->username), "%s", name);
		u->protocol = DEFAULT_PROTO;
		aim_users = g_list_prepend(aim_users, u);
	} else {		/* user already exists */
		aim_users = g_list_remove(aim_users, u);
		aim_users = g_list_prepend(aim_users, u);
	}
	save_prefs();
}


/* <name> is a comma-separated list of names, or NULL
   if NULL and there is at least one user defined in .gaimrc, try to login.
   if not NULL, parse <name> into separate strings, look up each one in 
   .gaimrc and, if it's there, try to login.
   returns:  0 if successful
            -1 if no user was found that had a saved password
*/
int do_auto_login(char *name)
{
	struct aim_user *u;
	char **names, **n;
	int retval = -1;

	if (name !=NULL) {	/* list of names given */
		names = g_strsplit(name, ",", 32);
		for (n = names; *n != NULL; n++) {
			u = find_user(*n, -1);
			if (u) {	/* found a user */
				if (u->options & OPT_USR_REM_PASS) {
					retval = 0;
					serv_login(u);
				}
			}
		}
		g_strfreev(names);
	} else {		/* no name given, use default */
		u = (struct aim_user *)aim_users->data;
		if (u->options & OPT_USR_REM_PASS) {
			retval = 0;
			serv_login(u);
		}
	}

	return retval;
}


int file_is_dir(const char *path, GtkWidget *w)
{
	struct stat st;
	char *name;

	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		/* append a / if needed */
		if (path[strlen(path) - 1] != '/')
			name = g_strconcat(path, "/", NULL);
		else
			name = g_strdup(path);
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(w), name);
		g_free(name);
		return 1;
	}

	return 0;
}

GSList *message_split(char *message, int limit)
{
	static GSList *ret = NULL;
	int lastgood = 0, curgood = 0, curpos = 0, len = strlen(message);
	gboolean intag = FALSE;

	if (ret) {
		GSList *tmp = ret;
		while (tmp) {
			g_free(tmp->data);
			tmp = g_slist_remove(tmp, tmp->data);
		}
		ret = NULL;
	}

	while (TRUE) {
		if (lastgood >= len)
			return ret;

		if (len - lastgood < limit) {
			ret = g_slist_append(ret, g_strdup(&message[lastgood]));
			return ret;
		}

		curgood = curpos = 0;
		intag = FALSE;
		while (curpos <= limit) {
			if (isspace(message[curpos + lastgood]) && !intag)
				curgood = curpos;
			if (message[curpos + lastgood] == '<')
				intag = TRUE;
			if (message[curpos + lastgood] == '>')
				intag = FALSE;
			curpos++;
		}

		if (curgood) {
			ret = g_slist_append(ret, g_strndup(&message[lastgood], curgood));
			if (isspace(message[curgood + lastgood]))
				lastgood += curgood + 1;
			else
				lastgood += curgood;
		} else {
			/* whoops, guess we have to fudge it here */
			ret = g_slist_append(ret, g_strndup(&message[lastgood], limit));
			lastgood += limit;
		}
	}
}

/* returns a string of the form ~/.gaim, where ~ is replaced by the user's home
 * dir. this string should be freed after it's used. Note that there is no
 * trailing slash after .gaim. */
gchar *gaim_user_dir()
{
	return g_strjoin(G_DIR_SEPARATOR_S, g_get_home_dir(), ".gaim", NULL);
}

/*
 * rcg10312000 This could be more robust, but it works for my current
 *  goal: to remove those annoying <BR> tags.  :)
 * dtf12162000 made the loop more readable. i am a neat freak. ;) */
void strncpy_nohtml(gchar *dest, const gchar *src, size_t destsize)
{
	gchar *ptr;
	g_snprintf(dest, destsize, "%s", src);

	while ((ptr = strstr(dest, "<BR>")) != NULL) {
		/* replace <BR> with a newline. */
		*ptr = '\n';
		memmove(ptr + 1, ptr + 4, strlen(ptr + 4) + 1);
	}
}

void strncpy_withhtml(gchar *dest, const gchar *src, size_t destsize)
{
	gchar *end = dest + destsize;

	while (dest < end) {
		if (*src == '\n' && dest < end - 5) {
			strcpy(dest, "<BR>");
			src++;
			dest += 4;
		} else {
			*dest++ = *src;
			if (*src == '\0') return;
			else src++;
		}
	}
}


void away_on_login (char *mesg)
{
	GSList *awy = away_messages;
	struct away_message *a, *message = NULL;
	
	if (!blist) {
		return;
	}

	if (mesg == NULL) {
		/* Use default message */
		do_away_message((GtkWidget*)NULL, default_away);
	} else {
		/* Use argument */
		while (awy) {
			a = (struct away_message *)awy->data;
			if (strcmp (a->name, mesg) == 0) {
				message = a;
				break;
			}
			awy = awy->next;
		}
		if (message == NULL)
			message = default_away;
		do_away_message((GtkWidget*)NULL, message);
	}
	return;
}

void system_log(enum log_event what, struct gaim_connection *gc, struct buddy *who, int why)
{
	FILE *fd;
	char text[256], html[256];

	if ((logging_options & why) != why)
		return;

	if (logging_options & OPT_LOG_INDIVIDUAL) {
		if (why & OPT_LOG_MY_SIGNON)
			fd = open_system_log_file(gc ? gc->username : NULL);
		else
			fd = open_system_log_file(who->name);
	} else
		fd = open_system_log_file(NULL);

	if (!fd)
		return;

	if (why & OPT_LOG_MY_SIGNON) {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), "+++ %s (%s) signed on @ %s",
					gc->username, (*gc->prpl->name)(), full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), "+++ %s (%s) signed off @ %s",
					gc->username, (*gc->prpl->name)(), full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), "+++ %s (%s) changed away state @ %s",
					gc->username, (*gc->prpl->name)(), full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), "+++ %s (%s) came back @ %s",
					gc->username, (*gc->prpl->name)(), full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), "+++ %s (%s) became idle @ %s",
					gc->username, (*gc->prpl->name)(), full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text), "+++ %s (%s) returned from idle @ %s",
					gc->username, (*gc->prpl->name)(), full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_quit:
			g_snprintf(text, sizeof(text), "+++ Program exit @ %s", full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		}
	} else if (strcmp(who->name, who->show)) {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s (%s) signed on @ %s",
				gc->username, (*gc->prpl->name)(), who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s (%s) signed off @ %s",
				gc->username, (*gc->prpl->name)(), who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s (%s) went away @ %s",
				gc->username, (*gc->prpl->name)(), who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s (%s) came back @ %s",
				gc->username, (*gc->prpl->name)(), who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s (%s) became idle @ %s",
				gc->username, (*gc->prpl->name)(), who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s (%s) returned from idle @ %s",
				gc->username, (*gc->prpl->name)(), who->show, who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		default:
			fclose(fd);
			return;
			break;
		}
	} else {
		switch (what) {
		case log_signon:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s signed on @ %s",
				gc->username, (*gc->prpl->name)(), who->name, full_date());
			g_snprintf(html, sizeof(html), "<B>%s</B>", text);
			break;
		case log_signoff:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s signed off @ %s",
				gc->username, (*gc->prpl->name)(), who->name, full_date());
			g_snprintf(html, sizeof(html), "<I><FONT COLOR=GRAY>%s</FONT></I>", text);
			break;
		case log_away:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s went away @ %s",
				gc->username, (*gc->prpl->name)(), who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=OLIVE>%s</FONT>", text);
			break;
		case log_back:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s came back @ %s",
				gc->username, (*gc->prpl->name)(), who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		case log_idle:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s became idle @ %s",
				gc->username, (*gc->prpl->name)(), who->name, full_date());
			g_snprintf(html, sizeof(html), "<FONT COLOR=GRAY>%s</FONT>", text);
			break;
		case log_unidle:
			g_snprintf(text, sizeof(text), "%s (%s) reported that %s returned from idle @ %s",
				gc->username, (*gc->prpl->name)(), who->name, full_date());
			g_snprintf(html, sizeof(html), "%s", text);
			break;
		default:
			fclose(fd);
			return;
			break;
		}
	}

	if (logging_options & OPT_LOG_STRIP_HTML) {
		fprintf(fd, "---- %s ----\n", text);
	} else {
		if (logging_options & OPT_LOG_INDIVIDUAL)
			fprintf(fd, "<HR>%s<BR><HR><BR>\n", html);
		else
			fprintf(fd, "%s<BR>\n", html);
	}

	fclose(fd);
}

unsigned char *utf8_to_str(unsigned char *in)
{
	int n = 0,i = 0;
	int inlen;
	unsigned char *result;

	if (!in)
		return NULL;

	inlen = strlen(in);

	result = g_malloc(inlen+1);

	while(n <= inlen-1) {
		long c = (long)in[n];
		if(c<0x80)
			result[i++] = (char)c;
		else {
			if((c&0xC0) == 0xC0)
				result[i++] = (char)(((c&0x03)<<6)|(((unsigned char)in[++n])&0x3F));
			else if((c&0xE0) == 0xE0) {
				if (n + 2 <= inlen) {
					result[i] = (char)(((c&0xF)<<4)|(((unsigned char)in[++n])&0x3F));
					result[i] = (char)(((unsigned char)result[i]) |(((unsigned char)in[++n])&0x3F));
					i++;
				} else n += 2;
			}
			else if((c&0xF0) == 0xF0)
				n += 3;
			else if((c&0xF8) == 0xF8)
				n += 4;
			else if((c&0xFC) == 0xFC)
				n += 5;
		}
		n++;
	}
	result[i] = '\0';

	return result;
}

char *str_to_utf8(unsigned char *in)
{
	int n = 0,i = 0;
	int inlen;
	char *result = NULL;

	if (!in)
		return NULL;

	inlen = strlen(in);

	result = g_malloc(inlen * 2 + 1);

	while (n < inlen) {
		long c = (long)in[n];
		if (c == 27) {
			n += 2;
			if (in[n] == 'x')
				n++;
			if (in[n] == '3')
				n++;
			n += 2;
			continue;
		}
		if ((c == 0x0D) || (c == 0x0A)) {
			n++; continue;
		}
		if (c < 128)
			result[i++] = (char)c;
		else {
			result[i++] = (char)((c>>6)|192);
			result[i++] = (char)((c&63)|128);
		}
		n++;
	}
	result[i] = '\0';

	return result;
}

time_t get_time(int year, int month, int day, int hour, int min, int sec)
{
	struct tm tm;

	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec >= 0 ? sec : time(NULL) % 60;
	return mktime(&tm);
}
