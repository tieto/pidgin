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
#include <pixmaps/aimicon.xpm>
#include "gaim.h"

static GdkPixmap *icon_pm = NULL;
static GdkBitmap *icon_bm = NULL;
static int state;

gint badchar(char c)
{
        if (c == ' ')
                return 1;
        if (c == ',')
                return 1;
        if (c == ')')
                return 1;
        if (c == '(')
                return 1;
        if (c == 0)
                return 1;
        if (c == '\n')
                return 1;
        return 0;


}


char *sec_to_text(int sec)
{
        int hrs, min;
        char minutes[64];
        char hours[64];
        char sep[16];
        char *ret = g_malloc(256);
        
        hrs = sec / 3600;
        min = sec % 3600;

        min = min / 60;
        sec = min % 60;

        if (min) {
                if (min == 1)
                        g_snprintf(minutes, sizeof(minutes), "%d minute.", min);
                else
                        g_snprintf(minutes, sizeof(minutes), "%d minutes.", min);
                sprintf(sep, ", ");
        } else {
                if (!hrs)
                        g_snprintf(minutes, sizeof(minutes), "%d minutes.", min);
                else {
                        minutes[0] = '.';
                        minutes[1] = '\0';
                }
                sep[0] = '\0';
        }

        if (hrs) {
                if (hrs == 1)
                        g_snprintf(hours, sizeof(hours), "%d hour%s", hrs, sep);
                else
                        g_snprintf(hours, sizeof(hours), "%d hours%s", hrs, sep);
        } else
                hours[0] = '\0';

        
        g_snprintf(ret, 256, "%s%s", hours, minutes);

        return ret;
}

gint linkify_text(char *text)
{
        char *c, *t;
        char cpy[BUF_LONG];
        char url_buf[512];
        int cnt=0;
        /* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */

        strncpy(cpy, text, strlen(text));
        cpy[strlen(text)] = 0;
        c = cpy;
        while(*c) {
                if (!strncasecmp(c, "<A", 2)) {
                        while(1) {
                                if (!strncasecmp(c, "/A>", 3)) {
                                        break;
                                }
                                text[cnt++] = *c;
                                c++;
                                if (!(*c))
                                        break;
                        }
                } else if (!strncasecmp(c, "http://", 7)) {
                        t = c;
                        while(1) {
                                if (badchar(*t)) {
                                        if (*(t-1) == '.')
                                                t--;
                                        strncpy(url_buf, c, t-c);
                                        url_buf[t-c] = 0;
                                        cnt += g_snprintf(&text[cnt++], 1024, "<A HREF=\"%s\">%s</A>", url_buf, url_buf);
                                        cnt--;
                                        c = t;
                                        break;
                                }
                                if (!t)
                                        break;
                                t++;

                        }
                } else if (!strncasecmp(c, "www.", 4)) {
                        if (strncasecmp(c, "www..", 5)) {
                        t = c;
                        while(1) {
                                if (badchar(*t)) {
                                        if (t-c == 4) {
                                                break;
                                        }
                                        if (*(t-1) == '.')
                                                t--;
                                        strncpy(url_buf, c, t-c);
                                        url_buf[t-c] = 0;
                                        cnt += g_snprintf(&text[cnt++], 1024, "<A HREF=\"http://%s\">%s</A>", url_buf, url_buf);
                                        cnt--;
                                        c = t;
                                        break;
                                }
                                if (!t)
                                        break;
                                t++;
                        }
                 }
                } else if (!strncasecmp(c, "ftp://", 6)) {
                        t = c;
                        while(1) {
                                if (badchar(*t)) {
                                        if (*(t-1) == '.')
                                                t--;
                                        strncpy(url_buf, c, t-c);
                                        url_buf[t-c] = 0;
                                        cnt += g_snprintf(&text[cnt++], 1024, "<A HREF=\"%s\">%s</A>", url_buf, url_buf);
                                        cnt--;
                                        c = t;
                                        break;
                                }
                                if (!t)
                                        break;
                                t++;

                        }
                } else if (!strncasecmp(c, "ftp.", 4)) {
                        t = c;
                        while(1) {
                                if (badchar(*t)) {
                                        if (t-c == 4) {
                                                break;
                                        }
                                        if (*(t-1) == '.')
                                                t--;
                                        strncpy(url_buf, c, t-c);
                                        url_buf[t-c] = 0;
                                        cnt += g_snprintf(&text[cnt++], 1024, "<A HREF=\"ftp://%s\">%s</A>", url_buf, url_buf);
                                        cnt--; 
                                        c = t;
                                        break;
                                }
                                if (!t)
                                        break;
                                t++;
                        }
                } else if (!strncasecmp(c, "mailto:", 7)) {
                        t = c;
                        while(1) {
                                if (badchar(*t)) {
                                        if (*(t-1) == '.')
                                                t--;
                                        strncpy(url_buf, c, t-c);
                                        url_buf[t-c] = 0;
                                        cnt += g_snprintf(&text[cnt++], 1024, "<A HREF=\"%s\">%s</A>", url_buf, url_buf);
                                        cnt--;
                                        c = t;
                                        break;
                                }
                                if (!t)
                                        break;
                                t++;

                        }
                } else if (!strncasecmp(c, "@", 1)) {
                        char *tmp;
                        int flag;
                        int len=0;
                        url_buf[0] = 0;

                        if (*(c-1) == ' ' || *(c+1) == ' ')
                                flag = 0;
                        else
                                flag = 1;

                        t = c;
                        while(flag) {
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

                        while(flag) {
                                if (badchar(*t)) {
                                        if (*(t-1) == '.')
                                                t--;
                                        cnt += g_snprintf(&text[cnt++], 1024, "<A HREF=\"mailto:%s\">%s</A>", url_buf, url_buf);
                                        text[cnt]=0;


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
        text[cnt]=0;
        return cnt; 
}


FILE *open_log_file (struct conversation *c)
{
        char *buf = g_malloc(BUF_LONG);
        char *buf2 = g_malloc(BUF_LONG);
        char log_all_file[256];
        struct log_conversation *l;
        struct stat st;
        int flag = 0;
        FILE *fd;
        int res;

        if (!(general_options & OPT_GEN_LOG_ALL)) {

                l = find_log_info(c->name);
                if (!l)
                        return NULL;

                if (stat(l->filename, &st) < 0)
                        flag = 1;

                fd = fopen(l->filename, "a");

                if (flag) { /* is a new file */
                        fprintf(fd, "<HTML><HEAD><TITLE>" );
                        fprintf(fd, "IM Sessions with %s", c->name );
                        fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"ffffff\">\n" );
                }

                return fd;
        }

        /*  Dont log yourself */
        g_snprintf(log_all_file, 256, "%s/.gaim", getenv("HOME"));

        stat(log_all_file, &st);
        if (!S_ISDIR(st.st_mode))
                unlink(log_all_file);

        fd = fopen(log_all_file, "r");

        if (!fd) {
                res = mkdir(log_all_file, S_IRUSR | S_IWUSR | S_IXUSR);
                if (res < 0) {
                        g_snprintf(buf, BUF_LONG, "Unable to make directory %s for logging", log_all_file);
                        do_error_dialog(buf, "Error!");
                        g_free(buf);
                        g_free(buf2);
                        return NULL;
                }
        } else
                fclose(fd);

        g_snprintf(log_all_file, 256, "%s/.gaim/%s", getenv("HOME"), current_user->username);

        if (stat(log_all_file, &st) < 0)
                flag = 1;
        if (!S_ISDIR(st.st_mode))
                unlink(log_all_file);

        fd = fopen(log_all_file, "r");
        if (!fd) {
                res = mkdir(log_all_file, S_IRUSR | S_IWUSR | S_IXUSR);
                if (res < 0) {
                        g_snprintf(buf, BUF_LONG, "Unable to make directory %s for logging", log_all_file);
                        do_error_dialog(buf, "Error!");
                        g_free(buf);
                        g_free(buf2);
                        return NULL;
                }
        } else
                fclose(fd);

        
        g_snprintf(log_all_file, 256, "%s/.gaim/%s/%s.log", getenv("HOME"), current_user->username, normalize(c->name));

        if (stat(log_all_file, &st) < 0)
                flag = 1;

        sprintf(debug_buff,"Logging to: \"%s\"\n", log_all_file);
        debug_print(debug_buff);

        fd = fopen(log_all_file, "a");

        if (flag) { /* is a new file */
		fprintf(fd, "<HTML><HEAD><TITLE>" );
		fprintf(fd, "IM Sessions with %s", c->name );
		fprintf(fd, "</TITLE></HEAD><BODY BGCOLOR=\"ffffff\">\n" );
        }
        return fd;
}


int escape_message(char *msg)
{
	char *c, *cpy;
	int cnt=0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */
	if (strlen(msg) > BUF_LEN) {
		sprintf(debug_buff, "Warning:  truncating message to 2048 bytes\n");
		debug_print(debug_buff);
		msg[2047]='\0';
	}
	
	cpy = g_strdup(msg);
	c = cpy;
	while(*c) {
		switch(*c) {
		case '$':
		case '[':
		case ']':
		case '(':
		case ')':
		case '#':
			msg[cnt++]='\\';
			/* Fall through */
		default:
			msg[cnt++]=*c;
		}
		c++;
	}
	msg[cnt]='\0';
	g_free(cpy);
	return cnt;
}

int escape_text(char *msg)
{
	char *c, *cpy;
	int cnt=0;
	/* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */
	if (strlen(msg) > BUF_LEN) {
		fprintf(stderr, "Warning:  truncating message to 2048 bytes\n");
		msg[2047]='\0';
	}
	
	cpy = g_strdup(msg);
	c = cpy;
	while(*c) {
                switch(*c) {
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
			msg[cnt++]='\\';
			/* Fall through */
		default:
			msg[cnt++]=*c;
		}
		c++;
	}
	msg[cnt]='\0';
	g_free(cpy);
	return cnt;
}

char * escape_text2(char *msg)
{
        char *c, *cpy;
	char *woo;
        int cnt=0;
        /* Assumes you have a buffer able to cary at least BUF_LEN * 2 bytes */
        if (strlen(msg) > BUF_LEN) {
                fprintf(stderr, "Warning:  truncating message to 2048 bytes\n");
                msg[2047]='\0';
        }

	woo = (char *)malloc(strlen(msg) * 2); 
        cpy = g_strdup(msg);
        c = cpy;
        while(*c) {
                switch(*c) {
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
                        woo[cnt++]='\\';
                        /* Fall through */
                default:
                        woo[cnt++]=*c;
                }
                c++;
        }
        woo[cnt]='\0';
        return woo;
}


char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                  "0123456789+/";
                  

char *tobase64(char *text)
{
	char *out = NULL;
	char *c;
	unsigned int tmp = 0;
	int len = 0, n = 0;
	
	c = text;
	
	while(c) {
		tmp = tmp << 8;
		tmp += *c;
		n++;

		if (n == 3) {
			out = g_realloc(out, len+4);
			out[len] = alphabet[(tmp >> 18) & 0x3f];
			out[len+1] = alphabet[(tmp >> 12) & 0x3f];
			out[len+2] = alphabet[(tmp >> 6) & 0x3f];
			out[len+3] = alphabet[tmp & 0x3f];
			len += 4;
			tmp = 0;
			n = 0;
		}
		c++;
	}
	switch(n) {
	
	case 2:
	        out = g_realloc(out, len+5);
	        out[len] = alphabet[(tmp >> 12) & 0x3f];
	        out[len+1] = alphabet[(tmp >> 6) & 0x3f];
	        out[len+2] = alphabet[tmp & 0x3f];
	        out[len+3] = '=';
	        out[len+4] = 0;
	        break;
	case 1:
	        out = g_realloc(out, len+4);
	        out[len] = alphabet[(tmp >> 6) & 0x3f];
	        out[len+1] = alphabet[tmp & 0x3f];
	        out[len+2] = '=';
	        out[len+3] = 0;
	        break;
	case 0:
		out = g_realloc(out, len+2);
		out[len] = '=';
		out[len+1] = 0;
		break;
	}
	return out;
}


char *frombase64(char *text)
{
        char *out = NULL;
        char tmp = 0;
        char *c;
        gint32 tmp2 = 0;
        int len = 0, n = 0;
        
        c = text;
        
        while(*c) {
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

        g_realloc(out, len+1);
        out[len] = 0;
        
        return out;
}


char *normalize(const char *s)
{
	static char buf[BUF_LEN];
        char *t, *u;
        int x=0;

        u = t = g_malloc(strlen(s) + 1);

        strcpy(t, s);
        g_strdown(t);
        
	while(*t) {
		if (*t != ' ') {
			buf[x] = *t;
			x++;
		}
		t++;
	}
        buf[x]='\0';
        g_free(u);
	return buf;
}

int query_state()
{
        return state;
}

void set_state(int i)
{
        state = i;
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
		icon_pm = gdk_pixmap_create_from_xpm_d(w, &icon_bm,
						       NULL, (gchar **)aimicon_xpm);
	}
	gdk_window_set_icon(w, NULL, icon_pm, icon_bm);
	if (mainwindow) gdk_window_set_group(w, mainwindow->window);
#endif
}

struct aim_user *find_user(const char *name)
{
        char *who = g_strdup(normalize(name));
        GList *usr = aim_users;
        struct aim_user *u;

        while(usr) {
                u = (struct aim_user *)usr->data;
                if (!strcmp(normalize(u->username), who)) {
                        g_free(who);
                        return u;
                }
                usr = usr->next;
        }
        g_free(who);
        return NULL;
}
